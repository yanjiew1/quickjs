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
