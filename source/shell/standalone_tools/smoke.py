#!/usr/bin/env python3
"""Smoke test for source/shell as a standalone C3 shell project."""

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

    def read_until_text(self, needle: str, timeout: float) -> str:
        deadline = time.monotonic() + timeout
        buf = bytearray()
        needle_text = needle
        while time.monotonic() < deadline:
            chunk = self.ser.read(256)
            if chunk:
                buf.extend(chunk)
                text = buf.decode("utf-8", errors="replace")
                if needle_text in text:
                    return text
                continue
            time.sleep(0.01)
        raise TimeoutError(f"timed out waiting for {needle!r}; got:\n{buf.decode('utf-8', errors='replace')}")

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


def renew(session: ShellSession, timeout: float) -> None:
    session.ser.write(b"RENEW\r")
    session.ser.flush()
    raw = session.read_until_text("Do you understand?", timeout)
    session.ser.write(b"Y\r")
    session.ser.flush()
    raw += session.read_until_text("Ready to continue?", timeout)
    session.ser.write(b"Y\r")
    session.ser.flush()
    raw += session._read_until_prompt(timeout)
    print_block("RENEW", raw)
    require("OK" in raw, "RENEW did not complete")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args(argv)

    token = f"SS_{time.time_ns()}"
    dirname = f"temp/{token}"

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)
        require("READY." in banner or banner.endswith("> "), "did not see shell prompt")

        renew(session, args.timeout)

        result = session.command("HELP", args.timeout)
        print_block("HELP", result.response)
        for command in ("HELP", "PWD", "LS", "CD", "MKDIR", "RMDIR", "CAT", "WRITE", "RM", "CP", "MV", "RENEW"):
            require(command in result.response.split(), f"HELP missing {command}")

        for removed in ("DIR", "COPY", "MOVE", "DELETE", "RENAME", "NEW", "LIST", "RUN", "LOAD", "SAVE", "PRINT"):
            require(removed not in result.response.split(), f"HELP exposes removed command {removed}")

        result = session.command("LS", args.timeout)
        print_block("LS root", result.raw)
        for root_dir in ("basic", "asm", "bin", "config", "data", "temp"):
            require(f"/{root_dir}" in result.raw, f"LS missing /{root_dir}")

        result = session.command(f"MKDIR {dirname}", args.timeout)
        print_block(f"MKDIR {dirname}", result.response)
        require("OK" in result.response, "MKDIR failed")

        result = session.command(f"CD {dirname}", args.timeout)
        print_block(f"CD {dirname}", result.response)
        require("OK" in result.response, "CD failed")

        result = session.command("WRITE note.txt standalone-ok", args.timeout)
        print_block("WRITE note.txt", result.response)
        require("OK" in result.response, "WRITE failed")

        result = session.command("CAT note.txt", args.timeout)
        print_block("CAT note.txt", result.raw)
        require("standalone-ok" in result.raw, "CAT did not read written data")

        result = session.command("CP note.txt copy.txt", args.timeout)
        print_block("CP note.txt copy.txt", result.response)
        require("OK" in result.response, "CP failed")

        result = session.command("MV copy.txt moved.txt", args.timeout)
        print_block("MV copy.txt moved.txt", result.response)
        require("OK" in result.response, "MV failed")

        result = session.command("MKDIR tree", args.timeout)
        print_block("MKDIR tree", result.response)
        require("OK" in result.response, "MKDIR tree failed")

        result = session.command("WRITE tree/leaf.txt leaf", args.timeout)
        print_block("WRITE tree/leaf.txt", result.response)
        require("OK" in result.response, "WRITE tree/leaf.txt failed")

        result = session.command("RM -R tree", args.timeout)
        print_block("RM -R tree", result.response)
        require("OK" in result.response, "RM -R failed")

        for line in ("DIR", "COPY A B", "MOVE A B", "DELETE X", "RENAME A B", "PRINT 1+2"):
            result = session.command(line, args.timeout)
            print_block(line, result.raw)
            require("UNKNOWN COMMAND" in result.raw, f"{line} was not rejected")

        result = session.command("RM /", args.timeout)
        print_block("RM /", result.response)
        require("Bad path" in result.response, "RM / was not rejected")

        print("PASS: source/shell standalone smoke")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
