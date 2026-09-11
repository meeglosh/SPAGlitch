# Port status

## Current native instrument

The AU, VST3 and standalone now share a 32-voice sample engine with all nine
categories, mapped sample playback, per-note pitch, Lo-Fi, Tube-style distortion,
four-pole low/high-pass filters, the recovered randomness arithmetic, output
control, meters, MIDI sustain/pitch bend, and an all-notes-off control.

A background worker loads the full library, reports progress/errors and restores
content from plugin state. The real-time callback does not read files or destroy
sample banks: pointer mailboxes hand new banks to audio and old banks back to
the worker. During a content change or failed restore, stale audio is silenced.
Offline rendering waits up to 60 seconds for content before rendering a block;
preload local content for dependable offline jobs. Cloud reads can exceed this
window and cannot be forcibly interrupted safely while an OS read is pending.

This is a functional development port, **not a verified sonic replica**.
The UI is a temporary functional control panel. Original artwork/filmstrip
recreation and the subsequent requested redesign are separate remaining work.

## Confirmed source evidence

The supplied NKI contains 26,025 bytes of FastLZ level-2 data at byte 1154, which
expand to the recorded 223,746 bytes. The 21,041-byte KSP is preserved locally in
`reference/Glitch.ksp` and excluded from the public repository. The extractor
verifies source and script hashes and rejects other NKI revisions.

`verify_zone_map.py` verifies all 479 zone records against their sample references:
velocity 1..127, one key per sample, root key equal to mapped key, and filename
number + 11 equals MIDI note. All nine category counts match the recovered KSP.
The embedded sample table orders Percussive before Rapid Modulation/Squelchy;
the engine deliberately indexes by category name, not by that table order.

Saved KSP values: category 0, pitch 0, Lo-Fi 4, distortion 488095, cutoff 476191,
resonance 49, randomness 0, destroy 1, filter 1.

Kontakt 8 Player loaded a temporary reference copy in demo mode. Its visible
editor confirmed:

- Source: Sampler, key tracking enabled, HQI Standard; 32-voice limit.
- Selected Squelchy zone: one key, root matches key, volume 0 dB, pan 0, tune 0.
- Selected group amplifier: -6 dB, centre pan; no group insert effects visible.
- Instrument inserts: Lo-Fi -> Distort -> AR L24 -> AR H24, all initially bypassed.
- Lo-Fi: 8.0 bits, sample rate displayed as 4.4 kHz, noise -inf, noise colour 20%, output 0 dB.
- Distortion: Tube mode, drive 48.8%, damping 0%, output -4.1 dB.
- LP: AR LP2/4, 441.2 Hz, resonance 61%; HP: AR HP2/4, 7.4 kHz, resonance 89%.
- No send/main effects visible. Exact group envelope/release behavior is not yet established.

The latent saved filter values differ from the saved UI values. The source script
does not synchronize all engine parameters on initialization, so identical knob
positions alone do not prove identical sound.

## Implemented KSP arithmetic

Randomness above zero perturbs pitch, bits, drive, cutoff and resonance on each
note. At 100 it also selects an eligible category and randomizes destroy/filter.
Integer arithmetic retains the source formulas:

- pitch = user pitch * 80000 + random(-amount, amount) * 10000
- drive = clamp(user drive + random * 4000, 0, 1000000)
- bits = max(250000, user Lo-Fi * 62500 + 250000 + random * 2300)
- cutoff = user cutoff + random * 1700
- resonance = user resonance + integer random/2

For safety, native cutoff/resonance are clamped at both ends. Above the highest
mapped note, Boom mode terminates with silence instead of the original endless
reroll. A seeded PRNG supports repeatable native sequences; it is not Kontakt's
PRNG, and purely visual random colour draws do not alter audio randomness.

User target controls remain distinct from effective randomized values. Native
callbacks now retain the last effective effects when randomness is turned off,
retain Boom's last category on a direct transition to 0..79, and restore the user
category on transition to 80..99. After randomness is disabled, pitch follows the
source's last random offset until a new pitch gesture. These are script-derived
behaviors, covered by regression tests; Kontakt audio comparison is pending.
Destroy follows its UI callback (0 on, 1 off), rather than the conflicting
initialization branch. Exact initialization of all persistent script variables,
visual RNG draws, and same-value UI gestures still require reference validation.

Low-pass and high-pass now retain separate cutoff/resonance settings and DSP
histories. Moving cutoff/resonance while bypassed changes the user target but
leaves both inserts unchanged. Ordinary notes no longer overwrite these latent
settings. Version 3 native state saves filter memories, effective values and
script transition state through atomic mailboxes; audio never waits for a restore
writer. Versions 1/2 remain readable.

## Sound and compatibility gaps

- The physical distortion curve, output compensation, and AR resonance response
  are provisional DSP. The native four-pole topology and fixed 4410 Hz resampling
  reflect inspected settings, but exact Lo-Fi clock/rate and response still need
  measurement. Fractional bit reduction and Tube-style asymmetry are implemented.
