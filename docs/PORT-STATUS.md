# Port status

## Recovered reference

The supplied NKI contains an ordinary FastLZ level-2 payload at byte 1154.
The 26,025 compressed bytes decode to exactly 223,746 bytes, matching the
recorded length. The embedded 21,041-byte KSP is preserved locally in reference/Glitch.ksp
and excluded from the public repository.
The extractor verifies both source and script SHA-256 hashes and intentionally
rejects other NKI revisions. No original file is modified.

The bundled READ ME.pdf contains general product/support information, not an
instrument manual. Kontakt 8 is installed, but native UI access timed out twice;
no Kontakt UI, sound comparison, zone mapping, or saved engine defaults have
been verified yet.

## What runs now

JUCE 8.0.14 AU, VST3, and standalone playback foundation, 32 sampler voices,
WAV selection, MIDI/onscreen keyboard, output parameter automation, and parameter
serialization. A selected sample is provisionally pitched from MIDI note 60.
This is an audition harness, not the original category mapping.

Sample paths are recorded in state but sample loading on restore is deliberately
not implemented yet: restore must not perform disk I/O on a host audio callback.
An asynchronous asset manager with explicit loading/error state is the next
prerequisite. Reload a sample explicitly in this milestone. UI loading is currently
synchronous; use downloaded local WAV files rather than cloud placeholders.

## Confirmed KSP behavior to port

- Nine categories, counts 46/14/95/76/24/65/32/50/77 in menu order.
- Keyboard highlighting begins at MIDI note 12. Actual zone sample ordering,
  root pitches, envelopes, tuning, looping, gain, and pan still require inspection.
- Pitch -12..12; Lo-Fi 0..8; distortion and cutoff 0..1,000,000;
  resonance and randomness 0..100; destroy 0..1; filter mode 0..2.
- Randomness above zero perturbs pitch, distortion, bit depth, cutoff, resonance
  on each note. At 100 it also picks an eligible category, destroy state and filter.
- Distortion compensation uses 400,000 - drive/8 in Kontakt parameter units.
- Bit setting uses Lo-Fi * 62,500 + 250,000, plus random amount * 2,300.
- Random pitch uses userPitch * 80,000 + random(-amount, amount) * 10,000.
- Random distortion adds random * 4,000, clamped to 0..1,000,000.
- Random cutoff adds random * 1,700; resonance adds integer random/2.
- Two output meters, category artwork, active-key highlighting and randomized colors.

These are source facts, not claims that corresponding DSP is implemented.
Kontakt effect units are not automatically physical Hz/dB/bits. Exact effect
models, response curves and saved parameter state must be established separately.

## Source quirks to characterize

- Destroy initialization and its UI callback use opposite bypass conventions.
- Randomness 100 rerolls categories until one covers the note. Notes above 106
  cannot succeed. The native implementation must terminate safely.
- Random cutoff and resonance can go below their parameter ranges; determine
  Kontakt's effective clamp behavior rather than reproducing invalid DSP values.
- Some transition logic restores the chosen category only for randomness 80..99.
- Pitch randomization changes displayed pitch separately from the saved user value.

## Remaining milestones

1. Inspect binary zone/engine data and Kontakt UI; capture reproducible reference audio.
2. Background asset loading, missing-content recovery, all categories, exact zones,
   sample state restore and deterministic offline rendering readiness.
3. Translate and test KSP state transitions and per-note modulation separately from DSP.
4. Implement and compare distortion/lo-fi/filter/envelope behavior against Kontakt.
5. Recreate original functional UI and meters, then validate AU/VST3 in DAWs and
   standalone at multiple sample rates/buffer sizes, MIDI overlaps and state recall.
6. Only after functional parity, begin the requested UI redesign.

Build success is not host validation or sonic parity. Current builds are local
Debug arm64 artifacts, not signed/notarized release installers. JUCE licensing
and distribution packaging are not configured by this scaffold.

## Required platform matrix

| Platform | Formats | Validation status |
| --- | --- | --- |
| macOS Apple Silicon | AU, VST3, standalone | Debug builds and processor tests passed; DAW/visual/audio parity pending |
| Windows x64 | VST3, standalone EXE | VS 2022 Release preset and CI prepared; Windows execution pending |

Windows is part of the original port acceptance criteria, not a later optional
port. Both platforms need sample-path handling, state recall, native file dialogs,
MIDI/audio device checks and DAW smoke tests. Signing and installers are separate
release deliverables. Windows ARM64 and 32-bit builds are not configured.
