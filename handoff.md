# SPAGlitch handoff (2026-09-21)

Quick "start here" for the next session. SPAGlitch has no `CLAUDE.md`, so
unlike SPASynth's handoff this one is self-contained: what is here is what
there is, plus the per-area READMEs it points at.

## Where we are

- **2026-09-21 (later): OTT is built on `codex/ott`, not merged and not
  released.** A three-band upward/downward compressor added to the FX chain
  as module id 8, taking the chain to nine modules. Full per-band control
  set (15 knobs), its own bidirectional band meter, in the randomizer, and
  eight new factory presets built around it (48 total). All four ctest
  suites pass. Nothing is versioned or staged for it yet: `CMakeLists.txt`
  still says 1.0.0, and the version bump belongs to whenever a build is
  actually cut. Mike chose the name "OTT" knowingly after being told the
  three-letter name originates with Xfer Records; that is a decision of
  record, not an oversight to re-raise.

- **2026-09-21: v1.0.0 is tagged, merged to `main`, and staged. Nothing is
  waiting on the agent.** The whole FX round was built in one session on
  `codex/fx-chain`, merged to `main` (`7bf8cbd`), README de-dev-ified
  (`00fea37`), and tagged `v1.0.0` (annotated, pushed). `main` went from 2
  commits to 65. The macOS pkg is signed + notarized; the Windows exe is
  built by CI and is **not signed** (see "Windows signing" below, the one
  real open item).
- **The build in `dist/` is the tag.** `git diff` between the commits the
  installers were built from (`c9cb4cf` mac, `edb104a` win) and `v1.0.0`
  touches no `Source/` or `CMakeLists.txt`, only docs and release layout.
  So the tag describes those exact binaries.
- **Not yet sent to Paul and Phil.** A changelog for them was written in
  session (customer voice, the 1.0.0 feature list); it is not committed
  anywhere, so if they need it again, regenerate it from the commit log.
- **Nothing has been installed-and-confirmed by Mike on a clean machine
  since the final staging.** The pkg was verified with `spctl` and a
  quarantine flag, and an earlier install failure that round was the
  agent's fault, not the package's (see Gotchas).

## What 1.0.0 actually is

The Kontakt port plus the entire SPASynth FX chain, and the UI work that
had to happen to fit it:

- **FX chain** (`Source/fx/`), ported from SPASynth minus Convolve (no IR
  library here): distortion, chorus, delay, Dattorro plate reverb, 8-band
  parametric EQ, phaser/flanger, tremolo/vibrato, limiter. Drag the tabs to
  reorder. Order is a packed uint64 atomic, 4 bits per module.
- **Preset browser** (`Source/PresetBrowser.*`), opens to the left and
  **grows the window** rather than squeezing the instrument. 40 factory
  presets in `Assets/Presets/`, embedded via `juce_add_binary_data`.
- **Randomize all** with `sound / filter / fx` lock groups, chain-order
  randomization (limiter stays last), and the never-silent/never-blaring
  clamps. Dice icon button.
- **MIDI learn** on any knob by right-click.
- **Five filter types** (LP/HP/BP/Notch/Peak) off one TPT SVF, with an
  on/off toggle. Notch is the only mode that must not get the band
  injection; peak is notch *with* it.
- Resizable window (scale 0.5-1.5), collapsible FX and keyboard drawers,
  glitch-font treatment on every piece of UI text while the faceplate zaps.
- DESTROY knob removed (the FX chain covers it), but **Boom's destroy draw
  is still made and discarded** in `GlitchEngine` to keep the KSP random
  sequence aligned. Do not delete that draw.

## Release artifacts

```
dist/SPAGlitch-1.0.0/
  SPAGlitch-1.0.0-macOS.pkg      346MB  md5 fe41f5d9d473d489172bb81ac79f9238
  SPAGlitch-1.0.0-Windows.exe    207MB  md5 d983d50ee855ed510447bca81da04546
  EULA.txt  QUICKSTART.txt  README.txt
```

Folder shape matches SPASynth's (`SPASynth-Standard-1.0.22`) minus the
edition segment, because **there is one edition and no Pro is planned**
(Mike, 2026-09-20). Both installers sit at the folder root; no `macOS/` or
`Windows/` subfolders, deliberately, so the two products hand over
identically.

macOS pkg verifies as `source=Notarized Developer ID`, origin
`Developer ID Installer: Kenzora Games (7K9WY5T49S)` — same team as
SPASynth.

## Windows signing: the one open item

The Windows exe is unsigned, so SmartScreen warns on first download. This
blocks SPASynth and SPAStation too, so solving it once solves it three
times. Researched 2026-09-20:

