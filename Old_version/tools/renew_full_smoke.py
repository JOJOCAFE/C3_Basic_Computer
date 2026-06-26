#!/usr/bin/env python3
"""Destructive serial smoke test for full RENEW on the ESP32-C3 BASIC shell."""

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
        raise TimeoutError(f"timed out waiting for shell prompt; got:\n{buf.decode('utf-8', errors='replace')}")

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

    def renew_yes_yes(self, timeout: float) -> str:
        self.ser.write(b"RENEW\rY\rY\r")
        self.ser.flush()
        return self._read_until_prompt(timeout)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def print_block(label: str, text: str) -> None:
    print(f"[{label}]")
    print(text.rstrip())


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args(argv)

    token = f"RENEW_{time.time_ns()}"
    target = f"TEMP/{token}"
    program_line = f'10 PRINT "{token}"'

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)
        require("READY." in banner or banner.endswith("> "), "did not see shell prompt")

        for command in ("NEW", program_line, f"SAVE {target}"):
            result = session.command(command, args.timeout)
            print_block(command, result.response)
            require("OK" in result.response, f"{command} did not return OK")

        result = session.command("DIR TEMP", args.timeout)
        print_block("DIR TEMP before RENEW", result.raw)
        require(token in result.raw, "marker file was not present before RENEW")

        renewed = session.renew_yes_yes(args.timeout)
        print_block("RENEW Y/Y", renewed)
        require("WARNING." in renewed, "RENEW warning was not shown")
        require("Do you understand? (Y/N)" in renewed, "first RENEW confirmation was not shown")
        require("Ready to continue? (Y/N)" in renewed, "second RENEW confirmation was not shown")
        require("OK" in renewed, "RENEW did not report OK")

        result = session.command("DIR TEMP", args.timeout)
        print_block("DIR TEMP after RENEW", result.raw)
        require(token not in result.raw, "marker file survived RENEW")

        result = session.command("DIR", args.timeout)
        print_block("DIR root after RENEW", result.raw)
        for dirname in ("basic", "asm", "bin", "config", "data", "temp"):
            require(dirname in result.raw, f"{dirname} directory missing after RENEW")

        result = session.command("HELP", args.timeout)
        print_block("HELP after RENEW", result.response)
        require("RENEW" in result.response and "SAVE" in result.response, "shell did not recover after RENEW")

        print(f"PASS: full RENEW wiped workspace marker and rebuilt layout for {target}")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
