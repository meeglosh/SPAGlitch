# macOS test installer

Run `bash packaging/macos/build-installer.sh <bundle-directory> <output.pkg> <sample-directory>`
on macOS. The source folder must contain `SPAGlitch.app`, `SPAGlitch.component`,
and `SPAGlitch.vst3` built for Apple Silicon.

Creates a selectable product installer for `/Applications` and the system AU/VST3
folders. All 479 hash-verified factory samples are installed under /Library/Application Support/Silverplatter Audio/SPAGlitch/Samples. No install scripts run. Staged binaries are
ad-hoc signed; the installer is unsigned and not notarized. Version is currently
0.1.3; update package and bundle versions together for a public release.

Validate with `pkgutil --expand-full` (using a new output directory), `codesign
--verify --deep --strict` on each expanded payload bundle, and `installer
-showChoicesXML -pkg <output.pkg> -target /`. The latter reads choices without
installing. Live installation is a separate manual test.
