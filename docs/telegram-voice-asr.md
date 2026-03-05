# Telegram 語音訊息 → ASR 轉文字

## Context

zclaw 目前只處理 Telegram 文字訊息，語音訊息被靜默丟棄。目標：偵測語音 → 下載 OGG → 送 Whisper-compatible ASR API → 轉錄文字 → 當成一般文字訊息處理。

使用者選擇**可設定端點**方案：NVS 存 ASR URL + key，預設 OpenAI Whisper。

---

## 架構流程

```
getUpdates → 偵測 voice → getFile(file_id) → 下載 OGG 到 PSRAM
→ POST multipart/form-data 到 ASR → 解析 {"text":"..."} → channel_msg_t → xQueueSend → agent
```

三個 HTTP 操作循序執行，在 poll task 內完成（已有 8KB stack + 30s blocking 設計）。

---

## 修改檔案

| 檔案 | 動作 |
|------|------|
| `main/nvs_keys.h` | +`NVS_KEY_ASR_API_URL`, `NVS_KEY_ASR_API_KEY`, `NVS_KEY_ASR_MODEL` |
| `main/config.h` | +ASR 常數（預設 URL、model、language、buffer size） |
| `main/telegram_voice.h` | 新檔：API 宣告 |
| `main/telegram_voice.c` | 新檔：getFile + 下載 OGG + multipart POST + 解析轉錄 |
| `main/telegram.c` | poll loop 加 voice 偵測分支 + init 呼叫 |
| `main/CMakeLists.txt` | +`telegram_voice.c` |
| `scripts/provision.sh` | +`--asr-api-url`, `--asr-api-key` 參數 |
| `scripts/provision-dev.sh` | +ASR env vars |
| `scripts/zclaw-nvs-tool.py` | `_KNOWN_KEYS` 加兩個 ASR key |
| `test/host/test_telegram_voice.c` | 新檔：5 個測試 |
| `scripts/test.sh` | 編譯清單加新檔 |

---

## 詳細變更

### 1. `main/nvs_keys.h`

```c
#define NVS_KEY_ASR_API_URL  "asr_api_url"
#define NVS_KEY_ASR_API_KEY  "asr_api_key"
#define NVS_KEY_ASR_MODEL    "asr_model"
```

### 2. `main/config.h`

```c
#define ASR_DEFAULT_API_URL     "https://api.openai.com/v1/audio/transcriptions"
#define ASR_DEFAULT_MODEL       "whisper-1"
#define ASR_DEFAULT_LANGUAGE    "zh"
#define ASR_DEFAULT_PROMPT      "以下是繁體中文語音轉錄。"
#define ASR_MAX_VOICE_SIZE      (128 * 1024)   // 128 KB max OGG
#define ASR_HTTP_TIMEOUT_MS     30000
#define ASR_RESPONSE_BUF_SIZE   1024
```

### 3. `main/telegram_voice.h` — 新檔

```c
esp_err_t telegram_voice_init(void);
bool telegram_voice_is_configured(void);
esp_err_t telegram_voice_transcribe(const char *bot_token, const char *file_id,
                                     char *out_text, size_t out_text_size);
```

### 4. `main/telegram_voice.c` — 新檔（核心）

**靜態狀態：** `s_asr_api_url[192]`, `s_asr_api_key[256]`, `s_asr_model[64]`, `s_asr_configured`

**`telegram_voice_init()`：**
- NVS 載入 `asr_api_url`（缺省 → `ASR_DEFAULT_API_URL`）
- NVS 載入 `asr_api_key`，若無 → fallback 到 `api_key`（LLM key）
- NVS 載入 `asr_model`（缺省 → `ASR_DEFAULT_MODEL` = `whisper-1`）
- 無 key → `s_asr_configured = false`

**`telegram_voice_transcribe()` 三步驟：**

| 步驟 | 動作 | 備註 |
|------|------|------|
| A. getFile | `GET /bot<TOKEN>/getFile?file_id=<ID>` | 解析 `result.file_path` |
| B. 下載 OGG | `GET /file/bot<TOKEN>/<file_path>` | PSRAM malloc，binary handler |
| C. ASR POST | `POST <asr_url>` multipart/form-data | 串流寫入：preamble → OGG binary → epilogue |

**Multipart 結構（固定 boundary）：**
```
------zclaw_asr_boundary\r\n
Content-Disposition: form-data; name="model"\r\n\r\nwhisper-1\r\n
------zclaw_asr_boundary\r\n
Content-Disposition: form-data; name="language"\r\n\r\nzh\r\n
------zclaw_asr_boundary\r\n
Content-Disposition: form-data; name="prompt"\r\n\r\n以下是繁體中文語音轉錄。\r\n
------zclaw_asr_boundary\r\n
Content-Disposition: form-data; name="file"; filename="voice.ogg"\r\n
Content-Type: audio/ogg\r\n\r\n
<OGG BINARY>\r\n
------zclaw_asr_boundary--\r\n
```

**`prompt` 欄位**：Whisper 只支援 `zh`（不支援 `zh-TW`），輸出可能是簡體或繁體。加 `prompt` 繁體範例文字可強烈引導輸出繁體中文。

**model 可配置**：OpenAI API 只能用 `whisper-1`（= large-v2）。Groq/自架可用 `whisper-large-v3`。預設 `whisper-1`，可透過 NVS 覆寫。

