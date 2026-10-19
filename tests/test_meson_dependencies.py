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

"""Check translation-unit identity and compile-only JSValue coverage."""
import argparse
import json
import re
from pathlib import Path
import shlex
import subprocess


def dependencies(build, item, output):
    report = subprocess.run(['ninja', '-C', str(build), '-t', 'deps', output],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True, timeout=20)
    if report.returncode == 0:
        return {(build / line.strip()).resolve() for line in report.stdout.splitlines()[1:]
                if line.startswith('    ')}
    if not re.search(r"unknown tool ['\"]deps['\"]", report.stderr):
        raise RuntimeError(report.stderr)
    # Samurai lacks Ninja's dependency-report tool. Check the compiler's actual
    # include closure without changing objects or files used by parallel tests.
    words = item.get('arguments') or shlex.split(item['command'])
    assert '-MD' in words or '-MMD' in words, words
    assert '-MF' in words, words
    command = []
    skip = False
    for word in words:
        if skip:
            skip = False
        elif word in ['-o', '-MF', '-MT', '-MQ']:
            skip = True
        elif word not in ['-c', '-MD', '-MMD']:
            command.append(word)
    text = subprocess.check_output(command + ['-M', '-MF', '-', '-MT', 'dependency-check'],
                                   cwd=item['directory'], universal_newlines=True,
                                   timeout=20)
    paths = shlex.split(text.replace('\\\n', '').split(':', 1)[1])
    return {(Path(item['directory']) / path).resolve() for path in paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('build', type=Path)
    args = parser.parse_args()
    source, build = args.source.resolve(), args.build.resolve()
    build_files = json.loads((build / 'meson-info/intro-buildsystem_files.json').read_text())
    assert str(source / 'include/quickjs.h') in build_files
    commands = json.loads((build / 'compile_commands.json').read_text())
    normal = {}
    compile_items = {}
    checked = set()
    for item in commands:
        filename = (Path(item['directory']) / item['file']).resolve()
        words = item.get('arguments') or shlex.split(item['command'])
        if '-o' in words:
            object_path = (Path(item['directory']) / words[words.index('-o') + 1]).resolve()
            assert build in object_path.parents, 'Object outside the build directory: ' + str(object_path)
        try:
            relative = filename.relative_to(source).as_posix()
        except ValueError:
            continue
        if '-DCONFIG_CHECK_JSVALUE' in words:
            checked.add(relative)
        if relative.startswith('src/quickjs/') and '-DCONFIG_CHECK_JSVALUE' not in words:
            if '-o' in words:
                normal.setdefault(relative, words[words.index('-o') + 1])
                compile_items.setdefault(relative, item)
    expected = {p.relative_to(source).as_posix() for directory in
                ['src/quickjs', 'src/quickjs-libc']
                for p in (source / directory).rglob('*.c')}
    options = {item['name']: item['value'] for item in
               json.loads((build / 'meson-info/intro-buildoptions.json').read_text())}
    if not options['temporal']:
        expected = {name for name in expected
                    if not name.startswith('src/quickjs/builtins/temporal/') or
                    name.endswith('/common.c')}
    if not options['temporal']:
        expected.discard('src/quickjs/builtins/duration-value.c')
    expected.update('tools/' + name + '.c' for name in ['qjs', 'qjsc', 'run-test262'])
    if options['check_jsvalue']:
        assert expected <= checked, 'Missing JSValue checks: ' + repr(sorted(expected - checked))
    else:
        assert not checked, 'JSValue diagnostic enabled without check_jsvalue'
    assert len(normal) == len(set(normal.values())), 'Different sources share an object output'
    assert 'src/quickjs/number.c' in normal and 'src/quickjs/builtins/number.c' in normal
    # The dependency scanner must retain private and public header ownership.
    for relative, required in [
            ('src/quickjs/allocator.c', ['include/quickjs.h', 'src/quickjs/internal/runtime.h']),
            ('src/quickjs/builtins/number.c', ['include/quickjs.h'])]:
        paths = dependencies(build, compile_items[relative], normal[relative])
        for name in required:
            header = (build / 'quickjs.h') if name == 'include/quickjs.h' else (source / name)
            assert header.resolve() in paths, sorted(map(str, paths))


if __name__ == '__main__':
    main()