- **EV certificates no longer bypass SmartScreen** (Microsoft's own docs);
  reputation is earned by download volume either way. So the expensive
  option buys nothing extra.
- **No downloadable `.pfx` from any CA since June 2023** (CA/Browser Forum
  hardware key mandate). USB tokens cannot work on a GitHub runner.
- **Recommendation: Azure Artifact Signing** (formerly Trusted Signing),
  Basic tier ~$9.99/mo. GA since Jan 2026, Canada is supported, and there
  is **no three-year-old-company rule** on that tier. Signs in CI, no
  hardware.
- Mike has to create the Azure account; the agent cannot. Once the account
  exists, wiring it into `.github/workflows/windows.yml` is a small change.

## How to rebuild after a code fix

macOS signing and notary are already set up on this machine. Full detail in
`packaging/macos/README.md`; the short version:

```
export SPAGLITCH_CODESIGN_IDENTITY="Developer ID Application: Kenzora Games (7K9WY5T49S)"
export SPAGLITCH_INSTALLER_IDENTITY="Developer ID Installer: Kenzora Games (7K9WY5T49S)"
cmake --build build-dist --config Release --parallel 4
bash packaging/macos/build-installer.sh \
     build-dist/SPAGlitch_artefacts/Release \
     dist/SPAGlitch-<v>-macOS.pkg \
     "/Users/mikejerugim/Music/Silverplatter Audio/Glitch Bundle Samples"
bash scripts/notarize.sh dist/SPAGlitch-<v>-macOS.pkg
```

**Build from `build-dist`, not `build-release`.** `build-dist` is
`arm64;x86_64`; `build-release` is arm64-only and is a local dev dir.
`validate_binaries.py` will reject an arm64-only bundle, but only after a
full build has been spent on it.

Windows: push `main` or a `codex/**` branch to trigger CI (the workflow is
branch-triggered, **not** tag-triggered), then pull the artifact down and
`scripts/stage-release.sh <version> <mac-pkg> <windows-exe-or-dir>`. Either
payload may be omitted; re-staging to add the second platform does not
undo the first, and a half-staged folder leaves a PENDING note saying what
is missing.

Bumping the version is two lines: `project(SPAGlitch VERSION ...)` in
`CMakeLists.txt`, and `AppVersion` in `packaging/windows/SPAGlitch.iss`
(Inno Setup cannot read CMake).

## Verification ritual for every change

1. `cmake --build build && ctest --test-dir build` — expect **4/4**
   (`processor`, `measured_velocity`, `measured_lofi`, `measured_filter`).
   Binary is `build/SPAGlitchTests`.
2. For UI changes, render and *look at* the screenshots:
   `SPAGlitchTests --layout-screenshots` and `--fx-screenshots`.
3. For audio-thread changes, `auval` the AU.

Tool modes on the test binary worth knowing: `--reverb-report`,
`--generate-factory-presets`, plus the reference-JSON modes ctest uses.

## Gotchas learned (this round, all of them cost real time)

- **`setMouseClickGrabsKeyboardFocus(false)`, not `setWantsKeyboardFocus`**,
  is what stops a component stealing QWERTY note entry. Same lesson SPASynth
  learned. `Source/KeyboardFocus.h` sweeps the whole tree; removing it
  showed **30** components stealing focus, including ones you would never
  guess (`SliderLabelComp`, `TabbedButtonBar::BehindFrontTabComp`,
  `ListBox::ListViewport`, `ScrollBar`, `ResizableCornerComponent`).
- **`setLatencySamples` must never be called from the audio thread** — it
  takes a listener lock. Publish to a `desiredLatency` atomic and apply it
  on the processor's timer. There is a test asserting it is *not* applied
  from the audio thread.
- **MIDI learn's `setValue()` does not reach the DSP.** SPASynth's comment
  claims the raw APVTS pointer updates; it does not, only
  `parameterValueChanged` refreshes it. The fix is an explicit
  `raw->store(param->convertFrom0to1(normValue))`. **SPASynth very likely
  still has this bug.**
- **`juce::Random` is an LCG: nearby seeds give nearly identical first
  draws.** 20 factory presets all landed on banks 5-7 because the seeds were
  `9000 + i*37`. `seededGenerator` now warms up 16 steps.
- **A LookAndFeel `createSliderTextBox` override never runs** — Slider builds
  its text box before the LnF is in the hierarchy. The first attempt
  produced a *byte-identical* render and was only caught by diffing
  screenshots. Do the work in `drawLabel` with a `dynamic_cast<juce::Slider*>
  (label.getParentComponent())`.
- **JUCE derives tab width from the bar depth, not the painted font**, so
  shrinking the font truncated "CHORUS". Needs a `getTabButtonBestWidth`
  override.
- **When a randomize sweep fails, suspect the fixture.** Seed 31 "failed"
  because the fixture was a 764 Hz sine and a high-pass at 1.17 kHz
  correctly removed it. The fixture became a broadband noise burst; the
  clamp was right all along.
- **Do not touch a pkg while Mike has Installer open on it.** The
  "Installer can't locate the data" screenshot was the agent deleting the
  file out from under it while re-staging. The package was fine.
- **CI-only failures are real failures.** A 260ms sleep passed on macOS and
  failed on the Windows runner; it is a poll loop now (up to 1.5s). And an
  `#ifndef AppVersion` nested inside `#ifndef BuildTag` was skipped entirely
  because CI passes `/DBuildTag`. Run the suite locally before pushing.
- **Installer HTML needs `<meta charset="utf-8">`** or you get `Â·`
  mojibake, which Mike will screenshot.
- **Appending an FX module breaks every saved chain order unless you
  migrate it.** The order packs 4 bits per module; a state written when
  there were eight packs eight nibbles and leaves the ninth at zero, which
  reads as a duplicate of module 0 and throws the user's whole chain away.
  `FXChain::unpackOrder` now falls back through shorter lengths and appends
  what is missing — **in front of a trailing limiter**, not behind it, or
  every factory preset silently stops ending in the limiter. There is a test
  for both halves of that.
- **A preset only stores the parameters that existed when it was saved.**
  `applyPreset` iterated the tree, so anything added since kept the previous
  patch's value: every pre-OTT preset would have inherited whatever OTT was
  left switched on. It now resets absent parameters to their defaults, and
  an absent `fxOrder` to the default order.
- **A shipped preset is an artifact, not a derived file.** Adding an entry
  to the randomize table shifts every later draw in the stream, so re-rolling
  seed N no longer reproduces the preset seed N produced before.
  `--generate-factory-presets` therefore refuses to overwrite a file that
  already exists; delete one by hand to deliberately re-roll it.
- **`FXTab::displayWidthFraction` decides how many knobs fit per row.** At
  the limiter's 0.46 the grid gets seven columns, and OTT's fifteen controls
  wrapped to a third row that the FX band has no height for — the fifteenth
  knob was simply cut off. Screenshots caught it; the layout tests did not.
  Check `--fx-screenshots` after adding controls to a tab.

## Sample hosting and GitHub storage

Windows CI pulls factory samples from the GitHub release
`spaglitch-factory-build-inputs`, gated by the repo variable
`SPAGLITCH_FACTORY_SAMPLES_ENABLED`. **Standing rule from Mike: only ever
one sample set uploaded at a time** — the free-tier quota got hit on
SPASynth this way. The workflow's prune step deletes superseded artifacts,
but **only ones created before `run_started_at`**, so concurrent runs no
longer delete each other's (they did; that was a bug). Retention is 5 days.

Storage went 967MB -> 241MB once the concurrent-run duplicates were
cleared. GitHub's storage alerts are account-wide across all of Mike's
repos, not per-repo, so check every repo before blaming this one.

## Branches

`codex/ott` is the live one (see the top of "Where we are"); it is ahead of
`main` and not merged.

`main` holds everything through 1.0.0. Two older `codex/` branches remain on
the remote and both are fully merged into it, so neither holds anything and
both can be deleted whenever:

- `codex/fx-chain` — the whole 1.0.0 round, merged at `7bf8cbd`.
- `codex/kontakt-port` — the original port, older still.

There are no abandoned branches here, unlike SPASynth, which has two that
must never be merged.

## Versioning rule (Mike's call, carried over from SPASynth)

Bump the version the moment a build has been **sent** to anyone, testers
included. If a build never left this machine, overwrite it in place at the
same version number.

## Load-bearing invariants (do not break)

- `CMAKE_OSX_DEPLOYMENT_TARGET=13.0`, `arm64;x86_64` for distribution.
- **Append-only choice orders**: FX module ids, filter modes, EQ band types,
  reverb/limiter character modes. They are stored in presets as indices. A
  new module goes on the end of `FXChain::Module` and nowhere else, and
  `unpackOrder` has to learn to migrate the shorter orders (see Gotchas).
- **RT-safety on the audio thread**: no allocation, no locks, no host
  notifications.
- The per-preset packed `fxOrder` atomic, and its permutation validation in
  `unpackOrder` — an invalid value must fall back to natural order, not be
  trusted. (`setFxOrder` used to accept duplicate permutations.)
- Boom's discarded destroy draw in `GlitchEngine`, as above.

## House style (customer-facing copy)

First-person company voice ("we"/"our"/Silverplatter Audio), never name
individuals, **no em dashes**. The shipped docs live in `packaging/docs/`
and are plain text in SPASynth's house style. EULA is **Quebec law**, with
a Consumer Protection Act carve-out and a Bill 96 bilingual clause. No
lawyer has reviewed it and Mike has said one will not before release; a
French translation is still outstanding.
