#!/usr/bin/env python3
"""Render completed Test262 variant timings without hiding partial logs."""
import argparse
import csv
import json
import math
from pathlib import Path

FIELDS = ('file', 'thread', 'index', 'mode', 'status', 'duration_ms')


def read_timings(path):
    rows, problems = [], []
    with path.open(encoding='utf-8') as stream:
        for number, line in enumerate(stream, 1):
            try:
                row = json.loads(line)
                if not isinstance(row, dict) or set(row) != set(FIELDS):
                    raise ValueError('unexpected fields')
                if not isinstance(row['file'], str):
                    raise ValueError('invalid file')
                for key in ('thread', 'index'):
                    if type(row[key]) is not int:
                        raise ValueError('invalid ' + key)
                duration = row['duration_ms']
                if (type(duration) not in (int, float)
                        or not math.isfinite(duration) or duration < 0):
                    raise ValueError('invalid duration')
                if row['mode'] not in ('non-strict', 'strict', 'module', 'none'):
                    raise ValueError('invalid mode')
                if row['status'] not in ('pass', 'fail', 'skipped'):
                    raise ValueError('invalid status')
                if (row['mode'] == 'none') != (row['status'] == 'skipped'):
                    raise ValueError('invalid skipped mode')
                if row['status'] == 'skipped' and duration != 0:
                    raise ValueError('nonzero skipped duration')
                rows.append(row)
            except (ValueError, TypeError) as error:
                problems.append('Line {}: {}'.format(number, error))
    return rows, problems


def cell(value):
    return str(value).replace('&', '&amp;').replace('<', '&lt;').replace(
        '>', '&gt;').replace('|', '&#124;').replace('`', '&#96;').replace(
        '\r', '&#13;').replace('\n', '&#10;')


def report(rows, problems, top):
    counts = {status: sum(row['status'] == status for row in rows)
              for status in ('pass', 'fail', 'skipped')}
    executed = [row for row in rows if row['status'] != 'skipped']
    lines = [
        '## Test262 variant timings', '',
        '{} completed records from {} source files: {} pass, {} fail, '
        '{} skipped.'.format(len(rows), len({row['file'] for row in rows}),
                             counts['pass'], counts['fail'], counts['skipped']),
        '',
        'Summed execution wall time: {:.3f} seconds. This is the sum across '
        'workers, not suite elapsed time.'.format(
            sum(row['duration_ms'] for row in executed) / 1000),
        '',
        'Each execution includes realm creation, harness, evaluation, pending '
        'jobs, and cleanup; source loading and metadata parsing are excluded.',
        'Only completed variants are recorded. A process abort can leave '
        'unreported work; the progress log identifies unfinished sources.', '',
    ]
    if problems:
        lines += ['**Partial or invalid log: {} rejected records.**'.format(
            len(problems)), '']
        lines += ['- ' + cell(problem) for problem in problems[:10]]
        lines += ['']
    lines += ['### Slowest completed variants', '',
              '| File | Mode | Result | Wall time (ms) | Worker |',
              '| --- | --- | --- | ---: | ---: |']
    for row in sorted(executed, key=lambda row: (
            -row['duration_ms'], row['file'], row['mode']))[:top]:
        lines.append('| {} | {} | {} | {:.6f} | {} |'.format(
            cell(row['file']), row['mode'], row['status'],
            row['duration_ms'], row['thread']))
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--csv', required=True, type=Path)
    parser.add_argument('--markdown', required=True, type=Path)
    parser.add_argument('--top', type=int, default=30)
    args = parser.parse_args()
    if args.top < 1:
        parser.error('--top must be positive')
    rows, problems = read_timings(args.input)
    with args.csv.open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    args.markdown.write_text(report(rows, problems, args.top), encoding='utf-8')
    return int(bool(problems))


if __name__ == '__main__':
    raise SystemExit(main())
