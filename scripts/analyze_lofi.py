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


def analyze(dry, wet, bits, sample_source=None):
    scale = 0.49165056986916567
    rows = []
    source = None
    if sample_source is not None:
        source_rate, source = read(sample_source)
        if source_rate != 96000:
            raise ValueError('Source reconstruction is verified only for the 96 kHz Digital 01 sample')
        source = source[::2]
    for path in sorted(wet.glob('*g144000.wav')):
        rate, x = read(dry / path.name)
        wet_rate, y = read(path)
        if rate != 48000 or wet_rate != rate or x.shape != y.shape:
            raise ValueError('This model is only measured at 48 kHz with matching captures')
        if source is not None:
            if len(source) > len(x) or source.shape[1] != x.shape[1] or not np.any(source):
                raise ValueError('Source sample dimensions or energy are invalid')
            gain = float(np.sum(source*x[:len(source)]) / np.sum(source**2))
            x = np.zeros_like(x)
            x[:len(source)] = source*gain
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
            'input': 'original PCM with fitted dry gain' if source is not None else 'quantized dry capture',
            'model': 'truncate toward zero in sample-amplitude domain; fitted free-running phase',
            'takes': len(rows), 'within_three_pcm_steps': sum(r['peak_error'] <= 3/8388608 for r in rows),
            'limitations': 'Dry PCM is quantized; phase fitted separately; no claim for other rates, filters or Tube.',
            'results': rows}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dry', type=Path)
    parser.add_argument('wet', type=Path)
    parser.add_argument('--bits', type=int, choices=range(4,17), required=True)
    parser.add_argument('--sample-source', type=Path, help='Original 96 kHz Digital 01 PCM for long-gate reconstruction')
    args = parser.parse_args()
    print(json.dumps(analyze(args.dry, args.wet, args.bits, args.sample_source), indent=2))
