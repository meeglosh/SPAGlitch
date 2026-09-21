# macOS installer

Run `bash packaging/macos/build-installer.sh <bundle-directory> <output.pkg> <sample-directory>`
on macOS. The source folder must contain `SPAGlitch.app`, `SPAGlitch.component`,
and `SPAGlitch.vst3` built for Apple Silicon and Intel.

Creates a selectable product installer for `/Applications` and the system AU/VST3
folders. All 479 hash-verified factory samples are installed under /Library/Application Support/Silverplatter Audio/SPAGlitch/Samples. No install scripts run.

The package and component versions are read from `project(SPAGlitch VERSION ...)`
in CMakeLists.txt, so they cannot drift from the bundles' own. Bumping the
version is that one line, plus `AppVersion` in packaging/windows/SPAGlitch.iss,
which Inno Setup cannot read from CMake.

## Signing and notarization

Signing is opt-in through the environment, so a local build needs no
certificates and a distribution build needs no separate script:

```
export SPAGLITCH_CODESIGN_IDENTITY="Developer ID Application: Kenzora Games (7K9WY5T49S)"
export SPAGLITCH_INSTALLER_IDENTITY="Developer ID Installer: Kenzora Games (7K9WY5T49S)"
bash packaging/macos/build-installer.sh <bundles> dist/SPAGlitch-1.0.0.pkg <samples>
bash scripts/notarize.sh dist/SPAGlitch-1.0.0.pkg
```

Unset, the bundles are ad-hoc signed -- enough to run locally on Apple silicon,
not enough to survive Gatekeeper once the file has been downloaded -- and the
pkg is unsigned.

Both identities live in the login keychain. Notarization credentials are read
from `~/.config/spaglitch/notary.env` (mode 600, never in the repo); see
`scripts/notarize.sh` for the fields and the keychain-profile fallback.

Verify a distribution build the way a customer receives it, with the quarantine
flag a download would attach:

```
xattr -w com.apple.quarantine "0083;00000000;Safari;" <copy of the pkg>
spctl -a -vvv -t install <copy of the pkg>      # expect: source=Notarized Developer ID
```

Signed bundles must report `flags=0x10000(runtime)` under `codesign -dv`: the
hardened runtime is a notarization requirement, and a bundle missing it is
rejected by the notary service rather than by anything local.

Validate with `pkgutil --expand-full` (using a new output directory), `codesign
--verify --deep --strict` on each expanded payload bundle, and `installer
-showChoicesXML -pkg <output.pkg> -target /`. The latter reads choices without
installing. Live installation is a separate manual test.

Mac distribution binaries must contain arm64 and x86_64 slices, each targeting
macOS 13.0. validate_binaries.py rejects packaging if those requirements differ.
Configure a fresh build directory with CMAKE_OSX_DEPLOYMENT_TARGET=13.0 and
CMAKE_OSX_ARCHITECTURES="arm64;x86_64". A modern SDK is allowed; the deployment
target controls the minimum OS, not the SDK version.
