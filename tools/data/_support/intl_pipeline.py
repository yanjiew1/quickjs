#!/usr/bin/env python3
"""Run the explicit, offline CLDR-to-Intl pipeline into a review directory."""
import argparse
import os
from pathlib import Path
import signal
import stat
import subprocess
import sys
import time

from .outputs import encoded, write_atomic

ROOT = Path(__file__).resolve().parents[3]


def commands(args):
    tools = ROOT / 'tools/intl'
    selected = ['--sources', str(args.sources)]
    if args.source_pin:
        selected += ['--source-pin', str(args.source_pin)]
    result = []
    segment = args.output_dir / 'segmentation.qjin'
    if 'native' in args.profiles:
        dictionary_metadata = (
            ['--dictionary-pin', str(args.dictionary_pin)] if args.dictionary_pin else [])
        result.append(('segmentation', [sys.executable, str(tools / 'segment_prepare.py')] +
                       selected + dictionary_metadata + [
            '--dictionaries', str(args.dictionaries),
            '--property-tool', str(args.property_tool),
            '--output', str(segment),
            '--manifest', str(args.output_dir / 'segmentation-manifest.json'),
            '--license', str(args.output_dir / 'LICENSE-SEGMENT-DICTIONARIES'),
            '--notices', str(args.output_dir / 'NOTICE-SEGMENT-DICTIONARIES')]))
    for profile in args.profiles:
        collation = args.output_dir / ('collation-' + profile + '.qjcl')
        output = args.output_dir / (profile + '.bin')
        manifest = args.output_dir / (profile + '-manifest.json')
        result.append(('collation-' + profile, [sys.executable, str(tools / 'collation_prepare.py')] + selected + [
            '--profile', profile, '--compiler', str(args.collation_compiler),
            '--output', str(collation)]))
        generate = [sys.executable, str(tools / 'generate.py')] + selected + [
            '--profile', profile, '--compiled-collation', str(collation),
            '--output', str(output), '--manifest', str(manifest)]
        if profile == 'native':
            generate += ['--compiled-segmentation', str(segment),
                         '--segmentation-manifest', str(args.output_dir / 'segmentation-manifest.json')]
        result.append(('generate-' + profile, generate))
        result.append(('verify-' + profile, [sys.executable, str(tools / 'check_data.py')] + selected + [
            '--input', str(output), '--manifest', str(manifest)]))
    result.append(('collation-oracle', [sys.executable, str(ROOT / 'tests/intl_collation_oracle.py')] +
                   selected + ['--output', str(args.output_dir / 'intl-collation-oracle.bin'),
                               '--manifest', str(args.output_dir / 'intl-collation-oracle.json')]))
    return result


def stage_timeout(label, override=None):
    """Keep established per-stage CI bounds unless explicitly overridden."""
    if override is not None:
        return override
    if label == 'segmentation':
        return 180
    if label.startswith('collation-') and label != 'collation-oracle':
        return 240
    return 600


def run(command, log, timeout):
    """Bound a generator stage and retain its diagnostic output."""
    with log.open('wb') as stream:
        process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT,
                                   start_new_session=os.name == 'posix')
        try:
            return process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            stop(process)
            return 124
        except BaseException:
            stop(process)
            raise


def stop(process):
    """Clean the stage group even when its leader exits before its children."""
    if os.name == 'posix':
        def group_signal(sig):
            try:
                os.killpg(process.pid, sig)
            except ProcessLookupError:
                pass
            except PermissionError:
                # Darwin can deny group signaling; still reap our owned child.
                if process.poll() is None:
                    process.send_signal(sig)

        group_signal(signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass
        # Waiting for the leader does not prove its descendants have exited.
        group_signal(signal.SIGKILL)
        process.wait(timeout=5)
    else:
        # Windows has no killpg. Ask its process-tree utility first, with a
        # finite wait, then retain a direct-child fallback if it is unavailable.
        try:
            subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           timeout=5, check=False)
        except (OSError, subprocess.TimeoutExpired):
            pass
        if process.poll() is None:
            process.kill()
        process.wait(timeout=5)


