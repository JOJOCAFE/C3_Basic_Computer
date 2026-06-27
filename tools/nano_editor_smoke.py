#!/usr/bin/env python3
"""Serial smoke test for the Sprint 004 EDIT -> /bin/nano .txt editor."""

from __future__ import annotations

import argparse
import dataclasses
import sys
import time
from typing import Optional

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

    def _read_until_prompt(self, timeout: float) -> str:
        return self._read_until_suffix((PROMPT,), timeout)

    def _read_until_editor_prompt(self, timeout: float) -> str:
        return self._read_until_suffix(EDITOR_PROMPTS, timeout)

    def _read_until_editor_or_shell_prompt(self, timeout: float) -> str:
        return self._read_until_suffix(EDITOR_PROMPTS + (PROMPT,), timeout)

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

    def write_editor_line(self, line: bytes) -> None:
        for offset in range(0, len(line), 48):
            self.ser.write(line[offset:offset + 48])
            self.ser.flush()
            time.sleep(0.01)
        self.ser.write(b"\r")
        self.ser.flush()

    @staticmethod
    def is_shell_prompt(text: str) -> bool:
        return text == "> " or text.endswith("\r\n> ") or text.endswith("\n> ")

    def edit(self, path: Optional[str], lines: list[str], timeout: float, command: str = "EDIT") -> ShellResult:
        raw_parts: list[str] = []
        launch = command if path is None else f"{command} {path}"
        self.ser.write(f"{launch}\r".encode("utf-8"))
        self.ser.flush()
        raw_parts.append(self._read_until_editor_prompt(timeout))
        for index, line in enumerate(lines):
            self.write_editor_line(line.encode("utf-8"))
            if index == len(lines) - 1 and line in (":q", ":q!", ":wq"):
                raw_parts.append(self._read_until_prompt(timeout))
            else:
                chunk = self._read_until_editor_or_shell_prompt(timeout)
                raw_parts.append(chunk)
                if self.is_shell_prompt(chunk):
                    break
        raw = "".join(raw_parts)
        return ShellResult(raw=raw, response=raw)

    def edit_bytes(self, path: str, chunks: list[bytes], timeout: float) -> ShellResult:
        raw_parts: list[str] = []
        self.ser.write(f"EDIT {path}\r".encode("utf-8"))
        self.ser.flush()
        raw_parts.append(self._read_until_editor_prompt(timeout))
        for index, chunk in enumerate(chunks):
            self.write_editor_line(chunk)
            if index == len(chunks) - 1 and chunk in (b":q", b":q!", b":wq"):
                raw_parts.append(self._read_until_prompt(timeout))
            else:
                part = self._read_until_editor_or_shell_prompt(timeout)
                raw_parts.append(part)
                if self.is_shell_prompt(part):
                    break
        raw = "".join(raw_parts)
        return ShellResult(raw=raw, response=raw)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def print_block(label: str, text: str) -> None:
    print(f"[{label}]")
    print(text.rstrip())


