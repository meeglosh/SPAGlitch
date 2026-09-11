# Adaptive filter and Lo-Fi timing calibration

## Status

The production fixed cascade has been replaced with an empirical AR LP/HP
model. This is a measured improvement, **not an exact-parity claim**.
A separate 34-case 48 kHz recording, not used to fit the model, gives a worst
relative RMS residual of 1.51641%. It includes Digital 01/02, both filter modes,
cutoffs 350000/650000/850000/950000, and resonance 40/70/90/100, plus two
initialization cases. The 2% validation ceiling is a regression guard, not a
perceptual or parity acceptance threshold. Per-case results, the previous
cascade's residuals, and reference hashes are in `filter-native-validation.json`.

The isolated Lo-Fi clock passes all 32 continuous-timeline measurements at
buffer sizes 32, 64, 256 and 512. Worst relative RMS residual is 2.166e-7.
These comparisons predict phase from the timeline; no per-note phase is fitted.
`lofi-clock-validation.json` records the measurements. This excludes downstream
Tube/filter tails and does not establish full Destroy-chain timing parity.

## Filter identification

A low-level 101-point resonance sweep at cutoff 500000 established the linear
transfer. Let q = 1-z^-1, t = g*(1+z^-1), D = q²+k*t*q+t², and r = resonance/100.
The transfers are:

- LP: gain * (t²*D + 2*r*t²*q²) / D²
- HP: gain * (q²*D + 2*r*t²*q²) / D²

This is a two-pole low/high-pass plus a pair of cascaded band-pass stages,
with shared damping. It is not two cascaded low/high-pass biquads.
Gain is 10^(-2.4*r/20). The ordinary low-level damping fit is
min(2, 0.28 + 1.72*10^(0.3-r)). Five independently fitted g/k pairs are retained
in `filter-transfer-fixtures.json`. CTest compares 30 complex responses of the
production implementation against those coefficients.

The cutoff sweep follows f = 8.17579891564 * 2^(145*cutoff/12000000), then
x = pi*f/fs and g = x*(1+x²/3+2*x⁴/15), with a measured 48 kHz ceiling of
5.6189228088. Using an actual tangent substantially changes the upper range.
Very low cutoffs have larger residuals; the nominal equation is not an exact
reconstruction of Kontakt's internal numerical approximation.

Matched LP/HP captures allow the first band's signal and damping to be inferred.
Loud signals share a time-varying damping value across stereo channels. Static
nonlinearities and a single attack/release follower did not reproduce it.
The implemented empirical approximation uses the sum of absolute second-band
signals, peak hold with exponential decay, a rational damping target, and a
separate one-pole smoothing stage. At 48 kHz, output-domain fitting gave
22.3373503 frames of smoothing, 2264.82240 frames of peak decay, and a detector
threshold of 0.452844742 in reference-output units. The implementation converts
the threshold to internal group-amplitude units. Two filters retain independent
state and parameter memory. All processing uses fixed storage on the audio thread.

## Confirmed remaining discrepancies

- The 34-case residual above is real. Weak nonlinear response, especially at low
  cutoffs/high resonance, is not yet reproduced exactly. The louder fitting
  matrix also retains residuals; matching fitted coefficients alone is not a pass.
- Fresh reference instances show first-active-note dependence. Initial cutoff
  100000 or 200000 changes later low-level damping at cutoff 500000; merely
  setting those cutoffs before the first note does not. At resonance 50, measured
  damping was approximately 1.49631 and 1.30042 respectively, versus 1.36524 for
  ordinary initialization. The cause is unestablished. Production does not
  hard-code these isolated observations or pretend to reproduce this behavior.
- The cutoff ceiling and adaptive time constants at other sample rates are
  extrapolated. Five-rate numerical stability tests are not sound-parity tests.
- Rapid parameter sweeps, other samples/categories, stacked voices and combined
  filter/Destroy processing still require held-out reference comparisons.

## Lo-Fi timing

At the saved reducer setting, a capture occurs on frame 10 and repeats every
11 frames. Bypass transitions clear held values and restart the countdown.
The clock advances through whole active host buffers and one additional buffer
when activity ends, then pauses. MIDI CC120 during an idle interval preserves
phase. Separate 44.1/96 kHz probes also supported an 11-frame hold; full
waveform agreement at 44.1 kHz is unresolved.

Downstream insert tails keep the clock active after the voices finish. Kontakt's
exact block-dependent tail shutdown criterion remains unresolved. Production
now includes effect-tail activity with a provisional 1e-6 peak floor; this is
explicitly an approximation and can produce a different phase on later notes.
Do not use the isolated clock tests as evidence that the full chain matches.

## Reproducing captures and compiled validation

Build with `SPAGLITCH_REFERENCE_HOST=ON`. On this Mac, `KontaktProbe` uses the
licensed Kontakt 6 VST3 at `/Library/Audio/Plug-Ins/VST3/Kontakt.vst3`.
It reloads a state, waits for asynchronous sample loading, and renders a MIDI
protocol without requiring the editor or an unlocked screen. Verify a dry
capture after changing the state; the current saved probe state produced a
bit-identical dry check against the accepted reference.

Install `tools/MidiEffectProbe.ksp` into a spare script slot of the temporary
reference copy, leaving the original script intact, and save its state through
Reference Host. The helper controls effect selection, cutoff, resonance,
group level and bit depth through MIDI CC. The state and audio stay under
ignored `local/parity`; never commit the NKI, state or sample library.

Example commands (replace STATE with the local saved state):

```sh
python3 scripts/effect_probe_protocols.py filter-holdout local/parity/new-filter \
  --probe /private/tmp/spaglitch-native-build/KontaktProbe --state STATE
python3 scripts/validate_filters.py \
  local/parity/new-filter/filter-holdout.json local/parity/new-filter/filter-holdout.wav \
  local/library /private/tmp/spaglitch-native-build/SPAGlitchRender local/parity/new-filter-native
python3 scripts/effect_probe_protocols.py lofi-clock local/parity/new-clock \
  --probe /private/tmp/spaglitch-native-build/KontaktProbe --state STATE
python3 scripts/validate_lofi_clock.py local/parity/new-clock \
  'local/library/Glitch Digital 01.wav' local/parity/new-clock/results.json
ctest --test-dir /private/tmp/spaglitch-native-build --output-on-failure
```

The validators require NumPy and SciPy. Output paths must be new. Filter
validation invokes the compiled production class via `SPAGlitchRender`; its
reference inputs are generated locally from the user's original PCM. Numeric
measurements and derived source code are published with the user's authorization.
