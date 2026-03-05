#!/bin/bash
# Non-interactive serial log reader for zclaw.
# Works in non-TTY environments (e.g. Claude Code).
# Uses ESP-IDF Python venv's pyserial.
#
# Usage: ./scripts/serial-log.sh [PORT] [TIMEOUT_SECONDS]
#   PORT    - serial device (auto-detected if omitted)
#   TIMEOUT - idle timeout in seconds (default: 60)

set -e

# --- Locate ESP-IDF Python with pyserial ---
ESP_PYTHON=""
for candidate in \
    "$HOME/.espressif/python_env/idf5.4_py3.14_env/bin/python3" \
    "$HOME/.espressif/python_env/idf5.4_py3.14_env/bin/python" \
    "$HOME/.espressif/python_env"/idf*_env/bin/python3 \
    "$HOME/.espressif/python_env"/idf*_env/bin/python; do
    if [ -x "$candidate" ] 2>/dev/null; then
        if "$candidate" -c "import serial" 2>/dev/null; then
            ESP_PYTHON="$candidate"
            break
        fi
    fi
done

if [ -z "$ESP_PYTHON" ]; then
    echo "Error: No Python with pyserial found in ~/.espressif/python_env/" >&2
    echo "Run: pip install pyserial (in ESP-IDF venv)" >&2
    exit 1
fi

# --- Auto-detect serial port ---
PORT="${1:-}"
if [ -z "$PORT" ]; then
    shopt -s nullglob
    if [ "$(uname -s)" = "Darwin" ]; then
        ports=(/dev/cu.usbmodem* /dev/cu.usbserial-*)
    else
        ports=(/dev/ttyUSB* /dev/ttyACM*)
    fi
    shopt -u nullglob

    if [ "${#ports[@]}" -eq 0 ]; then
        echo "Error: No serial port detected." >&2
        exit 1
    fi
    PORT="${ports[0]}"
fi

TIMEOUT="${2:-60}"

echo "serial-log: port=$PORT timeout=${TIMEOUT}s python=$ESP_PYTHON"

# --- Read serial via pyserial ---
exec "$ESP_PYTHON" -u -c "
import serial, sys, time

port = sys.argv[1]
timeout = int(sys.argv[2])

try:
    ser = serial.Serial(port, 115200, timeout=1)
except Exception as e:
    print(f'Error opening {port}: {e}', file=sys.stderr)
    sys.exit(1)

print(f'Listening on {port} (idle timeout {timeout}s)...', flush=True)
last_activity = time.time()

try:
    while True:
        line = ser.readline()
        if line:
            try:
                print(line.decode('utf-8', errors='replace').rstrip(), flush=True)
            except Exception:
                print(repr(line), flush=True)
            last_activity = time.time()
        elif time.time() - last_activity > timeout:
            print(f'--- {timeout}s idle timeout ---', flush=True)
            break
except KeyboardInterrupt:
    print('--- interrupted ---', flush=True)
finally:
    ser.close()
" "$PORT" "$TIMEOUT"
