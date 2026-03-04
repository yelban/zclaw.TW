#include "tools_handlers.h"
#include "memory.h"
#include "nvs_keys.h"
#include "config.h"
#include "esp_err.h"
#include <string.h>
#include <stdio.h>

bool tools_set_prompt_handler(const cJSON *input, char *result, size_t result_len)
{
    cJSON *text_json = cJSON_GetObjectItem(input, "text");
    esp_err_t err;

    if (!text_json || !cJSON_IsString(text_json)) {
        snprintf(result, result_len, "Error: 'text' required (string)");
        return false;
    }

    const char *text = text_json->valuestring;
    if (strlen(text) > 500) {
        snprintf(result, result_len, "Error: text too long (max 500 chars)");
        return false;
    }

    err = memory_set(NVS_KEY_SYS_PROMPT, text);
    if (err != ESP_OK) {
        snprintf(result, result_len, "Error: %s", esp_err_to_name(err));
        return false;
    }

    if (text[0] == '\0') {
        snprintf(result, result_len,
                 "System prompt suffix cleared. No suffix will be appended (including default).");
    } else {
        snprintf(result, result_len, "System prompt suffix set. Takes effect next request.");
    }
    return true;
}

bool tools_get_prompt_handler(const cJSON *input, char *result, size_t result_len)
{
    char stored[512] = {0};

    (void)input;

    if (!memory_get(NVS_KEY_SYS_PROMPT, stored, sizeof(stored))) {
        snprintf(result, result_len, "Current suffix (compiled default): %s",
                 DEFAULT_PROMPT_SUFFIX);
    } else if (stored[0] == '\0') {
        snprintf(result, result_len, "Suffix is cleared (no suffix appended).");
    } else {
        snprintf(result, result_len, "Current suffix (custom): %s", stored);
    }
    return true;
}

bool tools_reset_prompt_handler(const cJSON *input, char *result, size_t result_len)
{
    (void)input;

    esp_err_t err = memory_delete(NVS_KEY_SYS_PROMPT);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        snprintf(result, result_len, "Error: %s", esp_err_to_name(err));
        return false;
    }

    snprintf(result, result_len, "System prompt suffix reset to compiled default: %s",
             DEFAULT_PROMPT_SUFFIX);
    return true;
}
