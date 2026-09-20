# SPAGlitch @VERSION@

Silverplatter Audio · @DATE@

An unhinged glitch instrument: nine banks of glitch material under a
Kontakt-modelled filter, a full effects chain you can reorder by dragging, and
an x-ray that fires when you play.

Read **QUICKSTART.md** to install and get your first sound. **EULA.txt** has the
licence terms.

## In this folder

| | |
|---|---|
| `macOS/` | Installer, signed and notarized by Apple. Universal (Apple silicon + Intel), macOS 13 or later. |
| `Windows/` | Installer for Windows 10 or 11, 64-bit. See the note inside. |
| `QUICKSTART.md` | Install, first sound, and what is worth trying. |
| `EULA.txt` | Licence terms. |

## Formats

Standalone, Audio Unit (AU) and VST3. macOS is universal arm64 + x86_64;
Windows is x64. There is no AAX build, so SPAGlitch does not run in Pro Tools.

## What it does

- **Nine sample banks**, 479 sounds, mapped from C3 upward.
- **A filter** with five modes — low pass, high pass, band pass, notch, peak —
  built from the measured response of the original instrument.
- **Eight effects**: distortion (soft/hard/fold/crush), chorus, delay, reverb,
  8-band parametric EQ, phaser/flanger, tremolo/vibrato and a limiter.
  **Drag the tabs to reorder the chain**; the order is part of the patch.
- **Forty factory presets**, and save as many of your own as you like.
- **Randomize**, with a wildness amount and per-group locks. It will not hand
  you a silent patch or a painfully loud one.
- **MIDI learn** on any knob, by right-clicking it.

## Good to know

- **Tempo sync needs a host.** In the standalone there is no transport, so
  synced delay and modulation rates run at a fixed 120 BPM.
- **The factory banks are the instrument.** SPAGlitch does not load your own
  samples.
- **Presets do not carry your MIDI CC bindings.** Those stay with the instance,
  deliberately — they describe your hardware, not a sound.

## Support

Please include your operating system, your DAW and its version, the format
(Standalone / AU / VST3), and what you were doing. For a sound problem the
preset name or a screenshot of the faceplate is usually enough to reproduce it,
since every control that shapes the sound is on it.
