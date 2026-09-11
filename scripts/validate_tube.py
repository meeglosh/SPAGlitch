#!/usr/bin/env python3
"""Validate compiled TubeModel against held-out local Kontakt captures."""
import argparse
import json
import subprocess
from pathlib import Path
import numpy as np
from compare_audio import read

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('renderer', type=Path)
parser.add_argument('output', type=Path, help='New directory for native WAVs')
parser.add_argument('--captures', type=Path, default=Path('local/parity'))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
rows, excluded = [], []
for drive in (62500, 312500, 550000):
    for velocity in (32, 64, 127):
        for gate in (2400, 24000, 144000):
            name = f'n12-v{velocity}-g{gate}.wav'
            ref = args.captures / f'tube-k6-d{drive}-g12-o269055' / name
            rate, reference = read(ref)
            if rate != 48000 or reference.shape[1] != 2 or not np.any(reference):
                raise ValueError(f'Invalid reference: {ref}')
            if np.max(np.abs(reference)) >= .999:
                excluded.append({'drive': drive, 'file': name, 'reason': 'Reference clipping'})
                continue
            output = args.output / f'd{drive}-{name}'
            subprocess.run([str(args.renderer.resolve()), '--tube-only',
                            str((args.captures / 'dry-kontakt6-repeat' / name).resolve()),
                            str(output.resolve()), str(drive), '269055', '8'], check=True)
            other_rate, native = read(output)
            if other_rate != rate or native.shape != reference.shape or not np.isfinite(native).all():
                raise ValueError(f'Invalid native capture: {output}')
            error = native - reference
            rows.append({'drive': drive, 'file': name,
                         'relative_rms_error': float(np.sqrt(np.sum(error**2)/np.sum(reference**2))),
                         'peak_error': float(np.max(np.abs(error)))})
report = {'scope': 'Compiled production TubeModel; 48kHz; held-out drives; 8x input; output 269055; no alignment or normalization',
          'tested': len(rows), 'excluded': excluded, 'measurements': rows,
          'max_relative_rms_error': max(r['relative_rms_error'] for r in rows),
          'max_peak_error': max(r['peak_error'] for r in rows)}
print(json.dumps(report, indent=2))
if report['max_relative_rms_error'] > .0003 or report['max_peak_error'] > .0001:
    raise SystemExit('Tube reference regression exceeded measured bounds')
