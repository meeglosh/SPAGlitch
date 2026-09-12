"""Reject incomplete, altered or cloud-only factory libraries before packaging."""
import hashlib
import json
import pathlib
import sys
root = pathlib.Path(sys.argv[1])
manifest = json.loads(pathlib.Path(__file__).with_name('sample-manifest.json').read_text())
assert len(manifest) == 479
for name, expected in manifest.items():
    source = root / name
    assert not getattr(source.stat(), 'st_flags', 0) & 0x40000000, f'Cloud-only sample: {name}'
    assert hashlib.sha256(source.read_bytes()).hexdigest() == expected, f'Sample mismatch: {name}'
assert {p.name for p in root.glob('*.wav')} == set(manifest), 'Unexpected sample files'
print('Verified all 479 original factory samples')
