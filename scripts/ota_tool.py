#!/usr/bin/env python3
"""
OTA Firmware Upload Tool for GD32F450 FreeRTOS Project

Sends firmware binary to the device via the OTA UDP protocol.
Protocol details are documented in src/app/ota/ota.h.

Usage:
    python ota_tool.py <firmware.bin> [--ip 192.168.1.100] [--port 5001]

Protocol (simple TLV over UDP):
    REQ:  "OTAQ" | cmd(1B) | seq(4B) | total_size(4B)
    ACK:  "OTAA" | seq(4B) | status(1B)
    DATA: "OTAD" | seq(4B) | chunk_size(2B) | payload(N)
    DONE: "OTAO" | total_size(4B) | crc32(4B)
"""

import socket
import struct
import sys
import os
import time
import zlib
import argparse

# Protocol constants
OTA_MAGIC_REQ  = b"OTAQ"
OTA_MAGIC_ACK  = b"OTAA"
OTA_MAGIC_DATA = b"OTAD"
OTA_MAGIC_DONE = b"OTAO"

OTA_CHUNK_SIZE = 1024
OTA_ACK_TIMEOUT = 2.0  # seconds


def send_req(sock, addr, total_size):
    """Send OTA request."""
    packet = OTA_MAGIC_REQ
    packet += struct.pack("<B", 0x01)       # cmd: start
    packet += struct.pack("<I", 0)           # seq: 0
    packet += struct.pack("<I", total_size)  # total size
    sock.sendto(packet, addr)
    print(f"  REQ sent: total_size={total_size}")


def wait_ack(sock, expected_seq):
    """Wait for ACK with expected sequence number."""
    sock.settimeout(OTA_ACK_TIMEOUT)
    try:
        data, _ = sock.recvfrom(16)
        if len(data) < 9:
            return False
        magic = data[0:4]
        seq   = struct.unpack("<I", data[4:8])[0]
        status = data[8]
        if magic == OTA_MAGIC_ACK and seq == expected_seq and status == 0:
            return True
        print(f"  Unexpected ACK: magic={magic}, seq={seq}, status={status}")
        return False
    except socket.timeout:
        print(f"  ACK timeout for seq={expected_seq}")
        return False


def send_data_chunk(sock, addr, seq, chunk):
    """Send a data chunk."""
    packet = OTA_MAGIC_DATA
    packet += struct.pack("<I", seq)
    packet += struct.pack("<H", len(chunk))
    packet += chunk
    sock.sendto(packet, addr)


def send_done(sock, addr, total_size, crc):
    """Send DONE packet with CRC32 verification."""
    packet = OTA_MAGIC_DONE
    packet += struct.pack("<I", total_size)
    packet += struct.pack("<I", crc)
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
    sock.bind(("", 0))  # Bind to any available port
    sock.settimeout(OTA_ACK_TIMEOUT)

    addr = (ip, port)

    # Step 1: Send REQ
    print("Step 1: Sending REQ...")
    send_req(sock, addr, total_size)

    if not wait_ack(sock, 0):
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

        if not wait_ack(sock, seq + 1):
            print(f"Error: No ACK for chunk {seq + 1}/{num_chunks}")
            sock.close()
            return False

        pct = (seq + 1) * 100 // num_chunks
        print(f"  Chunk {seq + 1}/{num_chunks} OK [{pct}%]", end="\r")

    print()

    # Step 3: Send DONE
    print("\nStep 3: Sending DONE...")
    send_done(sock, addr, total_size, crc)

    if not wait_ack(sock, num_chunks + 1):
        print("Error: No ACK for DONE")
        sock.close()
        return False

    print("\nOTA update complete! Device will now reset.")

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
