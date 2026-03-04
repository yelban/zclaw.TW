#!/usr/bin/env python3
"""zclaw-nvs-tool: Read/write NVS credentials on ESP32-S3 without ESP-IDF.

Usage examples:
  # Read current NVS from device
  python zclaw-nvs-tool.py --port /dev/cu.usbmodem1101 --read

  # Command-line mode
  python zclaw-nvs-tool.py --port /dev/cu.usbmodem1101 \
    --ssid "MyWiFi" --pass "MyPass" \
    --backend openai --api-key sk-xxx --model gpt-4o

  # Config file mode (.env format)
  python zclaw-nvs-tool.py --port /dev/cu.usbmodem1101 --config device.env

  # Dry-run (generate CSV only, no flash write)
  python zclaw-nvs-tool.py --dry-run --ssid Test --pass 12345678 \
    --backend openai --api-key sk-test --model gpt-4o

  # Interactive mode (prompts for missing required fields)
  python zclaw-nvs-tool.py --port /dev/cu.usbmodem1101
"""

import argparse
import getpass
import io
import os
import re
import subprocess
import sys
import tempfile


# ---------------------------------------------------------------------------
# NVS partition layout (must match partitions.csv)
# ---------------------------------------------------------------------------
NVS_OFFSET = "0x9000"
NVS_SIZE = 0x4000          # 16 KiB
NVS_NAMESPACE = "zclaw"


# ---------------------------------------------------------------------------
# CSV helpers
# ---------------------------------------------------------------------------

def csv_escape(value: str) -> str:
    """Escape a value for NVS CSV: strip CR/LF, double-quote internal quotes."""
    value = value.replace("\r", " ").replace("\n", " ")
    value = value.replace('"', '""')
    return f'"{value}"'


def build_nvs_csv(cfg: dict) -> str:
    """Build NVS partition CSV content from config dict."""
    lines = [
        "key,type,encoding,value",
        f"{NVS_NAMESPACE},namespace,,",
        f"wifi_ssid,data,string,{csv_escape(cfg['wifi_ssid'])}",
        f"wifi_pass,data,string,{csv_escape(cfg['wifi_pass'])}",
        f"llm_backend,data,string,{csv_escape(cfg['llm_backend'])}",
        f"api_key,data,string,{csv_escape(cfg['api_key'])}",
        f"llm_model,data,string,{csv_escape(cfg['llm_model'])}",
    ]
    if cfg.get("llm_api_url"):
        lines.append(f"llm_api_url,data,string,{csv_escape(cfg['llm_api_url'])}")
    if cfg.get("tg_token"):
        lines.append(f"tg_token,data,string,{csv_escape(cfg['tg_token'])}")
    if cfg.get("tg_chat_ids"):
        primary = cfg["tg_chat_ids"].split(",")[0].strip()
        lines.append(f"tg_chat_id,data,string,{csv_escape(primary)}")
        lines.append(f"tg_chat_ids,data,string,{csv_escape(cfg['tg_chat_ids'])}")
    if cfg.get("timezone"):
        lines.append(f"timezone,data,string,{csv_escape(cfg['timezone'])}")
    if cfg.get("persona"):
        lines.append(f"persona,data,string,{csv_escape(cfg['persona'])}")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Config file (.env format)
# ---------------------------------------------------------------------------

def load_env_file(path: str) -> dict:
    """Parse a .env-style file into a dict (KEY=VALUE, # comments ignored)."""
    result = {}
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if "=" not in line:
                continue
            key, _, val = line.partition("=")
            key = key.strip()
            val = val.strip()
            # Strip surrounding quotes
            if len(val) >= 2 and val[0] in ('"', "'") and val[-1] == val[0]:
                val = val[1:-1]
            result[key] = val
    return result


# ENV_KEY -> cfg_key mapping (provision-dev.sh variable names)
_ENV_MAP = {
    "WIFI_SSID":   "wifi_ssid",
    "WIFI_PASS":   "wifi_pass",
    "BACKEND":     "llm_backend",
    "API_KEY":     "api_key",
    "MODEL":       "llm_model",
    "API_URL":     "llm_api_url",
    "TG_TOKEN":    "tg_token",
    "TG_CHAT_IDS": "tg_chat_ids",
    "TIMEZONE":    "timezone",
    "PERSONA":     "persona",
}


def apply_env_file(cfg: dict, path: str) -> None:
    """Merge .env file into cfg (only fill empty keys)."""
    env = load_env_file(path)
    for env_key, cfg_key in _ENV_MAP.items():
        if env_key in env and not cfg.get(cfg_key):
            cfg[cfg_key] = env[env_key]


# ---------------------------------------------------------------------------
# Flash encryption detection
# ---------------------------------------------------------------------------

