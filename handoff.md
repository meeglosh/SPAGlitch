# SPAGlitch handoff (2026-09-23)

Quick "start here" for the next session. SPAGlitch has no `CLAUDE.md`, so
unlike SPASynth's handoff this one is self-contained: what is here is what
there is, plus the per-area READMEs it points at.

## Where we are

- **2026-09-23: 1.0.1 is cut, signed, notarized, staged, tagged and merged.
  Nothing is waiting on the agent.** The release is the `v1.0.1` tag; the
  handoff commit sits on `main` just after it. All three `codex/` branches
  are merged and hold nothing. Four
  ctest suites pass. The build is in `dist/SPAGlitch-1.0.1/` with the docs
  beside it, and the tester note is committed at
  `docs/tester-note-1.0.1.txt`.

- **It has NOT been sent to Paul and Phil, and Mike has not installed it.**
  That is the next thing to happen, and it is his to do. The paste-ready
  changelog is the tester note above; it asks them specifically for an ear
  on the compressor's internal thresholds and for a verdict on whether
  calm mode gives enough feedback that a note landed.

- **Nobody has heard the compressor or seen calm mode in motion except
  Mike.** The agent verified both numerically and in still screenshots
  only. The compressor's fixed thresholds (-24 dB default per band), its
  per-band attack/release defaults and the internal +24 dB cap on upward
  gain are all judgement calls, not measurements. Same for whether the
  candle flicker and the screen tearing read strongly enough at playing
  speed. If any of that comes back as wrong, those are the numbers to
  move; the safety argument for calm mode does not depend on them (see
  Gotchas).

- **What 1.0.1 added over 1.0.0:** the `COMP` multiband compressor and
  calm mode. Both are described under "What the instrument is" below.

- **Two things were fixed in passing that were latent before:** a preset
  only stores the parameters that existed when it was saved, and loading
  one used to leave anything newer at the previous patch's value; and the
  CI artifact prune had never once run. Both in Gotchas.

## Open items

Mike's, not the agent's:

- **Send 1.0.1 to Paul and Phil.** Paste `docs/tester-note-1.0.1.txt`.
- **Install it himself** and confirm on a clean machine. Always hand him an
  **absolute** pkg path -- a relative one from the wrong directory failed
  silently on SPASynth's 1.0.14 and looked like it had worked.
- **Windows code signing.** Needs an Azure Artifact Signing account; only
  he can create it. See the section below -- it unblocks SPASynth and
  SPAStation too, so it is one job for three products.
- **A French translation of the EULA**, for the Bill 96 clause it already
  carries. Still outstanding.
- **Lawyer review of the EULA.** Mike has said one will not happen before
  release. Decision of record, not an oversight to re-raise.

Housekeeping, whenever:

- `dist/SPAGlitch-1.0.0/` is still on disk, about 600MB, superseded and
  never sent. Delete when he says so, not before.
- All three `codex/` branches are merged and can be deleted from the
  remote.

Nothing in the product is known-broken.

## What the instrument is

The Kontakt port plus the entire SPASynth FX chain, and the UI work that
had to happen to fit it:

- **FX chain** (`Source/fx/`), ported from SPASynth minus Convolve (no IR
  library here): distortion, chorus, delay, Dattorro plate reverb, 8-band
  parametric EQ, phaser/flanger, tremolo/vibrato, the multiband compressor
  and the limiter -- nine modules. Drag the tabs to reorder. Order is a
  packed uint64 atomic, 4 bits per module.
- **`COMP`, a three-band compressor** (`Source/fx/Multiband.h`,
  `MultibandEditor.*`), module id 8. Per band: threshold, ratio, up ratio,
  attack, release, makeup gain. UP RATIO compresses *upward* -- it lifts
  what sits below the threshold -- and 1:1 is off, which is the default, so
  switching the module on does something predictable. Two Linkwitz-Riley
  crossovers dragged on the tab's own graph, which also shows each band's
  live gain reduction. Only the selected band's six controls are shown;
  all eighteen knobs exist and are hidden, so no parameter attachment is
  ever torn down while the audio thread is reading it.
- **Calm mode** (`Source/CandleField.h`, `TechGlitch.h`, `VisualSettings.h`),
  for photosensitive epilepsy. Replaces the per-note image cut with a still
  scene whose 35 candles waver and whose six tech regions tear sideways by
  a few pixels, and zeroes the glitch energy so UI text stops tearing. Off
  by default, offered once on first launch, stored per machine.
- **Preset browser** (`Source/PresetBrowser.*`), opens to the left and
  **grows the window** rather than squeezing the instrument. 48 factory
  presets in `Assets/Presets/`, embedded via `juce_add_binary_data`: the
  original 40, plus 8 built around the compressor.
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
dist/SPAGlitch-1.0.1/                        <- current
  SPAGlitch-1.0.1-macOS.pkg      378MB  md5 83020f805e5e81e3bfb270f808e9152c
  SPAGlitch-1.0.1-Windows.exe    222MB  md5 88307bcb587726c6b5ecefe2d88a6426
                                 (CI run 35780830027, commit 6f7bdcf)
dist/SPAGlitch-1.0.0/                        <- superseded, never sent
```

**Mike sets the version number, not the agent.** This round was cut as
1.1.0 on the assumption that two features meant a minor bump, and had to
be rebuilt, re-notarized, re-staged and re-tagged as 1.0.1 when he said
so. The `v1.1.0` tag was deleted from the public repo. Ask, or use the
next patch number.

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

The full sequence is the next section. The two things that waste a build
if you get them wrong:

- **Build from `build-dist`, not `build-release`.** `build-dist` is
  `arm64;x86_64`; `build-release` is arm64-only and is a local dev dir.
  `validate_binaries.py` rejects an arm64-only bundle, but only after a
  full build has been spent on it.
- **`build-installer.sh` wants the three bundles flat in one directory**,
  not JUCE's `Standalone/`, `AU/`, `VST3/` layout. Handing it
  `build-dist/SPAGlitch_artefacts/Release` fails inside
  `validate_binaries.py` with `lipo: file not found`, which does not point
  at the real problem.

Signing and notary are already set up on this machine; full detail in
`packaging/macos/README.md`.

## How a release round goes, start to finish

Done twice now. In order, with the bits that are not obvious:

1. **Bump the version in TWO places**: `project(SPAGlitch VERSION ...)` in
   `CMakeLists.txt` and `#define AppVersion` in
   `packaging/windows/SPAGlitch.iss`. Inno Setup cannot read CMake.
   **The number is Mike's call** -- see Release artifacts.

2. **Push, to start Windows CI early.** It triggers on `main` and
   `codex/**`, runs about six minutes, and the Mac build can happen while
   it does. Put `[skip ci]` in the message of any commit that touches only
   docs, or it burns a build and 250MB for nothing.

3. **Build the Mac universal** from `build-dist`, never `build-release`
   (that one is arm64-only):
   `cmake --build build-dist --config Release --parallel 4`, about three
   minutes. Check `lipo -archs` gives `x86_64 arm64` and the app's
   `CFBundleShortVersionString` is the new version.

4. **Flatten the bundles before packaging.** `build-installer.sh` wants
   `SPAGlitch.app`, `SPAGlitch.component` and `SPAGlitch.vst3` side by side
   in one directory; JUCE emits them under `Standalone/`, `AU/` and
   `VST3/`. Passing the artefacts directory straight in fails in
   `validate_binaries.py` with a confusing `lipo: file not found`. Copy the
   three into a scratch directory first.

5. **Package, sign, notarize** (identities are in the login keychain,
   notary credentials in `~/.config/spaglitch/notary.env`):
   ```
   export SPAGLITCH_CODESIGN_IDENTITY="Developer ID Application: Kenzora Games (7K9WY5T49S)"
   export SPAGLITCH_INSTALLER_IDENTITY="Developer ID Installer: Kenzora Games (7K9WY5T49S)"
   bash packaging/macos/build-installer.sh <flat-bundle-dir> dist/SPAGlitch-<v>-macOS.pkg \
        "/Users/mikejerugim/Music/Silverplatter Audio/Glitch Bundle Samples"
   bash scripts/notarize.sh dist/SPAGlitch-<v>-macOS.pkg
   ```
   Notarization takes three to five minutes. The script staples and runs
   `spctl` itself.

6. **Verify it the way a customer receives it**, with the quarantine flag a
   download attaches:
   ```
   cp <pkg> /tmp/q.pkg && xattr -w com.apple.quarantine "0083;00000000;Safari;" /tmp/q.pkg
   spctl -a -vvv -t install /tmp/q.pkg     # expect source=Notarized Developer ID
   ```

7. **Fetch the Windows exe** once CI is green:
   `gh run download <run-id> --repo meeglosh/SPAGlitch --name SPAGlitch-Windows-x64-Installer --dir <dir>`

8. **Stage**: `scripts/stage-release.sh <version> <mac-pkg> <windows-exe>`,
   then delete the loose pkg left in `dist/`. Record both md5s here.

9. **Merge to `main`, tag `v<version>`, push both.** Before tagging, prove
   the binaries are the tag:
   `git diff --stat <build-commit>..HEAD -- Source/ CMakeLists.txt Assets/ packaging/`
   must be empty.

10. **Write the tester note** at `docs/tester-note-<version>.txt` and commit
    it. That file IS the paste-ready changelog; SPASynth does the same.
    House style is below. Mike has asked for these to be concise.

11. **Check CI artifact storage** is back to one set (see Sample hosting).

## Verification ritual for every change

1. `cmake --build build && ctest --test-dir build` — expect **4/4**
   (`processor`, `measured_velocity`, `measured_lofi`, `measured_filter`).
   Binary is `build/SPAGlitchTests`.
2. For UI changes, render and *look at* the screenshots:
   `SPAGlitchTests --layout-screenshots` and `--fx-screenshots`.
3. For audio-thread changes, `auval` the AU.

Tool modes on the test binary worth knowing: `--reverb-report`,
`--generate-factory-presets`, plus the reference-JSON modes ctest uses.

## Gotchas learned (every one of these cost real time)

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
  patch's value: every older preset would have inherited whatever the last
  patch left switched on. It now resets absent parameters to defaults, and
  an absent `fxOrder` to the default order.
- **A shipped preset is an artifact, not a derived file.** Adding an entry
  to the randomize table shifts every later draw in the stream, so re-rolling
  seed N no longer reproduces the preset seed N produced before.
  `--generate-factory-presets` therefore refuses to overwrite a file that
  already exists; delete one by hand to deliberately re-roll it.
- **`FXTab::displayWidthFraction` decides how many knobs fit per row.** At
  the limiter's 0.46 the grid gets seven columns, and a fifteen-control tab
  wrapped onto a third row that the FX band has no height for — the last
  knob was simply cut off. Screenshots caught it; the layout tests did not.
  **Check `--fx-screenshots` after adding controls to a tab**, and never
  estimate the column count from the faceplate width: the display takes a
  third of it.
- **The test suite must not read the machine's real visual settings.** Calm
  mode is stored in a `PropertiesFile`, so the editor would have rendered
  differently depending on Mike's own preference, and CI would have differed
  from local. `VisualSettings::useInMemoryStore()` is called at the top of
  the test main; anything else added to that file needs the same treatment.
- **Calm mode is a safety feature, so test the property, not the flag.** The
  test that matters is that rapid notes cannot make the candle field strobe:
  ten note-ons a second with a fast attack and a fast release would be a
  ten-per-second flash, which is squarely in the range that triggers
  seizures. The slow release is what prevents it, and there is a test
  asserting the field never falls back toward dark between rapid notes.
  There is also one that it is *perfectly* still at rest, and one that no
  two candles sit at the same brightness (a field pulsing in unison would
  be the large-area flash the mode exists to remove).
- **A rate-limit test that holds a note down tests nothing.** The tear
  pattern advances on note *onsets*, so feeding it a sounding note every
  frame produces no onsets at all and the bound passes without exercising
  the limiter. It alternates on/off now, and asserts both an upper bound
  (not a strobe) and a lower one (it still moves, so the upper bound means
  something).
- **Zeroing the zap energy for calm mode silently broke the SIGNAL ACTIVE
  readout**, which was driven from that same value and so would have read
  AT REST forever while playing. It has its own `signalEnergy` now. Worth
  grepping for other readers when a shared animation value gets forced to a
  constant.
- **The candle positions were found, not guessed.** A flame is a hot orange
  core sitting inside a pool of its own warm light, which separates it from
  the backlit mirror rim and the shelf LEDs in the same photograph; the
  survivors were then curated by eye against a marked-up render. Reflections
  were kept deliberately: a reflection flickers with its candle. If the
  scene image is ever replaced, that analysis has to be redone -- the table
  in `CandleField.h` is specific to this picture.
- **A parameter's own skew decides what a randomize window means.** UP RATIO
  spans 1:1 to 10:1 skewed so halfway is 2:1, which puts "off" across most
  of the bottom of the range. A window of 0..0.5 therefore produced 1:1 —
  upward compression disabled — in every preset of the first generated
  batch. Read the skew before picking the window, and check a generated
  batch rather than assuming.

## Sample hosting and GitHub storage

Windows CI pulls factory samples from the GitHub release
`spaglitch-factory-build-inputs`, gated by the repo variable
`SPAGLITCH_FACTORY_SAMPLES_ENABLED`. **Standing rule from Mike: only ever
one sample set uploaded at a time** — the free-tier quota got hit on
SPASynth this way. Retention is 5 days.

The workflow's prune step deletes superseded artifacts, but **only ones
created before `run_started_at`**, so concurrent runs cannot delete each
other's uploads (they did once; that was the bug it was written for).

**Two things to know about that step.** It had never once run before
2026-09-22 — PowerShell does not take `\"` as an escape, so the quotes
inside its `--jq` filter split the argument and `gh` failed with "accepts
1 arg(s), received 2" on every single run, silently, because
`continue-on-error` is set. A gigabyte had accumulated. It filters in
PowerShell now and prints how many it pruned, so a silent zero is visible.
And because it runs **before** its own upload, the previous run's set
survives one cycle: after a release, check and clear by hand.

```
gh api repos/meeglosh/SPAGlitch/actions/artifacts \
  -q '[.artifacts[] | select(.expired==false)] | "live: \(length), MB: \((map(.size_in_bytes)|add)/1048576|floor)"'
```
One set is about 251MB. GitHub's storage alerts are account-wide across
all of Mike's repos, not per-repo, so check every repo before blaming this
one.

## Branches

**`main` is the only branch to work from and holds everything.** All three
`codex/` branches are fully merged into it and hold nothing, so all three
can be deleted whenever:

- `codex/ott` — the 1.0.1 round (compressor + calm mode), merged at
  `1cd6925`. Named for the OTT clone it started as; the module it produced
  is not OTT any more.
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
individuals, **no em dashes**. Tester notes live at
`docs/tester-note-<version>.txt`, follow SPASynth's shape (what's new,
then "Worth trying specifically", then "Install notes"), and **Mike has
asked for them to be more concise than SPASynth's**. The shipped docs live in `packaging/docs/`
and are plain text in SPASynth's house style. EULA is **Quebec law**, with
a Consumer Protection Act carve-out and a Bill 96 bilingual clause. No
lawyer has reviewed it and Mike has said one will not before release; a
French translation is still outstanding.
