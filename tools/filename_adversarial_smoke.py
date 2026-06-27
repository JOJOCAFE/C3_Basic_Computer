#!/usr/bin/env python3
"""Adversarial filename/path smoke test for the ESP32-C3 BASIC shell."""

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
        raise TimeoutError(f"timed out waiting for quiet prompt; got:\n{buf.decode('utf-8', errors='replace')}")

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

    def raw_command(self, data: bytes, timeout: float) -> str:
        self.ser.write(data + b"\r")
        self.ser.flush()
        return self._read_until_quiet_prompt(timeout)

    def edit(self, command: str, path: str | None, lines: list[str], timeout: float) -> ShellResult:
        raw_parts: list[str] = []
        launch = command if path is None else f"{command} {path}"
        self.ser.write(f"{launch}\r".encode("utf-8"))
        self.ser.flush()
        raw_parts.append(self._read_until_suffix(EDITOR_PROMPTS + (PROMPT,), timeout))
        if is_shell_prompt(raw_parts[-1]):
            return ShellResult(raw="".join(raw_parts), response="".join(raw_parts))
        for index, line in enumerate(lines):
            self.ser.write(line.encode("utf-8") + b"\r")
            self.ser.flush()
            if index == len(lines) - 1 and line in (":q", ":q!", ":wq"):
                raw_parts.append(self._read_until_suffix((PROMPT,), timeout))
            else:
                chunk = self._read_until_suffix(EDITOR_PROMPTS + (PROMPT,), timeout)
                raw_parts.append(chunk)
                if is_shell_prompt(chunk):
                    break
        raw = "".join(raw_parts)
        return ShellResult(raw=raw, response=raw)


def is_shell_prompt(text: str) -> bool:
    return text == "> " or text.endswith("\r\n> ") or text.endswith("\n> ")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def print_block(label: str, text: str) -> None:
    print(f"[{label}]")
    print(text.rstrip())


def require_any(text: str, needles: tuple[str, ...], message: str) -> None:
    require(any(needle in text for needle in needles), message)