def flash_encryption_enabled(port: str) -> bool:
    """Return True if SPI flash encryption is enabled on the connected chip."""
    try:
        import espefuse  # type: ignore
        buf = io.StringIO()
        old_stdout, sys.stdout = sys.stdout, buf
        try:
            espefuse.main(["--port", port, "summary"])
        except SystemExit:
            pass
        finally:
            sys.stdout = old_stdout
        summary = buf.getvalue()
    except Exception:
        return False

    # Match SPI_BOOT_CRYPT_CNT (ESP32-S3) or FLASH_CRYPT_CNT (older chips)
    m = re.search(r"(?:SPI_BOOT_CRYPT_CNT|FLASH_CRYPT_CNT)\s*=\s*(\S+)", summary)
    if not m:
        return False
    raw = m.group(1)
    # Odd popcount → enabled
    try:
        val = int(raw, 0)
    except ValueError:
        return False
    return bin(val).count("1") % 2 == 1


# ---------------------------------------------------------------------------
# NVS bin generation
# ---------------------------------------------------------------------------

def generate_nvs_bin(csv_path: str, bin_path: str) -> None:
    """Generate NVS partition binary from CSV using nvs_partition_gen Python API."""
    import argparse as _ap
    from esp_idf_nvs_partition_gen import nvs_partition_gen  # type: ignore

    import os as _os
    args = _ap.Namespace(
        input=csv_path,
        output=bin_path,
        outdir=_os.path.dirname(bin_path),
        size=hex(NVS_SIZE),
        version=2,
        keygen=False,
        keyfile=None,
        inputkey=None,
    )
    nvs_partition_gen.generate(args)


# ---------------------------------------------------------------------------
# NVS read / dump
# ---------------------------------------------------------------------------

# All known string keys in namespace "zclaw" (order matches nvs_keys.h)
_KNOWN_KEYS = [
    "wifi_ssid",
    "wifi_pass",
    "llm_backend",
    "llm_model",
    "api_key",
    "llm_api_url",
    "tg_token",
    "tg_chat_id",
    "tg_chat_ids",
    "timezone",
    "persona",
    "sys_prompt",
]
_SENSITIVE_KEYS = {"wifi_pass", "api_key", "tg_token"}


def read_nvs_partition(port: str) -> bytes:
    """Dump NVS partition from device via esptool read_flash."""
    import esptool  # type: ignore
    import tempfile
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        tmp = f.name
    try:
        esptool.main(["--port", port, "read-flash", NVS_OFFSET, hex(NVS_SIZE), tmp])
        with open(tmp, "rb") as f:
            return f.read()
    finally:
        try:
            os.unlink(tmp)
        except OSError:
            pass


def _scan_nvs_strings(data: bytes) -> dict:
    """Scan NVS partition binary for known string keys.

    NVS entry layout (32 bytes, ESP-IDF nvs_types.hpp):
      byte  0   : namespace index
      byte  1   : type  (0x21 = SZ string)
      byte  2   : span  (total entries incl. this one)
      byte  3   : chunk index
      bytes 4-7 : CRC32 of entry header
      bytes 8-23: key (16 bytes, null-padded ASCII)
      bytes 24-25: data size (uint16 LE, for variable-length types)
      bytes 28-31: CRC32 of value data

    String value bytes follow in the next (span-1) entries as raw bytes.
    """
    results = {}
    ENTRY_SIZE = 32

    # Scan every 32-byte-aligned position in the partition
    for pos in range(0, len(data) - ENTRY_SIZE, ENTRY_SIZE):
        entry = data[pos: pos + ENTRY_SIZE]

        if entry[1] != 0x21:  # SZ string type only
            continue

        try:
            key = entry[8:24].rstrip(b"\x00").decode("ascii")
        except UnicodeDecodeError:
            continue
        if key not in _KNOWN_KEYS:
            continue

        span = entry[2]
        data_size = int.from_bytes(entry[24:26], "little")

        if span < 2 or data_size == 0 or data_size > (span - 1) * ENTRY_SIZE:
            continue

        val_bytes = data[pos + ENTRY_SIZE: pos + ENTRY_SIZE + data_size]
        try:
            val = val_bytes.rstrip(b"\x00").decode("utf-8")
        except UnicodeDecodeError:
            continue

        if val and all(ord(c) >= 0x20 or c in "\t\n" for c in val):
            results[key] = val

    return results


def cmd_read_nvs(port: str) -> int:
    """Read and display current NVS contents from device."""
    print(f"Reading NVS partition from {port} (offset {NVS_OFFSET}, size {hex(NVS_SIZE)})...")
    try:
        data = read_nvs_partition(port)
    except subprocess.CalledProcessError as e:
        print(f"Error reading flash: {e}", file=sys.stderr)
        return 1

    found = _scan_nvs_strings(data)

    if not found:
        print("No zclaw NVS entries found (device may be unprovisioned or flash-encrypted).")
        return 0

    print("\n=== Current NVS Configuration ===")
    for key in _KNOWN_KEYS:
        val = found.get(key)
        if val is None:
            continue
        display = _mask(val) if key in _SENSITIVE_KEYS else val
        print(f"  {key:<14}: {display}")
    print()
    return 0


