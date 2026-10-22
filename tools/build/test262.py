#!/usr/bin/env python3
"""Prepare the repository Test262 pin or run it with all available workers."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

PIN = 'd53ebc8b1931d9f1541e43d59650776e4b766248'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['bootstrap', 'run', 'default',
                                      'check', 'update', 'legacy'])
    parser.add_argument('--source', type=Path,
                        default=Path(__file__).resolve().parents[2])
    parser.add_argument('--runner', type=Path)
    parser.add_argument('--threads', type=int,
                        default=int(os.environ.get('TEST262_THREADS', os.cpu_count() or 1)))
    parser.add_argument('--timing-log', default=os.environ.get('TEST262_TIMING_LOG'))
    parser.add_argument('--progress-log', default=os.environ.get('TEST262_PROGRESS_LOG'))
    args, extra = parser.parse_known_args()
    source = args.source.resolve()
    if args.mode == 'bootstrap':
        checkout = source / 'test262'
        if not checkout.exists():
            subprocess.check_call(['git', 'clone', '--single-branch',
                                   '--shallow-since=2025-09-01',
                                   'https://github.com/tc39/test262.git', str(checkout)])
        subprocess.check_call(['git', 'fetch', 'origin'], cwd=str(checkout))
        subprocess.check_call(['git', 'reset', '--hard', PIN], cwd=str(checkout))
        for name in ['test262.patch', 'test262-unicode18.patch']:
            subprocess.check_call(['git', 'apply', str(source / 'tests' / name)],
                                  cwd=str(checkout))
        return 0
    if args.runner is None:
        parser.error('--runner is required when running tests')
    if args.threads < 1:
        parser.error('--threads must be positive')
    command = [str(args.runner.resolve()), '-T', str(args.threads), '-t']
    command += ['-u'] if args.mode == 'update' else ['-m']
    command += ['-c', 'test262o.conf' if args.mode == 'legacy' else 'test262.conf']
    if args.mode not in ['default', 'legacy']:
        command += ['-a']
    if args.mode == 'check':
        command += ['-E']
    for flag, path in [('--timing-log', args.timing_log),
                       ('--progress-log', args.progress_log)]:
        if path:
            command += [flag, str(Path(path).resolve())]
    return subprocess.call(command + extra, cwd=str(source))


if __name__ == '__main__':
    sys.exit(main())
