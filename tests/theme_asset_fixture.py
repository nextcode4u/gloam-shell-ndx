"""Independent prepared-theme fixtures. No external artwork or Pillow required."""

from pathlib import Path
import struct
import sys
import zlib

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
icon = struct.pack("<H", 0xFC00) * 1024
panel = struct.pack("<H", 0x801F) * 1024 + bytes([0x55]) * 256
records = struct.pack("<BBHI", 0, 0, 0, len(icon)) + icon
records += struct.pack("<BBHI", 1, 0, 3, len(panel)) + panel
data = b"GLMART01" + struct.pack("<II", len(records), 2) + zlib.compress(records)
(root / "good.gfx").write_bytes(data)
(root / "truncated.gfx").write_bytes(data[:-8])
duplicate = records + records
(root / "duplicate.gfx").write_bytes(
    b"GLMART01" + struct.pack("<II", len(duplicate), 4) + zlib.compress(duplicate)
)
(root / "oversized.gfx").write_bytes(
    b"GLMART01" + struct.pack("<II", 0xFFFFFFFF, 2) + data[16:]
)
frames = b"".join(
    struct.pack("<H", color) * 1024 + bytes([255]) * 256
    for color in (0x801F, 0x83E0, 0xFC00, 0xFFFF)
)
animation = b"GLMFX001" + bytes([4, 32, 32, 0, 0, 0, 0, 0]) + frames
(root / "good.gtm").write_bytes(animation)
(root / "truncated.gtm").write_bytes(animation[:-1])
(root / "oversized.gtm").write_bytes(animation[:8] + bytes([255]) + animation[9:])
