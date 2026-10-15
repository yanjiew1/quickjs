"""Collect current raw Test262 results without historical profile authority.

This collector never converts failures into passing tests. Its exit status is
the runner's status; reports retain failures and each original variant timing.
The sealed native-prefix qualification remains an independent historical tool.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/ci/native-prefix'))
from execution_threads import available_cpus
from native_test262_diagnostic import reconcile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def execution_identity():
    names = ('GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT', 'GITHUB_JOB',
             'GITHUB_WORKFLOW_REF', 'QJS_TEST262_COMPILER',
             'QJS_TEST262_SANITIZER', 'QJS_TEST262_BACKEND')
    values = {name: os.environ.get(name, '') for name in names}
    if os.environ.get('GITHUB_ACTIONS') == 'true' and not all(values.values()):
        raise ValueError('Current Test262 requires a complete CI execution identity')
    return values


def configuration(path=None):
    if path is not None:
        return path.read_text()
    # Re-run both historical data oracles as ordinary sources.
    oracle = json.loads((ROOT / 'tests/intl-date-time-oracle-policy.json').read_text())
    return ''.join(line for line in (ROOT / 'test262-intl.conf').read_text().splitlines(True)
                   if line.strip() not in oracle['fixture_sha256'])


def identity():
    paths = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).split(b'\0')
    sources = {os.fsdecode(p): digest((ROOT / os.fsdecode(p)).read_bytes())
               for p in paths if p and (ROOT / os.fsdecode(p)).is_file()}
    fixture = {p.relative_to(ROOT).as_posix(): digest(p.read_bytes())
               for p in (ROOT / 'test262').rglob('*') if p.is_file()
               and '.git' not in p.relative_to(ROOT / 'test262').parts}
    return {'checkout': subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                cwd=ROOT, text=True).strip(),
            'sources': sources, 'fixture': fixture,
            'qjs': digest((ROOT / 'qjs').read_bytes()),
            'runner': digest((ROOT / 'run-test262').read_bytes()),
            'execution': execution_identity(),
            'fixture_revision': subprocess.check_output(
                ['git', '-C', str(ROOT / 'test262'), 'rev-parse', 'HEAD'],
                cwd=ROOT, text=True).strip()}


def metadata_list(source, name):
    frontmatter = re.search(r'/\*---(.*?)---\*/', source, re.S)
    if not frontmatter:
        return set()
    lines = frontmatter.group(1).splitlines()
    for index, line in enumerate(lines):
        if not line.startswith(name + ':'):
            continue
        text = line.split(':', 1)[1].split('#', 1)[0].strip()
        if text:
            if not (text.startswith('[') and text.endswith(']')):
                raise ValueError('Unsupported metadata list: ' + name)
            values = text[1:-1].split(',')
        else:
            values = []
            for next_line in lines[index + 1:]:
                if next_line.strip() and not next_line.startswith((' ', '\t')):
                    break
                value = next_line.strip().split('#', 1)[0].strip()
                if not value:
                    continue
                if not value.startswith('- '):
                    raise ValueError('Unsupported metadata list entry: ' + name)
                values.append(value[2:])
        return {value.strip().strip('"\'') for value in values if value.strip()}
    return set()


def census(config, sources=None):
    """Selection is established independently of runner logs and timings."""
    features, disabled, exclusions = set(), set(), set()
    section = None
    for raw in config.splitlines():
        line = raw.strip()
        if not line or line.startswith(('#', ';')):
            continue
        if line.startswith('['):
            section = line
        elif section == '[features]':
            key, sep, value = line.partition('=')
            (features if not sep or value == 'yes' else disabled).add(key)
        elif section == '[exclude]':
            exclusions.add(line)
    if sources is None:
        paths = {p.relative_to(ROOT).as_posix() for p in (ROOT / 'test262/test').rglob('*.js')
                 if not p.name.endswith('_FIXTURE.js')}
        exclusions.update(line.strip() for line in (ROOT / 'test262-slow.txt').read_text().splitlines()
                          if line.strip() and not line.lstrip().startswith('#'))
    else:
        paths = set(sources)
        if len(paths) != len(sources):
            raise ValueError('Duplicate selected sources')
    expected, omitted = {}, set()
    for path in sorted(paths):
        if any(path == item or path.startswith(item.rstrip('/') + '/') for item in exclusions):
            omitted.add(path)
            continue
        source = (ROOT / path).read_text()
        tags = metadata_list(source, 'features')
        if tags - features - disabled:
            raise ValueError('Unknown features in ' + path + ': ' + repr(tags - features - disabled))
        flags = metadata_list(source, 'flags')
        if tags & disabled:
            modes = {'none'}
        elif 'module' in flags:
            modes = {'module'}
        elif flags & {'noStrict', 'raw'}:
            modes = {'non-strict'}
        elif 'onlyStrict' in flags:
            modes = {'strict'}
        else:
            modes = {'non-strict', 'strict'}
        expected[path] = sorted(modes)
    return expected, sorted(omitted)


def validate(stdout, stderr, returncode, rows, expected, excluded):
    allowed = {('pass', 'non-strict'), ('pass', 'strict'), ('pass', 'module'),
               ('fail', 'non-strict'), ('fail', 'strict'), ('fail', 'module'),
               ('skipped', 'none')}
    if any((r['status'], r['mode']) not in allowed for r in rows):
        raise ValueError('Invalid timing status/mode')
    if len({(r['file'], r['mode']) for r in rows}) != len(rows):
        raise ValueError('Duplicate source variants')
    by_source = {}
    for row in rows:
        by_source.setdefault(row['file'], {})[row['mode']] = row['status']
    if set(by_source) != set(expected):
        raise ValueError('Timing sources differ from independent selection')
    for path, modes in expected.items():
        actual = by_source[path]
        required = set(modes)
        if actual.get('non-strict') == 'fail' and 'non-strict' in required:
            required.discard('strict')  # Runner stops this source after its first failure.
        if set(actual) != required or ('none' in actual and actual['none'] != 'skipped'):
            raise ValueError('Missing, extra or skipped variant for ' + path)
    executed = {r['file'] for r in rows if r['status'] != 'skipped'}
    # Result supplies excluded cases, which have no variant timing records.
    from native_test262_diagnostic import parse_result
    counts, errors, unused = parse_result(stdout, stderr, 1000000)
    if errors:
        raise ValueError('; '.join(errors))
    if counts['excluded'] != len(excluded):
        raise ValueError('Exclusions differ from independent selection')
    denominator = len(executed) + counts['skipped'] + counts['excluded']
    parsed = reconcile(stdout, stderr, returncode, [], executed, denominator)
    if not parsed['raw_parser_complete']:
        raise ValueError('Incomplete raw collection: ' +
                         repr(parsed['reconciliation_errors'] or parsed['blocking_tool_records']))
    if len(executed) != counts['executed_source_count'] or \
            len({r['file'] for r in rows if r['status'] == 'skipped'}) != counts['skipped']:
        raise ValueError('Timing source selection disagrees with runner counters')
    failed = {(r['file'], r['mode'] == 'strict') for r in rows if r['status'] == 'fail'}
    observed = {(r['path'], r['strict']) for r in parsed['raw_observations']
                if r['status'] == 'observed-failure'}
    if failed != observed:
        raise ValueError('Timing failures disagree with raw diagnostics')
    parsed['executed_sources'] = sorted(executed)
    parsed['skipped_sources'] = sorted({r['file'] for r in rows if r['status'] == 'skipped'})
    parsed['excluded_sources'] = excluded
    return parsed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'ci-current-test262')
    parser.add_argument('--sources', nargs='*')
    parser.add_argument('--config', type=Path, help='Explicit feature-derived configuration')
    args = parser.parse_args()
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        parser.error('Test262 execution is delegated to GitHub Actions')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    before = identity()
    (out / 'identity-before.json').write_text(json.dumps(before, indent=2) + '\n')
    baseline = out / 'empty-errors.txt'
    baseline.write_text('')
    # Config paths are relative to the config file, not the process cwd.
    config = ROOT / 'current-test262.conf'
    config.write_text(configuration(args.config))
    config_bytes = config.read_bytes()
    expected, excluded = census(config_bytes.decode(), args.sources)
    (out / 'selection.json').write_text(json.dumps({'expected_modes': expected,
        'excluded_sources': excluded, 'config': config_bytes.decode()}, indent=2) + '\n')
    command = ['./run-test262', '-c', config.name, '-e', str(baseline), '-q',
               '-a', '--count_skipped_features', '-r', 'none', '-T', str(available_cpus()),
               '--timing-log', str(out / 'timings.jsonl')]
    selection = ROOT / 'current-test262-selection.txt'
    if args.sources:
        empty = out / 'empty-selection'
        empty.mkdir(exist_ok=True)
        selection.write_text(''.join(p + '\n' for p in args.sources))
        command += ['-d', str(empty), selection.name]
    else:
        command += ['-d', 'test262/test', '-x', 'test262-slow.txt']
    started = time.monotonic()
    try:
        with (out / 'stdout.log').open('w') as stdout, (out / 'stderr.log').open('w') as stderr:
            result = subprocess.run(command, cwd=ROOT, stdout=stdout, stderr=stderr, timeout=14400)
    finally:
        config.unlink()
        if args.sources:
            selection.unlink()
    rows = [json.loads(line) for line in (out / 'timings.jsonl').read_text().splitlines()]
    parsed = validate((out / 'stdout.log').read_text(), (out / 'stderr.log').read_text(),
                      result.returncode, rows, expected, excluded)
    after = identity()
    if after != before:
        raise ValueError('Source, fixture or engine changed during execution')
    parsed.update(command=command, wall_seconds=time.monotonic() - started,
                  lifecycle_closed=True, identities_unchanged=True,
                  conformance_accepted=result.returncode == 0)
    (out / 'summary.json').write_text(json.dumps(parsed, indent=2) + '\n')
    print(json.dumps(parsed['counters'], indent=2))
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())
