#!/usr/bin/env python3
"""Verify the Scout UF2 carries the exact published V6 DFU payload."""
import json
import pathlib
import re
import struct
import sys
import zipfile

import lz4.block

uf2_path, ota_path, header_path = map(pathlib.Path, sys.argv[1:4])
uf2 = uf2_path.read_bytes()
assert len(uf2) > 500000 and len(uf2) % 512 == 0, "Invalid UF2 size"
blocks = []
for index in range(len(uf2) // 512):
    block = uf2[index * 512 : (index + 1) * 512]
    magic0, magic1, flags, address, size, number, count, family = struct.unpack_from(
        "<8I", block
    )
    assert (magic0, magic1) == (0x0A324655, 0x9E5D5157)
    assert struct.unpack_from("<I", block, 508)[0] == 0x0AB16F30
    assert flags & 0x2000 and family == 0xADA52840, "Not nRF52840 application UF2"
    assert number == index and count == len(uf2) // 512 and size == 256
    assert 0x26000 <= address and address + size <= 0xF4000
    blocks.append((address, block[32 : 32 + size]))
blocks.sort()
assert blocks[0][0] == 0x26000
for previous, current in zip(blocks, blocks[1:]):
    assert previous[0] + len(previous[1]) == current[0], "Non-contiguous application"
image = b"".join(data for _, data in blocks)
header = header_path.read_text()


def array(name):
    section = re.search(r"\b" + name + r"\[\].*?=\s*\{(.*?)\};", header, re.S)
    assert section, name
    return bytes(
        int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", section.group(1))
    )


compressed = array("kEmbeddedDfuLz4")
dat = array("kEmbeddedDfuDat")
with zipfile.ZipFile(ota_path) as archive:
    assert archive.testzip() is None
    app = json.loads(archive.read("manifest.json"))["manifest"]["application"]
    expected = archive.read(app["bin_file"])
    expected_dat = archive.read(app["dat_file"])
assert (
    sys.argv[4][:7].encode() in expected and b"Model:" in expected
), "Unexpected target application"
assert dat == expected_dat and dat in image, "DFU init packet missing or mismatched"
assert compressed in image, "Compressed payload missing from Scout UF2"
assert lz4.block.decompress(compressed, uncompressed_size=len(expected)) == expected
print(
    f"PASS: nRF52840 Scout, {len(uf2)} bytes, exact V6 application and DFU init packet embedded"
)
