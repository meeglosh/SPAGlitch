# SPAGlitch — Quick start

Silverplatter Audio · test build

## macOS

Open `macOS/SPAGlitch-<version>.pkg` and follow the installer. It is signed and
notarized by Apple, so it should open with a double-click — no right-click, no
Terminal. If macOS ever says the developer cannot be verified, the download was
damaged; fetch it again rather than working around the warning.

**Use Customize** in the installer to pick formats. Factory sounds are required
and install automatically.

| | installs to |
|---|---|
| Standalone | `/Applications/SPAGlitch.app` |
| Audio Unit | `/Library/Audio/Plug-Ins/Components` |
| VST3 | `/Library/Audio/Plug-Ins/VST3` |
| Factory sounds | `/Library/Application Support/Silverplatter Audio/SPAGlitch` |

Quit SPAGlitch and your DAW before installing. Logic caches its plugin scan, so
if it was open during the install, restart it.

## Windows

See `Windows/README-WINDOWS.txt` in this folder for what is included.

Windows builds are unsigned, so SmartScreen will show **"Windows protected your
PC"**. Click **More info → Run anyway**. This is expected and is about the
absence of a code-signing certificate, not about the file.

## First sound

1. Open the standalone, or load SPAGlitch on an instrument track.
2. Pick a **SAMPLE BANK** in the header — nine banks of glitch material.
3. Play. The on-screen keyboard responds to your computer keyboard as well as
   to MIDI; the instrument plays from C3 upward.
4. Hit **PRESETS** (top left) for forty factory patches, or roll the **dice** in
   the RANDOMIZE strip for a new one.

## Worth trying

- **Drag the FX tabs** (`03 / CHAIN`) to reorder the chain. Distortion into
  delay is a different instrument from delay into distortion, and the order is
  saved with the patch.
- **Every effect starts off.** Switch one on with the `ON` pill in its tab.
- **The EQ tab is interactive**: double-click empty space to add a band (near
  the left or right edge you get a cut instead of a bell), double-click a node
  to remove it, drag to move, **Cmd-drag or scroll for Q**, right-click a node
  for its type and slope.
- **Right-click any knob** to bind it to a hardware MIDI CC.
- **Lock a group** (SOUND / FILTER / FX) to hold that part of the patch while
  the dice re-rolls the rest. **WILD** sets how far a roll may stray.
- **Both drawers fold** — click the `03 / CHAIN` or `04 / KEYBOARD` bar anywhere.
- **Drag the bottom-right corner** to resize. It is a pure zoom, 50–150%.

## Things that are meant to be like that

- **The UI glitches while you play.** The text tearing is tied to the x-ray
  artwork and the notes driving it. It stops the moment the notes do.
- **DELAY, MOD and TREM/VIB sync to host tempo** by default. The standalone has
  no host, so they run at 120 BPM — switch `SYNC` off to hear free rates.
- **CUTOFF and RESONANCE do nothing while FILTER is off.** That gating is
  inherited from the original instrument.
- **OUTPUT sits before the FX chain**, so the limiter still catches the final
  signal.

## Reporting

Please include your OS version, DAW and version, the format (Standalone / AU /
VST3), and what you were doing. If it is a sound problem, the preset name or a
screenshot of the faceplate is enough to reproduce it — every control is on it.
