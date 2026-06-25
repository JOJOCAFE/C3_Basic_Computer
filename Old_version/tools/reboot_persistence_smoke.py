#!/usr/bin/env python3
"""Verify that a saved BASIC program survives an ESP32-C3 hardware reset."""

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
        self.ser = serial.Serial(
            port=port,
            baudrate=baud,
            timeout=timeout,
            write_timeout=timeout,
        )
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
        text = buf.decode("utf-8", errors="replace")
        raise TimeoutError(f"timed out waiting for shell prompt; got:\n{text}")

    def resync(self, timeout: float) -> str:
        self.ser.write(b"\r")
        self.ser.flush()
        return self._read_until_prompt(time.monotonic() + timeout)

    def command(self, line: str, timeout: float) -> ShellResult:
        self.ser.write(line.encode("utf-8") + b"\r")
        self.ser.flush()
        raw = self._read_until_prompt(time.monotonic() + timeout)
        text = raw[: -len(PROMPT)]
        if "\r\n" in text:
            response = text.split("\r\n", 1)[1]
        elif "\n" in text:
            response = text.split("\n", 1)[1]
        else:
            response = ""
        return ShellResult(raw=raw, response=response)

    def hardware_reset(self) -> None:
        self.ser.dtr = False
        self.ser.rts = True
        time.sleep(0.1)
        self.ser.rts = False


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

    token = f"PERSIST_{time.time_ns()}"
    target = f"TEMP/{token}"
    program_line = f'10 PRINT "{token}"'

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        print_block("before reset", session.resync(args.timeout))

        for command in ("NEW", program_line, f"SAVE {target}"):
            result = session.command(command, args.timeout)
            print_block(command, result.response)
            require("OK" in result.response, f"{command} did not return OK")

        print("[hardware reset]")
        session.hardware_reset()
        time.sleep(1.0)
        session.ser.reset_input_buffer()
        banner = session.resync(args.timeout)
        print_block("after reset", banner)
        require(
            "READY." in banner or "C3 BASIC COMPUTER" in banner or banner.endswith("> "),
            "shell did not return after hardware reset",
        )

        result = session.command(f"LOAD {target}", args.timeout)
        print_block(f"LOAD {target}", result.response)
        require("OK" in result.response, "saved program did not survive reset")

        result = session.command("RUN", args.timeout)
        print_block("RUN after reset", result.raw)
        require(token in result.raw, "reloaded program did not run after reset")

        result = session.command(f"DELETE {target}", args.timeout)
        print_block(f"DELETE {target}", result.response)
        require("OK" in result.response, "cleanup DELETE did not return OK")

        print(f"PASS: filesystem persistence across hardware reset for {target}")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
