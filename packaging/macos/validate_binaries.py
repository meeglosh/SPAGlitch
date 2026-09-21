"""Enforce the distribution's Intel/Apple Silicon and macOS 13 contract."""
import pathlib
import re
import subprocess
import sys
root = pathlib.Path(sys.argv[1])
for bundle in ('SPAGlitch.app', 'SPAGlitch.component', 'SPAGlitch.vst3'):
    binary = root / bundle / 'Contents/MacOS/SPAGlitch'
    archs = subprocess.check_output(['lipo', '-archs', str(binary)], text=True).split()
    assert set(archs) == {'arm64', 'x86_64'}, f'{bundle}: missing universal architectures: {archs}'
    metadata = subprocess.check_output(['xcrun', 'vtool', '-show-build', str(binary)], text=True)
    minimums = re.findall(r'\bminos\s+([\d.]+)', metadata)
    assert len(minimums) == 2 and all(v in ('13.0', '13.0.0') for v in minimums), f'{bundle}: unexpected deployment minimums {minimums}'
    print(f'{bundle}: Intel + Apple Silicon, minimum macOS 13.0 verified')
