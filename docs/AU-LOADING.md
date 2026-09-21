# AU loading regression, 2026-09-12

The installed AU passed auval but was silent with the cloud-backed local/library
folder: 20 sample files were marked SF_DATALESS. ContentLoader publishes a bank
only after all 479 samples decode, so a blocked cloud read leaves the bank silent.
The original source folder also had 106 cloud-only files. Combining already-local
files from those two locations recovered all 479 without synthesizing/replacing
any sample content. The recovered copy is private, outside this repository.

The exact installed AU produced peak 0.31811908 with that fully local library
through AudioUnitRender and MIDI note 36, compared with zero for the incomplete
cloud copy. A generated local WAV and complete fixture bank also rendered audio.
Logic's current instance still needs its library location selected/confirmed by
the user; these checks do not claim a completed live Logic test.

Version 0.1.1 detects macOS SF_DATALESS before opening a sample and reports the
filename with instructions to keep the entire folder offline and reselect it.
The installed app already contained the correct AppIcon.icns (matching the build)
and CFBundleIconFile. Launch Services registration was refreshed. Visual Dock
cache behavior remains a user verification step.

Native AU smoke test (macOS, registered component required):

```
clang++ -std=c++17 -fobjc-arc tools/AUAudioSmoke.mm -framework Cocoa -framework AudioToolbox -o /tmp/SPAGlitchAUAudioSmoke
/tmp/SPAGlitchAUAudioSmoke /absolute/path/to/local/samples
auval -v aumu Spgl Spau
```

The test changes state only in its own new AU instance. It does not touch Logic.
