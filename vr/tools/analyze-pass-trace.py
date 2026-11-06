"""Rank disjoint CPU renderer intervals; requires the new *_passes.csv capture."""
import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path
from statistics import mean


def analyze(path):
    with path.open(newline='') as f:
        frames = list(csv.DictReader(f))
    passes = path.with_name(path.stem + '_passes.csv')
    if not passes.exists():
        raise ValueError(f'{passes} missing: old captures cannot identify renderer intervals')
    groups = {'fast': [], 'slow': []}
    for i, row in enumerate(frames):
        if float(row['eye0_draws']) <= 0:
            continue
        ms = float(row['frame_ms'])
        if ms < 16:
            groups['fast'].append(i)
        elif ms > 20:
            groups['slow'].append(i)
    spans = defaultdict(lambda: defaultdict(float))
    totals = defaultdict(float)
    edges = defaultdict(list)
    with passes.open(newline='') as f:
        for row in csv.DictReader(f):
            if int(row['overflow']):
                raise ValueError('bounded span table overflow: do not attribute incomplete partitions')
            i, eye = int(row['frame_index']), int(row['eye'])
            if not 0 <= i < len(frames) or eye not in (0, 1):
                raise ValueError('invalid frame index or eye in pass timings')
            start, end = int(row['from_site'], 16), int(row['to_site'], 16)
            key = f"eye{eye}:{row['from_site']}->{row['to_site']}"
            ms = float(row['cpu_ms'])
            calls = int(row['calls'])
            if not math.isfinite(ms) or ms < 0 or calls <= 0:
                raise ValueError('invalid duration or call count in pass timings')
            spans[key][i] += ms
            totals[i, eye] += ms
            edges[i, eye].append((start, end, calls))
    if any(not ids for ids in groups.values()):
        raise ValueError('both fast (<16 ms) and slow (>20 ms) groups are required')
    errors = [abs(totals[i, eye] - float(frames[i][f'eye{eye}_cpu_ms']))
              for ids in groups.values() for i in ids for eye in (0, 1)]
    for ids in groups.values():
        for i in ids:
            for eye in (0, 1):
                observed = edges[i, eye]
                if sum(n for a, b, n in observed if a == 0) != 1 or \
                        sum(n for a, b, n in observed if b == 0xFFFFFFFF) != 1:
                    raise ValueError(f'incomplete scene partition for frame {i}, eye {eye}')
                balance = defaultdict(int)
                for a, b, n in observed:
                    balance[a] += n
                    balance[b] -= n
                expected = {0: 1, 0xFFFFFFFF: -1}
                if {k: v for k, v in balance.items() if v} != expected:
                    raise ValueError('disconnected or unbalanced timing edges')
    if max(errors) > 0.1:
        raise ValueError(f'partition/eye mismatch {max(errors):.3f} ms exceeds 0.1 ms; attribution rejected')
    ranking = []
    for key, values in spans.items():
        avg = {g: mean(values.get(i, 0) for i in ids) for g, ids in groups.items()}
        ranking.append({'interval': key, 'fast_ms': avg['fast'], 'slow_ms': avg['slow'],
                        'increase_ms': avg['slow'] - avg['fast']})
        a, b = [int(value, 16) for value in key.split(':')[1].split('->')]
        ranking[-1]['kind'] = 'queued_render_routine' if b == (a | 0x80000000) and a != 0 else 'between_boundaries'
    return {'capture': str(path), 'group_sizes': {g: len(ids) for g, ids in groups.items()},
            'max_partition_vs_eye_error_ms': max(errors),
            'warning': 'CPU wall time, not GPU execution time or proof of causal ownership. '
                       'Loops aggregate repeated edges; match route and tracing overhead before attribution.',
            'intervals': sorted(ranking, key=lambda r: r['increase_ms'], reverse=True)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(json.dumps(analyze(args.capture), indent=2))