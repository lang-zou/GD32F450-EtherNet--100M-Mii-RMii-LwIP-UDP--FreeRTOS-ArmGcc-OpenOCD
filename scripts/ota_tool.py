#!/usr/bin/env python3
"""
OTA Firmware Upload Tool for GD32F450 FreeRTOS Project

Sends firmware binary to the device via the OTA UDP protocol.
Protocol details are documented in src/app/ota/ota.h.

Usage:
    python ota_tool.py <firmware.bin> [--ip 192.168.1.100] [--port 5001]

Protocol (big-endian TLV over UDP):
    REQ:  magic "OTAQ" (4B) | cmd (1B) | seq (4B) | total_size (4B) → 13 bytes
    ACK:  magic "OTAA" (4B) | seq (4B) | status (1B)                → 9 bytes
    DATA: magic "OTAD" (4B) | seq (4B) | payload_size (2B) | payload(N)
    DONE: magic "OTAO" (4B) | total_size (4B) | crc32 (4B)         → 12 bytes
"""

import socket
import struct
import sys
import os
import time
import zlib
import argparse

# Protocol constants — must match src/app/ota/ota.h
OTA_MAGIC_REQ  = b"OTAQ"
OTA_MAGIC_ACK  = b"OTAA"
OTA_MAGIC_DATA = b"OTAD"
OTA_MAGIC_DONE = b"OTAO"

# Packet sizes
OTA_REQ_SIZE  = 13
OTA_ACK_SIZE  = 9
OTA_DATA_HDR  = 10
OTA_DONE_SIZE = 12

OTA_CHUNK_SIZE = 1024
OTA_ACK_TIMEOUT = 2.0  # seconds

# ACK status codes — must match ota.h
ACK_OK         = 0
ACK_ERR_SIZE   = 1
ACK_ERR_SEQ    = 2
ACK_ERR_OVERFLOW = 3
ACK_ERR_FLASH  = 4
ACK_ERR_COPY   = 5
ACK_ERR_CRC    = 6

_STATUS_NAMES = {
    0: "OK", 1: "INVALID_SIZE", 2: "SEQ_MISMATCH",
    3: "OVERFLOW", 4: "FLASH_ERROR", 5: "COPY_ERROR", 6: "CRC_MISMATCH"
}


def send_req(sock, addr, total_size):
    """Send OTA request packet (big-endian)."""
    packet = OTA_MAGIC_REQ
    packet += struct.pack(">B", 0x01)        # cmd: start
    packet += struct.pack(">I", 0)            # seq: 0
    packet += struct.pack(">I", total_size)   # total size
    assert len(packet) == OTA_REQ_SIZE
    sock.sendto(packet, addr)
    print(f"  REQ sent: total_size={total_size}")


def wait_ack(sock, expected_seq):
    """Wait for ACK with expected sequence number. Returns (success, status)."""
    sock.settimeout(OTA_ACK_TIMEOUT)
    try:
        data, _ = sock.recvfrom(OTA_ACK_SIZE + 16)
        if len(data) < OTA_ACK_SIZE:
            return False, -1
        magic  = data[0:4]
        seq    = struct.unpack(">I", data[4:8])[0]
        status = data[8]

        if magic != OTA_MAGIC_ACK:
            print(f"  Bad ACK magic: {magic}")
            return False, -1
        if seq != expected_seq:
            print(f"  ACK seq mismatch: expected {expected_seq}, got {seq}")
            return False, status
        if status != ACK_OK:
            name = _STATUS_NAMES.get(status, f"UNKNOWN({status})")
            print(f"  Device error: {name}")
            return False, status
        return True, status
    except socket.timeout:
        print(f"  ACK timeout for seq={expected_seq}")
        return False, -1


def send_data_chunk(sock, addr, seq, chunk):
    """Send a data chunk (big-endian)."""
    packet = OTA_MAGIC_DATA
    packet += struct.pack(">I", seq)
    packet += struct.pack(">H", len(chunk))
    packet += chunk
    sock.sendto(packet, addr)


def send_done(sock, addr, total_size, crc):
    """Send DONE packet with CRC32 verification (big-endian)."""
    packet = OTA_MAGIC_DONE
    packet += struct.pack(">I", total_size)
    packet += struct.pack(">I", crc)
    assert len(packet) == OTA_DONE_SIZE
    sock.sendto(packet, addr)
    print(f"  DONE sent: total_size={total_size}, crc=0x{crc:08X}")


def upload_firmware(filepath, ip, port):
    """Main upload flow."""
    if not os.path.exists(filepath):
        print(f"Error: File not found: {filepath}")
        return False

    with open(filepath, "rb") as f:
        fw_data = f.read()

    total_size = len(fw_data)
    crc = zlib.crc32(fw_data) & 0xFFFFFFFF

    print(f"\nOTA Firmware Upload")
    print(f"  File: {filepath}")
    print(f"  Size: {total_size} bytes")
    print(f"  CRC32: 0x{crc:08X}")
    print(f"  Target: {ip}:{port}\n")

    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("", 0))
    sock.settimeout(OTA_ACK_TIMEOUT)

    addr = (ip, port)

    # Step 1: Send REQ
    print("Step 1: Sending REQ...")
    send_req(sock, addr, total_size)

    ok, status = wait_ack(sock, 0)
    if not ok:
        if status > 0:
            print(f"Error: Device rejected REQ (status={status})")
        else:
            print("Error: No ACK received for REQ")
        sock.close()
        return False
    print("  ACK received.")

    # Step 2: Send data chunks
    print(f"\nStep 2: Sending {total_size} bytes in {OTA_CHUNK_SIZE}-byte chunks...")
    num_chunks = (total_size + OTA_CHUNK_SIZE - 1) // OTA_CHUNK_SIZE

    for seq in range(num_chunks):
        start = seq * OTA_CHUNK_SIZE
        end   = min(start + OTA_CHUNK_SIZE, total_size)
        chunk = fw_data[start:end]

        send_data_chunk(sock, addr, seq + 1, chunk)

        ok, status = wait_ack(sock, seq + 1)
        if not ok:
            print(f"\nError: Chunk {seq + 1}/{num_chunks} failed (status={status})")
            sock.close()
            return False

        pct = (seq + 1) * 100 // num_chunks
        print(f"  Chunk {seq + 1}/{num_chunks} OK [{pct}%]", end="\r")

    print()

    # Step 3: Send DONE (device ACKs with seq=0 for DONE)
    print("\nStep 3: Sending DONE...")
    send_done(sock, addr, total_size, crc)

    ok, status = wait_ack(sock, 0)
    if not ok:
        if status > 0:
            print(f"Error: Device rejected DONE (status={status})")
        else:
            print("Error: No ACK for DONE")
        sock.close()
        return False

    print("\n✅ OTA update complete! Device will now reset.")

    sock.close()
    return True


def main():
    parser = argparse.ArgumentParser(
        description="GD32F450 OTA Firmware Upload Tool"
    )
    parser.add_argument("firmware", help="Path to firmware .bin file")
    parser.add_argument("--ip", default="192.168.1.100",
                        help="Device IP address (default: 192.168.1.100)")
    parser.add_argument("--port", type=int, default=5001,
                        help="OTA UDP port (default: 5001)")

    args = parser.parse_args()
    success = upload_firmware(args.firmware, args.ip, args.port)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
