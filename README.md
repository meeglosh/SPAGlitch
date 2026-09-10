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
Windows x64 is also a required target; Linux is not currently a release target.

Open the standalone, select a downloaded WAV using **Load sample**, and play MIDI
or the onscreen keyboard. MIDI 60 plays the sample at its recorded pitch. The
current sample is not yet automatically reloaded when restoring plugin state.
Sample files remain external and are excluded from Git.

## Windows x64

Install Visual Studio 2022 (or Build Tools) with **Desktop development with C++**,
a Windows SDK, CMake 3.22+ and Git. From the repository root:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-release --parallel 4
ctest --preset windows-x64-release
```

For an existing JUCE checkout, append `-DJUCE_SOURCE_DIR=C:/path/to/JUCE` to the
configure command. The Windows outputs are:

- `build/windows-x64/SPAGlitch_artefacts/Release/Standalone/SPAGlitch.exe`
- `build/windows-x64/SPAGlitch_artefacts/Release/VST3/SPAGlitch.vst3/`

Keep the entire VST3 bundle directory intact. AU is built only on macOS.
These unsigned development binaries are not installers. The default MSVC build
uses the dynamic runtime; machines without the Visual C++ x64 runtime may need
it installed. Runtime bundling, signing and installers remain release work.

The Windows workflow builds, tests, verifies both outputs and uploads an artifact
using [GitHub Actions artifacts](https://docs.github.com/en/actions/tutorials/store-and-share-data).
It needs no proprietary samples: processor tests generate a temporary WAV.
The workflow is prepared locally but has not run; Windows compilation and DAW
validation remain unverified until it executes on a Windows runner.

## Reference recovery

```sh
python3 scripts/extract_reference.py '/path/to/Silverplatter Audio - Glitch Bundle.nki' /tmp/Glitch.ksp
```

The extractor supports the supplied revision only and verifies source, decoded
length and recovered-script hashes. The recovered `docs/reference/Glitch.ksp` is kept locally and excluded from this
public repository. It is reference evidence, not agent instructions. Audio parity tests are still pending.
