#!/usr/bin/env python3
"""Verify sample keys/root keys against the supplied NKI revision."""
import argparse
import hashlib
from pathlib import Path
import re
import struct
from extract_reference import SOURCE_SHA256, unpack
from import_library import COUNTS


def verify(path):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError('Different NKI revision; offsets must be inspected again')
    data = unpack(data[1154:27179], 223746)
    names = [m.group().decode('utf-16le') for m in re.finditer(rb'(?:[ -~]\x00){8,}', data)]
    names = [name for name in names if name.endswith('.wav')]
    if len(names) != 479:
        raise ValueError('Expected 479 sample references')
    seen = set()
    for index, name in enumerate(names):
        match = re.fullmatch(r'(.+) (\d+)\.wav', name)
        if not match or match[1] not in COUNTS:
            raise ValueError(f'Unexpected sample: {name}')
        number = int(match[2])
        if not 1 <= number <= COUNTS[match[1]] or name in seen:
            raise ValueError(f'Invalid or duplicate sample: {name}')
        seen.add(name)
        offset = 43660 + 216 * index
        vlo, vhi, low, high = struct.unpack_from('<4H', data, offset)
        root = struct.unpack_from('<H', data, offset+16)[0]
        sample_index = struct.unpack_from('<I', data, offset+30)[0]
        if (vlo, vhi, low, high, root, sample_index) != (1, 127, number+11, number+11, number+11, index):
            raise ValueError(f'Unexpected zone mapping for {name}')
    return len(seen)

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('nki',type=Path)
    args=p.parse_args()
    print(f'Verified all {verify(args.nki)} sample keys, root keys, velocity ranges and file indices')
