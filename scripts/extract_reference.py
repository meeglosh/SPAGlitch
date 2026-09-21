#!/usr/bin/env python3
"""Recover the supplied SPAGlitch KSP; deliberately not a general NKI parser."""
import argparse
import hashlib
from pathlib import Path

SOURCE_SHA256 = '4e05bae1a2df26d333f5ec812c89b6641f4a1d54815a9efbda483e6fa49a1af9'
SCRIPT_SHA256 = 'd7f8ebc2496eb7b62246c04b13a1e61355113f047c602137284691248492b1c7'


def unpack(data, expected):
    """Decode this file's FastLZ level-2 payload with bounded references."""
    if not data or data[0] >> 5 != 1:
        raise ValueError('Expected FastLZ level 2')
    out = bytearray()
    pos, control = 1, data[0] & 31

    def byte():
        nonlocal pos
        if pos >= len(data):
            raise ValueError('Truncated payload')
        value = data[pos]
        pos += 1
        return value

    while True:
        if control < 32:
            length = control + 1
            if pos + length > len(data):
                raise ValueError('Truncated literal')
            out.extend(data[pos:pos + length])
            pos += length
        else:
            length = (control >> 5) - 1
            high = (control & 31) << 8
            if length == 6:
                while True:
                    extra = byte()
                    length += extra
                    if extra != 255:
                        break
            low = byte()
            distance = high + low + 1
            if high == 7936 and low == 255:
                distance = (byte() << 8) + byte() + 8192
            if distance > len(out) or len(out) + length + 3 > expected:
                raise ValueError('Invalid back reference')
            for _ in range(length + 3):
                out.append(out[-distance])
        if len(out) > expected:
            raise ValueError('Output exceeds header length')
        if pos == len(data):
            break
        control = byte()
    if len(out) != expected:
        raise ValueError('Decoded length differs from NKI header')
    return bytes(out)


def extract(path):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError('Different NKI revision; inspect its structure before extracting')
    expected = int.from_bytes(data[1146:1150], 'little')
    length = int.from_bytes(data[1150:1154], 'little')
    decoded = unpack(data[1154:1154 + length], expected)
    start = decoded.index(b'{\n*')
    script = decoded[start:decoded.index(b'\0', start)]
    if hashlib.sha256(script).hexdigest() != SCRIPT_SHA256:
        raise ValueError('Unexpected recovered script')
    return script


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('nki', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.write_bytes(extract(args.nki))
    print(f'Recovered 21,041 bytes of KSP into {args.output}')
