#!/usr/bin/env python3
"""Run firmware-backed /bin hardware GPIO commands."""

from __future__ import annotations

import argparse
import sys
import time

import serial


PROMPT = b"> "


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


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--pin", type=int, default=8)
    parser.add_argument("--seconds", type=int, default=10)
    parser.add_argument("--interval", type=float, default=0.25)
    parser.add_argument("--active-low", action="store_true")
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args(argv)

    on = 0 if args.active_low else 1
    off = 1 if args.active_low else 0
    command_timeout = max(args.timeout, args.interval + 2.0)

    with serial.Serial(args.port, args.baud, timeout=0.05, write_timeout=args.timeout) as ser:
        ser.reset_input_buffer()
        ser.write(b"\r")
        ser.flush()
        read_until(ser, PROMPT, args.timeout)

        text = send_command(
            ser,
            f"/bin/hardware gpio out -p {args.pin} -v {off}",
            command_timeout,
        )
        if f"GPIO{args.pin} OUT {off}" not in text:
            raise AssertionError("gpio output mode was not configured")

        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            text = send_command(
                ser,
                f"/bin/hardware gpio write -p {args.pin} -v {on}",
                command_timeout,
            )
            if f"GPIO{args.pin} WRITE {on}" not in text:
                raise AssertionError("gpio high write was not confirmed")
            time.sleep(args.interval)

            text = send_command(
                ser,
                f"/bin/hardware gpio write -p {args.pin} -v {off}",
                command_timeout,
            )
            if f"GPIO{args.pin} WRITE {off}" not in text:
                raise AssertionError("gpio low write was not confirmed")
            time.sleep(args.interval)

        send_command(ser, f"/bin/hardware gpio write -p {args.pin} -v {off}", command_timeout)

    print("PASS: /bin hardware gpio")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
