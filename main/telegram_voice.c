#include "telegram_voice.h"
#include "config.h"
#include "nvs_keys.h"
#include "memory.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "tg_voice";

#define MULTIPART_BOUNDARY "----zclaw_asr_boundary"

// Static configuration loaded from NVS at init time.
static char s_asr_api_url[192];
static char s_asr_api_key[256];
static char s_asr_model[64];
static bool s_asr_configured = false;

// --- Testable helpers ---

bool telegram_voice_parse_file_path(const char *json, char *out_path, size_t out_path_size,
                                     size_t *out_file_size)
{
    if (!json || !out_path || out_path_size == 0) {
        return false;
    }

    if (out_file_size) {
        *out_file_size = 0;
    }

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }

    bool found = false;
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (result) {
        cJSON *file_path = cJSON_GetObjectItem(result, "file_path");
        if (file_path && cJSON_IsString(file_path) && file_path->valuestring[0] != '\0') {
            strncpy(out_path, file_path->valuestring, out_path_size - 1);
            out_path[out_path_size - 1] = '\0';
            found = true;
        }
        if (out_file_size) {
            cJSON *fs = cJSON_GetObjectItem(result, "file_size");
            if (fs && cJSON_IsNumber(fs)) {
                *out_file_size = (size_t)fs->valuedouble;
            }
        }
    }

    cJSON_Delete(root);
    return found;
}

bool telegram_voice_parse_transcription(const char *json, char *out_text, size_t out_text_size)
{
    if (!json || !out_text || out_text_size == 0) {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }

    bool found = false;
    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (text && cJSON_IsString(text)) {
        strncpy(out_text, text->valuestring, out_text_size - 1);
        out_text[out_text_size - 1] = '\0';
        found = true;
    }

    cJSON_Delete(root);
    return found;
}

int telegram_voice_build_multipart_preamble(char *buf, size_t buf_size,
                                             const char *model, const char *language,
                                             const char *prompt, const char *boundary)
{
    if (!buf || buf_size == 0 || !boundary) {
        return -1;
    }

    int n = snprintf(buf, buf_size,
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"language\"\r\n\r\n%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"prompt\"\r\n\r\n%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"voice.ogg\"\r\n"
        "Content-Type: audio/ogg\r\n\r\n",
        boundary,
        model ? model : ASR_DEFAULT_MODEL,
        boundary,
        language ? language : ASR_DEFAULT_LANGUAGE,
        boundary,
        prompt ? prompt : ASR_DEFAULT_PROMPT,
        boundary);

    if (n < 0 || (size_t)n >= buf_size) {
        return -1;
    }
    return n;
}

int telegram_voice_build_multipart_epilogue(char *buf, size_t buf_size, const char *boundary)
{
    if (!buf || buf_size == 0 || !boundary) {
        return -1;
    }

    int n = snprintf(buf, buf_size, "\r\n--%s--\r\n", boundary);
    if (n < 0 || (size_t)n >= buf_size) {
        return -1;
    }
    return n;
}

// --- Init ---

esp_err_t telegram_voice_init(void)
{
    s_asr_configured = false;

    // Load ASR API URL (default to OpenAI Whisper endpoint).
    if (!memory_get(NVS_KEY_ASR_API_URL, s_asr_api_url, sizeof(s_asr_api_url))) {
        strncpy(s_asr_api_url, ASR_DEFAULT_API_URL, sizeof(s_asr_api_url) - 1);
        s_asr_api_url[sizeof(s_asr_api_url) - 1] = '\0';
    }

    // Load ASR API key; fall back to the LLM api_key if not set.
    if (!memory_get(NVS_KEY_ASR_API_KEY, s_asr_api_key, sizeof(s_asr_api_key))) {
        ESP_LOGI(TAG, "No dedicated ASR key, trying LLM api_key fallback");
        if (!memory_get(NVS_KEY_API_KEY, s_asr_api_key, sizeof(s_asr_api_key))) {
            ESP_LOGW(TAG, "No ASR API key configured; voice transcription disabled");
            return ESP_OK;
        }
        ESP_LOGI(TAG, "Using LLM api_key as ASR fallback");
    }

    // Load ASR model (default to whisper-1).
    if (!memory_get(NVS_KEY_ASR_MODEL, s_asr_model, sizeof(s_asr_model))) {
        strncpy(s_asr_model, ASR_DEFAULT_MODEL, sizeof(s_asr_model) - 1);
        s_asr_model[sizeof(s_asr_model) - 1] = '\0';
    }

    s_asr_configured = true;
    ESP_LOGI(TAG, "ASR configured: url=%s model=%s", s_asr_api_url, s_asr_model);
    return ESP_OK;
}

