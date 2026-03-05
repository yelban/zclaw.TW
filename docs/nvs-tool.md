# zclaw-nvs-tool

Standalone NVS provisioning tool for zclaw devices. Reads and writes WiFi, LLM, and Telegram credentials without requiring ESP-IDF on the host machine.

## Build

```bash
./scripts/build-nvs-tool.sh
```

Produces `dist/zclaw-nvs-tool` (macOS/Linux) via PyInstaller. Requires `uv`.

### Pre-built binaries

Pre-built binaries are attached to each [GitHub Release](https://github.com/yelban/zclaw.TW/releases):

| Platform | Asset |
|----------|-------|
| macOS Apple Silicon | `zclaw-nvs-tool-macos-arm64` |
| macOS Intel | `zclaw-nvs-tool-macos-x64` |
| Linux x64 | `zclaw-nvs-tool-linux-x64` |
| Windows x64 | `zclaw-nvs-tool-windows-x64.exe` |

Download and run directly — no Python or ESP-IDF required.

## Usage

### Read current NVS

```bash
./dist/zclaw-nvs-tool --port /dev/cu.usbmodem1101 --read
```

Displays all known system keys: `wifi_ssid`, `wifi_pass`, `llm_backend`, `llm_model`, `api_key`, `llm_api_url`, `tg_token`, `tg_chat_id`, `tg_chat_ids`, `timezone`, `persona`, `sys_prompt`, `asr_api_url`, `asr_api_key`, `asr_model`. Sensitive values are masked.

User memories (`u_*` keys created via the `memory_set` tool) are shown in a separate **User Memories** section.

### Write credentials (CLI)

```bash
./dist/zclaw-nvs-tool --port /dev/cu.usbmodem1101 \
  --ssid "MyWiFi" --pass "MyPass" \
  --backend openrouter --api-key sk-or-xxx \
  --model "deepseek/deepseek-chat-v3-0324"
```

### Write ASR (voice transcription) credentials

```bash
./dist/zclaw-nvs-tool --port /dev/cu.usbmodem1101 \
  --asr-api-key sk-proj-xxx
```

Optional ASR parameters:

| Flag | NVS key | Default |
|------|---------|---------|
| `--asr-api-key` | `asr_api_key` | falls back to `api_key` |
| `--asr-api-url` | `asr_api_url` | `https://api.openai.com/v1/audio/transcriptions` |
| `--asr-model` | `asr_model` | `whisper-1` |

For Groq (free Whisper):

```bash
./dist/zclaw-nvs-tool --port /dev/cu.usbmodem1101 \
  --asr-api-url "https://api.groq.com/openai/v1/audio/transcriptions" \
  --asr-api-key gsk-xxx \
  --asr-model whisper-large-v3-turbo
```

### Write from config file

```bash
./dist/zclaw-nvs-tool --port /dev/cu.usbmodem1101 --config device.env
```

Config file uses `.env` format compatible with `provision-dev.sh`:

```
WIFI_SSID=MyWiFi
WIFI_PASS=secret
BACKEND=openrouter
API_KEY=sk-or-xxx
MODEL=deepseek/deepseek-chat-v3-0324
TG_TOKEN=123456:ABC
TG_CHAT_IDS=8536405120
TIMEZONE=Asia/Taipei
ASR_API_KEY=sk-proj-xxx
```

### Interactive mode

```bash
./dist/zclaw-nvs-tool --port /dev/cu.usbmodem1101
```

Prompts for missing required fields. Reads existing NVS first to pre-fill.

### Dry-run

```bash
./dist/zclaw-nvs-tool --dry-run --ssid Test --pass 12345678 \
  --backend openai --api-key sk-test --model gpt-4o
```

Generates CSV/bin without writing to device.

## Flash encryption

Automatically detects flash encryption via eFuse and adds `--encrypt` flag when writing.

## Build pitfalls

**Always use `./scripts/build-nvs-tool.sh`** to rebuild the binary. Do NOT run `pyinstaller` manually.

The build script creates an isolated `uv` venv (Python 3.11) with pinned `esptool`, `esp-idf-nvs-partition-gen`, and `pyinstaller`. Manual builds fail because:

| Mistake | Symptom |
|---------|---------|
| System pyinstaller without esptool | `ModuleNotFoundError: No module named 'esptool'` at runtime |
| ESP-IDF venv's esptool (v4.11) | `read-flash` vs `read_flash` naming mismatch (v4 uses underscore, v5 uses hyphen) |
| Wrong Python version | Bundled binary may crash on target machines |

The build script avoids all of these by pinning `esptool>=4.7` in a clean venv where PyInstaller collects the correct modules.

## Known issues

- `esptool` renamed `read-flash` to `read_flash` in v5.x. The tool uses the Python API directly (`esptool.main()`) so this is handled transparently.
- NVS scan uses binary pattern matching (not the NVS library), so only string-type entries in the `zclaw` namespace are returned.
- UTF-8 values (e.g. Chinese text in `sys_prompt`) are supported in the scanner.
