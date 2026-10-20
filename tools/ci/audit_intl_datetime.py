#!/usr/bin/env python3
"""Audit every prepared DateTimeFormat locale/calendar across bounded workers."""
import argparse
import concurrent.futures
import os
import pathlib
import subprocess
import sys


def positive(value):
    result = int(value)
    if result < 1:
        raise argparse.ArgumentTypeError('must be positive')
    return result


def audit(runner, shard, threads, timeout):
    command = [str(runner), '--audit', str(shard), str(threads)]
    try:
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                universal_newlines=True, timeout=timeout)
        return result.returncode, result.stdout
    except subprocess.TimeoutExpired as error:
        output = error.stdout or ''
        if isinstance(output, bytes):
            output = output.decode('utf-8', errors='replace')
        return 124, output + '\nAdmission shard {} timed out.\n'.format(shard)
    except OSError as error:
        return 1, '{}: {}\n'.format(command, error)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runner', type=pathlib.Path)
    parser.add_argument('--threads', type=positive, default=os.cpu_count() or 1)
    parser.add_argument('--timeout', type=positive, default=240,
                        help='maximum seconds per worker, including sanitizer overhead')
    args = parser.parse_args()
    runner = args.runner.resolve()
    failed = False
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.threads) as pool:
        futures = [pool.submit(audit, runner, shard, args.threads, args.timeout)
                   for shard in range(args.threads)]
        for future in concurrent.futures.as_completed(futures):
            status, output = future.result()
            print(output, end='' if output.endswith('\n') else '\n', flush=True)
            failed |= status != 0
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