bool telegram_voice_is_configured(void)
{
    return s_asr_configured;
}

// --- HTTP context for binary downloads ---

typedef struct {
    uint8_t *buf;
    size_t len;
    size_t capacity;
    bool overflow;
} binary_http_ctx_t;

static esp_err_t binary_http_event_handler(esp_http_client_event_t *evt)
{
    binary_http_ctx_t *ctx = (binary_http_ctx_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && ctx) {
        size_t remaining = ctx->capacity - ctx->len;
        if ((size_t)evt->data_len > remaining) {
            if (!ctx->overflow) {
                ctx->overflow = true;
                ESP_LOGW(TAG, "Binary download buffer overflow");
            }
            return ESP_OK;
        }
        memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
        ctx->len += evt->data_len;
    }
    return ESP_OK;
}

// --- Text HTTP response context ---

typedef struct {
    char *buf;
    size_t len;
    size_t capacity;
} text_http_ctx_t;

static esp_err_t text_http_event_handler(esp_http_client_event_t *evt)
{
    text_http_ctx_t *ctx = (text_http_ctx_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && ctx) {
        size_t remaining = ctx->capacity - ctx->len - 1; // reserve NUL
        size_t to_copy = (size_t)evt->data_len < remaining ? (size_t)evt->data_len : remaining;
        if (to_copy > 0) {
            memcpy(ctx->buf + ctx->len, evt->data, to_copy);
            ctx->len += to_copy;
            ctx->buf[ctx->len] = '\0';
        }
    }
    return ESP_OK;
}

// --- Step A: getFile ---

static esp_err_t get_telegram_file_path(const char *bot_token, const char *file_id,
                                         char *out_path, size_t out_path_size,
                                         size_t *out_file_size)
{
    char url[384];
    snprintf(url, sizeof(url), "%s%s/getFile?file_id=%s",
             TELEGRAM_API_URL, bot_token, file_id);

    char *resp_buf = calloc(1, ASR_RESPONSE_BUF_SIZE);
    if (!resp_buf) {
        return ESP_ERR_NO_MEM;
    }

    text_http_ctx_t ctx = { .buf = resp_buf, .len = 0, .capacity = ASR_RESPONSE_BUF_SIZE };

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = text_http_event_handler,
        .user_data = &ctx,
        .timeout_ms = ASR_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(resp_buf);
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "getFile failed: err=%s status=%d", esp_err_to_name(err), status);
        free(resp_buf);
        return ESP_FAIL;
    }

    bool parsed = telegram_voice_parse_file_path(resp_buf, out_path, out_path_size, out_file_size);
    free(resp_buf);

    if (!parsed) {
        ESP_LOGW(TAG, "Failed to parse file_path from getFile response");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// --- Step B: Download OGG ---

static esp_err_t download_telegram_file(const char *bot_token, const char *file_path,
                                         size_t file_size_hint,
                                         uint8_t **out_data, size_t *out_len)
{
    char url[384];
    snprintf(url, sizeof(url), "https://api.telegram.org/file/bot%s/%s",
             bot_token, file_path);

    // Use file_size from Telegram (+ small margin) instead of fixed max.
    size_t alloc_size = file_size_hint > 0 ? file_size_hint + 512 : ASR_MAX_VOICE_SIZE;
    if (alloc_size > ASR_MAX_VOICE_SIZE) {
        alloc_size = ASR_MAX_VOICE_SIZE;
    }

    // Allocate in PSRAM if available, fall back to regular heap.
    uint8_t *buf = heap_caps_malloc(alloc_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        buf = malloc(alloc_size);
    }
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes for voice download", (unsigned)alloc_size);
        return ESP_ERR_NO_MEM;
    }

    binary_http_ctx_t ctx = { .buf = buf, .len = 0, .capacity = alloc_size, .overflow = false };

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = binary_http_event_handler,
        .user_data = &ctx,
        .timeout_ms = ASR_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(buf);
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200 || ctx.overflow) {
        ESP_LOGW(TAG, "File download failed: err=%s status=%d overflow=%d",
                 esp_err_to_name(err), status, ctx.overflow);
        free(buf);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Downloaded voice file: %u bytes", (unsigned)ctx.len);
    *out_data = buf;
    *out_len = ctx.len;
    return ESP_OK;
}

// --- Step C: ASR POST ---

