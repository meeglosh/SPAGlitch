#!/bin/bash
set -euo pipefail

# Lays out dist/<version>/ for a release, so every build has the same shape and
# the docs travel with the binaries:
#
#   dist/<version>/
#     macOS/SPAGlitch-<version>.pkg        signed + notarized
#     Windows/...                          see README-WINDOWS.txt
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

abspath() { echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; }

# Each side is staged if a payload is present, whether this run supplied it or
# an earlier one did. Re-staging to add the second platform is how a release
# gets completed, so a run that supplies only one must not undo the other.
staged_pkg="$out/macOS/SPAGlitch-$version.pkg"
if [ -n "$mac_pkg" ] && [ "$(abspath "$mac_pkg")" != "$staged_pkg" ]; then
    cp "$mac_pkg" "$staged_pkg"
fi

if [ -f "$staged_pkg" ]; then
    rm -f "$out/macOS/PENDING.txt"
    if spctl -a -vv -t install "$staged_pkg" 2>&1 | grep -q 'source=Notarized Developer ID'; then
        echo "macOS: signed and notarized"
    else
        echo "macOS: WARNING - not notarized; Gatekeeper will block this" >&2
    fi
else
    echo "Pending: the signed, notarized macOS installer has not been staged yet." \
        > "$out/macOS/PENDING.txt"
    echo "macOS: pending"
fi

if [ -n "$windows_dir" ] && [ "$(cd "$windows_dir" && pwd)" != "$out/Windows" ]; then
    ditto "$windows_dir" "$out/Windows"
fi

if find "$out/Windows" -type f ! -name 'README-WINDOWS.txt' ! -name 'PENDING.txt' | grep -q .; then
    rm -f "$out/Windows/PENDING.txt"
    # Rewritten every run, so revised wording reaches an already-staged folder.
    cat > "$out/Windows/README-WINDOWS.txt" <<'TXT'
SPAGlitch for Windows - 64-bit, Windows 10 or 11.

Run the Setup .exe. It installs the standalone instrument, the VST3 plugin and
all 479 factory sounds. Choose the formats you want during setup, and quit your
DAW first. Uninstall through Windows Settings > Apps.

This installer is not yet code signed, so SmartScreen may show "Windows
protected your PC" the first time you run it: click More info, then Run anyway.
That warning is about the absence of a signing certificate, not about the file.
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
