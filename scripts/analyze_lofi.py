#!/usr/bin/env python3
"""Test a candidate Lo-Fi model against local dry/effect captures.

Phase is fitted per take: this tests the transfer function and hold period,
not startup phase parity. Dry 24-bit quantization can cross a bit threshold;
one-step residuals must not be silently treated as exact matches.
"""
import argparse
import json
from pathlib import Path
import numpy as np
from compare_audio import read


def analyze(dry, wet, bits):
    scale = 0.49165056986916567
    rows = []
    for path in sorted(wet.glob('*g144000.wav')):
        rate, x = read(dry / path.name)
        wet_rate, y = read(path)
        if rate != 48000 or wet_rate != rate or x.shape != y.shape:
            raise ValueError('This model is only measured at 48 kHz with matching captures')
        best = None
        for phase in range(11):
            indices = np.maximum(0, ((np.arange(len(x)) - phase) // 11) * 11 + phase)
            predicted = np.trunc(x[indices] / scale * 2**(bits-1)) / 2**(bits-1) * scale
            error = predicted - y
            result = {'file': path.name, 'phase_frames': phase,
                      'rms_error': float(np.sqrt(np.mean(error**2))),
                      'peak_error': float(np.max(np.abs(error)))}
            if best is None or result['rms_error'] < best['rms_error']:
                best = result
        rows.append(best)
    if not rows:
        raise ValueError('No long-gate reference files found')
    return {'bits': bits, 'rate': 48000, 'hold_frames': 11,
            'model': 'truncate toward zero in sample-amplitude domain; fitted free-running phase',
            'takes': len(rows), 'within_three_pcm_steps': sum(r['peak_error'] <= 3/8388608 for r in rows),
            'limitations': 'Dry PCM is quantized; phase fitted separately; no claim for other rates, filters or Tube.',
            'results': rows}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dry', type=Path)
    parser.add_argument('wet', type=Path)
    parser.add_argument('--bits', type=int, choices=range(4,17), required=True)
    args = parser.parse_args()
    print(json.dumps(analyze(args.dry, args.wet, args.bits), indent=2))
