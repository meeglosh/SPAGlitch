#!/usr/bin/env python3
"""Validate isolated Lo-Fi timelines without fitting a per-note clock phase."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from compare_audio import read


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    p.add_argument('sample', type=Path)
    p.add_argument('report', type=Path)
    args = p.parse_args()
    rate, source = read(args.sample)
    if rate != 96000:
        p.error('Expected original 96 kHz Digital 01 source')
    source = source[::2]
    rows = []
    for block in [32, 64, 256, 512]:
        path = args.directory / f'lofi-clock-b{block}.wav'
        meta = json.loads(path.with_suffix('.json').read_text())
        rate, audio = read(path)
        if rate != 48000 or meta['block'] != block or len(audio) != meta['frames']:
            p.error('Unexpected capture format or length')
        processed = 0
        for case in meta['cases']:
            start, gate = case['start'], case['gate']
            start_block = start//block*block
            voice_end = start+min(gate+480, len(source))
            end_block = (voice_end+block-1)//block*block+block
            phase = (10-processed-(start-start_block)) % 11
            processed += end_block-start_block
            n = 32000
            x = np.zeros((n, 2))
            x[:len(source)] = source*np.clip(1-(np.arange(len(source))-gate)/479, 0, 1)[:,None]
            index = np.maximum(0, ((np.arange(n)-phase)//11)*11+phase)
            predicted = np.trunc(x[index]*128)/128*.49165056986916567
            ref = audio[start:start+n]
            error = predicted-ref
            rows.append(dict(block=block, start=start, gate=gate, predicted_phase=int(phase),
                             relative_rms=float(np.linalg.norm(error)/np.linalg.norm(ref)),
                             peak_error=float(abs(error).max()),
                             reference_sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    worst = max(r['relative_rms'] for r in rows)
    report = dict(scope='isolated Lo-Fi, Digital 01, velocity 127, 8 bits, 48 kHz; excludes downstream tails',
                  worst_relative_rms=worst, measurements=rows)
    with args.report.open('x') as output:
        json.dump(report, output, indent=2)
        output.write('\n')
    print(f'{len(rows)} cases; worst relative RMS {worst:.9g}')
    if worst > 1e-6:
        raise SystemExit('Isolated Lo-Fi timing regression')


if __name__ == '__main__':
    main()
