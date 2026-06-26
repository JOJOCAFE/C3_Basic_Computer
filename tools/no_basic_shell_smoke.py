#!/usr/bin/env python3
"""Serial smoke test that BASIC is not exposed by the micro Unix shell."""

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
    parser.add_argument("--timeout", type=float, default=7.0)
    args = parser.parse_args(argv)

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)
        require("READY." in banner or banner.endswith("> "), "did not see shell prompt")

        help_result = session.command("HELP", args.timeout)
        print_block("HELP", help_result.response)
        help_tokens = set(help_result.response.split())
        for removed in ("NEW", "LIST", "RUN", "LOAD", "SAVE", "DELETE", "PRINT", "DIR", "COPY", "MOVE", "RENAME"):
            require(removed not in help_tokens, f"HELP still exposes {removed}")

        for line in (
            "print",
            "PRINT 1+2",
            "10 PRINT \"HELLO\"",
            "NEW",
            "LIST",
            "RUN",
            "LOAD TEST",
            "SAVE TEST",
            "DELETE TEST",
            "DIR",
            "COPY A B",
            "MOVE A B",
            "RENAME A B",
        ):
            result = session.command(line, args.timeout)
            print_block(line, result.raw)
            require("UNKNOWN COMMAND" in result.raw, f"{line!r} was not rejected")
            require("cpu_start:" not in result.raw, f"board rebooted while rejecting {line!r}")

        print("PASS: BASIC commands are not exposed by the shell")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
