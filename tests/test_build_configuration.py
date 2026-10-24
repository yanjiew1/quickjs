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

"""Check selected test capabilities and the unit/integration CI partition."""
import argparse
import json
from pathlib import Path
import shlex


def load(build, name):
    return json.loads((build / name).read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('build', type=Path)
    args = parser.parse_args()
    source, build = args.source.resolve(), args.build.resolve()
    options = {row['name']: row['value'] for row in
               load(build, 'meson-info/intro-buildoptions.json')}
    # Installed static bitcode must carry the same final-link mode as qjsc.
    # Shared consumers need neither the plugin nor an LTO implementation detail.
    fields = dict(line.split(':', 1) for line in
                  (build / 'meson-private/quickjs.pc').read_text().splitlines()
                  if line.startswith(('Libs:', 'Libs.private:')))
    public_flags = shlex.split(fields.get('Libs', ''))
    private_flags = shlex.split(fields.get('Libs.private', ''))
    lto_flag = next(shlex.split(line)[2] for line in
                    (build / 'qjsc-config.h').read_text().splitlines()
                    if line.startswith('#define CONFIG_LTO_FLAG '))
    expected_lto = [lto_flag] if options['b_lto'] and options['default_library'] != 'shared' else []
    assert not any(flag.startswith('-flto') for flag in public_flags), public_flags
    actual_lto = [flag for flag in private_flags if flag.startswith('-flto')]
    assert actual_lto == expected_lto, (actual_lto, expected_lto)

    tests = load(build, 'meson-info/intro-tests.json')
    targets = load(build, 'meson-info/intro-targets.json')
    names = {row['name'] for row in tests}
    for row in tests:
        suites = {name.rsplit(':', 1)[-1] for name in row['suite']}
        if 'repo' in suites:
            assert len(suites & {'unit', 'integration'}) == 1, row['name']

    intl = options.get('intl', 'disabled')
    temporal = options.get('temporal', False)
    calendars = temporal or intl != 'disabled'
    prepared = {row['name'] for row in targets
                if row['name'].startswith('intl-fixture-data-')}
    expected = {'intl-fixture-data-' + intl} if intl != 'disabled' else set()
    assert prepared == expected, (prepared, expected)
    if 'intl' in options and intl == 'disabled':
        # The public disabled-API contract remains tested; real components and
        # no-op feature OOM executables must not create extra configurations.
        assert not {name for name in names if name.startswith('intl-')}, names
        assert not {name for name in names if name.startswith('test_intl_')
                    and name not in ('test_intl_api', 'test_intl_defaults')}, names
    if 'temporal' in options and not temporal:
        assert not {name for name in names if name.startswith('test_temporal_')}, names

    providers = set()
    commands = load(build, 'compile_commands.json')
    checked = False
    for row in commands:
        words = row.get('arguments') or shlex.split(row['command'])
        checked |= '-DCONFIG_CHECK_JSVALUE' in words
        if '-o' not in words:
            continue
        # The four provider paths distinguish selected backends regardless of
        # whether their owners live beside the engine or the test drivers.
        filename = (Path(row['directory']) / row['file']).resolve()
        try:
            relative = filename.relative_to(source).as_posix()
        except ValueError:
            continue
        if relative in {
            'src/calendar/calendar-minimal.c',
            'src/calendar/calendar-native.c',
            'src/timezone/timezone-minimal.c',
            'src/timezone/timezone-native.c',
        }:
            providers.add(relative)
    # Foundations can precede the option that selects their runtime provider.
    # Assert exclusivity only once that option actually exists in this snapshot.
    expected = set()
    controlled = set()
    for option, family in [('calendars', 'calendar'), ('timezone_backend', 'timezone')]:
        if option not in options:
            continue
        controlled.update('src/' + family + '/' + family + '-' + name + '.c'
                          for name in ('minimal', 'native'))
        if calendars:
            selected = options[option]
            if option == 'calendars' and selected == 'iso':
                selected = 'minimal'
            expected.add('src/' + family + '/' + family + '-' + selected + '.c')
    assert providers & controlled == expected, (providers & controlled, expected)
    assert checked == options['check_jsvalue'], (checked, options['check_jsvalue'])
    print('Selected configuration and repository suite partition verified.')


if __name__ == '__main__':
    main()
