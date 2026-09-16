#!/usr/bin/env python3
"""Run correctness comparisons against pinned main. Never run benchmarks."""
from __future__ import annotations
import argparse
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile

BASE = 'b63ab025d402e46c647a8a239bffe72a58f73a35'
TEST262 = '5c8206929d81b2d3d727ca6aac56c18358c8d790'
ROOT = Path(__file__).resolve().parent.parent


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    baseline = out / 'baseline'
    baseline.mkdir()
    archive = subprocess.check_output(['git', 'archive', BASE], cwd=ROOT)
    with tarfile.open(fileobj=io.BytesIO(archive)) as tf:
        tf.extractall(baseline, filter='data')
    results = {'base': BASE, 'test262_commit': TEST262, 'performance_tested': False,
               'compilers': {}, 'success': False}

    def run(label, command, cwd=ROOT, accepted=(0,)):
        print(f'{label}: {command}', flush=True)
        with (out / (label + '.log')).open('w') as log:
            status = subprocess.run(command, cwd=cwd, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=1800).returncode
        if status not in accepted:
            raise RuntimeError(f'{label} failed with exit {status}; see log')
        return status

    def symbols(cwd, label):
        raw = subprocess.check_output(['nm', '-D', '--defined-only', 'qjs'], cwd=cwd).decode()
        names = sorted(line.split()[-1] for line in raw.splitlines() if line.split())
        (out / (label + '.symbols')).write_text('\n'.join(names) + '\n')
        return names

    def conformance(cwd, label):
        errors = out / (label + '.errors')
        errors.touch()
        status = run(label, ['./run-test262', '-C', '-c', 'test262.conf', '-a',
                             '-u', '-e', str(errors), '-r', str(out / (label + '.report'))],
                     cwd=cwd, accepted=(0, 1))
        text = (out / (label + '.log')).read_text(errors='replace')
        matches = re.findall(r'Result: (\d+)/(\d+) errors(?:, (\d+) excluded)?(?:, (\d+) skipped)?', text)
        if len(matches) != 1:
            raise RuntimeError(f'{label}: missing or ambiguous Test262 summary')
        failed, total, excluded, skipped = [int(n or 0) for n in matches[0]]
        if total < 50000 or status != int(failed > 0):
            raise RuntimeError(f'{label}: incomplete or abnormal Test262 execution')
        return {'failed': failed, 'total': total, 'excluded': excluded, 'skipped': skipped}, errors.read_bytes()

    try:
        # The Makefile pins and applies the project's accepted Test262 patch.
        run('test262-bootstrap', ['make', 'test2-bootstrap'])
        actual = subprocess.check_output(['git', '-C', 'test262', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
        if actual != TEST262 or not (ROOT / 'test262/features.txt').is_file():
            raise RuntimeError('Test262 checkout does not match the accepted pin')
        (baseline / 'test262').symlink_to(ROOT / 'test262', target_is_directory=True)
        jobs = str(min(4, os.cpu_count() or 2))
        for compiler in ['gcc', 'clang']:
            flags = ['CONFIG_WERROR=y'] + (['CONFIG_CLANG=y'] if compiler == 'clang' else [])
            one = {}
            results['compilers'][compiler] = one
            run(compiler + '-version', [compiler, '--version'])
            for kind, cwd in [('baseline', baseline), ('candidate', ROOT)]:
                label = compiler + '-' + kind
                run(label + '-clean', ['make', 'clean'], cwd)
                run(label + '-build', ['make', '-j' + jobs] + flags, cwd)
                run(label + '-tests', ['make'] + flags + ['test'], cwd)
                one[kind] = {'build': 'PASS', 'built_in_tests': 'PASS'}
                one[kind]['symbols'] = symbols(cwd, label)
                summary, failures = conformance(cwd, label + '-test262')
                one[kind]['test262'] = summary
                one[kind]['failure_bytes'] = failures
            before, after = one['baseline'], one['candidate']
            if before['symbols'] != after['symbols']:
                raise RuntimeError(f'{compiler}: exported symbol sets differ')
            if before['test262'] != after['test262'] or before['failure_bytes'] != after['failure_bytes']:
                raise RuntimeError(f'{compiler}: Test262 results/failure sets differ')
            one['exported_symbol_count'] = len(before['symbols'])
            one['exports_match'] = True
            one['test262_failure_sets_match'] = True
            for side in [before, after]:
                del side['symbols'], side['failure_bytes']
            print(f'{compiler}: builds, tests, exports and full Test262 comparison PASS', flush=True)
        results['success'] = True
    finally:
        # On failure the workflow uploads this report but does not publish code.
        report = json.dumps(results, indent=2, default=lambda x: '<binary failure list in artifact>') + '\n'
        (out / 'validation.json').write_text(report)
        (ROOT / 'SPLIT-05-VALIDATION.json').write_text(report)


if __name__ == '__main__':
    main()