def preflight(args, steps):
    """Reject output aliases before even opening a stage log for truncation."""
    outputs = {args.output_dir / 'regeneration-report.json'}
    for label, command in steps:
        outputs.add(args.output_dir / (label + '.log'))
        for index, value in enumerate(command[:-1]):
            if value in ('--output', '--manifest', '--license', '--notices'):
                outputs.add(Path(command[index + 1]).resolve())
    directories = [path for path in (args.sources, args.dictionaries) if path]
    files = [path for path in (args.source_pin, args.dictionary_pin, args.collation_compiler,
                              args.property_tool) if path]
    aliases = {}
    for output in outputs:
        # Logs are opened directly while a stage runs. Refuse special files,
        # and remember hard-linked leaves that may alias source-tree members.
        if output.exists():
            status = output.stat()
            if not stat.S_ISREG(status.st_mode):
                raise ValueError('output must be a regular file: ' + str(output))
            if status.st_nlink > 1:
                aliases[(status.st_dev, status.st_ino)] = output
        # Logs and reports can also be symlinks into an input tree. Resolve
        # every leaf before run() opens any stage log for truncation.
        output = output.resolve()
        if any(output == root or root in output.parents for root in directories):
            raise ValueError('output must be outside source directories: ' + str(output))
        for path in files:
            if output == path or (output.exists() and path.exists() and output.samefile(path)):
                raise ValueError('output would replace an input: ' + str(output))
    if aliases:
        # Ordinary fresh output directories need no tree walk. For an existing
        # hard link, resolve inode ownership before any stage log is truncated.
        for root in directories:
            for directory, _, names in os.walk(str(root)):
                for name in names:
                    member = Path(directory) / name
                    status = member.stat()
                    alias = aliases.get((status.st_dev, status.st_ino))
                    if alias is not None:
                        raise ValueError('output would replace a source member: ' + str(alias))


def positive(value):
    value = int(value)
    if value < 1:
        raise argparse.ArgumentTypeError('must be positive')
    return value


def main(argv=None, prog=None):
    parser = argparse.ArgumentParser(prog=prog, description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--source-pin', type=Path)
    parser.add_argument('--dictionaries', type=Path)
    parser.add_argument('--dictionary-pin', type=Path)
    parser.add_argument('--collation-compiler', type=Path, required=True,
                        help='build-host intl-collation-compile executable')
    parser.add_argument('--property-tool', type=Path,
                        help='build-host intl-segment-property executable')
    parser.add_argument('--profiles', nargs='+', choices=['minimal', 'native'],
                        default=['minimal', 'native'])
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--timeout', type=positive,
                        help='override per-stage limits (180s segmentation, 240s collation, 600s otherwise)')
    args = parser.parse_args(argv)
    if len(set(args.profiles)) != len(args.profiles):
        parser.error('profiles must be unique')
    if 'native' in args.profiles and (not args.dictionaries or not args.property_tool):
        parser.error('native requires --dictionaries and --property-tool')
    for name in ('sources', 'source_pin', 'dictionaries', 'dictionary_pin', 'collation_compiler', 'property_tool', 'output_dir'):
        value = getattr(args, name)
        if value is not None:
            setattr(args, name, value.resolve())
    steps = commands(args)
    preflight(args, steps)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    receipt = {'status': 'running', 'stages': []}
    try:
        for label, command in steps:
            log = args.output_dir / (label + '.log')
            print(label + ': ' + str(log), flush=True)
            start = time.monotonic()
            code = run(command, log, stage_timeout(label, args.timeout))
            receipt['stages'].append({'name': label, 'command': command,
                                      'seconds': time.monotonic() - start,
                                      'returncode': code, 'log': str(log)})
            if code:
                receipt['status'] = 'failed'
                return code
        receipt['status'] = 'passed'
        return 0
    finally:
        # Partial outputs remain candidates. A failed stage never becomes an
        # accepted repository update, and later dependent stages do not run.
        if receipt['status'] == 'running':
            receipt['status'] = 'failed'
        write_atomic(args.output_dir / 'regeneration-report.json', encoded(receipt))
