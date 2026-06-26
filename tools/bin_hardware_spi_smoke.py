#!/usr/bin/env python3
"""Run firmware-backed /bin hardware SPI commands."""

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
    parser.add_argument("--mosi", type=int, required=True)
    parser.add_argument("--miso", type=int, required=True)
    parser.add_argument("--sclk", type=int, required=True)
    parser.add_argument("--cs", type=int)
    parser.add_argument("--frequency", type=int, default=1_000_000)
    parser.add_argument("--mode", type=int, default=0, choices=range(4))
    parser.add_argument("--tx", default="9F")
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args(argv)

    config = (
        f"/bin/hardware spi config -mosi {args.mosi} -miso {args.miso} "
        f"-sclk {args.sclk} -f {args.frequency} -m {args.mode}"
    )
    if args.cs is not None:
        config += f" -cs {args.cs}"

    with serial.Serial(args.port, args.baud, timeout=0.05, write_timeout=args.timeout) as ser:
        ser.reset_input_buffer()
        ser.write(b"\r")
        ser.flush()
        read_until(ser, PROMPT, args.timeout)

        text = send_command(ser, config, args.timeout)
        if "SPI CONFIG" not in text:
            raise AssertionError("spi config was not confirmed")

        text = send_command(ser, f"/bin/hardware spi xfer -tx {args.tx}", args.timeout)
        if "SPI RX " not in text:
            raise AssertionError("spi transfer did not return RX bytes")

    print("PASS: /bin hardware spi")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
