#!/usr/bin/env python3
"""Reject a stale, wrong-target, or non-unified RAK V7 UF2."""
from pathlib import Path
import hashlib
import struct
import sys

path = Path(sys.argv[1])
source_sha = sys.argv[2][:7].encode()
data = path.read_bytes()
assert len(data) >= 500000 and len(data) % 512 == 0, "Unexpected RAK image size"
blocks = [struct.unpack_from("<8I", data, i) for i in range(0, len(data), 512)]
assert all(b[0] == 0x0A324655 and b[1] == 0x9E5D5157 and b[2] & 0x2000 and b[7] == 0xADA52840 for b in blocks), "Wrong chip family"
assert all(struct.unpack_from("<I", data, i + 508)[0] == 0x0AB16F30 for i in range(0, len(data), 512)), "Invalid UF2 block"
assert all(b[5] == j and b[6] == len(blocks) and 0 < b[4] <= 476 for j, b in enumerate(blocks)), "Invalid block numbering"
minimum = min(b[3] for b in blocks)
maximum = max(b[3] + b[4] for b in blocks)
assert minimum == 0x26000 and maximum < 0xF4000, "Not an app-only RAK image"
payload = b"".join(data[i + 32:i + 32 + blocks[i // 512][4]] for i in range(0, len(data), 512))
assert source_sha in payload, "Image does not match source commit"
for marker in (b"RAK WATER DISTANCE + HOBO V7", b"HOBO BLE OFF", b"HOBO BLE ON", b"SENSOR options: MB7388, A01NYUB"):
    assert marker in payload, f"Missing V7 marker: {marker!r}"
print(f"PASS {path.name}: {len(blocks)} NRF52840 blocks, app {minimum:#x}..{maximum:#x}, sha256={hashlib.sha256(data).hexdigest()}")
