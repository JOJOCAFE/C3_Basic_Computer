#!/usr/bin/env python3
"""Smoke checks for the disjoint /bin hardware I2C command slice."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
import time

try:
    import serial
except ImportError:  # pragma: no cover - source-only mode does not need pyserial.
    serial = None


PROMPT = b"> "
REPO_ROOT = Path(__file__).resolve().parents[1]
I2C_SOURCE = REPO_ROOT / "source" / "bin" / "bin_hardware_i2c.c"
FORBIDDEN_INTEGRATION_FILES = (
    REPO_ROOT / "source" / "bin" / "bin_hardware.c",
    REPO_ROOT / "source" / "bin" / "bin_internal.h",
    REPO_ROOT / "source" / "bin" / "c3_bin_sources.cmake",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def source_contract_smoke() -> None:
    text = I2C_SOURCE.read_text(encoding="utf-8")

    require(
        "shell_status_t bin_hardware_i2c_exec(const char *args, const shell_exec_io_t *io)"
        in text,
        "missing dispatch-ready bin_hardware_i2c_exec function",
    )
    require("hardware_i2c_configure(&config)" in text, "config command does not call hardware_i2c_configure")
    require("hardware_i2c_probe(address" in text, "probe command does not call hardware_i2c_probe")
    require("hardware_i2c_is_configured()" in text, "scan command does not guard configured state")
    require('strcasecmp(action, "config")' in text, "missing config action")
    require('strcasecmp(action, "probe")' in text, "missing probe action")
    require('strcasecmp(action, "scan")' in text, "missing scan action")
    for path in FORBIDDEN_INTEGRATION_FILES:
        require(path.exists(), f"expected integration boundary file is missing: {path}")


def read_until(ser: serial.Serial, needle: bytes, timeout: float) -> bytes:
    deadline = time.monotonic() + timeout
    buf = bytearray()
    while time.monotonic() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf.extend(chunk)
            if needle in buf:
                return bytes(buf)
        else:
            time.sleep(0.01)
    raise TimeoutError(f"timed out waiting for {needle!r}; got:\n{buf.decode(errors='replace')}")


def send_command(ser: serial.Serial, command: str, timeout: float) -> str:
    ser.write(command.encode("utf-8") + b"\r")
    ser.flush()
    raw = read_until(ser, PROMPT, timeout)
    text = raw.decode("utf-8", errors="replace")
    print(text.rstrip())
    if "OK" not in text:
        raise AssertionError(f"command failed: {command}")
    return text


def serial_smoke(args: argparse.Namespace) -> None:
    require(serial is not None, "pyserial is required for --port smoke mode")

    with serial.Serial(args.port, args.baud, timeout=0.05, write_timeout=args.timeout) as ser:
        ser.reset_input_buffer()
        ser.write(b"\r")
        ser.flush()
        read_until(ser, PROMPT, args.timeout)

        text = send_command(
            ser,
            (
                f"/bin/hardware i2c config -sda {args.sda} -scl {args.scl} "
                f"-f {args.frequency} --pullups"
            ),
            args.timeout,
        )
        require("I2C CONFIG" in text, "I2C config confirmation missing")

        text = send_command(ser, f"/bin/hardware i2c probe -a {args.address}", args.timeout)
        require("I2C PROBE" in text, "I2C probe confirmation missing")

        text = send_command(ser, "/bin/hardware i2c scan", args.timeout)
        require("I2C SCAN" in text, "I2C scan confirmation missing")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Run firmware-backed commands on this serial port.")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--sda", type=int, default=6)
    parser.add_argument("--scl", type=int, default=7)
    parser.add_argument("--frequency", type=int, default=100000)
    parser.add_argument("--address", default="0x50")
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args(argv)

    source_contract_smoke()
    if args.port:
        serial_smoke(args)

    print("PASS: /bin hardware i2c slice")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
