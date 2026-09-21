# Tube calibration

The native Tube insert now uses an empirical asymmetric transfer function and
a measured three-pole linear filter, replacing the provisional tanh model.
The source is licensed Kontakt 6.8.0, Digital 01, offline stereo at 48 kHz.
This is a derived model, not Kontakt source code or a claim of sample identity.

## Evidence and limits

`tube-native-validation.json` compares the compiled production model against
24 held-out drive/velocity/gate combinations without alignment or gain fitting.
Worst relative RMS error is 0.000240293 (0.02403%); worst absolute sample error
is 0.0000833273. Three full-velocity cases at drive 62500 were excluded because
the reference clipped. They need quieter recaptures; they are not passes.

Training uses drives 125000, 250000, 375000, 488095, 625000, 750000, 875000,
and 1000000 at velocities 1, 16, 32, 64, 96, and 127. Held-out drives are
62500, 312500, and 550000. Strong-input captures use eight times the normal
Digital group gain and output units 269055 to avoid clipping. The original
output setting is 338989. Measured output amplitude follows `16*(units/1e6)^3`.
The normal group gain is 0.5; the remaining measured dry trim follows inserts.

Negative and positive transfer branches have separate drive-dependent dry
weights and saturate at input magnitude one. A cubic-input Chebyshev surface
models the negative wet branch; a linear-input surface models the positive.
Both use degree 12 in input and degree 4 in drive. Zero drive still filters
the signal. Parameter changes do not clear filter history.

Other sample rates currently use a bilinear pole mapping. That is an inference,
not measured parity. Dynamic drive changes, bypass transitions, multiple notes,
other samples, and listening comparisons remain unverified. The full Destroy
chain also retains unresolved Lo-Fi clock phase. Adaptive filters remain a
separate unfinished model.

## Reproduce

From the repository root, with NumPy and SciPy installed and the ignored local
captures present:

```sh
python3 scripts/tube_calibration/fit_linear.py
python3 scripts/tube_calibration/fit_transfer.py
python3 scripts/tube_calibration/fit_surface.py
python3 scripts/tube_calibration/export_header.py
```

These preserve the fitting procedure and produce local NPZ intermediates under
`local/parity`. `fit_linear.py` uses the zero-drive reference; `fit_transfer.py`
fits constrained cubic splines to the strong-input anchors; `fit_surface.py`
approximates those splines with the production polynomial surface. Audio and
Kontakt states stay local. The production coefficients are in
`Source/TubeCoefficients.h`.

The compiled renderer supports isolated model verification:

```text
SPAGlitchRender --tube-only ABS_DRY_WAV ABS_NEW_OUTPUT_WAV DRIVE_UNITS OUTPUT_UNITS INPUT_SCALE
```

Use input scale 8 for the strong-input reference. The renderer removes and
reapplies the instrument output trim around the actual production TubeModel.
It does not align, normalize, or fit the resulting audio.

`scripts/validate_tube.py ABS_RENDERER NEW_OUTPUT_DIRECTORY` runs the 27-case
held-out matrix, explicitly excludes clipped references, and fails if usable
cases exceed 0.0003 relative RMS or 0.0001 absolute sample error. These bounds
are regression limits for this matrix, not a general perceptual parity claim.
