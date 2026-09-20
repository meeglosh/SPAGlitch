#!/bin/bash
set -euo pipefail

# Lays out dist/<version>/ for a tester build, so every release has the same
# shape and the docs travel with the binaries:
#
#   dist/<version>/
#     macOS/SPAGlitch-<version>.pkg        signed + notarized
#     Windows/...                          unsigned; see README-WINDOWS.txt
#     README.md                            version + date substituted
#     QUICKSTART.md
#     EULA.txt
#
# Usage: scripts/stage-release.sh <version> [mac-pkg] [windows-artifact-dir]
#
# Either payload may be omitted; the folder is created and the missing side is
# left with a placeholder naming what is still needed, so a half-built release
# says so out loud instead of looking finished.

version="${1:?Supply a version, e.g. 0.1.4}"
mac_pkg="${2:-}"
windows_dir="${3:-}"

repo=$(cd "$(dirname "$0")/.." && pwd)
docs="$repo/packaging/docs"
out="$repo/dist/$version"

mkdir -p "$out/macOS" "$out/Windows"

sed -e "s/@VERSION@/$version/g" -e "s/@DATE@/$(date '+%-d %B %Y')/g" \
    "$docs/README.md" > "$out/README.md"
cp "$docs/QUICKSTART.md" "$docs/EULA.txt" "$out/"

if [ -n "$mac_pkg" ]; then
    cp "$mac_pkg" "$out/macOS/SPAGlitch-$version.pkg"
    # State the Gatekeeper verdict here rather than trusting that signing ran:
    # an unnotarized pkg looks identical until a tester downloads it.
    if spctl -a -vv -t install "$out/macOS/SPAGlitch-$version.pkg" 2>&1 | grep -q 'source=Notarized Developer ID'; then
        echo "macOS: signed and notarized"
    else
        echo "macOS: WARNING - not notarized; testers will be blocked by Gatekeeper" >&2
    fi
else
    echo "Pending: the signed, notarized macOS installer has not been staged yet." \
        > "$out/macOS/PENDING.txt"
    echo "macOS: pending"
fi

if [ -n "$windows_dir" ]; then
    ditto "$windows_dir" "$out/Windows"
    cat > "$out/Windows/README-WINDOWS.txt" <<'TXT'
Windows x64 build.

This build is NOT code signed. SmartScreen will show "Windows protected your
PC" the first time you run it: click More info, then Run anyway. That warning
is about the absence of a signing certificate, not about the file itself.

If this folder contains a Setup .exe, run it and it installs the standalone,
the VST3 and the factory sounds for you.

If it contains loose binaries instead, there is no installer for this build:
  SPAGlitch.exe    the standalone; run it from wherever you put it
  SPAGlitch.vst3   copy the whole folder to C:\Program Files\Common Files\VST3
and the factory sounds are NOT included, so the instrument will report that
sounds are unavailable and will not make a noise. Ask for an installer build if
you need to hear it on Windows.
TXT
    echo "Windows: staged"
else
    echo "Pending: no Windows build has been staged yet. The installer is built by" \
         "the Windows CI workflow, and needs the factory samples enabled to include sounds." \
        > "$out/Windows/PENDING.txt"
    echo "Windows: pending"
fi

echo
echo "staged: $out"
find "$out" -maxdepth 2 -mindepth 1 | sed "s|$out|  .|" | sort
