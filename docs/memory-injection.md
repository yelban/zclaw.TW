# User memory injection into system prompt

## Problem

zclaw has persistent user memory via NVS (`u_*` keys written by `memory_set`). Data survives reboots, but the LLM loses all conversation context on restart. After reboot, the model doesn't know stored memories exist unless it proactively calls `memory_get` — which it may or may not do depending on the question.

Example: user sets `u_nickname=小蝦` via Telegram. Model confirms and remembers within the session. After reboot/reflash, asking "你是誰" returns the default "I'm zclaw" with no mention of the nickname.

## Solution

`build_system_prompt()` in `main/agent.c` now iterates all `u_*` keys from NVS at startup and appends them to the system prompt:

```
[Stored memories: u_nickname=小蝦;] Use these memories naturally in responses.
```

This ensures the model sees all stored memories on every boot without requiring a tool call.

## Implementation

In `build_system_prompt()`, after the custom prompt suffix block:

1. Open NVS namespace `"zclaw"` in read-only mode.
2. Iterate all string entries via `nvs_entry_find_in_handle`.
3. Filter to keys starting with `u_` (`memory_keys_is_user_key`).
4. Read each value via `memory_get` and append as `key=value;` pairs.
5. Wrap in `[Stored memories: ...] Use these memories naturally in responses.`

Buffer safety: stops appending if `s_system_prompt_buf` (2048 bytes) has less than 40 bytes remaining.

## Verification

```bash
# Check NVS for stored user memories
./dist/zclaw-nvs-tool --port /dev/cu.usbmodem... --read

# Should show under "=== User Memories ===" section
# Then ask via Telegram: "你是誰" — should mention stored info naturally
```

## Limitations

- Total system prompt buffer is 2048 bytes. With many/large user memories, some may be truncated.
- Memories are loaded once at prompt build time, not dynamically refreshed mid-conversation (a `memory_set` during a session takes effect on the next `build_system_prompt` call).
- Individual memory values are truncated to 128 chars during injection.