def require_responsive(session: ShellSession, timeout: float) -> None:
    result = session.command("HELP", timeout)
    print_block("HELP responsiveness", result.response)
    require("HELP" in result.response and "RENEW" in result.response, "shell did not remain responsive")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args(argv)

    token = f"FN_{time.time_ns()}"
    ok_path = f"/data/{token}.txt"
    moved_path = f"/data/{token}_moved.txt"
    copied_path = f"/data/{token}_copy.txt"
    basic_path = f"/basic/{token}.bas"
    utf8_path = f"/data/ภาษาไทย_{token}.txt"
    long_component = "L" * 220
    long_path = f"/data/{long_component}.txt"
    near_component = "N" * 96
    near_path = f"/data/{near_component}.txt"

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)
        require("READY." in banner or banner.endswith("> "), "did not see shell prompt")

        no_name_cases = [
            "CAT",
            "WRITE",
            "RM",
            "CP",
            "MV",
            "EDIT ",
            "BASIC ",
            "/bin/nano too many args",
            "BASIC /basic/a.bas extra",
        ]
        for line in no_name_cases:
            result = session.command(line, args.timeout)
            print_block(line, result.response)
            require_any(result.response,
                        ("Bad", "Usage", "EDIT (untitled)", ".bas mode"),
                        f"{line!r} did not fail or enter expected no-name mode cleanly")
            if "EDIT (untitled)" in result.raw or ".bas mode" in result.raw:
                session.ser.write(b":q\r")
                session.ser.flush()
                quit_raw = session._read_until_suffix((PROMPT,), args.timeout)
                print_block(f"{line} quit", quit_raw)

        result = session.command(f"WRITE {ok_path} stable", args.timeout)
        print_block("WRITE normal", result.response)
        require("OK" in result.response, "normal filename write failed")

        result = session.command(f"CAT {ok_path}", args.timeout)
        print_block("CAT normal", result.raw)
        require("stable" in result.raw, "normal filename read failed")

        result = session.command(f"WRITE {utf8_path} utf8-ok", args.timeout)
        print_block("WRITE UTF-8 filename", result.response)
        require("OK" in result.response, "valid UTF-8 filename write failed")
        result = session.command(f"CAT {utf8_path}", args.timeout)
        print_block("CAT UTF-8 filename", result.raw)
        require("utf8-ok" in result.raw, "valid UTF-8 filename read failed")

        for line in (
            f"WRITE {long_path} too-long",
            f"CAT {long_path}",
            f"EDIT {long_path}",
            f"BASIC /basic/{long_component}.bas",
            f"WRITE /data/bad\\name.txt backslash",
            "WRITE /data/. hidden",
            "WRITE /data/.. parent",
        ):
            result = session.command(line, args.timeout)
            print_block(line[:80], result.response)
            require_any(result.response,
                        ("Bad", "Open failed", "WRITE failed", "Bad editor path", "Exists", "Is directory"),
                        f"{line!r} did not fail cleanly")
            require_responsive(session, args.timeout)

        space_token_path = f"/data/{token}_space"
        result = session.command(f"WRITE {space_token_path} content after whitespace", args.timeout)
        print_block("WRITE whitespace-token path", result.response)
        require("OK" in result.response, "whitespace-token WRITE did not use the first token as the filename")
        result = session.command(f"CAT {space_token_path}", args.timeout)
        print_block("CAT whitespace-token path", result.raw)
        require("content after whitespace" in result.raw, "whitespace-token file content was not preserved")

        result = session.command(f"WRITE {near_path} near-limit", args.timeout)
        print_block("WRITE near limit", result.response)
        require_any(result.response, ("OK", "WRITE failed", "Bad path"),
                    "near-limit filename did not return a bounded result")
        require_responsive(session, args.timeout)

        result = session.command(f"CP {ok_path} {copied_path}", args.timeout)
        print_block("CP normal", result.response)
        require("OK" in result.response, "normal copy failed")

        result = session.command(f"MV {copied_path} {moved_path}", args.timeout)
        print_block("MV normal", result.response)
        require("OK" in result.response, "normal move failed")

        raw_cases = [
            (b"CAT \x00hidden.txt", None, ("Bad path",), "raw NUL filename"),
            (b"WRITE /data/bin\xffname.txt payload", b"RM /data/bin\xffname.txt", ("Bad path",), "raw 0xff filename"),
            (b"WRITE /data/esc\x1b[31mname.txt payload", b"RM /data/esc\x1b[31mname.txt", ("Bad path",), "raw ESC filename"),
            (b"WRITE /data/" + b"A" * 700 + b".txt payload", None, ("Bad input", "Bad path"), "raw overlong filename"),
        ]
        for data, cleanup, expected, label in raw_cases:
            raw = session.raw_command(data, args.timeout)
            print_block(label, raw[-1000:])
            require(raw.endswith("> "), f"{label} did not return to prompt")
            require_any(raw, expected, f"{label} did not return the expected clean rejection")
            if cleanup is not None:
                cleanup_raw = session.raw_command(cleanup, args.timeout)
                print_block(f"{label} cleanup", cleanup_raw[-1000:])
                require(cleanup_raw.endswith("> "), f"{label} cleanup did not return to prompt")
            require_responsive(session, args.timeout)

        result = session.edit("EDIT", ok_path, ["append-line", ":q!"], args.timeout)
        print_block("EDIT existing path", result.raw)
        require("EDIT " in result.raw and result.raw.endswith("> "), "EDIT existing path did not return cleanly")

        result = session.edit("BASIC", basic_path, ["10 PRINT \"OK\"", "20 END", ":wq"], args.timeout)
        print_block("BASIC valid path", result.raw)
        require("SAVED" in result.raw and result.raw.endswith("> "), "BASIC valid path did not save cleanly")

        for cleanup in (ok_path, copied_path, moved_path, basic_path, near_path, space_token_path, utf8_path):
            result = session.command(f"RM {cleanup}", args.timeout)
            print_block(f"RM {cleanup}", result.response)

        require_responsive(session, args.timeout)
        print("PASS: filename adversarial smoke test")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise
