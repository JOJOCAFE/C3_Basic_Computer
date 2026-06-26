#!/usr/bin/env python3
"""Run firmware-backed /bin hardware ADC read commands."""

from __future__ import annotations

import argparse
import re
import sys
import time

import serial


PROMPT = b"> "
ADC_RE = re.compile(r"ADC GPIO(?P<pin>\d+) RAW (?P<raw>\d+)(?: MV (?P<mv>\d+))?")


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
    return text


def validate_adc_read(text: str, pin: int) -> None:
    if "OK" not in text:
        raise AssertionError("ADC read did not report OK")

    match = ADC_RE.search(text)
    if not match:
        raise AssertionError("ADC read output did not include raw ADC data")

    got_pin = int(match.group("pin"))
    if got_pin != pin:
        raise AssertionError(f"ADC read reported GPIO{got_pin}, expected GPIO{pin}")

    raw = int(match.group("raw"))
    if raw < 0:
        raise AssertionError(f"ADC raw value was negative: {raw}")

    mv_text = match.group("mv")
    if mv_text is not None and int(mv_text) < 0:
        raise AssertionError(f"ADC millivolt value was negative: {mv_text}")


def validate_bad_input(text: str) -> None:
    if "Bad input" not in text:
        raise AssertionError("incomplete ADC command was not rejected as Bad input")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--pin", type=int, default=0)
    parser.add_argument("--reads", type=int, default=3)
    parser.add_argument("--interval", type=float, default=0.25)
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args(argv)

    if args.reads < 1:
        raise SystemExit("--reads must be at least 1")

    with serial.Serial(args.port, args.baud, timeout=0.05, write_timeout=args.timeout) as ser:
        ser.reset_input_buffer()
        ser.write(b"\r")
        ser.flush()
        read_until(ser, PROMPT, args.timeout)

        for _ in range(args.reads):
            text = send_command(
                ser,
                f"/bin/hardware adc read -p {args.pin}",
                args.timeout,
            )
            validate_adc_read(text, args.pin)
            time.sleep(args.interval)

        text = send_command(ser, "/bin/hardware adc read", args.timeout)
        validate_bad_input(text)

    print("PASS: /bin hardware adc")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
