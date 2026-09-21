#!/usr/bin/env python3
"""Validate isolated filters at the rates in guarded Kontakt probe protocols."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
import numpy as np
from scipy.io import wavfile
from compare_audio import read


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    p.add_argument('renderer', type=Path)
    p.add_argument('output', type=Path)
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    rows, guards = [], []
    for rate in [44100, 96000]:
        path = args.directory/f'filter-rate-{rate}.wav'
        meta = json.loads(path.with_suffix('.json').read_text())
        actual_rate, audio = read(path)
        if actual_rate != rate or len(audio) != meta['frames']:
            p.error('Reference format/length mismatch')
        cases = meta['cases']
        if len(cases) != 20 or cases[0]['type'] != 'dry' or cases[-1]['type'] != 'dry':
            p.error('Expected two dry guards surrounding eighteen filter cases')
        n = rate*2
        dry = audio[cases[0]['start']:cases[0]['start']+n]
        repeat = audio[cases[-1]['start']:cases[-1]['start']+n]
        if not np.any(dry) or not np.any(repeat):
            p.error('Silent dry guard; verify content and license')
        residual = float(np.linalg.norm(dry-repeat)/np.linalg.norm(repeat))
        if residual > .001:
            p.error('Dry guards disagree; reject capture and verify asynchronous sample loading')
        guards.append(dict(rate=rate, dry_repeat_relative_rms=residual,
                           reference_sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
        dry_path = args.output/f'dry-{rate}.wav'
        wavfile.write(dry_path, rate, dry.astype(np.float32))
        for index, case in enumerate(cases[1:-1]):
            output = args.output/f'{rate}-{index}.wav'
            subprocess.run([str(args.renderer.resolve()), '--filter-only', str(dry_path.resolve()),
                            str(output.resolve()), str(case['cutoff']), str(case['resonance']),
                            case['type']], check=True, capture_output=True)
            _, candidate = read(output)
            reference = audio[case['start']:case['start']+n]
            error = candidate-reference
            rows.append(dict(rate=rate, **case,
                             relative_rms=float(np.linalg.norm(error)/np.linalg.norm(reference)),
                             peak_error=float(abs(error).max())))
    report = dict(scope='isolated filters; Digital 01, velocity 127; supplied dry reference removes sampler interpolation from this comparison',
                  guards=guards, measurements=rows, regression_limit=.02,
                  worst_relative_rms=max(r['relative_rms'] for r in rows))
    (args.output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='measurements'}, indent=2))
    if report['worst_relative_rms'] > report['regression_limit']:
        raise SystemExit('Cross-rate filter regression ceiling exceeded')


if __name__ == '__main__':
    main()
