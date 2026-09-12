#!/usr/bin/env python3
"""Validate and name combined field-node download artifacts."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--env', required=True)
p.add_argument('--label', choices=['rak', 'seed'], required=True)
args = p.parse_args()
out = Path('downloads')
out.mkdir(exist_ok=True)
records = {}
for suffix in ['uf2', 'zip']:
    matches = list(Path('release').glob(f'firmware-{args.env}-*.{suffix}'))
    assert len(matches) == 1, (suffix, matches)
    source = matches[0]
    if suffix == 'zip':
        with zipfile.ZipFile(source) as z:
            assert z.testzip() is None
            manifest = json.loads(z.read('manifest.json'))['manifest']['application']
            image = z.read(manifest['bin_file'])
            init = z.read(manifest['dat_file'])
            assert len(init) >= 12 and len(image) > 100000
            assert f'{args.label}-distance-hobo-safe 1.0'.encode() in image
            assert b'CAL STAGE' in image and b'NEWREAD64' in image
    else:
        data = source.read_bytes()
        assert len(data) % 512 == 0
        for pos in range(0, len(data), 512):
            block = data[pos:pos + 512]
            assert struct.unpack_from('<II', block) == (0x0A324655, 0x9E5D5157)
            assert struct.unpack_from('<I', block, 508)[0] == 0x0AB16F30
            assert struct.unpack_from('<I', block, 28)[0] == 0xADA52840
    dest = out / f'{args.label}-distance-hobo-safe-1.0.{suffix}'
    shutil.copyfile(source, dest)
    records[dest.name] = {'sha256': hashlib.sha256(dest.read_bytes()).hexdigest(), 'bytes': dest.stat().st_size}
print(json.dumps(records, indent=2))
(out / f'{args.label}-distance-hobo-safe-checksums.json').write_text(json.dumps(records, indent=2) + '\n')
