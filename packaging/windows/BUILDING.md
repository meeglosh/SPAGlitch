# Windows factory installer

Place the authorized original sample library in `local/factory-samples`, then
run `python packaging/validate_samples.py local/factory-samples` before compiling
SPAGlitch.iss with Inno Setup 6. Sample installation is mandatory for every
selected format and the instrument discovers the ProgramData location itself.

CI factory packaging is disabled unless the repository variable
SPAGLITCH_FACTORY_SAMPLES_ENABLED is true. When authorized and enabled, CI reads
factory-samples.zip from the unpublished spaglitch-factory-build-inputs draft
release. Do not publish that build-input release. Sample transfer requires user
authorization; currently no build-input upload has been approved or performed.

The Windows smoke test validates installed sample hashes, automatic playback in
all nine categories, stale project path recovery, app launch and uninstall.