- The first dry reference calibrated full-velocity level and the 48 kHz release
  (479 linear fade intervals). New instances default to Output 0 dB, with a small
  fixed output trim matching the captured dry level. Existing states retain their
  saved Output setting. Internal effect gain staging, other sample rates, pitch
  interpolation and pitch-bend range still require controlled comparisons.
- Velocity now uses a cubic curve verified against all 127 clean 48 kHz
  offline reference takes. The native 32/64/127 velocity
  captures, at all three note gates, match within two 24-bit PCM steps.
  Other groups/rates remain unverified. Details and hashes: `velocity-reference-results.json`.
- Separate LP/HP memories are implemented, but their initial physical values
  remain provisional until the original parameter mapping is measured.
- Original graphics, parameter gestures, keyboard colours and exact animation
  behavior still need parity work. Current keyboard marks mapped keys and colours
  Boom mode; the user category remains stable while a label shows the playing group.
- File paths are stored per instance. Missing/moved libraries require Locate
  library; portable content identifiers and distribution installation remain work.
- Windows build and tests passed for commit dd1d132 (run 34599524871), including
  the corrected library-picker capture. Later changes require their own CI run.

## Validation completed

- macOS Debug AU, VST3 and standalone builds.
- Tests: sample-accurate MIDI onset, WAV decoding, parameter/sample restore,
  missing-content recovery, newest request wins, all 479 mapping boundaries,
  nine categories, Boom out-of-range termination, offline first-block restore,
  sustain/release, bounded voice stealing, all-sound-off, seeded random bounds,
  and finite effect output at 44.1/48/96 kHz.
- Original 479 WAVs copied and SHA-256 hashed into ignored `local/library`.
- Original library loaded in the native engine; first note of all nine categories
  renders nonzero finite audio.
- Native editor snapshot inspected; standalone launched and controls exposed.
- A later Documents build hit an object-file filesystem timeout. Rebuilding in
  `/private/tmp/spaglitch-native-build` avoided the synced build directory; all
  three formats, processor tests and original-library rendering passed there.

## Required platform matrix

| Platform | Formats | Status |
| --- | --- | --- |
| macOS Apple Silicon | AU, VST3, standalone | Builds and native engine tests passed; DAW/audio-parity validation pending |
| Windows x64 | VST3, standalone EXE | Build/tests passed on dd1d132; DAW validation pending |

Do not call the port complete until Kontakt sound/behavior comparisons and host
validation pass on both required platforms. Current artifacts are unsigned Debug
builds, not signed/notarized installers. JUCE release licensing and packaging
remain separate release work.

## Current parity measurement work

An optional `GlitchReferenceHost` loads the installed Kontakt VST3. The first
live capture (2026-09-11) measured note 12 in Glitch Digital at velocities
127/64/32, gates 50 ms/500 ms/3 s, 48 kHz, with the original saved dry settings.
Reference audio is safely stored in ignored `local/parity/dry`.

After dry gain/release calibration, the native full-velocity captures have zero
sample offset and maximum absolute error 1.1920928955078125e-7 (one 24-bit PCM
step). These results cover only that sample, rate and velocity. Detailed metrics
and WAV hashes are in `dry-reference-results.json`. No effect match is claimed.

The expanded live capture obtained all 127 velocity takes at 48 kHz before
attempting rate changes. The repeated full-velocity take was bit-identical to
the first reference. Three takes had large shape residuals and were excluded
from fitting; clean gains fit a normalized cubic with maximum gain error below
3.4e-7. Fit: cube-root gains by least squares against MIDI velocity, then normalize
the fitted line to unity at velocity 127.

Kontakt requested a restart after the sample-rate change; a subsequent capture
was silent. ALL 44.1/96 kHz data from that session are rejected. The host now locks
the selected rate before loading Kontakt, uses VST3 offline rendering, and never
changes rates during capture. Each new rate requires a fresh host/NKI load. A fresh
offline capture validated all 127 velocities with no shape-corrupted takes. Its
full-velocity waveform is bit-identical to the initial reference.
Restoring the captured Kontakt state in a separate process also returned silence;
that path was not used as evidence and the experimental batch host was removed.

The companion `SPAGlitchRender` renders the initial nine-file matrix, and
`scripts/compare_audio.py` reports delay, raw/aligned error, gain mismatch and
gain-corrected residual separately. Silence is an error, not a passing match.
All captures and proprietary plugin states stay in ignored `local/parity`.
See `PARITY-CAPTURE.md` for the repeatable procedure.

## Effect measurement blocker

The first Destroy-on capture at 48 kHz produced audio in 47 of 133 takes, then
became silent starting at velocity 82. Kontakt displayed **Kontakt full version
required** for the non-Player Glitch instrument. No restart or licensing bypass
was attempted after identifying this condition. The initial full-velocity files
are retained for exploratory analysis; the rest of this incomplete sweep is not
accepted as calibration evidence. See `destroy-reference-status.json`.

Further isolated Lo-Fi, distortion and filter measurements require a working
full-Kontakt instance here or reference renders from a licensed machine. No
physical effect DSP was changed based on the interrupted capture. The completed
dry/velocity evidence and native calibration remain valid. The capture host now
reports silent-take counts even when the first full-velocity take was nonzero.
