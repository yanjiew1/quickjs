"""Collect raw correction evidence without changing historical qualification."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

from execution_threads import available_cpus
from native_test262_diagnostic import reconcile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUT = ROOT / 'ci-native-hour-width'
MANIFEST = HERE / 'hour-width-staging.json'
POLICY = HERE / 'native-profile-diagnostic-policy.json'


def require(value, message):
    if not value:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return sha(json.dumps(value, sort_keys=True, separators=(',', ':')).encode())


def identities(manifest):
    require(sha(POLICY.read_bytes()) == manifest['historical_policy_sha256'],
            'Historical diagnostic policy changed')
    subject = manifest['implementation_source']
    require(sha((ROOT / subject['path']).read_bytes()) == subject['current_sha256'],
            'Implementation differs from reviewed correction')
    for path, key in [('test262-intl.conf', 'config_sha256'),
                      ('test262-slow.txt', 'exclusions_sha256')]:
        require(sha((ROOT / path).read_bytes()) == manifest[key],
                'Configuration or exclusions changed')
    fixture = {str(p.relative_to(ROOT / 'test262')): sha(p.read_bytes())
               for p in (ROOT / 'test262').rglob('*')
               if p.is_file() and '.git' not in p.relative_to(ROOT / 'test262').parts}
    require(sha(json.dumps(fixture, sort_keys=True).encode()) ==
            manifest['fixture_source_map_sha256'], 'Test262 fixture changed')
    for entry in manifest['repaired_sources']:
        require(sha((ROOT / entry['path']).read_bytes()) == entry['fixture_sha256'],
                'Repaired fixture changed')
    return {'checkout': subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                cwd=ROOT, text=True).strip(), 'fixture': canonical(fixture),
            'subject': subject['current_sha256'], 'qjs': sha((ROOT / 'qjs').read_bytes()),
            'runner': sha((ROOT / 'run-test262').read_bytes()),
            'manifest': sha(MANIFEST.read_bytes())}


def expected_entries(policy, manifest):
    entries = policy['adjudicated_diagnostic_entries']
    repaired = {row['path']: row['fixture_sha256'] for row in manifest['repaired_sources']}
    require(len(repaired) == 6, 'Correction must name six distinct sources')
    found = {row['path']: row['fixture_sha256'] for row in entries if row['path'] in repaired}
    require(found == repaired, 'Correction is outside historical diagnostic scope')
    return [row for row in entries if row['path'] not in repaired]


def validate_full(stdout, stderr, returncode, rows, policy, manifest):
    require(all((row['status'], row['mode']) in {
        ('pass', 'non-strict'), ('pass', 'strict'), ('pass', 'module'),
        ('fail', 'non-strict'), ('fail', 'strict'), ('fail', 'module'),
        ('skipped', 'none')} for row in rows), 'Invalid timing status or mode')
    executed = sorted({row['file'] for row in rows if row['status'] in ('pass', 'fail')})
    require(len(executed) == manifest['executed_source_count'] and
            canonical(executed) == manifest['executed_sources_sha256'],
            'Complete executed source selection changed')
    require(len({(r['file'], r['mode']) for r in rows}) == len(rows),
            'Duplicate timing records')
    skipped = sorted(row['file'] for row in rows if row['status'] == 'skipped')
    require(len(skipped) == manifest['skipped'] and
            canonical(skipped) == manifest['skipped_sources_sha256'],
            'Skipped source selection changed')
    distinct = manifest['executed_source_count'] + manifest['excluded'] + manifest['skipped']
    parsed = reconcile(stdout, stderr, returncode, [], set(executed), distinct)
    require(parsed['raw_parser_complete'], 'Incomplete raw collection')
    expected = expected_entries(policy, manifest)
    observations = [x for x in parsed['raw_observations'] if x['status'] == 'observed-failure']
    key = lambda x: (x['path'], x['strict'], x['line'], x['signature'])
    require(sorted(map(key, observations)) == sorted(map(key, expected)),
            'Failure identities or signatures differ beyond six repairs')
    require({(row['file'], row['mode'] == 'strict') for row in rows
             if row['status'] == 'fail'} ==
            {(row['path'], row['strict']) for row in observations},
            'Timing failures do not match raw diagnostic variants')
    counts = dict(failed_source_count=len(expected),
                  executed_source_count=manifest['executed_source_count'],
                  excluded=manifest['excluded'], skipped=manifest['skipped'],
                  new=len(expected), changed=0, fixed=0)
    require(parsed['counters'] == counts and returncode == 1,
            'Raw counters or runner lifecycle differ')
    repaired = {x['path'] for x in manifest['repaired_sources']}
    for path in repaired:
        require({(x['mode'], x['status']) for x in rows if x['file'] == path} ==
                {('non-strict', 'pass'), ('strict', 'pass')},
                'Repaired source lacks both positive PASS observations')
    return counts


def validate_focused(stdout, stderr, returncode, rows, manifest):
    paths = {row['path'] for row in manifest['repaired_sources']}
    expected = {(path, mode, 'pass') for path in paths for mode in ('non-strict', 'strict')}
    require(len(rows) == 12 and
            {(row['file'], row['mode'], row['status']) for row in rows} == expected,
            'Focused original source variants did not all pass')
    parsed = reconcile(stdout, stderr, returncode, [], paths, 6)
    require(parsed['raw_parser_complete'] and returncode == 0 and
            parsed['counters'] == dict(failed_source_count=0, executed_source_count=6,
                excluded=0, skipped=0, new=0, changed=0, fixed=0),
            'Focused result or lifecycle does not match exact selection')


def selection_args(paths):
    if not paths:
        return ['-d', 'test262/test', '-x', 'test262-slow.txt']
    empty = OUT / 'empty-selection'
    empty.mkdir(exist_ok=True)
    # List entries are relative to the list's parent. A basename-only list
    # in the checkout preserves the same paths as the historical full run.
    selected = ROOT / 'native-hour-width-sources.txt'
    selected.write_text(''.join(path + '\n' for path in paths))
    # Directory-list mode reads a positional file as a source list, not JS.
    # -d suppresses configuration enumeration and keeps the selection exact.
    return ['-d', str(empty), selected.name]


def run(label, paths, manifest, before):
    empty = OUT / 'empty-errors.txt'
    empty.write_text('')
    argv = ['./run-test262', '-c', 'test262-intl.conf', '-e', str(empty),
            '-q', '-a', '--count_skipped_features', '-r', 'none',
            '-T', str(available_cpus()), '--timing-log',
            str(OUT / (label + '-timings.jsonl'))]
    argv += selection_args(paths)
    started = time.monotonic()
    with (OUT / (label + '.stdout.log')).open('w') as stdout, \
            (OUT / (label + '.stderr.log')).open('w') as stderr:
        completed = subprocess.run(argv, cwd=ROOT, stdout=stdout, stderr=stderr,
                                   timeout=14400)
    after = identities(manifest)
    require(after == before, 'Source, fixture or engine changed during execution')
    receipt = {'argv': argv, 'returncode': completed.returncode,
               'wall_seconds': time.monotonic() - started, 'input_identity': before,
               'lifecycle_closed': True, 'timed_out': False, 'interrupted': False}
    (OUT / (label + '-command.json')).write_text(json.dumps(receipt, indent=2) + '\n')
    rows = [json.loads(line) for line in
            (OUT / (label + '-timings.jsonl')).read_text().splitlines()]
    return completed.returncode, rows


def main():
    OUT.mkdir(exist_ok=True)
    manifest = json.loads(MANIFEST.read_bytes())
    require(manifest['schema'] == 'native-hour-width-staging-v1' and
            manifest['conformance_accepted'] is False and manifest['failure_waiver'] is False,
            'Invalid staging scope')
    before = identities(manifest)
    require(before['checkout'] == os.environ['GITHUB_SHA'], 'Unexpected CI checkout')
    policy = json.loads(POLICY.read_bytes())
    expected_entries(policy, manifest)
    status, rows = run('focused', [x['path'] for x in manifest['repaired_sources']],
                       manifest, before)
    validate_focused((OUT / 'focused.stdout.log').read_text(),
                     (OUT / 'focused.stderr.log').read_text(), status, rows, manifest)
    status, rows = run('full', [], manifest, before)
    counts = validate_full((OUT / 'full.stdout.log').read_text(),
        (OUT / 'full.stderr.log').read_text(), status, rows, policy, manifest)
    summary = {'schema': manifest['schema'], 'repaired_source_count': 6,
               'counts': counts, 'input_identity': before,
               'original_witness_qualification_pending': True,
               'conformance_accepted': False, 'failure_waiver': False}
    (OUT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
