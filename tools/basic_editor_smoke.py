#!/usr/bin/env python3
"""Serial smoke test for BASIC nano mode path policy and save validation."""

from __future__ import annotations

import argparse
import dataclasses
import sys
import time

import serial


PROMPT = b"> "
EDITOR_PROMPTS = (b"edit> ", b"edit* ")
DEBUG_PROMPTS = (b"debug> ",)


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
        data = line.encode("utf-8")
        for offset in range(0, len(data), 48):
            self.ser.write(data[offset:offset + 48])
            self.ser.flush()
            time.sleep(0.01)
        self.ser.write(b"\r")
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
                chunk = self._read_until_suffix(EDITOR_PROMPTS + DEBUG_PROMPTS + (PROMPT,), timeout)
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

    token = f"BASIC_{time.time_ns()}"
    path = f"/basic/{token}.bas"
    bad_path = f"/basic/{token}_bad.bas"
    debug_path = f"/basic/{token}_debug.bas"
    bridge_path = f"/basic/{token}_bridge.bas"
    blocked_path = f"/basic/{token}_blocked.bas"
    typed_hw_path = f"/basic/{token}_typed_hw.bas"
    protected_hw_path = f"/basic/{token}_protected_hw.bas"
    large_path = f"/basic/{token}_large.bas"
    txt_path = f"/data/{token}.txt"
    large_program = [f"{10 + i * 10} REM line {i:03d} " + ("X" * 160) for i in range(90)]
    large_program.append("1000 PRINT \"LARGE BASIC OK\"")
    large_program.append("1010 END")

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)
        require("READY." in banner or banner.endswith("> "), "did not see shell prompt")

        result = session.edit("BASIC", path, ["10 PRINT \"HELLO\"", "20 END", ":wq"], args.timeout)
        print_block("BASIC valid save", result.raw)
        require(".bas mode" in result.raw, "BASIC mode did not start")
        require("SAVED" in result.raw, "valid BASIC program did not save")
        require(result.raw.endswith("> "), "BASIC valid save did not return to shell")

        result = session.command(f"CAT {path}", args.timeout)
        print_block("CAT valid", result.raw)
        require("10 PRINT \"HELLO\"" in result.raw, "saved BASIC file missing line 10")
        require("20 END" in result.raw, "saved BASIC file missing line 20")

        result = session.edit("BASIC", path, [":p", ":q"], args.timeout)
        print_block("BASIC reopen", result.raw)
        require("10 PRINT \"HELLO\"" in result.raw, "reopened BASIC file missing line 10")
        require("20 END" in result.raw, "reopened BASIC file missing line 20")

        result = session.edit("BASIC", large_path, [*large_program, ":run", ":q"], args.timeout + 20.0)
        print_block("BASIC large run", result.raw[:1200] + "\n...[large BASIC output truncated]...")
        require("SAVED" in result.raw, "large BASIC program was not saved before run")
        require("RUN" in result.raw, "large BASIC program did not enter run path")
        require("LARGE BASIC OK" in result.raw, "large BASIC program did not run")
        require(result.raw.endswith("> "), "large BASIC run did not return through editor to shell")

        debug_program = [
            "10 LET A=1",
            "20 PRINT \"DBG OK\"",
            "30 A=A+1",
            "40 END",
            ":debug",
            "s",
            "p",
            "l",
            "s",
            "c",
            ":q",
        ]
        result = session.edit("BASIC", debug_path, debug_program, args.timeout)
        print_block("BASIC debug", result.raw)
        require("DEBUG" in result.raw, "debug mode did not start")
        require("DBG 10 LET A=1" in result.raw, "debug did not show first line")
        require("A=1" in result.raw, "debug variable print missing A=1")
        require("DBG 20 PRINT \"DBG OK\"" in result.raw, "debug list/step missing line 20")
        require("DBG OK" in result.raw, "debug did not execute PRINT")
        require("DEBUG END" in result.raw, "debug did not finish")
        require(result.raw.endswith("> "), "debug did not return through editor to shell")

        bridge_program = [
            "10 SHELL \"PWD\"",
            "20 HARDWARE \"gpio read -p 8\"",
            "30 PRINT \"BRIDGE OK\"",
            "40 END",
            ":run",
            ":q",
        ]
        result = session.edit("BASIC", bridge_path, bridge_program, args.timeout)
        print_block("BASIC bridge", result.raw)
        require("BRIDGE OK" in result.raw, "bridge program did not complete")
        require("\r\n/\r\n" in result.raw or "\n/\r\n" in result.raw or "\n/\n" in result.raw,
                "SHELL PWD bridge did not print current workspace path")
        require("GPIO8 READ" in result.raw, "HARDWARE gpio read bridge did not run")

        typed_hw_program = [
            "10 PINMODE 8, OUTPUT",
            "20 DWRITE 8, LOW",
            "30 PRINT DREAD(8)",
            "40 DWRITE 8, HIGH",
            "50 PRINT DREAD(8)",
            "60 DTOGGLE 8",
            "70 PRINT \"TYPED HW OK\"",
            "80 END",
            ":run",
            ":q",
        ]
        result = session.edit("BASIC", typed_hw_path, typed_hw_program, args.timeout)
        print_block("BASIC typed hardware", result.raw)
        require("TYPED HW OK" in result.raw, "typed BASIC hardware program did not complete")
        require("BASIC ERROR" not in result.raw, "typed BASIC hardware success path failed")
        require("\n0\r\n" in result.raw or "\n0\n" in result.raw, "DREAD did not print low level")

        protected_hw_program = [
            "10 PINMODE 18, OUTPUT",
            "20 PRINT \"SHOULD NOT PRINT\"",
            "30 END",
            ":run",
            ":q!",
        ]
        result = session.edit("BASIC", protected_hw_path, protected_hw_program, args.timeout)
        print_block("BASIC protected hardware", result.raw)
        require("Line 10: protected pin" in result.raw, "protected pin error was not line-anchored")
        require("BASIC ERROR" in result.raw, "protected pin did not stop BASIC")

        blocked_program = [
            "10 SHELL \"RM /basic/not_allowed.bas\"",
            "20 PRINT \"SHOULD NOT PRINT\"",
            "30 END",
            ":run",
            ":q!",
        ]
        result = session.edit("BASIC", blocked_path, blocked_program, args.timeout)
        print_block("BASIC blocked shell", result.raw)
        require("BASIC shell command blocked" in result.raw, "destructive SHELL command was not blocked")
        require("BASIC ERROR" in result.raw, "blocked SHELL command did not stop BASIC")

        result = session.edit("BASIC", bad_path, ["123ABC", ":w", ":p", ":q!"], args.timeout)
        print_block("BASIC invalid save", result.raw)
        require("BASIC validation failed line 1" in result.raw, "invalid BASIC line was not rejected")
        require("malformed line number" in result.raw, "invalid BASIC error missing reason")
        require("123ABC" in result.raw, "invalid BASIC buffer was not preserved after rejection")
        require(result.raw.endswith("> "), "invalid BASIC test did not return to shell")

        result = session.command(f"CAT {bad_path}", args.timeout)
        print_block("CAT invalid", result.raw)
        require("Open failed" in result.raw, "invalid BASIC file should not have been saved")

        result = session.command(f"BASIC {txt_path}", args.timeout)
        print_block("BASIC txt path", result.raw)
        require("Bad editor path. Use /basic/name.bas" in result.raw, "BASIC accepted /data/*.txt")

        result = session.command(f"EDIT {path}", args.timeout)
        print_block("EDIT bas path", result.raw)
        require("Bad editor path. Use /data/name.txt" in result.raw, "EDIT accepted /basic/*.bas")

        result = session.command("HELP", args.timeout)
        print_block("HELP", result.raw)
        require("BASIC" not in result.response, "HELP should not expose BASIC as boot-shell immediate mode")

        session.command(f"RM {path}", args.timeout)
        session.command(f"RM {large_path}", args.timeout)
        session.command(f"RM {debug_path}", args.timeout)
        session.command(f"RM {bridge_path}", args.timeout)
        session.command(f"RM {blocked_path}", args.timeout)
        session.command(f"RM {typed_hw_path}", args.timeout)
        session.command(f"RM {protected_hw_path}", args.timeout)
        print(f"PASS: BASIC nano mode validation smoke test for {path}")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise
