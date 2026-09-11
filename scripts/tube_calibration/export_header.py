"""Export the fitted surface; run from the repository root after fit_surface.py."""
from pathlib import Path
import numpy as np

model = np.load('local/parity/tube-polynomial.npz')
lines = ['// Derived from licensed Kontakt 6.8.0 measurements; see docs/TUBE-CALIBRATION.md.',
         '#pragma once', '#include <array>', 'namespace glitch::tubeCalibration', '{']
for name in ('negative', 'positive'):
    coefficients = model[name]
    assert coefficients.shape == (13, 5) and np.isfinite(coefficients).all()
    lines.append(f'inline constexpr std::array<std::array<double,5>,13> {name} {{{{')
    lines.extend('    {{' + ', '.join(format(x, '.17g') for x in row) + '}},'
                 for row in coefficients)
    lines.append('}};')
lines.append('inline constexpr std::array<double,4> denominator {{' +
             ', '.join(format(x, '.17g') for x in model['denominator']) + '}};')
lines.append(f'inline constexpr double numeratorGain={float(model["A"]):.17g};')
lines.append('}')
Path('Source/TubeCoefficients.h').write_text('\n'.join(lines) + '\n')
