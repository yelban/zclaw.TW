#ifndef NVS_KEYS_H
#define NVS_KEYS_H

// System/configuration keys stored in NVS namespace "zclaw".
#define NVS_KEY_BOOT_COUNT   "boot_count"
#define NVS_KEY_WIFI_SSID    "wifi_ssid"
#define NVS_KEY_WIFI_PASS    "wifi_pass"
#define NVS_KEY_LLM_BACKEND  "llm_backend"
#define NVS_KEY_API_KEY      "api_key"
#define NVS_KEY_LLM_MODEL    "llm_model"
#define NVS_KEY_LLM_API_URL  "llm_api_url"
#define NVS_KEY_TG_TOKEN     "tg_token"
#define NVS_KEY_TG_CHAT_ID   "tg_chat_id"
#define NVS_KEY_TG_CHAT_IDS  "tg_chat_ids"
#define NVS_KEY_TIMEZONE     "timezone"
#define NVS_KEY_PERSONA      "persona"
#define NVS_KEY_SYS_PROMPT   "sys_prompt"

// ASR (speech-to-text) keys.
#define NVS_KEY_ASR_API_URL  "asr_api_url"
#define NVS_KEY_ASR_API_KEY  "asr_api_key"
#define NVS_KEY_ASR_MODEL    "asr_model"

// Device aliases (GPIO name mapping).
#define NVS_KEY_DEV_ALIASES  "dev_aliases"

// Rate-limit bookkeeping keys.
#define NVS_KEY_RL_DAILY     "rl_daily"
#define NVS_KEY_RL_DAY       "rl_day"
#define NVS_KEY_RL_YEAR      "rl_year"

#endif // NVS_KEYS_H
