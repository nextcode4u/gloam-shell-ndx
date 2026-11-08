"""Interchange codec for the frontend's bounded DSCFG001 configuration archives."""

from __future__ import annotations

import struct
import zlib

MAGIC = b"DSCFG001"
HEADER = struct.Struct("<HHII")
MAX_FILES = 1024
MAX_BYTES = 4 * 1024 * 1024
MAX_RECORD = 65536


def encode_records(records: dict[str, bytes]) -> bytes:
    from configuration import allowed_record

    if len(records) > MAX_FILES or sum(map(len, records.values())) > MAX_BYTES:
        raise ValueError("Native configuration exceeds archive limits")
    archive = bytearray(MAGIC + struct.pack("<I", len(records)))
    for name, data in sorted(records.items()):
        if not allowed_record(name) or len(data) > MAX_RECORD or b"\0" in data:
            raise ValueError("Unsupported native configuration record")
        encoded = name.encode("ascii")
        if not 0 < len(encoded) < 96:
            raise ValueError("Unsupported native configuration path length")
        archive.extend(HEADER.pack(len(encoded), 0, len(data), zlib.crc32(data)))
        archive.extend(encoded)
        archive.extend(data)
    return bytes(archive)


def decode_records(archive: bytes) -> dict[str, bytes]:
    from configuration import allowed_record

    if len(archive) < 12 or archive[:8] != MAGIC or len(archive) > MAX_BYTES + MAX_FILES * 108:
        raise ValueError("Invalid native configuration header or size")
    count = struct.unpack_from("<I", archive, 8)[0]
    if count > MAX_FILES:
        raise ValueError("Too many native configuration records")
    records: dict[str, bytes] = {}
    offset, total = 12, 0
    for _ in range(count):
        if offset + HEADER.size > len(archive):
            raise ValueError("Truncated native record header")
        length, flags, size, crc = HEADER.unpack_from(archive, offset)
        offset += HEADER.size
        if (
            flags
            or not 0 < length < 96
            or size > MAX_RECORD
            or offset + length + size > len(archive)
        ):
            raise ValueError("Invalid native record bounds")
        try:
            name = archive[offset : offset + length].decode("ascii")
        except UnicodeDecodeError as error:
            raise ValueError("Invalid native configuration path") from error
        offset += length
        data = archive[offset : offset + size]
        offset += size
        total += size
        if (
            not allowed_record(name)
            or name in records
            or b"\0" in data
            or zlib.crc32(data) != crc
            or total > MAX_BYTES
        ):
            raise ValueError("Invalid native configuration record")
        records[name] = data
    if offset != len(archive):
        raise ValueError("Trailing native configuration data")
    return records
