# Configurable system prompt suffix

## Overview

The system prompt is composed of multiple parts at runtime:

```
SYSTEM_PROMPT (hardcoded in config.h)
+ device info (target chip, GPIO policy)
+ persona instruction
+ configurable suffix (this feature)
```

The suffix is the only part modifiable at runtime without reflashing.

## NVS key

`sys_prompt` in the `zclaw` namespace (max 500 chars).

## Compiled default

```c
#define DEFAULT_PROMPT_SUFFIX \
    "Respond to users in Traditional Chinese (zh-TW) using Taiwan-standard terminology."
```

## Logic

| NVS state | Behavior |
|-----------|----------|
| Key does not exist | Append `DEFAULT_PROMPT_SUFFIX` |
| Key has a value | Append that value |
| Key is empty string | Append nothing (user cleared all suffixes) |

## Tools

Three built-in tools exposed to the LLM:

- **`set_prompt`** — Set custom suffix (max 500 chars). Empty string disables all suffixes including default.
- **`get_prompt`** — Show current effective suffix.
- **`reset_prompt`** — Delete NVS key, restoring compiled default.

## Security

- The full `SYSTEM_PROMPT` is not exposed to tools (prevents prompt injection from reading constraints).
- `get_prompt` only returns the suffix portion.
- `sys_prompt` is not accessible via `memory_set`/`memory_get` (those require `u_` prefix).

## Examples via Telegram

```
User: 改成用英文回覆
Bot: (calls set_prompt with "Respond in English.")

User: 恢復預設的系統提示
Bot: (calls reset_prompt)

User: 不要任何額外提示
Bot: (calls set_prompt with "")
```

## zclaw-nvs-tool

`sys_prompt` is included in `--read` output. Values containing UTF-8 (e.g. Chinese) are displayed correctly.
