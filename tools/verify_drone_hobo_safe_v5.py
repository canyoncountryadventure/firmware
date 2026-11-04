#!/usr/bin/env python3
"""Verify Scout UF2 embeds exactly the application and init packet in a pinned V5 OTA."""
import argparse
import json
import pathlib
import re
import struct
import zipfile
import lz4.block

parser = argparse.ArgumentParser()
parser.add_argument("scout_uf2", type=pathlib.Path)
parser.add_argument("target_ota_zip", type=pathlib.Path)
parser.add_argument("generated_header", type=pathlib.Path)
args = parser.parse_args()
binary = args.scout_uf2.read_bytes()
assert len(binary) > 500000 and len(binary) % 512 == 0, "invalid Scout UF2"
blocks = []
for index in range(len(binary) // 512):
    block = binary[index * 512:(index + 1) * 512]
    magic0, magic1, flags, address, nbytes, number, total, family = struct.unpack_from("<8I", block)
    assert magic0 == 0x0A324655 and magic1 == 0x9E5D5157, "invalid UF2 block"
    assert struct.unpack_from("<I", block, 508)[0] == 0x0AB16F30, "invalid UF2 footer"
    assert flags & 0x2000 and family == 0xADA52840, "not nRF52840 application UF2"
    assert number == index and total == len(binary) // 512 and nbytes == 256, "inconsistent UF2 block"
    assert 0x26000 <= address and address + nbytes <= 0xF4000, "UF2 out of application region"
    blocks.append((address, block[32:32 + nbytes]))
blocks.sort()
assert blocks[0][0] == 0x26000, "invalid application origin"
assert all(a[0] + len(a[1]) == b[0] for a, b in zip(blocks, blocks[1:])), "noncontiguous UF2"
image = b"".join(value for _, value in blocks)
header = args.generated_header.read_text()
def extract_array(name):
    match = re.search(r"\b" + name + r"\[\].*?=\s*\{(.*?)\};", header, re.S)
    assert match, f"missing {name}"
    return bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", match.group(1)))
compressed = extract_array("kEmbeddedDfuLz4")
init_packet = extract_array("kEmbeddedDfuDat")
with zipfile.ZipFile(args.target_ota_zip) as archive:
    assert archive.testzip() is None, "invalid Nordic DFU ZIP"
    app = json.loads(archive.read("manifest.json"))["manifest"]["application"]
    target_image = archive.read(app["bin_file"])
    target_init = archive.read(app["dat_file"])
assert target_image and target_init, "empty target application"
assert init_packet == target_init and target_init in image, "DFU init mismatch"
assert compressed in image, "target compressed application missing from Scout"
assert lz4.block.decompress(compressed, uncompressed_size=len(target_image)) == target_image, "wrong embedded application"
assert f"kEmbeddedDfuOriginalSize = {len(target_image)}UL" in header, "embedded image length mismatch"
print(f"PASS Scout UF2={len(binary)} bytes, target={len(target_image)} bytes, "
      f"LZ4={len(compressed)} bytes; exact pinned application and Nordic init verified")
