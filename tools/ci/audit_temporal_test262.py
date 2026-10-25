#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Verify full native Temporal coverage from actual CI timing records."""
import argparse
from pathlib import Path
import re

from report_test262_timings import read_timings

PREFIX = 'test/intl402/Temporal/'


def source_selection(checkout):
    root = checkout / PREFIX
    if not root.is_dir():
        raise ValueError('Temporal Test262 directory is missing: ' + str(root))
    all_cases, applicable = set(), set()
    for path in sorted(root.rglob('*.js')):
        name = PREFIX + path.relative_to(root).as_posix()
        all_cases.add(name)
        match = re.search(r'/\*---(.*?)---\*/', path.read_text(encoding='utf-8'), re.S)
        if not match:
            raise ValueError('missing Test262 metadata: ' + name)
        # Support inline lists and YAML block lists without parsing JS strings
        # or assertion messages, which may merely mention Intl as a comparison.
        features = set()
        in_features = False
        for line in match.group(1).splitlines():
            if line.startswith('features:'):
                in_features = True
                features.update(re.findall(r'[\w.-]+', line[len('features:'):]))
            elif in_features and line[:1].isspace():
                features.update(re.findall(r'[\w.-]+', line))
            else:
                in_features = False
        if ('toLocaleString' not in path.relative_to(root).parts and
                'Intl-enumeration' not in features):
            applicable.add(name)
    if not all_cases or not applicable:
        raise ValueError('empty Temporal conformance selection')
    return all_cases, applicable


def audit(rows, problems, all_cases, applicable):
    seen, executed = set(), set()
    skipped = set()
    for row in rows:
        name = row['file'].replace('\\', '/')
        position = name.find(PREFIX)
        if position < 0:
            continue
        name = name[position:]
        seen.add(name)
        if row['status'] == 'skipped':
            skipped.add(name)
        else:
            executed.add(name)
    issues = list(problems)
    for label, names in [
            ('Missing timing record', all_cases - seen),
            ('Applicable source skipped', applicable & skipped),
            ('Applicable source not executed', applicable - executed),
            ('Unsupported Intl source executed', executed - applicable),
            ('Unknown source in timing log', seen - all_cases)]:
        if names:
            issues.append('{} ({}): {}'.format(label, len(names),
                                              ', '.join(sorted(names)[:5])))
    lines = ['## Native Temporal Test262 selection', '',
             '{} of {} source files are applicable without Intl; {} executed.'
             .format(len(applicable), len(all_cases), len(executed)), '',
             'Execution includes failures: this audits coverage, while the '
             'runner reports conformance. Formatting and Intl enumeration '
             'remain capability skips.', '']
    if issues:
        lines += ['**Selection audit failed.**', '']
        lines += ['- ' + issue for issue in issues]
    else:
        lines += ['Every applicable source executed; unsupported Intl sources '
                  'have skipped timing records.']
    return '\n'.join(lines) + '\n', bool(issues)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('timings', type=Path)
    parser.add_argument('--test262', type=Path, default=Path('test262'))
    parser.add_argument('--markdown', type=Path, required=True)
    args = parser.parse_args()
    try:
        all_cases, applicable = source_selection(args.test262)
        rows, problems = read_timings(args.timings)
        report, failed = audit(rows, problems, all_cases, applicable)
    except (OSError, ValueError) as error:
        report = '## Native Temporal Test262 selection\n\n**Audit failed:** {}\n'.format(error)
        failed = True
    args.markdown.write_text(report, encoding='utf-8')
    print(report, end='')
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
