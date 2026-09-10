#!/usr/bin/env python3
"""Copy downloaded Glitch WAVs into a local working library; source is untouched."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path

COUNTS = {
    'Glitch Digital': 46, 'Glitch Digital Long': 14, 'Glitch Digital Short': 95,
    'Glitch Heavy': 76, 'Glitch Heavy Long': 24, 'Glitch Heavy Short': 65,
    'Glitch Rapid Modulation': 32, 'Glitch Squelchy': 50, 'Glitch Percussive': 77,
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    source, destination = args.source.resolve(), args.destination.resolve()
    if source == destination:
        parser.error('Source and destination must differ')
    names = [f'{group} {i:02}.wav' for group, count in COUNTS.items() for i in range(1, count+1)]
    missing = [name for name in names if not (source/name).is_file()]
    if missing:
        parser.error(f'Missing {len(missing)} WAV files; first: {missing[0]}')
    destination.mkdir(parents=True, exist_ok=True)

    def copy(name):
        target = destination/name
        # Content comparison also hydrates cloud placeholders before the native
        # instrument opens them. Never overwrite different local content.
        data = (source/name).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if target.exists():
            if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
                raise ValueError(f'Destination has different content: {target}')
        else:
            temp = target.with_suffix('.wav.partial')
            with temp.open('xb') as handle:
                handle.write(data)
            temp.rename(target)
        return name, digest

    hashes = {}
    with ThreadPoolExecutor(max_workers=8) as pool:
        for task in as_completed([pool.submit(copy, name) for name in names]):
            name, digest = task.result()
            hashes[name] = digest
            if len(hashes) % 25 == 0:
                print(f'Copied and hashed {len(hashes)}/479 samples', flush=True)
    (destination/'manifest.json').write_text(json.dumps(dict(sorted(hashes.items())), indent=2)+'\n')
    print(f'Imported 479 samples to {destination}', flush=True)

if __name__ == '__main__':
    main()
