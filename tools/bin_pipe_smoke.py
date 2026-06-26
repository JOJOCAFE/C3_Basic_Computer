#!/usr/bin/env python3
"""Serial smoke test for one RAM-backed shell pipe."""

from __future__ import annotations

import argparse
import dataclasses
import sys
import time

import serial


PROMPT = b"> "


@dataclasses.dataclass
class ShellResult:
    raw: str
    response: str


class ShellSession:
    def __init__(self, port: str, baud: int, timeout: float) -> None:
        self.ser = serial.Serial(port=port, baudrate=baud, timeout=0.05, write_timeout=timeout)
        self.ser.reset_input_buffer()

    def close(self) -> None:
        self.ser.close()

    def _read_until_prompt(self, timeout: float) -> str:
        deadline = time.monotonic() + timeout
        buf = bytearray()
        while time.monotonic() < deadline:
            chunk = self.ser.read(256)
            if chunk:
                buf.extend(chunk)
                if buf.endswith(PROMPT):
                    return buf.decode("utf-8", errors="replace")
                continue
            time.sleep(0.01)
        raise TimeoutError(f"timed out waiting for prompt; got:\n{buf.decode('utf-8', errors='replace')}")

    def resync(self, timeout: float) -> str:
        self.ser.write(b"\r")
        self.ser.flush()
        return self._read_until_prompt(timeout)

    def command(self, line: str, timeout: float) -> ShellResult:
        self.ser.write(line.encode("utf-8") + b"\r")
        self.ser.flush()
        raw = self._read_until_prompt(timeout)
        text = raw[: -len(PROMPT)]
        if "\r\n" in text:
            response = text.split("\r\n", 1)[1]
        elif "\n" in text:
            response = text.split("\n", 1)[1]
        else:
            response = ""
        return ShellResult(raw=raw, response=response)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=7.0)
    args = parser.parse_args(argv)

    token = f"PIPE_{time.time_ns()}"
    src = f"/data/{token}_src.txt"
    dst = f"/data/{token}_dst.txt"
    payload = f"pipe-payload-{token}"

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        session.resync(args.timeout)

        result = session.command(f"WRITE {src} {payload}", args.timeout)
        require("OK" in result.response, "source WRITE failed")

        result = session.command(f"CAT {src} | WRITE {dst}", args.timeout)
        require("OK" in result.response, "pipe WRITE failed")

        result = session.command(f"CAT {dst}", args.timeout)
        require(payload in result.raw, "piped payload missing from destination")

        session.command(f"RM {src}", args.timeout)
        session.command(f"RM {dst}", args.timeout)

        print(f"PASS: one-pipe smoke test for {src} -> {dst}")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
