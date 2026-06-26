#!/usr/bin/env python3
"""Serial smoke test for shell-owned YMODEM file transfer."""

from __future__ import annotations

import argparse
import dataclasses
import posixpath
import sys
import time

import serial


PROMPT = b"> "

SOH = 0x01
STX = 0x02
EOT = 0x04
ACK = 0x06
NAK = 0x15
CAN = 0x18
CRC_REQ = ord("C")
PAD = 0x1A

PACKET_128 = 128
PACKET_1K = 1024


@dataclasses.dataclass
class ShellResult:
    raw: str
    response: str


@dataclasses.dataclass
class YmodemPacket:
    block_no: int
    data: bytes


class ShellSession:
    def __init__(self, port: str, baud: int, timeout: float) -> None:
        self.ser = serial.Serial(port=port, baudrate=baud, timeout=0.05, write_timeout=timeout)
        self.ser.reset_input_buffer()
        self.rxbuf = bytearray()

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

    def _read_byte(self, timeout: float) -> int:
        if self.rxbuf:
            return self.rxbuf.pop(0)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            data = self.ser.read(1)
            if data:
                return data[0]
            time.sleep(0.005)
        raise TimeoutError("timed out waiting for serial byte")

    def _read_exact(self, size: int, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        buf = bytearray()
        if self.rxbuf:
            take = min(size, len(self.rxbuf))
            buf.extend(self.rxbuf[:take])
            del self.rxbuf[:take]
        while len(buf) < size and time.monotonic() < deadline:
            chunk = self.ser.read(size - len(buf))
            if chunk:
                buf.extend(chunk)
                continue
            time.sleep(0.005)
        if len(buf) != size:
            raise TimeoutError(f"timed out waiting for {size} bytes; got {len(buf)}")
        return bytes(buf)

    def _read_protocol_byte(self, expected: int, timeout: float, label: str) -> None:
        got = self._read_byte(timeout)
        if got != expected:
            raise AssertionError(f"{label}: expected 0x{expected:02x}, got 0x{got:02x}")

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

    def start_transfer_command(self, line: str) -> None:
        self.ser.write(line.encode("utf-8") + b"\r")
        self.ser.flush()

    def wait_for_crc_request(self, timeout: float) -> None:
        deadline = time.monotonic() + timeout
        seen = bytearray()
        while time.monotonic() < deadline:
            data = self.ser.read(1)
            if not data:
                time.sleep(0.005)
                continue
            ch = data[0]
            if ch == CRC_REQ:
                return
            seen.append(ch)
            if seen.endswith(PROMPT):
                text = seen.decode("utf-8", errors="replace")
                raise AssertionError(f"transfer command returned to prompt before CRC request:\n{text}")
            if ch == CAN:
                raise AssertionError("transfer cancelled before CRC request")
        text = seen.decode("utf-8", errors="replace")
        raise TimeoutError(f"timed out waiting for YMODEM CRC request; got:\n{text}")

    def wait_for_command_echo(self, line: str, timeout: float) -> None:
        deadline = time.monotonic() + timeout
        seen = bytearray()
        expected = line.encode("utf-8")
        saw_expected = False
        while time.monotonic() < deadline:
            data = self.ser.read(1)
            if not data:
                time.sleep(0.005)
                continue
            seen.extend(data)
            if expected in seen:
                saw_expected = True
            if saw_expected and data == b"\n":
                return
            if seen.endswith(PROMPT):
                text = seen.decode("utf-8", errors="replace")
                raise AssertionError(f"command returned to prompt before transfer:\n{text}")
        text = seen.decode("utf-8", errors="replace")
        raise TimeoutError(f"timed out waiting for command echo; got:\n{text}")

    def drain_echo_until_idle(self, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        idle_deadline = time.monotonic() + 0.1
        buf = bytearray()
        while time.monotonic() < deadline:
            data = self.ser.read(256)
            if data:
                buf.extend(data)
                idle_deadline = time.monotonic() + 0.1
                continue
            if time.monotonic() >= idle_deadline:
                return bytes(buf)
            time.sleep(0.005)
        raise TimeoutError(f"timed out draining command echo; got:\n{buf.decode('utf-8', errors='replace')}")

    def send_ymodem_file(self, command: str, ymodem_name: str, payload: bytes, timeout: float) -> str:
        self.start_transfer_command(command)
        self.wait_for_command_echo(command, timeout)
        self.wait_for_crc_request(timeout)

        self._write_packet(0, _metadata_block(ymodem_name, len(payload)), PACKET_128)
        self._read_protocol_byte(ACK, timeout, "metadata ACK")
        self._read_protocol_byte(CRC_REQ, timeout, "data CRC request")

        block_no = 1
        for offset in range(0, len(payload), PACKET_1K):
            chunk = payload[offset : offset + PACKET_1K]
            if len(chunk) < PACKET_1K:
                chunk += bytes([PAD]) * (PACKET_1K - len(chunk))
            self._write_packet(block_no, chunk, PACKET_1K)
            self._read_protocol_byte(ACK, timeout, f"data block {block_no} ACK")
            block_no = (block_no + 1) & 0xFF

        self.ser.write(bytes([EOT]))
        self.ser.flush()
        self._read_protocol_byte(NAK, timeout, "first EOT NAK")
        self.ser.write(bytes([EOT]))
        self.ser.flush()
        self._read_protocol_byte(ACK, timeout, "second EOT ACK")
        self._read_protocol_byte(CRC_REQ, timeout, "final metadata CRC request")
        self._write_packet(0, bytes(PACKET_128), PACKET_128)
        self._read_protocol_byte(ACK, timeout, "final metadata ACK")

        return self._read_until_prompt(timeout)

    def recv_ymodem_file(self, command: str, timeout: float) -> tuple[str, bytes]:
        self.start_transfer_command(command)
        self.wait_for_command_echo(command, timeout)
        time.sleep(0.2)
        self.ser.write(bytes([CRC_REQ]))
        self.ser.flush()

        packet = self._read_packet(timeout)
        if packet.block_no != 0:
            raise AssertionError(f"metadata block number was {packet.block_no}, not 0")
        filename, size = _parse_metadata(packet.data)
        self.ser.write(bytes([ACK, CRC_REQ]))
        self.ser.flush()

        received = bytearray()
        expected = 1
        while len(received) < size:
            packet = self._read_packet(timeout)
            if packet.block_no != expected:
                raise AssertionError(f"expected data block {expected}, got {packet.block_no}")
            received.extend(packet.data)
            self.ser.write(bytes([ACK]))
            self.ser.flush()
            expected = (expected + 1) & 0xFF

        got = self._read_byte(timeout)
        if got != EOT:
            raise AssertionError(f"expected first EOT, got 0x{got:02x}")
        self.ser.write(bytes([NAK]))
        self.ser.flush()
        got = self._read_byte(timeout)
        if got != EOT:
            raise AssertionError(f"expected second EOT, got 0x{got:02x}")
        self.ser.write(bytes([ACK, CRC_REQ]))
        self.ser.flush()

        packet = self._read_packet(timeout)
        if packet.block_no != 0 or any(packet.data):
            raise AssertionError("final YMODEM metadata block was not empty")
        self.ser.write(bytes([ACK]))
        self.ser.flush()

        raw = self._read_until_prompt(timeout)
        return raw, bytes(received[:size])

    def _write_packet(self, block_no: int, data: bytes, packet_size: int) -> None:
        if len(data) != packet_size:
            raise ValueError(f"packet data must be {packet_size} bytes")
        packet_type = SOH if packet_size == PACKET_128 else STX
        crc = ymodem_crc16(data)
        packet = bytes([packet_type, block_no & 0xFF, (~block_no) & 0xFF])
        packet += data
        packet += bytes([(crc >> 8) & 0xFF, crc & 0xFF])
        self.ser.write(packet)
        self.ser.flush()

    def _read_packet(self, timeout: float) -> YmodemPacket:
        deadline = time.monotonic() + timeout
        first = self._read_byte(timeout)
        skipped = bytearray()
        while first not in (SOH, STX, CAN) and time.monotonic() < deadline:
            skipped.append(first)
            try:
                first = self._read_byte(max(0.1, deadline - time.monotonic()))
            except TimeoutError as exc:
                text = skipped.decode("utf-8", errors="replace")
                raise TimeoutError(f"timed out waiting for YMODEM packet; skipped:\n{text}") from exc
        if first not in (SOH, STX, CAN):
            text = skipped.decode("utf-8", errors="replace")
            raise TimeoutError(f"timed out waiting for YMODEM packet; skipped:\n{text}")
        if first == CAN:
            second = self._read_byte(1.0)
            if second == CAN:
                raise AssertionError("remote cancelled YMODEM transfer")
            raise AssertionError(f"unexpected CAN followed by 0x{second:02x}")
        if first not in (SOH, STX):
            raise AssertionError(f"expected YMODEM packet, got 0x{first:02x}")

        packet_size = PACKET_128 if first == SOH else PACKET_1K
        header = self._read_exact(2, timeout)
        block_no = header[0]
        if ((header[0] + header[1]) & 0xFF) != 0xFF:
            raise AssertionError(f"bad YMODEM block complement for block {block_no}")

        data = self._read_exact(packet_size, timeout)
        crc_bytes = self._read_exact(2, timeout)
        expected_crc = (crc_bytes[0] << 8) | crc_bytes[1]
        got_crc = ymodem_crc16(data)
        if got_crc != expected_crc:
            raise AssertionError(f"bad CRC for block {block_no}: got 0x{got_crc:04x}, expected 0x{expected_crc:04x}")
        return YmodemPacket(block_no=block_no, data=data)


def ymodem_crc16(data: bytes) -> int:
    crc = 0
    for ch in data:
        crc ^= ch << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def _metadata_block(filename: str, size: int) -> bytes:
    if not filename or "/" in filename or "\x00" in filename:
        raise ValueError("YMODEM filename must be a simple basename")
    metadata = filename.encode("ascii") + b"\x00" + str(size).encode("ascii") + b"\x00"
    if len(metadata) > PACKET_128:
        raise ValueError("YMODEM metadata is too long")
    return metadata + bytes(PACKET_128 - len(metadata))


def _parse_metadata(block: bytes) -> tuple[str, int]:
    parts = block.split(b"\x00", 2)
    if len(parts) < 2 or not parts[0] or not parts[1]:
        raise AssertionError("YMODEM metadata missing filename or size")
    filename = parts[0].decode("ascii", errors="replace")
    try:
        size = int(parts[1].decode("ascii"))
    except ValueError as exc:
        raise AssertionError(f"YMODEM metadata has bad size: {parts[1]!r}") from exc
    return filename, size


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def print_block(label: str, text: str) -> None:
    print(f"[{label}]")
    print(text.rstrip())


def require_ok_text(raw: str, command: str) -> None:
    require("OK" in raw, f"{command} did not return OK; got:\n{raw}")


def fixture_basic(token: str) -> bytes:
    text = (
        f"10 REM YMODEM BASIC FIXTURE {token}\r\n"
        "20 PRINT \"YMODEM TEXT OK\"\r\n"
        "30 FOR I = 1 TO 3\r\n"
        "40 PRINT I\r\n"
        "50 NEXT I\r\n"
    )
    return text.encode("ascii")


def fixture_com(token: str) -> bytes:
    prefix = b"C3COM\x00\xff\x10\x80"
    body = bytes((i * 37 + 11) & 0xFF for i in range(257))
    return prefix + token.encode("ascii") + b"\x00" + body


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args(argv)

    token = f"YMD_{time.time_ns()}"
    basic_name = f"{token}.bas"
    com_name = f"{token}.com"
    basic_path = posixpath.join("/basic", basic_name)
    com_path = posixpath.join("/bin", com_name)
    basic_payload = fixture_basic(token)
    com_payload = fixture_com(token)

    session = ShellSession(args.port, args.baud, args.timeout)
    try:
        banner = session.resync(args.timeout)
        print_block("banner", banner)
        require("READY." in banner or banner.endswith("> "), "did not see shell prompt")

        result = session.command("DF", args.timeout)
        print_block("DF", result.raw)
        require("Filesystem" in result.raw and "Available" in result.raw, "DF did not print capacity header")
        require(" /" in result.raw or "\t/" in result.raw, "DF did not include root mount")

        raw = session.send_ymodem_file(f"RECV {basic_path}", basic_name, basic_payload, args.timeout)
        print_block(f"RECV {basic_path}", raw)
        require_ok_text(raw, f"RECV {basic_path}")

        result = session.command(f"RECV {basic_path}", args.timeout)
        print_block(f"RECV existing {basic_path}", result.response)
        require("Exists" in result.response, "RECV without -F did not reject existing file")

        raw = session.send_ymodem_file(f"RECV -F {com_path}", com_name, com_payload, args.timeout)
        print_block(f"RECV -F {com_path}", raw)
        require_ok_text(raw, f"RECV -F {com_path}")

        session.close()
        session = ShellSession(args.port, args.baud, args.timeout)
        session.resync(args.timeout)

        basic_send_raw, basic_download = session.recv_ymodem_file(f"SEND {basic_path}", args.timeout)
        print_block(f"SEND {basic_path}", basic_send_raw)
        require_ok_text(basic_send_raw, f"SEND {basic_path}")
        require(basic_download == basic_payload, "downloaded BASIC fixture did not match upload")

        com_send_raw, com_download = session.recv_ymodem_file(f"SEND {com_path}", args.timeout)
        print_block(f"SEND {com_path}", com_send_raw)
        require_ok_text(com_send_raw, f"SEND {com_path}")
        require(com_download == com_payload, "downloaded .com fixture did not match upload")

        for path in (basic_path, com_path):
            result = session.command(f"RM {path}", args.timeout)
            print_block(f"RM {path}", result.response)
            require("OK" in result.response or "Open failed" in result.response, f"cleanup RM failed for {path}")

        print(f"PASS: YMODEM transfer smoke uploaded and downloaded {basic_path} and {com_path}")
        return 0
    finally:
        session.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
