#!/usr/bin/env bash
# build-nvs-tool.sh — Package zclaw-nvs-tool.py into a standalone executable
# via PyInstaller (no ESP-IDF / Python required on target machine).
#
# Usage:
#   ./scripts/build-nvs-tool.sh            # build for current platform
#   ./scripts/build-nvs-tool.sh --version  # print version and exit
#
# Output:
#   dist/zclaw-nvs-tool        (macOS / Linux)
#   dist/zclaw-nvs-tool.exe    (Windows)
#
# Prerequisites (auto-installed into an isolated venv):
#   uv  — https://github.com/astral-sh/uv

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TOOL_SRC="$SCRIPT_DIR/zclaw-nvs-tool.py"
DIST_DIR="$REPO_ROOT/dist"
VENV_DIR="$REPO_ROOT/.venv-nvs-tool"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

log() { printf '\033[1;34m[build-nvs-tool]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[build-nvs-tool] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# --version flag
# ---------------------------------------------------------------------------

if [[ "${1:-}" == "--version" ]]; then
    VERSION="$(grep -m1 'version' "$REPO_ROOT/pyproject.toml" 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || echo "dev")"
    echo "zclaw-nvs-tool $VERSION"
    exit 0
fi

# ---------------------------------------------------------------------------
# Sanity checks
# ---------------------------------------------------------------------------

command -v uv >/dev/null 2>&1 || die "uv not found. Install from https://github.com/astral-sh/uv"
[[ -f "$TOOL_SRC" ]] || die "Source not found: $TOOL_SRC"

# ---------------------------------------------------------------------------
# Set up isolated venv
# ---------------------------------------------------------------------------

log "Creating isolated venv at $VENV_DIR …"
uv venv --python 3.11 "$VENV_DIR" --quiet

# Activate
# shellcheck disable=SC1091
if [[ -f "$VENV_DIR/bin/activate" ]]; then
    source "$VENV_DIR/bin/activate"
elif [[ -f "$VENV_DIR/Scripts/activate" ]]; then
    # Windows Git Bash
    source "$VENV_DIR/Scripts/activate"
fi

log "Installing dependencies …"
uv pip install \
    --quiet \
    "esptool>=4.7" \
    "esp-idf-nvs-partition-gen>=0.1.2" \
    "pyinstaller>=6.0"

# ---------------------------------------------------------------------------
# Determine output name
# ---------------------------------------------------------------------------

PLATFORM="$(uname -s)"
ARCH="$(uname -m)"

case "$PLATFORM" in
    Darwin)  SUFFIX="" ;;
    Linux)   SUFFIX="" ;;
    MINGW*|MSYS*|CYGWIN*) SUFFIX=".exe" ;;
    *)       SUFFIX="" ;;
esac

BINARY_NAME="zclaw-nvs-tool${SUFFIX}"

# ---------------------------------------------------------------------------
# PyInstaller build
# ---------------------------------------------------------------------------

log "Building $BINARY_NAME ($PLATFORM $ARCH) …"

pyinstaller \
    --onefile \
    --name "zclaw-nvs-tool" \
    --distpath "$DIST_DIR" \
    --workpath "$REPO_ROOT/build/nvs-tool" \
    --specpath "$REPO_ROOT/build/nvs-tool" \
    --collect-data esptool \
    --collect-data espefuse \
    --clean \
    --noconfirm \
    "$TOOL_SRC"

# ---------------------------------------------------------------------------
# Result
# ---------------------------------------------------------------------------

BINARY_PATH="$DIST_DIR/$BINARY_NAME"
[[ -f "$BINARY_PATH" ]] || die "Build failed: $BINARY_PATH not found"

log "Build complete: $BINARY_PATH"
log "Size: $(du -sh "$BINARY_PATH" | cut -f1)"

# Quick smoke-test
log "Smoke test: $BINARY_PATH --help"
"$BINARY_PATH" --help | head -5

echo ""
echo "Distribute '$BINARY_NAME' to customers."
echo "They can run it without Python or ESP-IDF installed."
