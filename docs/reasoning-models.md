# Reasoning model support

## Problem

Reasoning models (DeepSeek R1, QwQ, etc.) on OpenRouter often return `content: null` with thinking in a `reasoning_content` field. Without handling this, zclaw shows "(No response)" to the user.

Some models also wrap their reasoning in `<think>...</think>` tags inside `reasoning_content`.

## Solution

`parse_openai_response()` in `main/json_util.c` now includes a fallback:

1. If `content` is null or empty, check `message.reasoning_content`.
2. Strip `<think>...</think>` tags from the reasoning text.
3. If only `<think>` content exists (no text after closing tag), use the inner thinking text.

The `extract_reasoning_content()` helper handles tag stripping with zero allocation (writes directly to the output buffer).

## Limitations

- Only the first `<think>...</think>` block is stripped. Multiple blocks are not handled.
- Some OpenRouter models don't support function calling at all — this fallback only helps with text responses.

## tool_choice

`build_openai_request()` now adds `"tool_choice": "auto"` when tools are present. This nudges models toward using function calls instead of describing tool invocations as plain text.

Some models (e.g. DeepSeek Chat v3, smaller Qwen variants) may still write tool calls as text on the first request. The system prompt includes an explicit instruction to always use function calls directly.

## Testing

Host tests in `test/host/test_json_util_integration.c`:

| Test | Validates |
|------|-----------|
| `reasoning_fallback` | `content:null` + `reasoning_content` -> extracts reasoning |
| `reasoning_prefers_content` | Both present -> prefers `content` |
| `reasoning_strips_think` | `<think>...</think>answer` -> extracts answer |
| `reasoning_think_only` | Only `<think>thinking</think>` -> extracts inner text |
| `tool_choice_present` | Tools present -> request includes `tool_choice: "auto"` |
| `tool_choice_absent_no_tools` | No tools -> no `tool_choice` field |
