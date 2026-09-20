#!/bin/bash
set -euo pipefail

# Usage: build-installer.sh <folder containing the three bundles> <output.pkg> <sample-directory>
#
# Signing is opt-in through the environment, so a local test build needs no
# certificates and a distribution build needs no separate script:
#
#   SPAGLITCH_CODESIGN_IDENTITY   "Developer ID Application: ..." — signs each
#                                 bundle with the hardened runtime and a secure
#                                 timestamp, both of which notarization requires.
#   SPAGLITCH_INSTALLER_IDENTITY  "Developer ID Installer: ..." — signs the pkg.
#
# Unset, the bundles are ad-hoc signed (enough to run locally on Apple silicon,
# not enough to survive Gatekeeper after a download) and the pkg is unsigned.
# Notarization is a separate step: scripts/notarize.sh <pkg>.
script_dir=$(cd "$(dirname "$0")" && pwd)
sample_dir=$(cd "${3:?Supply the complete sample directory}" && pwd)
python3 "$script_dir/../validate_samples.py" "$sample_dir"
source_dir=$(cd "${1:?Supply the bundle directory}" && pwd)
python3 "$script_dir/validate_binaries.py" "$source_dir"
output_dir=$(dirname "${2:?Supply the output .pkg path}")
mkdir -p "$output_dir"
output_path="$(cd "$output_dir" && pwd)/$(basename "$2")"
package_work=$(mktemp -d /private/tmp/spaglitch-installer.XXXXXX)
trap 'rm -rf "$package_work"' EXIT
mkdir -p "$package_work/packages" "$package_work/resources"

make_component() {
    local name="$1" bundle="$2" destination="$3"
    local root="$package_work/$name"
    mkdir -p "$root$destination"
    # Package clean copies; do not carry cloud-provider/Finder metadata.
    ditto --norsrc --noextattr "$source_dir/$bundle" "$root$destination/$bundle"
    if [ -n "${SPAGLITCH_CODESIGN_IDENTITY:-}" ]; then
        codesign --force --deep --options runtime --timestamp \
            --sign "$SPAGLITCH_CODESIGN_IDENTITY" "$root$destination/$bundle"
    else
        codesign --force --deep --sign - "$root$destination/$bundle"
    fi
    codesign --verify --deep --strict "$root$destination/$bundle"
    pkgbuild --analyze --root "$root" "$package_work/$name.plist"
    if [ "$name" = standalone ]; then
        /usr/libexec/PlistBuddy -c 'Set :0:BundleIsRelocatable false' "$package_work/$name.plist"
    fi
    pkgbuild --root "$root" --component-plist "$package_work/$name.plist" \
        --identifier "com.silverplatteraudio.spaglitch.$name" --version 0.1.4 \
        --install-location / --ownership recommended "$package_work/packages/$name.pkg"
}
make_component standalone SPAGlitch.app /Applications
make_component au SPAGlitch.component /Library/Audio/Plug-Ins/Components
make_component vst3 SPAGlitch.vst3 /Library/Audio/Plug-Ins/VST3

mkdir -p "$package_work/samples/Library/Application Support/Silverplatter Audio/SPAGlitch/Samples"
for sample in "$sample_dir"/*.wav; do
    ditto --norsrc --noextattr "$sample" "$package_work/samples/Library/Application Support/Silverplatter Audio/SPAGlitch/Samples/$(basename "$sample")"
done
pkgbuild --root "$package_work/samples" --identifier com.silverplatteraudio.spaglitch.samples --version 0.1.4 --install-location / --ownership recommended "$package_work/packages/samples.pkg"

if [ -n "${SPAGLITCH_INSTALLER_IDENTITY:-}" ]; then
    signing_note="Signed by Kenzora Games Inc. and notarized by Apple."
else
    signing_note="This development installer is not Developer ID signed or notarized."
fi
cat > "$package_work/resources/welcome.html" <<HTML
<html><body style="font-family: -apple-system; color: #203a30">
<h1>SPA / GLITCH</h1><p>Silverplatter Audio · Development test build</p>
<p>Install the standalone instrument, Audio Unit, and VST3 plugin.</p>
<p>Quit SPAGlitch and your DAW before continuing. Use Customize to choose formats.</p>
<p>All 479 factory sounds are included and load automatically in every format.</p>
<p>$signing_note</p>
</body></html>
HTML
cat > "$package_work/distribution.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>SPAGlitch</title>
  <welcome file="welcome.html"/>
  <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <volume-check><allowed-os-versions><os-version min="13.0"/></allowed-os-versions></volume-check>
  <domains enable_localSystem="true" enable_currentUserHome="false" enable_anywhere="false"/>
  <choices-outline>
    <line choice="samples"/><line choice="standalone"/><line choice="au"/><line choice="vst3"/>
  </choices-outline>
  <choice id="samples" title="Factory sounds (required)" start_selected="true" start_enabled="false"><pkg-ref id="com.silverplatteraudio.spaglitch.samples"/></choice>
  <choice id="standalone" title="Standalone instrument" description="Installs SPAGlitch.app in Applications." start_selected="true"><pkg-ref id="com.silverplatteraudio.spaglitch.standalone"/></choice>
  <choice id="au" title="Audio Unit (AU)" description="Installs in /Library/Audio/Plug-Ins/Components." start_selected="true"><pkg-ref id="com.silverplatteraudio.spaglitch.au"/></choice>
  <choice id="vst3" title="VST3" description="Installs in /Library/Audio/Plug-Ins/VST3." start_selected="true"><pkg-ref id="com.silverplatteraudio.spaglitch.vst3"/></choice>
  <pkg-ref id="com.silverplatteraudio.spaglitch.samples" version="0.1.4" onConclusion="none">samples.pkg</pkg-ref>
  <pkg-ref id="com.silverplatteraudio.spaglitch.standalone" version="0.1.4" onConclusion="none">standalone.pkg</pkg-ref>
  <pkg-ref id="com.silverplatteraudio.spaglitch.au" version="0.1.4" onConclusion="none">au.pkg</pkg-ref>
  <pkg-ref id="com.silverplatteraudio.spaglitch.vst3" version="0.1.4" onConclusion="none">vst3.pkg</pkg-ref>
</installer-gui-script>
XML
if [ -n "${SPAGLITCH_INSTALLER_IDENTITY:-}" ]; then
    productbuild --distribution "$package_work/distribution.xml" --resources "$package_work/resources" \
        --package-path "$package_work/packages" "$package_work/unsigned.pkg"
    productsign --sign "$SPAGLITCH_INSTALLER_IDENTITY" "$package_work/unsigned.pkg" "$output_path"
    pkgutil --check-signature "$output_path" | sed -n '1,3p'
else
    productbuild --distribution "$package_work/distribution.xml" --resources "$package_work/resources" \
        --package-path "$package_work/packages" "$output_path"
    echo "note: unsigned pkg (set SPAGLITCH_INSTALLER_IDENTITY to sign)"
fi
