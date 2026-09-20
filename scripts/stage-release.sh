#!/bin/bash
set -euo pipefail

# Lays out dist/<version>/ for a release, matching SPASynth's shape so the two
# products hand over identically:
#
#   dist/<version>/
#     SPAGlitch-<version>-macOS.pkg      signed + notarized
#     SPAGlitch-<version>-Windows.exe    see README.txt
#     README.txt                         version + date substituted
#     QUICKSTART.txt
#     EULA.txt
#
# Usage: scripts/stage-release.sh <version> [mac-pkg] [windows-installer-or-dir]
#
# Either payload may be omitted. Each side counts as staged when its file is
# present, whether this run supplied it or an earlier one did, so re-staging to
# add the second platform never undoes the first. A missing payload leaves a
# PENDING note naming what is still needed, so a half-built release says so
# rather than looking finished.

version="${1:?Supply a version, e.g. 1.0.0}"
mac_pkg="${2:-}"
windows_src="${3:-}"

repo=$(cd "$(dirname "$0")/.." && pwd)
docs="$repo/packaging/docs"
out="$repo/dist/$version"
mkdir -p "$out"

abspath() { echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; }

for doc in README.txt QUICKSTART.txt; do
    sed -e "s/@VERSION@/$version/g" -e "s/@DATE@/$(date '+%-d %B %Y')/g" \
        "$docs/$doc" > "$out/$doc"
done
cp "$docs/EULA.txt" "$out/"

mac_target="$out/SPAGlitch-$version-macOS.pkg"
if [ -n "$mac_pkg" ] && [ "$(abspath "$mac_pkg")" != "$mac_target" ]; then
    cp "$mac_pkg" "$mac_target"
fi

if [ -f "$mac_target" ]; then
    rm -f "$out/PENDING-macOS.txt"
    # State the Gatekeeper verdict rather than trusting that signing ran: an
    # unnotarized pkg looks identical until someone downloads it.
    if spctl -a -vv -t install "$mac_target" 2>&1 | grep -q 'source=Notarized Developer ID'; then
        echo "macOS: signed and notarized"
    else
        echo "macOS: WARNING - not notarized; Gatekeeper will block this" >&2
    fi
else
    echo "Pending: the signed, notarized macOS installer has not been staged yet." \
        > "$out/PENDING-macOS.txt"
    echo "macOS: pending"
fi

win_target="$out/SPAGlitch-$version-Windows.exe"
if [ -n "$windows_src" ]; then
    # Accepts the installer itself, or the directory a CI artifact unpacks to.
    if [ -d "$windows_src" ]; then
        found=$(find "$windows_src" -maxdepth 1 -name '*.exe' | head -1)
        [ -n "$found" ] || { echo "error: no .exe in $windows_src" >&2; exit 1; }
        windows_src="$found"
    fi
    [ "$(abspath "$windows_src")" != "$win_target" ] && cp "$windows_src" "$win_target"
fi

if [ -f "$win_target" ]; then
    rm -f "$out/PENDING-Windows.txt"
    echo "Windows: staged"
else
    echo "Pending: no Windows build has been staged yet. The installer is built by" \
         "the Windows CI workflow, and needs the factory samples enabled to include sounds." \
        > "$out/PENDING-Windows.txt"
    echo "Windows: pending"
fi

echo
echo "staged: $out"
ls -1 "$out" | sed 's/^/  /'
