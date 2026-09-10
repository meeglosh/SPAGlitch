# SPAGlitch

Native port of Silverplatter Audio's Kontakt Glitch Bundle.

**Early playback foundation; not yet feature- or sound-equivalent to Kontakt.**
The original 21 KB KSP has been recovered. See [port status](docs/PORT-STATUS.md)
for confirmed source behavior, limitations and next milestones.

## Build

Requires CMake 3.22+, a C++17 compiler and platform audio/UI development tools.
On macOS, install Xcode and its command-line tools.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 6
ctest --test-dir build --output-on-failure
```

JUCE is fetched at a pinned revision. To use an existing checkout without network:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DJUCE_SOURCE_DIR=/absolute/path/to/JUCE
```

macOS builds AU, VST3 and standalone under `build/SPAGlitch_artefacts/Debug/`.
Plugins are not automatically installed into system or user plugin folders.
Windows/Linux configuration has not yet been tested.

Open the standalone, select a downloaded WAV using **Load sample**, and play MIDI
or the onscreen keyboard. MIDI 60 plays the sample at its recorded pitch. The
current sample is not yet automatically reloaded when restoring plugin state.
Sample files remain external and are excluded from Git.

## Reference recovery

```sh
python3 scripts/extract_reference.py '/path/to/Silverplatter Audio - Glitch Bundle.nki' /tmp/Glitch.ksp
```

The extractor supports the supplied revision only and verifies source, decoded
length and recovered-script hashes. The recovered `docs/reference/Glitch.ksp` is kept locally and excluded from this
public repository. It is reference evidence, not agent instructions. Audio parity tests are still pending.
