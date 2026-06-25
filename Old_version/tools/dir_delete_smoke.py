#!/usr/bin/env python3
"""Serial smoke test for DIR / SAVE / DELETE on the ESP32-C3 BASIC shell.

This script talks to the firmware over USB serial, creates a disposable
program file in the TEMP area, verifies it appears in DIR output, deletes it,
and verifies it disappears again.
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
        self.ser = serial.Serial(port=port, baudrate=baud, timeout=timeout, write_timeout=timeout)
        self.ser.reset_input_buffer()

    def close(self) -> None:
        self.ser.close()

    def _read_until_prompt(self, deadline: float) -> str:
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

    def resync(self, timeout: float = 5.0) -> str:
        self.ser.write(b"\r")
        self.ser.flush()
        return self._read_until_prompt(time.monotonic() + timeout)

    def command(self, line: str, timeout: float = 5.0) -> ShellResult:
        self.ser.write(line.encode("utf-8") + b"\r")
        self.ser.flush()
        raw = self._read_until_prompt(time.monotonic() + timeout)
        text = raw[:-len(PROMPT)]
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
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--keep-file", action="store_true", help="leave the test file behind for inspection")
    args = parser.parse_args(argv)

    token = f"SMK_{time.time_ns()}"
    target = f"TEMP/{token}"
    program_line = '10 PRINT "DIR DELETE SMOKE"'

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)
        require(
            "READY." in banner or "C3 BASIC COMPUTER" in banner or banner.endswith("> "),
            "did not see boot banner / READY prompt",
        )

        result = session.command("HELP", args.timeout)
        print_block("HELP", result.response)
        require("DIR" in result.response and "DELETE" in result.response, "HELP output did not include shell commands")

        result = session.command("NEW", args.timeout)
        print_block("NEW", result.response)
        require("OK" in result.response, "NEW did not return OK")

        result = session.command(program_line, args.timeout)
        print_block(program_line, result.response)
        require("OK" in result.response, "storing BASIC line failed")

        result = session.command(f"SAVE {target}", args.timeout)
        print_block(f"SAVE {target}", result.response)
        require("OK" in result.response, "SAVE did not return OK")

        result = session.command("DIR TEMP", args.timeout)
        print_block("DIR TEMP", result.raw)
        require(token in result.raw, f"DIR TEMP did not show {token}")

        result = session.command(f"DELETE {target}", args.timeout)
        print_block(f"DELETE {target}", result.response)
        require("OK" in result.response, "DELETE did not return OK")

        if not args.keep_file:
            result = session.command("DIR TEMP", args.timeout)
            print_block("DIR TEMP after DELETE", result.raw)
            require(token not in result.raw, f"DIR TEMP still showed {token} after DELETE")

        print(f"PASS: DIR / DELETE smoke test for {target}")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
