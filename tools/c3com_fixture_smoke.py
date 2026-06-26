#!/usr/bin/env python3
"""Build and optionally smoke-test a tiny guarded C3COM executable."""

from __future__ import annotations

import argparse
import binascii
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time


C3_COM_MAGIC = b"C3CM"
C3_COM_ABI_VERSION = 1
C3_COM_TARGET_ESP32C3_RV32 = 1
C3_COM_HEADER_SIZE = 48
C3_COM_CODE_OFFSET = C3_COM_HEADER_SIZE


FIXTURE_C = r"""
typedef unsigned int size_t;

typedef struct {
    unsigned int abi_version;
    int argc;
    const char *const *argv;
    int (*stdin_read_line)(char *buf, size_t len);
    void (*stdout_write)(const void *data, size_t len);
    void (*stderr_write)(const void *data, size_t len);
} c3_com_api_t;

static void write_ch(const c3_com_api_t *api, char ch)
{
    api->stdout_write(&ch, 1);
}

static void write_crlf(const c3_com_api_t *api)
{
    write_ch(api, '\r');
    write_ch(api, '\n');
}

static void write_null_text(const c3_com_api_t *api)
{
    write_ch(api, '(');
    write_ch(api, 'n');
    write_ch(api, 'u');
    write_ch(api, 'l');
    write_ch(api, 'l');
    write_ch(api, ')');
}

static void write_cstr(const c3_com_api_t *api, const char *s)
{
    if (!s) {
        write_null_text(api);
        return;
    }
    while (*s) {
        write_ch(api, *s++);
    }
}

static void write_uint(const c3_com_api_t *api, unsigned int value)
{
    char tmp[10];
    unsigned int pos = 0;
    if (value == 0) {
        write_ch(api, '0');
        return;
    }
    while (value && pos < sizeof(tmp)) {
        tmp[pos++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (pos) {
        pos--;
        write_ch(api, tmp[pos]);
    }
}

__attribute__((section(".text.c3com_entry"), used))
int c3com_entry(const c3_com_api_t *api, int argc, const char *const argv[])
{
    write_ch(api, 'C');
    write_ch(api, '3');
    write_ch(api, 'C');
    write_ch(api, 'O');
    write_ch(api, 'M');
    write_ch(api, ' ');
    write_ch(api, 'f');
    write_ch(api, 'i');
    write_ch(api, 'x');
    write_ch(api, 't');
    write_ch(api, 'u');
    write_ch(api, 'r');
    write_ch(api, 'e');
    write_ch(api, ' ');
    write_ch(api, 'a');
    write_ch(api, 'r');
    write_ch(api, 'g');
    write_ch(api, 'c');
    write_ch(api, '=');
    write_uint(api, (unsigned int)argc);
    write_crlf(api);

    for (int i = 0; i < argc; i++) {
        write_ch(api, 'a');
        write_ch(api, 'r');
        write_ch(api, 'g');
        write_ch(api, 'v');
        write_ch(api, '[');
        write_uint(api, (unsigned int)i);
        write_ch(api, ']');
        write_ch(api, '=');
        write_cstr(api, argv[i]);
        write_crlf(api);
    }
    return 0;
}
"""


LINKER_SCRIPT = r"""
OUTPUT_ARCH(riscv)
ENTRY(c3com_entry)
SECTIONS
{
    . = 0;
    .text : ALIGN(4)
    {
        KEEP(*(.text.c3com_entry))
        *(.text*)
        *(.srodata*)
        *(.rodata*)
        . = ALIGN(4);
    }
}
"""


def find_tool(name: str) -> str:
    env_name = name.upper().replace("-", "_")
    if os.environ.get(env_name):
        return os.environ[env_name]

    found = shutil.which(name)
    if found:
        return found

    candidates = sorted(
        Path.home().glob(f".espressif/tools/riscv32-esp-elf/*/riscv32-esp-elf/bin/{name}")
    )
    if candidates:
        return str(candidates[-1])

    arduino_candidates = sorted(Path.home().glob(f".arduino15/packages/esp32/tools/esp-rv32/*/bin/{name}"))
    if arduino_candidates:
        return str(arduino_candidates[-1])

    raise FileNotFoundError(f"{name} not found; source ESP-IDF export.sh or set {env_name}")


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True)


