#!/usr/bin/env python3
"""Serial smoke test for the Sprint 002 workspace shell command set."""

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


def require_ok(result: ShellResult, command: str) -> None:
    require("OK" in result.response, f"{command} did not return OK")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=7.0)
    args = parser.parse_args(argv)

    token = f"WS_{time.time_ns()}"
    dirname = f"temp/{token}"

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)
        require("READY." in banner or banner.endswith("> "), "did not see shell prompt")

        result = session.command("PWD", args.timeout)
        print_block("PWD", result.response)
        require(result.response.strip() == "/", "PWD at boot did not print /")

        result = session.command("LS", args.timeout)
        print_block("LS", result.raw)
        for dirname_root in ("basic", "asm", "bin", "config", "data", "temp"):
            require(f"/{dirname_root}" in result.raw, f"LS root missing directory marker for {dirname_root}")

        result = session.command(f"MKDIR {dirname}", args.timeout)
        print_block(f"MKDIR {dirname}", result.response)
        require_ok(result, f"MKDIR {dirname}")

        result = session.command(f"CD {dirname}", args.timeout)
        print_block(f"CD {dirname}", result.response)
        require_ok(result, f"CD {dirname}")

        result = session.command("PWD", args.timeout)
        print_block("PWD after CD", result.response)
        require(result.response.strip() == f"/{dirname}", "PWD after CD did not show workspace-relative dir")

        result = session.command("WRITE note.txt hello-from-c3", args.timeout)
        print_block("WRITE note.txt", result.response)
        require_ok(result, "WRITE note.txt")

        result = session.command("WRITE demo.bas 10 PRINT WILDCARD", args.timeout)
        print_block("WRITE demo.bas", result.response)
        require_ok(result, "WRITE demo.bas")

        result = session.command("WRITE xx.txt wildcard-text", args.timeout)
        print_block("WRITE xx.txt", result.response)
        require_ok(result, "WRITE xx.txt")

        result = session.command("WRITE xx.bas wildcard-basic", args.timeout)
        print_block("WRITE xx.bas", result.response)
        require_ok(result, "WRITE xx.bas")

        result = session.command("CAT note.txt", args.timeout)
        print_block("CAT note.txt", result.raw)
        require("hello-from-c3" in result.raw, "CAT did not print written text")

        result = session.command("LS *.*", args.timeout)
        print_block("LS *.*", result.raw)
        for filename in ("note.txt", "demo.bas", "xx.txt", "xx.bas"):
            require(filename in result.raw, f"LS *.* missing {filename}")

        result = session.command("LS *.bas", args.timeout)
        print_block("LS *.bas", result.raw)
        require("demo.bas" in result.raw and "xx.bas" in result.raw, "LS *.bas missing .bas files")
        require("note.txt" not in result.raw and "xx.txt" not in result.raw, "LS *.bas included non-bas files")

        result = session.command("CAT xx.*", args.timeout)
        print_block("CAT xx.*", result.raw)
        require("wildcard-text" in result.raw, "CAT xx.* missing txt file content")
        require("wildcard-basic" in result.raw, "CAT xx.* missing bas file content")

        result = session.command("MKDIR empty-dir", args.timeout)
        print_block("MKDIR empty-dir", result.response)
        require_ok(result, "MKDIR empty-dir")

        result = session.command("RMDIR empty-dir", args.timeout)
        print_block("RMDIR empty-dir", result.response)
        require_ok(result, "RMDIR empty-dir")

        result = session.command("CP note.txt copy.txt", args.timeout)
        print_block("CP note.txt copy.txt", result.response)
        require_ok(result, "CP note.txt copy.txt")

        result = session.command("CP note.txt alias.txt", args.timeout)
        print_block("CP note.txt alias.txt", result.response)
        require_ok(result, "CP note.txt alias.txt")

        result = session.command("MKDIR copies", args.timeout)
        print_block("MKDIR copies", result.response)
        require_ok(result, "MKDIR copies")

        result = session.command("CP *.bas copies", args.timeout)
        print_block("CP *.bas copies", result.response)
        require_ok(result, "CP *.bas copies")

        result = session.command("CAT copies/demo.bas", args.timeout)
        print_block("CAT copies/demo.bas", result.raw)
        require("10 PRINT WILDCARD" in result.raw, "CP *.bas did not copy demo.bas")

        result = session.command("CAT copies/xx.bas", args.timeout)
        print_block("CAT copies/xx.bas", result.raw)
        require("wildcard-basic" in result.raw, "CP *.bas did not copy xx.bas")

        result = session.command("MV copy.txt moved.txt", args.timeout)
        print_block("MV copy.txt moved.txt", result.response)
        require_ok(result, "MV copy.txt moved.txt")

        result = session.command("MV alias.txt alias-moved.txt", args.timeout)
        print_block("MV alias.txt alias-moved.txt", result.response)
        require_ok(result, "MV alias.txt alias-moved.txt")

        result = session.command("MKDIR moved-wildcards", args.timeout)
        print_block("MKDIR moved-wildcards", result.response)
        require_ok(result, "MKDIR moved-wildcards")

        result = session.command("MV xx.* moved-wildcards", args.timeout)
        print_block("MV xx.* moved-wildcards", result.response)
        require_ok(result, "MV xx.* moved-wildcards")

        result = session.command("CAT moved-wildcards/xx.txt", args.timeout)
        print_block("CAT moved-wildcards/xx.txt", result.raw)
        require("wildcard-text" in result.raw, "MV xx.* did not move xx.txt")

        result = session.command("CAT moved-wildcards/xx.bas", args.timeout)
        print_block("CAT moved-wildcards/xx.bas", result.raw)
        require("wildcard-basic" in result.raw, "MV xx.* did not move xx.bas")

        result = session.command("MKDIR move-dir", args.timeout)
        print_block("MKDIR move-dir", result.response)
        require_ok(result, "MKDIR move-dir")

        result = session.command("WRITE move-dir/nested.txt mv-dir-ok", args.timeout)
        print_block("WRITE move-dir/nested.txt", result.response)
        require_ok(result, "WRITE move-dir/nested.txt")

        result = session.command("MV move-dir moved-dir", args.timeout)
        print_block("MV move-dir moved-dir", result.response)
        require_ok(result, "MV move-dir moved-dir")

        result = session.command("CAT moved-dir/nested.txt", args.timeout)
        print_block("CAT moved-dir/nested.txt", result.raw)
        require("mv-dir-ok" in result.raw, "MV directory target did not preserve child file")

        result = session.command("RMDIR moved-dir", args.timeout)
        print_block("RMDIR moved-dir", result.response)
        require("failed" in result.response.lower(), "RMDIR removed a non-empty directory")

        result = session.command("MKDIR tree", args.timeout)
        print_block("MKDIR tree", result.response)
        require_ok(result, "MKDIR tree")

        result = session.command("MKDIR tree/child", args.timeout)
        print_block("MKDIR tree/child", result.response)
        require_ok(result, "MKDIR tree/child")

        result = session.command("WRITE tree/child/leaf.txt recursive-delete-ok", args.timeout)
        print_block("WRITE tree/child/leaf.txt", result.response)
        require_ok(result, "WRITE tree/child/leaf.txt")

        result = session.command("RM -R tree", args.timeout)
        print_block("RM -R tree", result.response)
        require_ok(result, "RM -R tree")

        result = session.command("LS", args.timeout)
        print_block("LS working dir", result.raw)
        for filename in ("note.txt", "moved.txt", "alias-moved.txt"):
            require(filename in result.raw, f"LS missing {filename}")
            require(f"/{filename}" not in result.raw, f"LS marked file {filename} as a directory")
        require("/moved-dir" in result.raw, "LS missing moved directory marker")
        require("/move-dir" not in result.raw, "MV left source directory behind")
        require("/tree" not in result.raw, "RM -R left recursive directory behind")
        require("/empty-dir" not in result.raw, "RMDIR left empty directory behind")
        require("copy.txt" not in result.raw, "MV left copy.txt behind")
        require("alias.txt" not in result.raw, "MV left alias.txt behind")

        result = session.command("RM -R moved-dir", args.timeout)
        print_block("RM -R moved-dir", result.response)
        require_ok(result, "RM -R moved-dir")

        result = session.command("RM -R copies", args.timeout)
        print_block("RM -R copies", result.response)
        require_ok(result, "RM -R copies")

        result = session.command("RM -R moved-wildcards", args.timeout)
        print_block("RM -R moved-wildcards", result.response)
        require_ok(result, "RM -R moved-wildcards")

        result = session.command("RM *.bas", args.timeout)
        print_block("RM *.bas", result.response)
        require_ok(result, "RM *.bas")

        result = session.command("LS *.bas", args.timeout)
        print_block("LS *.bas after RM", result.response)
        require("No match" in result.response, "RM *.bas left .bas files behind")

        for filename in ("note.txt", "moved.txt", "alias-moved.txt"):
            result = session.command(f"RM {filename}", args.timeout)
            print_block(f"RM {filename}", result.response)
            require_ok(result, f"RM {filename}")

        result = session.command("CAT ../../BAD", args.timeout)
        print_block("CAT ../../BAD", result.response)
        require("Bad" in result.response or "failed" in result.response, "unsafe CAT path was not rejected")

        result = session.command("CD /", args.timeout)
        print_block("CD /", result.response)
        require_ok(result, "CD /")

        result = session.command("PWD", args.timeout)
        print_block("PWD final", result.response)
        require(result.response.strip() == "/", "PWD final did not return to /")

        print(f"PASS: workspace shell smoke test for {dirname}")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
