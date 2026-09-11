# Kontakt comparison procedure

The measurement tools are development utilities, not instrument dependencies.
Reference captures and plugin states are local and ignored by Git. Never infer
sonic parity from native unit tests alone.

## Build and capture

Configure with `-DSPAGLITCH_REFERENCE_HOST=ON` on macOS to build
`Glitch Reference Host.app`. Select a licensed installed Kontakt VST3 version
(6, 7, or 8; defaults to 6) before loading. Kontakt 6 uses `Kontakt.vst3`.
The selector does not verify activation; confirm full mode in Kontakt. Click
**Load Kontakt**, then use Kontakt's editor to load the reference Glitch NKI.
Do not overwrite the source instrument. Wait for all samples to load, choose
the Digital category, and record the actual insert bypass states and knob values.
Name the capture and click **Capture matrix**. Results go to
`~/Documents/ChatGPT/SPAGlitch/local/parity/<name>` in a new directory.

The live host selects one fixed rate (48/44.1/96 kHz) **before** Kontakt loads.
The rate selector is then disabled. Restart the host and reload the NKI to test a
new rate: Kontakt warned that in-session rate changes require restarting, and the
old multi-rate captures are invalid. Processing now uses VST3 offline mode.

The matrix uses MIDI channel 1, note 12, 256-frame blocks and four seconds per
file. "All velocities" captures 1..127 with a 3 s gate, plus 50/500 ms gates for
127/64/32. "Three velocities" captures only those nine baseline files. Non-48 kHz
filenames carry a rate prefix. The native renderer currently produces the
nine-file baseline at 48 kHz. One second of drain after all-sound-off separates takes.
The host records the first stereo output and saves Kontakt state alongside WAVs.
It renders through the normal plugin processing API without playing through a
physical device. A silent full-velocity capture or a Kontakt demo timeout invalidates the take.
Do not bypass Kontakt licensing or demo limits.

Native example (use absolute paths):

```sh
SPAGlitchRender /path/to/library /path/to/new/native-dry category=0 randomness=0 destroy=1 filter=1
python3 scripts/compare_audio.py /path/to/kontakt/n12-v127-g144000.wav /path/to/native/n12-v127-g144000.wav
```

The native output folder must be new. Parameter assignments use the IDs/ranges
in `Source/Plugin.cpp`. Native `gain` defaults to 0 dB; explicitly record any
adjustment. Do not normalize either WAV before comparison. The comparator needs
NumPy and accepts integer PCM WAVs; it rejects mismatched rates, shapes or silence.
Positive delay means the candidate lags the reference. Alignment can hide startup
differences, so inspect both raw and aligned errors. A gain-corrected residual is
diagnostic, not proof of equal output levels.

## Measurement order and completion evidence

1. Dry playback: match original output/group gain; compare velocity ratios and
   onset/release envelopes across the gate matrix. Inspect group modulation before
   deciding whether velocity, sustain or pitch wheel should affect playback.
2. Pitch: extend the capture matrix to -12/-7/+7/+12 semitones and 44.1/96 kHz;
   compare phase/interpolation and note lengths. The current matrix alone cannot
   validate interpolation across rates.
3. Effects: capture Lo-Fi alone, distortion alone, then both; sweep depth/drive,
   and measure sample-hold clock, transfer curve and output compensation. Use a
   separate editable measurement instrument with known signals if authorized
   Kontakt editing is available; preserve the original NKI.
4. Filters: capture low/high-pass separately across cutoff and resonance,
   including high resonance, transient response, mode changes, and bypass tails.
   Map normalized engine parameters to physical values; do not assume the
   provisional native frequency curve is Kontakt's.
5. Script: compare manual gestures, random 0/1/79/80/99/100 transitions, sustained
   voices during changes, missing key zones, save/reload and control automation.
   Keep the safe unmapped-Boom termination; document any intentional deviation.

Archive exact settings, source/plugin versions, rates, MIDI, reference WAV hashes,
metrics, and listening notes for every accepted calibration. Pin tolerances to
measured repeatability of Kontakt, not a convenient arbitrary threshold. Do not
declare this phase complete until effect sweeps, playback tests and transition
comparisons have actual reference results.