def first_free_untitled(session: ShellSession, timeout: float) -> str:
    for index in range(1, 1000):
        path = f"/data/untitled-{index}.txt"
        result = session.command(f"CAT {path}", timeout)
        if "Open failed" in result.raw:
            return path
    raise RuntimeError("no free /data/untitled-N.txt path")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=15.0)
    args = parser.parse_args(argv)

    token = f"NANO_{time.time_ns()}"
    path = f"/data/{token}.txt"
    cancel_path = f"/data/{token}_cancel.txt"
    clear_path = f"/data/{token}_clear.txt"
    large_path = f"/data/{token}_large.txt"
    hostile_path = f"/data/{token}_hostile.txt"
    bin_nano_path = f"/data/{token}_bin_nano.txt"
    overflow_path = f"/data/{token}_overflow.txt"
    first = f"first-{token}"
    second = f"second-{token}"
    long_line = "LONG-" + ("A" * 1024) + "-END"
    large_lines = [f"LARGE-BEGIN-{token}", long_line]
    for i in range(120):
        large_lines.append(f"L{i:02d}-" + ("B" * 500))
    large_lines.append(f"LARGE-END-{token}")
    large_size = sum(len(line) for line in large_lines) + len(large_lines) - 1
    require(large_size > 60 * 1024, "large fixture is not large enough")
    require(large_size < 64 * 1024, "large fixture exceeds editor target size")
    overflow_lines = [f"OVERFLOW-BEGIN-{token}"]
    for i in range(132):
        overflow_lines.append(f"O{i:02d}-" + ("C" * 500))

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)
        require("READY." in banner or banner.endswith("> "), "did not see shell prompt")

        untitled_path = first_free_untitled(session, args.timeout)

        result = session.edit(None, [":q"], args.timeout)
        print_block("EDIT untitled clean q", result.raw)
        require("EDIT (untitled)" in result.raw, "clean untitled editor did not start")
        require(result.raw.endswith("> "), "clean untitled :q did not return to shell")

        result = session.command(f"CAT {untitled_path}", args.timeout)
        print_block("CAT clean untitled", result.raw)
        require("Open failed" in result.raw, "clean untitled :q should not create a file")

        result = session.edit(None, ["dirty-untitled", ":q", ":q!"], args.timeout)
        print_block("EDIT untitled dirty q/q!", result.raw)
        require("Unsaved changes" in result.raw, "dirty untitled :q did not refuse to quit")
        require(":w to save" in result.raw and ":q! to discard" in result.raw,
                "dirty untitled :q did not explain save/discard choices")
        require(result.raw.endswith("> "), "dirty untitled :q! did not return to shell")

        result = session.command(f"CAT {untitled_path}", args.timeout)
        print_block("CAT discarded untitled", result.raw)
        require("Open failed" in result.raw, "discarded untitled buffer should not create a file")

        result = session.edit(None, ["untitled-text-line", ":wq"], args.timeout)
        print_block("EDIT untitled save", result.raw)
        require("EDIT (untitled)" in result.raw, "untitled save did not start untitled")
        require(f"EDIT {untitled_path}" in result.raw, "untitled save did not select expected default path")
        require("SAVED" in result.raw, "untitled text did not save")
        result = session.command(f"CAT {untitled_path}", args.timeout)
        print_block("CAT untitled", result.raw)
        require("untitled-text-line" in result.raw, "untitled text content missing")
        session.command(f"RM {untitled_path}", args.timeout)

        untitled_path = first_free_untitled(session, args.timeout)
        result = session.edit(None, ["bin-nano-untitled", ":wq"], args.timeout, command="/bin/nano")
        print_block("DIRECT /bin/nano untitled", result.raw)
        require(f"EDIT {untitled_path}" in result.raw, "direct /bin/nano untitled did not select default path")
        require("SAVED" in result.raw, "direct /bin/nano untitled did not save")
        result = session.command(f"CAT {untitled_path}", args.timeout)
        require("bin-nano-untitled" in result.raw, "direct /bin/nano untitled saved content missing")
        session.command(f"RM {untitled_path}", args.timeout)

        result = session.edit(path, [":help", first, second, ":p", ":w", ":q"], args.timeout)
        print_block("EDIT help/p/save/q", result.raw)
        require("EDIT " in result.raw, "editor did not start")
        require("Commands:" in result.raw, "editor help did not print")
        require(first in result.raw and second in result.raw, "editor print did not show buffer")
        require("SAVED" in result.raw, "editor did not save")

        result = session.command(f"CAT {path}", args.timeout)
        print_block("CAT", result.raw)
        require(first in result.raw, "saved file missing first line")
        require(second in result.raw, "saved file missing second line")

        result = session.edit(bin_nano_path, ["bin-nano-entry", ":wq"], args.timeout, command="/bin/nano")
        print_block("DIRECT /bin/nano", result.raw)
        require("SAVED" in result.raw, "direct /bin/nano did not save")
        result = session.command(f"CAT {bin_nano_path}", args.timeout)
        require("bin-nano-entry" in result.raw, "direct /bin/nano saved content missing")

        result = session.edit(cancel_path, ["dirty-but-cancelled", ":q", ":q!"], args.timeout)
        print_block("EDIT dirty q/q!", result.raw)
        require("Unsaved changes" in result.raw, "dirty :q did not refuse to quit")
        result = session.command(f"CAT {cancel_path}", args.timeout)
        print_block("CAT cancelled", result.raw)
        require("Open failed" in result.raw, "cancelled file should not exist")

        result = session.edit(clear_path, ["will-be-cleared", ":clear", "after-clear", ":wq"], args.timeout)
        print_block("EDIT clear/wq", result.raw)
        require("CLEARED" in result.raw, "clear command did not run")
        require("SAVED" in result.raw, "wq did not save")
        result = session.command(f"CAT {clear_path}", args.timeout)
        print_block("CAT clear", result.raw)
        require("after-clear" in result.raw, "clear file missing replacement text")
        require("will-be-cleared" not in result.raw, "clear file still contains old text")

        result = session.edit(large_path, [*large_lines, ":wq"], args.timeout + 15.0)
        print_block("EDIT large", result.raw[:1000] + "\n...[large output truncated]...")
        require("SAVED" in result.raw, "large file did not save")
        result = session.command(f"CAT {large_path}", args.timeout + 15.0)
        print_block("CAT large", result.raw[:1000] + "\n...[large output truncated]...")
        require(large_lines[0] in result.raw, "large file missing first marker")
        require(long_line in result.raw, "large file missing long line")
        require(large_lines[-1] in result.raw, "large file missing final marker")

        result = session.edit(overflow_path, overflow_lines, args.timeout + 15.0)
        print_block("EDIT overflow", result.raw[:1000] + "\n...[overflow output truncated]...")
        require("Buffer full" in result.raw, "overflow did not return Buffer full")
        require(result.raw.endswith("> "), "overflow did not return to shell prompt")

        hostile = b"\x00\xff\x1b[31mBINARY??"
        result = session.edit_bytes(hostile_path, [hostile, b":p", b":q!"], args.timeout)
        print_block("EDIT hostile", result.raw)
        require("EDIT " in result.raw, "hostile editor did not start")
        require("--- FILE ---" in result.raw, "hostile print did not return buffer view")
        require(result.raw.endswith("> "), "hostile input did not return to shell prompt")

        result = session.command(f"RM {path}", args.timeout)
        print_block("RM", result.response)
        require("OK" in result.response, "cleanup failed")
        for cleanup_path in (clear_path, large_path, hostile_path, bin_nano_path):
            session.command(f"RM {cleanup_path}", args.timeout)

        print(f"PASS: nano editor full command/large/hostile smoke test for {path}")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