# ---------------------------------------------------------------------------
# Flash write
# ---------------------------------------------------------------------------

def write_nvs_flash(port: str, bin_path: str, encrypt: bool) -> None:
    """Write nvs.bin to device using esptool."""
    import esptool  # type: ignore
    cmd = ["--port", port, "write_flash"]
    if encrypt:
        cmd.append("--encrypt")
    cmd += [NVS_OFFSET, bin_path]
    esptool.main(cmd)


# ---------------------------------------------------------------------------
# Interactive prompts
# ---------------------------------------------------------------------------

def _prompt(label: str, default: str = "", secret: bool = False) -> str:
    prompt_str = f"  {label}"
    if default:
        prompt_str += f" [{default}]"
    prompt_str += ": "
    if secret:
        val = getpass.getpass(prompt_str)
    else:
        val = input(prompt_str).strip()
    return val if val else default


def interactive_fill(cfg: dict) -> None:
    """Prompt user for any missing required fields."""
    print("\n=== zclaw NVS Provisioner — Interactive Mode ===")
    print("Press Enter to keep existing value; Ctrl+C to abort.\n")

    required = [
        ("wifi_ssid",   "WiFi SSID",         False),
        ("wifi_pass",   "WiFi password",      True),
        ("llm_backend", "LLM backend (e.g. openai, ollama)", False),
        ("api_key",     "API key",            True),
        ("llm_model",   "LLM model",          False),
    ]
    optional = [
        ("llm_api_url", "LLM API URL (optional)", False),
        ("tg_token",    "Telegram bot token (optional)", True),
        ("tg_chat_ids", "Telegram chat IDs, comma-separated, e.g. 123,456 (optional, supports multiple)", False),
        ("timezone",    "Timezone (optional, e.g. Asia/Taipei)", False),
    ]

    for key, label, secret in required:
        if not cfg.get(key):
            cfg[key] = _prompt(label, secret=secret)

    print()
    for key, label, secret in optional:
        if not cfg.get(key):
            val = _prompt(label, secret=secret)
            if val:
                cfg[key] = val


# ---------------------------------------------------------------------------
# Config display
# ---------------------------------------------------------------------------

def _mask(val: str) -> str:
    if len(val) <= 4:
        return val[:2] + "****"
    return val[:4] + "****" + val[-2:]


def show_config(cfg: dict, port: str) -> None:
    SENSITIVE = {"api_key", "wifi_pass", "tg_token"}
    print("\n=== Resolved Configuration ===")
    if port:
        print(f"  port        : {port}")
    for key, val in cfg.items():
        if not val:
            continue
        display = _mask(val) if key in SENSITIVE else val
        print(f"  {key:<14}: {display}")
    print()


# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Write NVS credentials to zclaw device (no ESP-IDF needed).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    p.add_argument("--port", "-p", metavar="PORT",
                   help="Serial port (e.g. /dev/cu.usbmodem1101, COM3)")
    p.add_argument("--config", metavar="FILE",
                   help="Load settings from .env file (provision-dev.sh compatible)")
    p.add_argument("--ssid",      metavar="SSID",    help="WiFi SSID")
    p.add_argument("--pass",      metavar="PASS",    dest="wifi_pass", help="WiFi password")
    p.add_argument("--backend",   metavar="BACKEND", help="LLM backend (openai/ollama/…)")
    p.add_argument("--api-key",   metavar="KEY",     help="LLM API key")
    p.add_argument("--model",     metavar="MODEL",   help="LLM model name")
    p.add_argument("--api-url",   metavar="URL",     help="LLM API URL (optional)")
    p.add_argument("--tg-token",  metavar="TOKEN",   help="Telegram bot token (optional)")
    p.add_argument("--tg-chat-ids", metavar="IDS",  help="Telegram allowed chat IDs, comma-separated (e.g. 123,456); supports multiple channels")
    p.add_argument("--timezone",  metavar="TZ",      help="Timezone string (optional)")
    p.add_argument("--read",      action="store_true",
                   help="Read and display current NVS from device (requires --port)")
    p.add_argument("--dry-run",   action="store_true",
                   help="Generate CSV/bin only; do not write to device")
    p.add_argument("--show-config", action="store_true",
                   help="Display resolved config (sensitive values masked) and exit")
    p.add_argument("--no-interactive", action="store_true",
                   help="Disable interactive prompts; fail if required fields missing")
    return p


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    # --read mode: dump current NVS from device
    if args.read:
        if not args.port:
            print("Error: --port is required for --read.", file=sys.stderr)
            return 1
        return cmd_read_nvs(args.port)

    # Build cfg dict from CLI args (explicit args take highest precedence)
    cfg: dict = {
        "wifi_ssid":   args.ssid or "",
        "wifi_pass":   args.wifi_pass or "",
        "llm_backend": args.backend or "",
        "api_key":     args.api_key or "",
        "llm_model":   args.model or "",
        "llm_api_url": args.api_url or "",
        "tg_token":    args.tg_token or "",
        "tg_chat_ids": args.tg_chat_ids or "",
        "timezone":    args.timezone or "",
    }

    # Overlay .env config file (CLI args take precedence)
    if args.config:
        if not os.path.exists(args.config):
            print(f"Error: config file not found: {args.config}", file=sys.stderr)
            return 1
        apply_env_file(cfg, args.config)

    # Read current NVS from device and fill any still-empty keys
    # Priority: CLI args > --config > device NVS > interactive
    if args.port and not args.dry_run:
        required_keys = ["wifi_ssid", "wifi_pass", "llm_backend", "api_key", "llm_model"]
        missing = [k for k in required_keys if not cfg.get(k)]
        if missing:
            print(f"Reading current NVS from {args.port} to fill missing fields: {', '.join(missing)}")
            try:
                nvs_data = read_nvs_partition(args.port)
                device_cfg = _scan_nvs_strings(nvs_data)
                for key, val in device_cfg.items():
                    if not cfg.get(key):
                        cfg[key] = val
                filled = [k for k in missing if cfg.get(k)]
                if filled:
                    print(f"  Loaded from device: {', '.join(filled)}")
            except Exception as e:
                print(f"  Warning: could not read device NVS ({e}), will prompt for missing fields.")

    # Interactive fill for missing required fields
    if not args.no_interactive and not args.show_config:
        required_keys = ["wifi_ssid", "wifi_pass", "llm_backend", "api_key", "llm_model"]
        missing = [k for k in required_keys if not cfg.get(k)]
        if missing:
            try:
                interactive_fill(cfg)
            except (KeyboardInterrupt, EOFError):
                print("\nAborted.", file=sys.stderr)
                return 1

    # Validate required fields
    required_keys = ["wifi_ssid", "wifi_pass", "llm_backend", "api_key", "llm_model"]
    missing = [k for k in required_keys if not cfg.get(k)]
    if missing:
        print(f"Error: missing required fields: {', '.join(missing)}", file=sys.stderr)
        print("Use --help for usage or omit --no-interactive for prompts.", file=sys.stderr)
        return 1

    if args.show_config:
        show_config(cfg, args.port or "")
        return 0

    if not args.dry_run and not args.port:
        print("Error: --port is required unless --dry-run is specified.", file=sys.stderr)
        return 1

    # Show config summary
    show_config(cfg, args.port or "(dry-run)")

    with tempfile.TemporaryDirectory(prefix="zclaw-nvs-") as tmpdir:
        csv_path = os.path.join(tmpdir, "nvs.csv")
        bin_path = os.path.join(tmpdir, "nvs.bin")

        # Write CSV
        csv_content = build_nvs_csv(cfg)
        with open(csv_path, "w", encoding="utf-8") as f:
            f.write(csv_content)

        if args.dry_run:
            print("=== Generated NVS CSV ===")
            print(csv_content)
            print("Dry-run complete. No device write performed.")
            return 0

        # Generate NVS binary
        print("Generating NVS binary...")
        try:
            generate_nvs_bin(csv_path, bin_path)
        except Exception as e:
            print(f"Error generating NVS binary: {e}", file=sys.stderr)
            print(
                "Ensure esp-idf-nvs-partition-gen is installed:\n"
                "  pip install esp-idf-nvs-partition-gen",
                file=sys.stderr,
            )
            return 1

        # Detect flash encryption
        print(f"Checking flash encryption on {args.port}...")
        encrypt = flash_encryption_enabled(args.port)
        if encrypt:
            print("  Flash encryption: ENABLED — using --encrypt flag")
        else:
            print("  Flash encryption: disabled")

        # Write to device
        print(f"Writing NVS to {args.port} at offset {NVS_OFFSET}...")
        try:
            write_nvs_flash(args.port, bin_path, encrypt)
        except subprocess.CalledProcessError as e:
            print(f"Error writing to device: {e}", file=sys.stderr)
            return 1

    print("\nProvisioning complete.")
    print(f"  WiFi SSID : {cfg['wifi_ssid']}")
    print(f"  Backend   : {cfg['llm_backend']}")
    print(f"  Model     : {cfg['llm_model']}")
    if cfg.get("llm_api_url"):
        print(f"  API URL   : {cfg['llm_api_url']}")
    print("\nBoard will reset automatically after write.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
