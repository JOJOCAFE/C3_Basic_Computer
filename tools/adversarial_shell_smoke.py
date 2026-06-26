#!/usr/bin/env python3
"""Adversarial serial smoke test for the ESP32-C3 BASIC shell.

This test intentionally sends malformed commands, unsafe paths, overlong input,
and fast command bursts. It passes only if the shell remains responsive and
keeps dangerous paths rejected.
"""

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

    def _read_until_quiet_prompt(self, timeout: float, quiet: float = 0.25) -> str:
        deadline = time.monotonic() + timeout
        buf = bytearray()
        last_data = time.monotonic()
        saw_prompt = False

        while time.monotonic() < deadline:
            chunk = self.ser.read(256)
            if chunk:
                buf.extend(chunk)
                last_data = time.monotonic()
                saw_prompt = buf.endswith(PROMPT)
                continue
            if saw_prompt and time.monotonic() - last_data >= quiet:
                return buf.decode("utf-8", errors="replace")
            time.sleep(0.01)

        raise TimeoutError(f"timed out waiting for quiet shell prompt; got:\n{buf.decode('utf-8', errors='replace')}")

    def resync(self, timeout: float = 5.0) -> str:
        self.ser.write(b"\r")
        self.ser.flush()
        return self._read_until_prompt(timeout)

    def command(self, line: str, timeout: float = 5.0) -> ShellResult:
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

    def write_raw_and_wait(self, data: bytes, timeout: float = 8.0) -> str:
        self.ser.write(data)
        self.ser.flush()
        return self._read_until_quiet_prompt(timeout)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def print_block(label: str, text: str) -> None:
    print(f"[{label}]")
    print(text.rstrip())


def require_responsive(session: ShellSession, timeout: float) -> None:
    result = session.command("HELP", timeout)
    print_block("HELP responsiveness check", result.response)
    require("HELP" in result.response and "RENEW" in result.response, "shell did not respond correctly to HELP")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=7.0)
    args = parser.parse_args(argv)

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)
        require("READY." in banner or banner.endswith("> "), "did not see boot prompt")

        for line in ("THIS IS NOT A COMMAND", "#$%^&*", "123ABC"):
            result = session.command(line, args.timeout)
            print_block(line, result.response)
            require("UNKNOWN COMMAND" in result.response, f"{line!r} did not fail cleanly")

        for line in (
            "PRINT 1+2",
            "10 PRINT \"NOPE\"",
            "NEW",
            "LIST",
            "LOAD X",
            "SAVE X",
            "DELETE X",
            "DIR",
            "COPY A B",
            "MOVE A B",
            "RENAME A B",
        ):
            result = session.command(line, args.timeout)
            print_block(line, result.response)
            require("UNKNOWN COMMAND" in result.response, f"{line!r} should not be exposed by the shell")

        result = session.command("RUN", args.timeout)
        print_block("RUN", result.response)
        require("Usage: RUN /bin/name.com [args...]" in result.response, "bare RUN did not show guarded runner usage")

        for line, expected in (
            ("LS ..", "/basic"),
            ("LS /../../", "/basic"),
            ("RM /", "Bad path"),
            ("RM -R /", "Bad path"),
        ):
            result = session.command(line, args.timeout)
            print_block(line, result.response)
            require(expected in result.response, f"{line!r} did not stay safely inside workspace")

        print_block("RENEW abort", "")
        self_check = session.write_raw_and_wait(b"RENEW\rN\r", args.timeout)
        print_block("RENEW abort raw", self_check)
        require("CANCELLED" in self_check, "RENEW did not abort on first non-Y confirmation")
        require_responsive(session, args.timeout)

        long_line = b"A" * 1400 + b"\r"
        long_raw = session.write_raw_and_wait(long_line, args.timeout)
        print_block("long-line flood", long_raw[-800:])
        require("UNKNOWN COMMAND" in long_raw, "overlong input did not fail as an unknown command")
        require_responsive(session, args.timeout)

        binaryish = b"\x00\xff\x1b[31m??\r"
        binary_raw = session.write_raw_and_wait(binaryish, args.timeout)
        print_block("binary-ish input", binary_raw)
        require(binary_raw.endswith("> "), "binary-ish input did not return to prompt")
        require_responsive(session, args.timeout)

        burst = b"NOPE\rLS ..\rRM ../../BAD\rHELP\r"
        burst_raw = session.write_raw_and_wait(burst, args.timeout)
        print_block("burst commands", burst_raw)
        require("UNKNOWN COMMAND" in burst_raw, "burst did not process unknown command")
        require("HELP" in burst_raw and "PWD" in burst_raw and "RENEW" in burst_raw, "burst did not recover to HELP")

        require_responsive(session, args.timeout)
        print("PASS: adversarial shell smoke test")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
