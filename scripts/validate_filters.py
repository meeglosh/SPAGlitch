#!/usr/bin/env python3
"""Compare compiled production filters with the separate 34-case Kontakt capture."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
import numpy as np
from scipy.io import wavfile
from compare_audio import read


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('protocol', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('library', type=Path)
    parser.add_argument('renderer', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    protocol = json.loads(args.protocol.read_text())
    rate, reference = read(args.reference)
    if rate != 48000 or protocol['rate'] != rate or len(protocol['cases']) != 34:
        parser.error('Expected the 34-case, 48 kHz filter-holdout protocol')
    if len(reference) != protocol['frames']:
        parser.error('Reference length does not match protocol')
    args.output.mkdir(parents=True, exist_ok=False)
    n = 65536
    rows = []
    for index, case in enumerate(protocol['cases']):
        source = args.library / f"Glitch Digital {case['note']-11:02}.wav"
        source_rate, pcm = read(source)
        if source_rate != 96000:
            parser.error('This fixture expects original 96 kHz source samples')
        pcm = pcm[::2]
        dry = np.zeros((n, 2))
        length = min(n, len(pcm))
        dry[:length] = pcm[:length] * .49165056986916567
        input_path, output_path = args.output/f'{index}-dry.wav', args.output/f'{index}-native.wav'
        wavfile.write(input_path, rate, dry.astype(np.float32))
        subprocess.run([str(args.renderer.resolve()), '--filter-only', str(input_path.resolve()),
                        str(output_path.resolve()), str(case['cutoff']), str(case['resonance']),
                        case['type']], check=True, capture_output=True)
        _, actual = read(output_path)
        expected = reference[case['start']:case['start']+n]
        error = actual-expected
        denominator = np.linalg.norm(expected)
        # Exact linear transfer of the previous provisional two-biquad cascade.
        z = np.exp(-2j*np.pi*np.arange(n//2+1)/n)
        q = 1-z
        old_g = np.tan(np.pi*min(20*1000**(case['cutoff']/1e6),rate*.45)/rate)
        t = old_g*(1+z)
        k = 1/(.70710678118+case['resonance']*.093)
        h = ((t*t if case['type']=='lp' else q*q)/(t*t+k*t*q+q*q))**2
        old = np.fft.irfft(np.fft.rfft(dry, axis=0)*h[:,None], n=n, axis=0)
        rows.append(dict(**case, relative_rms=float(np.linalg.norm(error)/denominator),
                         peak_error=float(abs(error).max()),
                         previous_relative_rms=float(np.linalg.norm(old-expected)/denominator),
                         source_sha256=digest(source)))
    report = dict(status='regression pass; exact parity NOT established',
                  sample_rate=rate, reference_sha256=digest(args.reference),
                  protocol_sha256=digest(args.protocol),
                  regression_limit=.02, measurements=rows,
                  worst_relative_rms=max(r['relative_rms'] for r in rows))
    (args.output/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='measurements'}, indent=2))
    if report['worst_relative_rms'] > report['regression_limit']:
        raise SystemExit('Filter residual exceeds the current regression ceiling')


if __name__ == '__main__':
    main()
