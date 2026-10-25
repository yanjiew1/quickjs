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

"""Package tracked sources or build binaries in separate Meson directories."""
import argparse
import io
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

SOURCE = Path(__file__).resolve().parents[1]


def run(command, env=None):
    subprocess.check_call(command, cwd=str(SOURCE), env=env, timeout=3600)


def source_release(output, build_root, version):
    files = subprocess.check_output(['git', 'ls-files', '-z'], cwd=str(SOURCE))
    prefix = 'quickjs-' + version
    documentation = []
    if shutil.which('makeinfo'):
        build = build_root / 'documentation'
        run(['meson', 'setup', str(build), str(SOURCE), '-Dtests=false',
             '-Dexamples=false', '-Ddocumentation=enabled'])
        run(['meson', 'compile', '-C', str(build), 'docs'])
        documentation.append(build / 'doc/quickjs.html')
        if shutil.which('texi2pdf'):
            run(['meson', 'compile', '-C', str(build), 'docs-pdf'])
            documentation.append(build / 'doc/quickjs.pdf')
    with tarfile.open(str(output / (prefix + '.tar.xz')), 'w:xz') as archive:
        for name in sorted(os.fsdecode(value) for value in files.split(b'\0') if value):
            path = SOURCE / name
            if path.is_file() or path.is_symlink():
                archive.add(str(path), arcname=prefix + '/' + name, recursive=False)
        for path in documentation:
            archive.add(str(path), arcname=prefix + '/doc/' + path.name)
        revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=str(SOURCE))
        if subprocess.call(['git', 'diff', '--quiet', 'HEAD'], cwd=str(SOURCE)):
            revision += b'Working tree contains changes.\n'
        info = tarfile.TarInfo(prefix + '/SOURCE_REVISION')
        info.size = len(revision)
        archive.addfile(info, io.BytesIO(revision))


def binary_release(output, build_root, version, name, compiler=None, cross=None, lto=True):
    build = build_root / name
    env = dict(os.environ)
    if compiler:
        env['CC'] = compiler
    command = ['meson', 'setup', str(build), str(SOURCE), '-Dtests=false',
               '-Dexamples=false', '-Ddefault_library=static',
               '-Db_lto=' + ('true' if lto else 'false')]
    if cross:
        command += ['--cross-file', str(SOURCE / 'build-aux/cross' / cross)]
    run(command, env)
    run(['meson', 'compile', '-C', str(build), 'qjs', 'run-test262'], env)
    suffix = '.exe' if cross else ''
    archive_name = 'quickjs-' + name + '-' + version + '.zip'
    with zipfile.ZipFile(str(output / archive_name), 'w', zipfile.ZIP_DEFLATED) as archive:
        for target in ['qjs', 'run-test262']:
            archive.write(str(build / (target + suffix)), target + suffix)
        if cross:
            # The static engine still uses the platform's pthread runtime.
            cc = 'x86_64-w64-mingw32-gcc' if cross == 'mingw64.ini' else 'i686-w64-mingw32-gcc'
            runtime = subprocess.check_output([cc, '-print-file-name=libwinpthread-1.dll']).decode().strip()
            path = Path(runtime)
            if not path.is_file():
                triple = cc[:-4]
                candidates = [Path('/usr') / triple / 'bin/libwinpthread-1.dll',
                              Path('/usr') / triple / 'lib/libwinpthread-1.dll',
                              Path('/usr') / triple / 'sys-root/mingw/bin/libwinpthread-1.dll']
                path = next((candidate for candidate in candidates if candidate.is_file()), path)
            if not path.is_file():
                raise RuntimeError('Cannot locate libwinpthread-1.dll for ' + cc)
            archive.write(str(path), path.name)


def extras_release(output, version, unicode):
    if not unicode.is_dir():
        raise RuntimeError('The extras release requires --unicode-data with the generator inputs')
    prefix = 'quickjs-' + version
    with tarfile.open(str(output / ('quickjs-extras-' + version + '.tar.xz')), 'w:xz') as archive:
        archive.add(str(unicode), arcname=prefix + '/unicode')
        for name in ['bench-v8', 'octane']:
            archive.add(str(SOURCE / 'tests' / name), arcname=prefix + '/tests/' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('releases', nargs='*', help='quickjs (default), extras, binary, win_binary, cosmo_binary')
    parser.add_argument('--output', type=Path, default=Path.cwd() / 'releases')
    parser.add_argument('--unicode-data', type=Path, default=SOURCE / 'unicode')
    args = parser.parse_args()
    releases = [name for value in (args.releases or ['quickjs']) for name in value.split()]
    unknown = set(releases) - {'quickjs', 'extras', 'binary', 'win_binary', 'cosmo_binary'}
    if unknown:
        parser.error('Unknown release: ' + ', '.join(sorted(unknown)))
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    version = (SOURCE / 'VERSION').read_text().strip()
    with tempfile.TemporaryDirectory(prefix='.quickjs-build-', dir=str(output)) as directory:
        build = Path(directory)
        for release in releases:
            if release == 'quickjs':
                source_release(output, build, version)
            elif release == 'extras':
                extras_release(output, version, args.unicode_data.resolve())
            elif release == 'binary':
                binary_release(output, build, version, 'linux-x86_64')
                binary_release(output, build, version, 'linux-i686', compiler='gcc -m32')
            elif release == 'win_binary':
                binary_release(output, build, version, 'win-x86_64', cross='mingw64.ini')
                binary_release(output, build, version, 'win-i686', cross='mingw32.ini')
            else:
                env_ar = os.environ.get('AR')
                os.environ['AR'] = 'cosmoar'
                try:
                    binary_release(output, build, version, 'cosmo', compiler='cosmocc', lto=False)
                finally:
                    if env_ar is None:
                        del os.environ['AR']
                    else:
                        os.environ['AR'] = env_ar
    print('Releases written to ' + str(output))


if __name__ == '__main__':
    main()
