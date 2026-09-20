# SPAGlitch @VERSION@

Silverplatter Audio · confidential test build · @DATE@

An unhinged glitch instrument: nine banks of glitch material under a
Kontakt-modelled filter, a full effects chain you can reorder by dragging, and
an x-ray that fires when you play.

Please read **QUICKSTART.md** first, and **EULA.txt** before installing — this
build is confidential and must not be shared or shown publicly.

## In this folder

| | |
|---|---|
| `macOS/` | Signed and notarized installer. Universal (Apple silicon + Intel), macOS 13 or later. |
| `Windows/` | Windows x64 build. See the note inside — unsigned. |
| `QUICKSTART.md` | Install, first sound, and what is worth trying. |
| `EULA.txt` | Licence terms for this test build. |

## Formats

Standalone, Audio Unit (AU), VST3. macOS is universal arm64 + x86_64; Windows
is x64. There is no AAX build.

## What it does

- **Nine sample banks** of the original glitch library, 479 sounds, mapped from
  C3 up.
- **A filter** with five modes — low pass, high pass, band pass, notch, peak —
  taken from the measured response of the original instrument.
- **Eight effects**: distortion (soft/hard/fold/crush), chorus, delay, reverb,
  8-band parametric EQ, phaser/flanger, tremolo/vibrato, and a limiter.
  **Drag the tabs to reorder the chain**; the order is part of the patch.
- **Forty factory presets**, and you can save your own.
- **Randomize** with a wildness amount and per-group locks. It will not hand you
  a silent patch or a painfully loud one.
- **MIDI learn** on any knob, by right-clicking it.

## Known limitations in this build

- **Windows is unsigned.** SmartScreen will warn. See QUICKSTART.
- **No AAX**, so no Pro Tools.
- **Tempo sync needs a host.** In the standalone, synced delay and modulation
  rates run at a fixed 120 BPM.
- **No custom sample loading.** The nine factory banks are what there is.
- **Presets do not carry your MIDI CC bindings** — those stay with the
  instance, deliberately, since they describe your hardware rather than a sound.

## Feedback

Please include your OS, DAW and version, format, and what you were doing. Sound
problems are easiest to chase with the preset name or a screenshot of the
faceplate — every control that shapes the sound is visible on it.

Anything that crashes, hangs, or makes an unexpectedly loud noise is worth
reporting immediately, with the DAW's crash log if there is one.