用 `esp_http_client_open()` + `esp_http_client_write()` 串流，不需在 RAM 組裝完整 body。算好 Content-Length = preamble_len + ogg_size + epilogue_len。

### 5. `main/telegram.c` — 整合

**init：** `telegram_init()` 尾端呼叫 `telegram_voice_init()`

**poll loop：** 現有 text 處理的 `if` 後加 `else if`：
- 檢查 `telegram_voice_is_configured()`
- 取 `message.voice` object
- 驗證 chat_id 授權（同 text 邏輯）
- 檢查 `file_size <= ASR_MAX_VOICE_SIZE`
- 呼叫 `telegram_voice_transcribe(s_bot_token, file_id, buf, 512)`
- 成功 → 組 `channel_msg_t` → `xQueueSend`

### 6. 供應腳本

**`provision.sh`：** 加 `--asr-api-url`, `--asr-api-key` 參數 → 寫入 NVS CSV
**`provision-dev.sh`：** 加 `ZCLAW_ASR_API_URL`, `ZCLAW_ASR_API_KEY` env vars
**`zclaw-nvs-tool.py`：** `_KNOWN_KEYS` + `_SENSITIVE_KEYS` 加入 ASR key

### 7. Host tests（5 個）

| 測試 | 驗證 |
|------|------|
| `multipart_body_construction` | preamble/epilogue 格式正確 |
| `getfile_response_parsing` | mock JSON 提取 file_path |
| `asr_response_parsing` | `{"text":"hello"}` 提取文字 |
| `asr_response_empty` | `{"text":""}` 不 crash |
| `voice_not_configured` | 未設定 key 時回傳 false |

---

## 資源估算

| 項目 | 預估 |
|------|------|
| 韌體增量 | ~4-5 KiB（剩餘 ~38 KiB） |
| 內部 heap 峰值 | ~60 KB（單一 TLS session） |
| PSRAM 用量 | 最多 128 KB（OGG 暫存） |
| 語音處理延遲 | ~6-8 秒（三步 HTTP） |

---

## 驗證

```bash
./scripts/test.sh host
./scripts/build.sh
./scripts/flash.sh /dev/cu.usbmodem...
./scripts/provision-dev.sh --asr-api-key sk-xxx
# Telegram: 傳語音「開啟 GPIO 4」→ LED 亮
# Telegram: 傳語音「你是誰」→ 回覆含小蝦暱稱
./dist/zclaw-nvs-tool --port ... --read   # 確認 asr_api_key 出現
```

---

## 已知問題與解法

### OGG 記憶體分配失敗（已修復）

**症狀：** `Failed to allocate 131072 bytes for voice download` / `ESP_ERR_NO_MEM`

**原因：** 初版固定分配 `ASR_MAX_VOICE_SIZE`（128 KB），但裝置未啟用 PSRAM（`CONFIG_SPIRAM is not set`），內部 heap 最大連續 block 僅 ~73 KB。

**解法：** `download_telegram_file()` 改為接收 Telegram `getFile` 回傳的 `file_size`，以實際大小 + 512 bytes margin 動態分配。典型語音檔 5-20 KB，遠小於 73 KB 限制。

### ASR API Key 讀取失敗（已修復）

**症狀：** `nvs_get_str('asr_api_key') failed: ESP_ERR_NVS_INVALID_LENGTH (0x110c)`，導致 fallback 到 LLM `api_key`（OpenRouter key），ASR 回傳 401。

**原因：** `s_asr_api_key` buffer 原為 128 bytes，但 OpenAI 新版 project API key（`sk-proj-...`）長度通常 160-200+ 字元，超過 buffer 容量。ESP-IDF `nvs_get_str()` 在 buffer 不足時回傳 `ESP_ERR_NVS_INVALID_LENGTH` 而非截斷。

**解法：** `s_asr_api_key` 從 128 → 256 bytes，`auth_header` 從 160 → 280 bytes。

**教訓：** OpenAI API key 有多種格式：
| 格式 | 長度 | 範例前綴 |
|------|------|----------|
| Legacy | ~51 字元 | `sk-...` |
| Project | 160-200+ 字元 | `sk-proj-...` |
| Service account | ~160 字元 | `sk-svcacct-...` |

NVS string buffer 應預留足夠空間（256+ bytes）給 API key 類欄位。

### 診斷技巧

在 `memory_get()` 加入失敗 log（`ESP_LOGD` 層級）可快速定位 NVS 讀取問題：

```c
ESP_LOGD(TAG, "nvs_get_str('%s') failed: %s (0x%x)", key, esp_err_to_name(err), err);
```

常見 NVS 錯誤碼：
| 錯誤碼 | 意義 |
|--------|------|
| `ESP_ERR_NVS_NOT_FOUND` (0x1102) | key 不存在（正常，未設定） |
| `ESP_ERR_NVS_INVALID_LENGTH` (0x110c) | buffer 太小，需加大 |

---

## 未解決

- 語音超過 128KB（~30-40 秒）→ 跳過 + log 警告
- 無 PSRAM 板子 → graceful fallback（malloc 失敗時 skip）
- ASR 語言碼硬編碼 `zh`（Whisper 不支援 `zh-TW`），用 `prompt` 引導繁體輸出
- ASR model 可透過 NVS `asr_model` 覆寫（Groq/自架用 `whisper-large-v3`）
- poll task 處理語音期間（~6-8s）暫停收新訊息，可接受
