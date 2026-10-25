#!/usr/bin/env python3
# Copyright (c) 2026 Yan-Jie Wang
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

"""Build the optional pinned ICU4X companion archive and its distribution notices."""
import argparse
import filecmp
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile


def run(command, *, env=None, input=None, timeout=900):
    # The outer build remains observable; a stalled Cargo/compiler also has a
    # bounded lifetime. Do not execute a target program during cross builds.
    process = subprocess.Popen(command, universal_newlines=True,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, env=env,
                               start_new_session=os.name == 'posix')
    try:
        stdout, stderr = process.communicate(input=input, timeout=timeout)
    except BaseException as error:
        if os.name == 'posix':
            os.killpg(process.pid, signal.SIGKILL)
        else:
            process.kill()
        process.communicate(timeout=10)
        if isinstance(error, subprocess.TimeoutExpired):
            raise SystemExit('Timed out building/querying the ICU4X bridge')
        raise
    if process.returncode:
        sys.stderr.write(stdout + stderr)
        raise SystemExit(process.returncode)
    return subprocess.CompletedProcess(command, process.returncode, stdout, stderr)


def copy_atomic(source, destination):
    destination = Path(destination)
    if destination.is_file() and filecmp.cmp(str(source), str(destination), shallow=False):
        shutil.copymode(str(source), str(destination))
        return
    with tempfile.NamedTemporaryFile(prefix=destination.name + '.',
                                     dir=str(destination.parent), delete=False) as output:
        temporary = Path(output.name)
    try:
        shutil.copyfile(str(source), str(temporary))
        shutil.copymode(str(source), str(temporary))
        os.replace(str(temporary), str(destination))
    finally:
        if temporary.exists():
            temporary.unlink()


def rust_flags(args):
    # Cargo gives encoded flags priority over RUSTFLAGS. Use the same vector
    # for dependency probing and the real build, with required PIC/unwind last.
    # Cross-build generators must not inherit target-specific machine flags.
    if args.build_machine:
        flags = []
    elif 'CARGO_ENCODED_RUSTFLAGS' in os.environ:
        flags = [value for value in os.environ['CARGO_ENCODED_RUSTFLAGS'].split('\x1f') if value]
    else:
        flags = shlex.split(os.environ.get('RUSTFLAGS', ''))
    return flags + ['-C', 'relocation-model=pic', '-C', 'panic=unwind']


def native_libs(stderr):
    match = re.search(r'native-static-libs:\s*(.+)', stderr)
    if not match:
        raise SystemExit('Rust did not report its target native static-library dependencies')
    return shlex.split(match.group(1))


def probe(args):
    root = Path(args.directory)
    root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rust-link-probe-', dir=str(root)) as directory:
        result = run([args.rustc, '--crate-name', 'quickjs_icu4x_link_probe',
                      '--crate-type=staticlib', '--print=native-static-libs',
                      '--target', args.target, '--out-dir', directory, '-'] + rust_flags(args),
                     input='pub fn quickjs_icu4x_probe() {}\n', timeout=120)
    for flag in native_libs(result.stderr):
        print(flag)


def notices(metadata, rustc, destination):
    # Group identical notices, retaining the exact package/version attribution.
    # Cargo authenticates registry sources using Cargo.lock before this read.
    records = {}
    for package in sorted(metadata['packages'], key=lambda p: (p['name'], p['version'])):
        if not package['source']:
            continue
        directory = Path(package['manifest_path']).parent
        paths = sorted(p for p in directory.iterdir()
                       if p.is_file() and p.name.upper().startswith(('LICENSE', 'COPYRIGHT', 'NOTICE')))
        if package.get('license_file'):
            paths.append(directory / package['license_file'])
        if not paths:
            raise SystemExit('Missing license notice for ' + package['name'])
        for path in paths:
            text = path.read_text(encoding='utf-8')
            key = hashlib.sha256(text.encode()).hexdigest()
            entry = records.setdefault(key, [text, set()])
            entry[1].add(package['name'] + ' ' + package['version'] + ' / ' + path.name)
    toolchain = Path(run([rustc, '--print=sysroot'], timeout=30).stdout.strip())
    rust_docs = toolchain / 'share/doc/rust'
    copyright = rust_docs / 'COPYRIGHT-library.html'
    if not copyright.is_file():
        raise SystemExit('Rust standard-library copyright notices are missing from ' + str(rust_docs))
    shutil.copyfile(str(copyright), str(destination / 'ICU4X-RUST-COPYRIGHT.html'))
    output = ['ICU4X companion dependencies\n', run([rustc, '-vV'], timeout=30).stdout]
    for text, packages in records.values():
        output.extend(['\n' + '\n'.join(sorted(packages)) + '\n', text, '\n'])
    # The standard-library copyright file identifies incorporated components;
    # retain their installed full license texts alongside those attributions.
    for path in sorted((rust_docs / 'licenses').glob('*')):
        if path.is_file():
            output.extend(['\nRust toolchain / ' + path.name + '\n', path.read_text(encoding='utf-8'), '\n'])
    (destination / 'ICU4X-NOTICES.txt').write_text(''.join(output), encoding='utf-8')


def build(args):
    directory = Path(args.directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env['RUSTC'] = args.rustc
    # One PIC archive serves the selected static and shared C engine variants.
    env.pop('RUSTFLAGS', None)
    env['CARGO_ENCODED_RUSTFLAGS'] = '\x1f'.join(rust_flags(args))
    target_dir = directory / 'cargo'
    command = [args.cargo, 'rustc', '--lib', '--manifest-path', args.manifest, '--locked',
               '--release', '--target', args.target, '--target-dir', str(target_dir),
               '--features', ','.join(args.features)]
    if 'CARGO_BUILD_JOBS' not in env:
        command.extend(['--jobs', '4'])
    command.extend(['--', '--print=native-static-libs'])
    result = run(command, env=env)
    sys.stderr.write(result.stderr)
    if native_libs(result.stderr) != shlex.split(args.native_libs):
        raise SystemExit('Rust native library flags changed; reconfigure the Meson build')
    archive = target_dir / args.target / 'release/libquickjs_icu4x.a'
    shutil.copyfile(str(archive), str(directory / 'libquickjs-icu4x.a'))
    metadata = run([args.cargo, 'metadata', '--manifest-path', args.manifest,
                    '--locked', '--offline', '--format-version', '1',
                    '--features', ','.join(args.features)], env=env)
    notices(json.loads(metadata.stdout), args.rustc, directory)
    for source, destination in zip(
            ['libquickjs-icu4x.a', 'ICU4X-NOTICES.txt', 'ICU4X-RUST-COPYRIGHT.html'], args.outputs):
        copy_atomic(directory / source, destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['probe', 'build'])
    parser.add_argument('--rustc', required=True)
    parser.add_argument('--target', required=True)
    parser.add_argument('--directory', required=True)
    parser.add_argument('--cargo')
    parser.add_argument('--manifest')
    parser.add_argument('--features', nargs='+', default=[])
    parser.add_argument('--outputs', nargs=3)
    parser.add_argument('--native-libs', default='')
    parser.add_argument('--build-machine', action='store_true')
    args = parser.parse_args()
    if args.action == 'probe':
        probe(args)
    else:
        if not args.cargo or not args.manifest or not args.features or not args.outputs:
            parser.error('build requires --cargo, --manifest and --features/--outputs')
        build(args)


if __name__ == '__main__':
    main()
