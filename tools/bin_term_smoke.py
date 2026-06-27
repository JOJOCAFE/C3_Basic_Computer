#!/usr/bin/env python3
"""Serial smoke test for /bin/term and BASIC TERM escape output."""

from __future__ import annotations

import argparse
import dataclasses
import sys
import time

import serial


PROMPT = b"> "
EDITOR_PROMPTS = (b"edit> ", b"edit* ")


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

    def _read_until_suffix(self, suffixes: tuple[bytes, ...], timeout: float) -> str:
        deadline = time.monotonic() + timeout
        buf = bytearray()
        while time.monotonic() < deadline:
            chunk = self.ser.read(256)
            if chunk:
                buf.extend(chunk)
                if any(buf.endswith(suffix) for suffix in suffixes):
                    return buf.decode("utf-8", errors="replace")
                continue
            time.sleep(0.01)
        raise TimeoutError(f"timed out waiting for prompt; got:\n{buf.decode('utf-8', errors='replace')}")

    def resync(self, timeout: float) -> str:
        self.ser.write(b"\r")
        self.ser.flush()
        return self._read_until_suffix((PROMPT,), timeout)

    def command(self, line: str, timeout: float) -> ShellResult:
        self.ser.write(line.encode("utf-8") + b"\r")
        self.ser.flush()
        raw = self._read_until_suffix((PROMPT,), timeout)
        text = raw[: -len(PROMPT)]
        if "\r\n" in text:
            response = text.split("\r\n", 1)[1]
        elif "\n" in text:
            response = text.split("\n", 1)[1]
        else:
            response = ""
        return ShellResult(raw=raw, response=response)

    def write_editor_line(self, line: str) -> None:
        self.ser.write(line.encode("utf-8") + b"\r")
        self.ser.flush()

    @staticmethod
    def is_shell_prompt(text: str) -> bool:
        return text == "> " or text.endswith("\r\n> ") or text.endswith("\n> ")

    def edit(self, command: str, path: str, lines: list[str], timeout: float) -> ShellResult:
        raw_parts: list[str] = []
        self.ser.write(f"{command} {path}\r".encode("utf-8"))
        self.ser.flush()
        raw_parts.append(self._read_until_suffix(EDITOR_PROMPTS + (PROMPT,), timeout))
        if self.is_shell_prompt(raw_parts[-1]):
            return ShellResult(raw="".join(raw_parts), response="".join(raw_parts))

        for index, line in enumerate(lines):
            self.write_editor_line(line)
            if index == len(lines) - 1 and line in (":q", ":q!", ":wq"):
                raw_parts.append(self._read_until_suffix((PROMPT,), timeout))
            else:
                chunk = self._read_until_suffix(EDITOR_PROMPTS + (PROMPT,), timeout)
                raw_parts.append(chunk)
                if self.is_shell_prompt(chunk):
                    break
        raw = "".join(raw_parts)
        return ShellResult(raw=raw, response=raw)


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
    parser.add_argument("--timeout", type=float, default=15.0)
    args = parser.parse_args(argv)

    token = f"TERM_{time.time_ns()}"
    term_path = f"/basic/{token}.bas"
    blocked_path = f"/basic/{token}_blocked.bas"

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)
        require("READY." in banner or banner.endswith("> "), "did not see shell prompt")

        result = session.command("/bin list", args.timeout)
        print_block("/bin list", result.raw)
        require("/bin/term" in result.raw, "/bin list missing /bin/term")

        result = session.command("HELP", args.timeout)
        print_block("HELP", result.raw)
        require("/bin/term" not in result.raw and " TERM" not in result.raw,
                "HELP should not list /bin/term")

        checks = [
            ("/bin/term clear", "\x1b[2J\x1b[H"),
            ("/bin/term home", "\x1b[H"),
            ("/bin/term goto -r 3 -c 4", "\x1b[3;4H"),
            ("/bin/term color -f 2", "\x1b[32m"),
            ("/bin/term color -f 2 -b 4", "\x1b[32;44m"),
            ("/bin/term reset", "\x1b[0m"),
            ("/bin/term hide-cursor", "\x1b[?25l"),
            ("/bin/term show-cursor", "\x1b[?25h"),
        ]
        for command, expected in checks:
            result = session.command(command, args.timeout)
            print_block(command, result.raw.encode("unicode_escape").decode("ascii"))
            require(expected in result.raw, f"{command} missing expected escape")

        result = session.command("/bin/term color -f 8", args.timeout)
        print_block("bad color", result.raw)
        require("Bad argument" in result.raw, "bad color did not fail")

        overlong_direct = "/bin/term clear " + (" " * 140) + "x"
        result = session.command(overlong_direct, args.timeout)
        print_block("overlong direct", result.raw)
        require("Bad argument" in result.raw, "overlong direct TERM command did not fail")

        program = [
            "10 TERM \"clear\"",
            "20 TERM \"goto -r 3 -c 4\"",
            "30 TERM \"color -f 2\"",
            "40 PRINT \"TERM OK\"",
            "50 TERM \"reset\"",
            "60 END",
            ":run",
            ":q",
        ]
        result = session.edit("BASIC", term_path, program, args.timeout)
        print_block("BASIC TERM", result.raw.encode("unicode_escape").decode("ascii"))
        require("\x1b[2J\x1b[H" in result.raw, "BASIC TERM clear missing escape")
        require("\x1b[3;4H" in result.raw, "BASIC TERM goto missing escape")
        require("\x1b[32m" in result.raw, "BASIC TERM color missing escape")
        require("\x1b[0m" in result.raw, "BASIC TERM reset missing escape")
        require("TERM OK" in result.raw, "BASIC TERM program did not print")

        blocked_program = [
            "10 TERM \"rm -r /\"",
            "20 PRINT \"SHOULD NOT PRINT\"",
            "30 END",
            ":run",
            ":q!",
        ]
        result = session.edit("BASIC", blocked_path, blocked_program, args.timeout)
        print_block("BASIC TERM blocked", result.raw)
        require("BASIC TERM command blocked" in result.raw, "unsafe TERM command was not blocked")
        require("BASIC ERROR" in result.raw, "blocked TERM command did not stop BASIC")

        overlong_program = [
            "10 TERM \"goto -r 3 -c 4 " + (" " * 100) + "-x 1\"",
            "20 PRINT \"SHOULD NOT PRINT\"",
            "30 END",
            ":run",
            ":q!",
        ]
        result = session.edit("BASIC", f"/basic/{token}_overlong.bas", overlong_program, args.timeout)
        print_block("BASIC TERM overlong", result.raw)
        require("BASIC TERM command blocked" in result.raw, "overlong BASIC TERM command was not blocked")
        require("BASIC ERROR" in result.raw, "overlong BASIC TERM command did not stop BASIC")

        session.command(f"RM {term_path}", args.timeout)
        session.command(f"RM {blocked_path}", args.timeout)
        session.command(f"RM /basic/{token}_overlong.bas", args.timeout)
        print("PASS: /bin/term and BASIC TERM smoke")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise
