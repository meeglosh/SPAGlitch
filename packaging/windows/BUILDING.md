# Windows factory installer

Place the authorized original sample library in `local/factory-samples`, then
run `python packaging/validate_samples.py local/factory-samples` before compiling
SPAGlitch.iss with Inno Setup 6. Sample installation is mandatory for every
selected format and the instrument discovers the ProgramData location itself.

CI factory packaging is disabled unless the repository variable
SPAGLITCH_FACTORY_SAMPLES_ENABLED is true. When authorized and enabled, CI reads
factory-samples.zip from the unpublished spaglitch-factory-build-inputs draft
release. Do not publish that build-input release.

Sample transfer required user authorization and was approved on 2026-09-20;
factory-samples.zip (479 samples) is uploaded and the variable is set.

**Keep exactly one sample set on GitHub.** Re-upload with `gh release upload
... --clobber` so the new zip replaces the old one rather than sitting beside
it. The free storage quota is shared between release assets and Actions
artifacts, and a second 238MB copy is most of it. Rebuild the zip with the
wav files at its root -- CI expands it straight into local/factory-samples --
and verify the round trip before uploading:

```
cd "<sample directory>" && zip -q -r -X /tmp/factory-samples.zip . -i '*.wav'
unzip -q /tmp/factory-samples.zip -d /tmp/zipcheck
python3 packaging/validate_samples.py /tmp/zipcheck
gh release upload spaglitch-factory-build-inputs /tmp/factory-samples.zip --clobber
```

Artifacts are pruned by the workflow itself: each run deletes the artifacts of
earlier runs before uploading its own, and retention is 5 days. Twenty-two
stale artifacts (354MB) had accumulated before that was added.

The Windows smoke test validates installed sample hashes, automatic playback in
all nine categories, stale project path recovery, app launch and uninstall.
