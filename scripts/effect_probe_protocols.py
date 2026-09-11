#!/usr/bin/env python3
"""Generate local MIDI effect measurements; audio/state must remain untracked."""
import argparse
import json
import subprocess
from pathlib import Path


def cc(frame, key, value):
    return dict(frame=frame, type='cc', key=key, value=value)


def filter_holdout():
    events, cases = [], []
    def add(mode, cutoff, resonance, note):
        start = len(cases) * 144000
        events.extend([cc(start, 120, 0), cc(start, 22, mode), cc(start, 21, cutoff),
                       cc(start, 20, resonance), cc(start, 23, 50),
                       dict(frame=start+12000, type='on', key=note, value=127),
                       dict(frame=start+84000, type='off', key=note, value=0)])
        cases.append(dict(start=start+12000, gate=72000, resonance=resonance,
                          cutoff=cutoff*10000, scale=1, note=note,
                          type='lp' if mode == 2 else 'hp'))
    # Initialize each insert at the ordinary cutoff before testing new settings.
    add(2, 50, 0, 12)
    add(3, 50, 0, 12)
    for mode in [2, 3]:
        for cutoff in [35, 65, 85, 95]:
            for resonance, note in [(40, 12), (70, 13), (90, 12), (100, 13)]:
                add(mode, cutoff, resonance, note)
    return dict(rate=48000, block=256, frames=len(cases)*144000,
                float=True, events=events, cases=cases)


def lofi_clock(block):
    starts = [1024, 51025, 99333, 148001, 196003, 244017, 292099, 340103]
    gates = [2400, 24000, 30000, 4800]*2
    events = [cc(0, 22, 1), cc(0, 23, 50), cc(0, 24, 8)]
    for start, gate in zip(starts, gates):
        events.extend([dict(frame=start, type='on', key=12, value=127),
                       dict(frame=start+gate, type='off', key=12, value=0)])
    return dict(rate=48000, block=block, frames=400000, float=True,
                events=events, cases=[dict(start=s, gate=g) for s, g in zip(starts, gates)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=['filter-holdout', 'lofi-clock'])
    parser.add_argument('directory', type=Path)
    parser.add_argument('--probe', type=Path)
    parser.add_argument('--state', type=Path)
    args = parser.parse_args()
    if bool(args.probe) != bool(args.state):
        parser.error('--probe and --state must be supplied together to capture audio')
    args.directory.mkdir(parents=True, exist_ok=False)
    protocols = {'filter-holdout': filter_holdout()} if args.kind == 'filter-holdout' else {
        f'lofi-clock-b{block}': lofi_clock(block) for block in [32, 64, 256, 512]}
    for name, protocol in protocols.items():
        path = args.directory / (name+'.json')
        path.write_text(json.dumps(protocol, indent=2)+'\n')
        if args.probe:
            subprocess.run([str(args.probe.resolve()), str(args.state.resolve()),
                            str(path.resolve()), str(path.with_suffix('.wav').resolve())], check=True)


if __name__ == '__main__':
    main()
