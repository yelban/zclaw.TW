## Codebase Overview

zclaw is an ESP-IDF firmware project for an embedded assistant that ingests local/Telegram/cron messages, routes them through an agent loop, optionally executes tools, and responds through queue-driven channels. The repository also includes a full host tooling layer for build/flash/provision/test workflows plus a static documentation site and CI/release automation.

**Stack**: C (ESP-IDF/FreeRTOS/NVS/WiFi/HTTP), shell + Python tooling, GitHub Actions, static HTML/CSS/JS docs.
**Structure**: `main/` firmware runtime, `scripts/` operational tooling, `test/` host and API tests, `docs-site/` documentation web content, `.github/workflows/` CI/CD.

For detailed architecture, see [docs/CODEBASE_MAP.md](docs/CODEBASE_MAP.md).

## Key Scripts

| Script | Purpose |
|--------|---------|
| `scripts/build.sh` | Build firmware (`idf.py build` wrapper) |
| `scripts/flash.sh [PORT]` | Flash firmware to device (auto-detects port) |
| `scripts/provision.sh` | Write NVS credentials via ESP-IDF toolchain |
| `scripts/provision-dev.sh` | Non-interactive provisioning from `~/.config/zclaw/dev.env` |
| `scripts/build-nvs-tool.sh` | **Rebuild `dist/zclaw-nvs-tool` binary** (uses `uv` + isolated venv; never run `pyinstaller` manually) |
| `scripts/test.sh host` | Run host unit tests |
| `scripts/monitor.sh [PORT]` | Serial monitor (requires TTY) |
| `scripts/serial-log.sh [PORT] [TIMEOUT]` | Non-interactive serial log reader (works in Claude Code) |

## Memory (Kiroku)
- Use memory_search to check relevant history before starting new tasks
- Use memory_save when the user says "remember" or states important decisions
- Use memory_forget when the user says "forget" or information is outdated
