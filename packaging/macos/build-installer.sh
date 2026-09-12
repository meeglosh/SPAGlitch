#!/bin/bash
set -euo pipefail

# Usage: build-installer.sh <folder containing the three bundles> <output.pkg>
source_dir=$(cd "${1:?Supply the bundle directory}" && pwd)
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
    codesign --force --deep --sign - "$root$destination/$bundle"
    codesign --verify --deep --strict "$root$destination/$bundle"
    pkgbuild --analyze --root "$root" "$package_work/$name.plist"
    if [ "$name" = standalone ]; then
        /usr/libexec/PlistBuddy -c 'Set :0:BundleIsRelocatable false' "$package_work/$name.plist"
    fi
    pkgbuild --root "$root" --component-plist "$package_work/$name.plist" \
        --identifier "com.silverplatteraudio.spaglitch.$name" --version 0.1.1 \
        --install-location / --ownership recommended "$package_work/packages/$name.pkg"
}
make_component standalone SPAGlitch.app /Applications
make_component au SPAGlitch.component /Library/Audio/Plug-Ins/Components
make_component vst3 SPAGlitch.vst3 /Library/Audio/Plug-Ins/VST3

cat > "$package_work/resources/welcome.html" <<'HTML'
<html><body style="font-family: -apple-system; color: #203a30">
<h1>SPA / GLITCH</h1><p>Silverplatter Audio · Development test build</p>
<p>Install the standalone instrument, Audio Unit, and VST3 plugin.</p>
<p>Quit SPAGlitch and your DAW before continuing. Use Customize to choose formats.</p>
<p>The original Glitch Bundle samples are not included. After installation,
open SPAGlitch and use <b>Locate library</b> to select your existing sample folder.</p>
<p>This development installer is not Developer ID signed or notarized.</p>
</body></html>
HTML
cat > "$package_work/distribution.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>SPAGlitch</title>
  <welcome file="welcome.html"/>
  <options customize="always" require-scripts="false" hostArchitectures="arm64"/>
  <domains enable_localSystem="true" enable_currentUserHome="false" enable_anywhere="false"/>
  <choices-outline>
    <line choice="standalone"/><line choice="au"/><line choice="vst3"/>
  </choices-outline>
  <choice id="standalone" title="Standalone instrument" description="Installs SPAGlitch.app in Applications." start_selected="true"><pkg-ref id="com.silverplatteraudio.spaglitch.standalone"/></choice>
  <choice id="au" title="Audio Unit (AU)" description="Installs in /Library/Audio/Plug-Ins/Components." start_selected="true"><pkg-ref id="com.silverplatteraudio.spaglitch.au"/></choice>
  <choice id="vst3" title="VST3" description="Installs in /Library/Audio/Plug-Ins/VST3." start_selected="true"><pkg-ref id="com.silverplatteraudio.spaglitch.vst3"/></choice>
  <pkg-ref id="com.silverplatteraudio.spaglitch.standalone" version="0.1.1" onConclusion="none">standalone.pkg</pkg-ref>
  <pkg-ref id="com.silverplatteraudio.spaglitch.au" version="0.1.1" onConclusion="none">au.pkg</pkg-ref>
  <pkg-ref id="com.silverplatteraudio.spaglitch.vst3" version="0.1.1" onConclusion="none">vst3.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution "$package_work/distribution.xml" --resources "$package_work/resources" \
    --package-path "$package_work/packages" "$output_path"
