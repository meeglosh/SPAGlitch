# SPAGlitch

Native port of Silverplatter Audio's Kontakt Glitch Bundle.

**Functional development port; Kontakt sound/behavior parity is still in progress.**
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
If a synced Documents folder times out while reading build objects, use an
unsynced build directory, e.g. `cmake -S . -B /private/tmp/spaglitch-native-build`,
then build and test that directory.
Windows x64 is also a required target; Linux is not currently a release target.

## Playing the instrument

Open the standalone and select **Locate library**. Choose the original Glitch
Bundle folder, its Kontakt Files folder, or its 479-WAV sample folder. All nine
categories load in the background. Numbered samples map from MIDI note 12, as
verified against the original zone records. A progress/error message shows loading
state. **Audition WAV** is an optional single-sample mode rooted at MIDI 60.

Pitch, bits, crunch, filter, cutoff, resonance and randomness are functional.
At 100% randomness, each note can choose another eligible category. The current
DSP is an approximation of Kontakt's effects; consult the port status before
using this as a replacement in existing projects.

The library location and controls are saved in plugin state. Missing content can
be relocated with **Locate library**. Sample files remain external and excluded
from Git. For Dropbox content, prefer a local working copy:

```sh
python3 scripts/import_library.py '/path/to/Silverplatter Audio - Glitch Bundle Samples' local/library
```

The import verifies 479 expected names, copies with SHA-256 hashes, preserves the
source, and rejects conflicting destination content. It may wait for cloud files
to download. The `local/` directory is ignored by Git.

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

Verify the recovered MIDI mapping against the original instrument:

```sh
python3 scripts/verify_zone_map.py '/path/to/Silverplatter Audio - Glitch Bundle.nki'
```