def build_code_blob(workdir: Path) -> bytes:
    gcc = find_tool("riscv32-esp-elf-gcc")
    objcopy = find_tool("riscv32-esp-elf-objcopy")

    src = workdir / "fixture.c"
    linker = workdir / "c3com.ld"
    obj = workdir / "fixture.o"
    elf = workdir / "fixture.elf"
    binary = workdir / "fixture.bin"
    src.write_text(FIXTURE_C, encoding="ascii")
    linker.write_text(LINKER_SCRIPT, encoding="ascii")

    common_flags = [
        "-march=rv32imc",
        "-mabi=ilp32",
        "-Os",
        "-ffreestanding",
        "-fno-builtin",
        "-fno-stack-protector",
        "-fdata-sections",
        "-ffunction-sections",
        "-mno-relax",
    ]
    run([gcc, *common_flags, "-c", str(src), "-o", str(obj)])
    run(
        [
            gcc,
            "-nostdlib",
            "-Wl,--no-relax",
            "-Wl,--gc-sections",
            f"-Wl,-T,{linker}",
            str(obj),
            "-o",
            str(elf),
        ]
    )
    run([objcopy, "-O", "binary", "-j", ".text", str(elf), str(binary)])
    code = binary.read_bytes()
    if not code:
        raise RuntimeError("compiled fixture has empty .text")
    if len(code) > 64 * 1024:
        raise RuntimeError(f"compiled fixture is too large: {len(code)} bytes")
    return code


def c3com_header(code: bytes, *, corrupt_crc: bool = False) -> bytes:
    crc = binascii.crc32(code) & 0xFFFFFFFF
    if corrupt_crc:
        crc ^= 0xFFFFFFFF
    return struct.pack(
        "<4sHHIIIIII4I",
        C3_COM_MAGIC,
        C3_COM_HEADER_SIZE,
        C3_COM_ABI_VERSION,
        C3_COM_TARGET_ESP32C3_RV32,
        0,
        C3_COM_CODE_OFFSET,
        len(code),
        0,
        crc,
        0,
        0,
        0,
        0,
    )


def build_fixture(output: Path, bad_crc_output: Path | None) -> None:
    with tempfile.TemporaryDirectory(prefix="c3com-fixture-") as tmp:
        code = build_code_blob(Path(tmp))

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(c3com_header(code) + code)
    print(f"wrote {output} ({output.stat().st_size} bytes, code {len(code)} bytes)")

    if bad_crc_output:
        bad_crc_output.parent.mkdir(parents=True, exist_ok=True)
        bad_crc_output.write_bytes(c3com_header(code, corrupt_crc=True) + code)
        print(f"wrote {bad_crc_output} ({bad_crc_output.stat().st_size} bytes, bad CRC)")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def print_block(label: str, text: str) -> None:
    print(f"[{label}]")
    print(text.rstrip())


def smoke_serial(args: argparse.Namespace, fixture: Path, bad_fixture: Path | None) -> None:
    import ymodem_transfer_smoke as ymodem

    session = ymodem.ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)

        remote = "/bin/fixture.com"
        raw = session.send_ymodem_file(
            f"RECV -F {remote}",
            "fixture.com",
            fixture.read_bytes(),
            args.timeout,
        )
        print_block(f"RECV -F {remote}", raw)
        require("OK" in raw, "fixture upload did not return OK")

        result = session.command(f"RUN {remote} one two", args.timeout)
        print_block(f"RUN {remote} one two", result.raw)
        require("C3COM fixture argc=3" in result.raw, "fixture argc output missing")
        require("argv[0]=/bin/fixture.com" in result.raw, "fixture argv[0] output missing")
        require("argv[1]=one" in result.raw, "fixture argv[1] output missing")
        require("argv[2]=two" in result.raw, "fixture argv[2] output missing")
        require("EXIT 0" in result.raw, "fixture did not report EXIT 0")

        if bad_fixture:
            remote_bad = f"/bin/fixture_bad_{time.time_ns()}.com"
            raw = session.send_ymodem_file(
                f"RECV -F {remote_bad}",
                Path(remote_bad).name,
                bad_fixture.read_bytes(),
                args.timeout,
            )
            print_block(f"RECV -F {remote_bad}", raw)
            require("OK" in raw, "bad-CRC fixture upload did not return OK")

            result = session.command(f"RUN {remote_bad}", args.timeout)
            print_block(f"RUN {remote_bad}", result.raw)
            require("Invalid .com image" in result.raw, "bad-CRC fixture was not rejected")

            session.command(f"RM {remote_bad}", args.timeout)

        session.command(f"RM {remote}", args.timeout)
        print("PASS: C3COM fixture smoke")
    finally:
        session.close()


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("build-c3com-fixture/fixture.com"))
    parser.add_argument("--bad-crc-output", type=Path, default=Path("build-c3com-fixture/fixture_bad_crc.com"))
    parser.add_argument("--no-bad-crc", action="store_true", help="do not write or smoke a bad-CRC fixture")
    parser.add_argument("--smoke", action="store_true", help="upload fixture and run it over serial")
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args(argv)

    bad_crc_output = None if args.no_bad_crc else args.bad_crc_output
    build_fixture(args.output, bad_crc_output)
    if args.smoke:
        smoke_serial(args, args.output, bad_crc_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