static esp_err_t asr_transcribe(const uint8_t *ogg_data, size_t ogg_len,
                                 char *out_text, size_t out_text_size)
{
    // Build preamble and epilogue to compute Content-Length.
    char preamble[512];
    char epilogue[64];

    int preamble_len = telegram_voice_build_multipart_preamble(
        preamble, sizeof(preamble), s_asr_model, ASR_DEFAULT_LANGUAGE,
        ASR_DEFAULT_PROMPT, MULTIPART_BOUNDARY);
    int epilogue_len = telegram_voice_build_multipart_epilogue(
        epilogue, sizeof(epilogue), MULTIPART_BOUNDARY);

    if (preamble_len < 0 || epilogue_len < 0) {
        ESP_LOGE(TAG, "Failed to build multipart body");
        return ESP_FAIL;
    }

    int content_length = preamble_len + (int)ogg_len + epilogue_len;

    char *resp_buf = calloc(1, ASR_RESPONSE_BUF_SIZE);
    if (!resp_buf) {
        return ESP_ERR_NO_MEM;
    }

    text_http_ctx_t resp_ctx = { .buf = resp_buf, .len = 0, .capacity = ASR_RESPONSE_BUF_SIZE };

    esp_http_client_config_t config = {
        .url = s_asr_api_url,
        .method = HTTP_METHOD_POST,
        .event_handler = text_http_event_handler,
        .user_data = &resp_ctx,
        .timeout_ms = ASR_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(resp_buf);
        return ESP_FAIL;
    }

    // Set headers.
    char content_type[80];
    snprintf(content_type, sizeof(content_type),
             "multipart/form-data; boundary=%s", MULTIPART_BOUNDARY);
    esp_http_client_set_header(client, "Content-Type", content_type);

    char auth_header[280];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", s_asr_api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);

    // Stream write: open → preamble → OGG → epilogue → finish.
    esp_err_t err = esp_http_client_open(client, content_length);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ASR HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(resp_buf);
        return ESP_FAIL;
    }

    // Write preamble.
    int written = esp_http_client_write(client, preamble, preamble_len);
    if (written != preamble_len) {
        ESP_LOGW(TAG, "ASR preamble write failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(resp_buf);
        return ESP_FAIL;
    }

    // Write OGG binary in chunks.
    size_t offset = 0;
    while (offset < ogg_len) {
        size_t chunk = ogg_len - offset;
        if (chunk > 4096) {
            chunk = 4096;
        }
        written = esp_http_client_write(client, (const char *)(ogg_data + offset), chunk);
        if (written <= 0) {
            ESP_LOGW(TAG, "ASR OGG write failed at offset %u", (unsigned)offset);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            free(resp_buf);
            return ESP_FAIL;
        }
        offset += written;
    }

    // Write epilogue.
    written = esp_http_client_write(client, epilogue, epilogue_len);
    if (written != epilogue_len) {
        ESP_LOGW(TAG, "ASR epilogue write failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(resp_buf);
        return ESP_FAIL;
    }

    // Read response.
    int content_len_resp = esp_http_client_fetch_headers(client);
    (void)content_len_resp;

    // The event handler collects the response body into resp_buf via text_http_event_handler.
    // We need to read the response to trigger the event handler.
    int read_len;
    char read_buf[256];
    while ((read_len = esp_http_client_read(client, read_buf, sizeof(read_buf))) > 0) {
        // text_http_event_handler already captures into resp_ctx
    }

    int status = esp_http_client_get_status_code(client);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status != 200) {
        ESP_LOGW(TAG, "ASR API returned status %d: %s", status, resp_buf);
        free(resp_buf);
        return ESP_FAIL;
    }

    bool parsed = telegram_voice_parse_transcription(resp_buf, out_text, out_text_size);
    free(resp_buf);

    if (!parsed) {
        ESP_LOGW(TAG, "Failed to parse ASR response");
        return ESP_FAIL;
    }

    if (out_text[0] == '\0') {
        ESP_LOGW(TAG, "ASR returned empty transcription");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// --- Public API ---

esp_err_t telegram_voice_transcribe(const char *bot_token, const char *file_id,
                                     char *out_text, size_t out_text_size)
{
    if (!bot_token || !file_id || !out_text || out_text_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_asr_configured) {
        return ESP_ERR_INVALID_STATE;
    }

    // Step A: Get file path from Telegram.
    char file_path[128];
    size_t file_size = 0;
    esp_err_t err = get_telegram_file_path(bot_token, file_id, file_path, sizeof(file_path), &file_size);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "Voice file_path: %s (size=%u)", file_path, (unsigned)file_size);

    // Step B: Download OGG file.
    uint8_t *ogg_data = NULL;
    size_t ogg_len = 0;
    err = download_telegram_file(bot_token, file_path, file_size, &ogg_data, &ogg_len);
    if (err != ESP_OK) {
        return err;
    }

    // Step C: Send to ASR API.
    err = asr_transcribe(ogg_data, ogg_len, out_text, out_text_size);
    free(ogg_data);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Transcription: %s", out_text);
    }

    return err;
}
