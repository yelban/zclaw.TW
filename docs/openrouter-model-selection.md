# OpenRouter Model Selection for zclaw

## Requirements

zclaw's LLM usage has specific constraints:

- **30 built-in tools** requiring reliable function/tool calling
- **Traditional Chinese (zh-TW)** user interaction
- **Concise responses** (max_tokens=1024, response buffer 16KB)
- **Low cost** for embedded device with frequent daily calls
- **Low latency** for interactive Telegram conversations

## Model Comparison (March 2026)

| Model | Input $/M | Output $/M | Tool Calling | zh-TW | Notes |
|-------|-----------|------------|-------------|-------|-------|
| `google/gemini-2.0-flash-001` | $0.10 | $0.40 | Stable | Excellent | **Recommended** |
| `google/gemini-2.5-flash-lite` | $0.10 | $0.40 | Stable | Excellent | Newer, similar price |
| `deepseek/deepseek-v3.2` | $0.25 | $0.40 | Strong | Excellent | Agentic pipeline optimized |
| `qwen/qwen3.5-35b-a3b` | $0.16 | $1.30 | Unstable | Good | MoE 3B active, tool call degrades |
| `minimax/minimax-m2.5` | varies | varies | OK | OK | Previous default |

## Decision: google/gemini-2.0-flash-001

Selected as new default OpenRouter model for these reasons:

1. **Cheapest input** ($0.10/M) and **low output** ($0.40/M)
2. **Output cost 3x cheaper** than qwen3.5-35b-a3b ($0.40 vs $1.30)
3. **Stable tool calling** — mature function calling implementation
4. **Excellent Chinese** comprehension and generation
5. **Fast inference** — optimized for low latency

## Known Issues with qwen3.5-35b-a3b

- Only 3B active parameters (MoE sparse model)
- [Tool calling degrades after first few calls](https://github.com/QwenLM/Qwen3.5/issues/12) — model outputs raw text instead of tool invocations
- Output pricing ($1.30/M) is expensive for the capability level

## Changing the Model

### Via NVS tool (from host)
```bash
# Write model to device NVS and reboot
./dist/zclaw-nvs-tool --port /dev/cu.usbmodem* --model google/gemini-2.0-flash-001
```

### Via config.h (compile-time default)
```c
#define LLM_DEFAULT_MODEL_OPENROUTER  "google/gemini-2.0-flash-001"
```

Note: Model is loaded at boot time (`llm_init()`). NVS changes require device reboot to take effect.
No Telegram tool exists to change the model at runtime (would also need reboot).
