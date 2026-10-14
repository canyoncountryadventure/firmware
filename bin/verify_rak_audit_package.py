#!/usr/bin/env python3
"""Verify an application-only legacy Nordic ZIP matches its app-only UF2."""
from pathlib import Path
import json
import struct
import sys
import zipfile

uf2, package = map(Path, sys.argv[1:3])
data = uf2.read_bytes()
blocks = []
for offset in range(0, len(data), 512):
    header = struct.unpack_from('<8I', data, offset)
    blocks.append((header[3], data[offset + 32:offset + 32 + header[4]]))
base = min(address for address, _ in blocks)
end = max(address + len(payload) for address, payload in blocks)
assert base == 0x26000 and end < 0xF4000
image = bytearray(b'\xff' * (end - base))
for address, payload in blocks:
    image[address - base:address - base + len(payload)] = payload
with zipfile.ZipFile(package) as archive:
    assert archive.testzip() is None, 'Corrupt ZIP'
    manifest = json.loads(archive.read('manifest.json'))['manifest']
    assert set(manifest) == {'application'}, 'Not application-only'
    app = manifest['application']
    expected_names = {'manifest.json', app['bin_file'], app['dat_file']}
    assert set(archive.namelist()) == expected_names, 'Unexpected ZIP contents'
    binary = archive.read(app['bin_file'])
    init = archive.read(app['dat_file'])
    assert binary == image, 'DFU application differs from UF2'
    device, revision, version, count = struct.unpack_from('<HHIH', init)
    assert device == 0x0052, 'Wrong device type'
    required = struct.unpack_from('<' + 'H' * count, init, 10)
    assert 0x00B6 in required, 'Wrong S140 6.1.1 requirement'
    crc_offset = 10 + 2 * count
    # Legacy Adafruit genpkg may add a zero CRC-extension identifier.
    if len(init) == crc_offset + 4:
        assert struct.unpack_from('<H', init, crc_offset)[0] == 0
        crc_offset += 2
    assert len(init) == crc_offset + 2, 'Unexpected init format'
    expected_crc = struct.unpack_from('<H', init, crc_offset)[0]
    crc = 0xFFFF
    for byte in binary:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    assert crc == expected_crc, 'Init CRC does not match application'
print(f'PASS {package.name}: app-only, {len(binary)} bytes, matches UF2, device=0x0052, S140=0x00B6, CRC16={crc:04X}')
