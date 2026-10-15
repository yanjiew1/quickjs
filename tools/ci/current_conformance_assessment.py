"""Assess exact reviewed assertion conflicts while retaining original raw failures.

Original Test262 sources are never modified or excluded. Companions preserve
valid assertions and run in every applicable original mode. Unknown failures,
changed inputs and incomplete or failing companions remain blocking.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    return json.loads(path.read_text())


def transform(source, recipe):
    require(sha(source) == recipe['source_sha256'], 'Original source hash changed')
    text = source.decode('utf-8')
    for change in recipe['changes']:
        require(change['before'] and change['before'] != change['after'] and
                isinstance(change['count'], int) and change['count'] > 0, 'Empty transformation')
        require(text.count(change['before']) == change['count'], 'Transformation match changed')
        text = text.replace(change['before'], change['after'])
    output = text.encode('utf-8')
    require(sha(output) == recipe['generated_sha256'], 'Companion output hash changed')
    return output


def check_raw(summary, timings, selection):
    require(summary['raw_parser_complete'] and summary['lifecycle_closed'] and
            summary['identities_unchanged'], 'Original collection is incomplete')
    require(not summary['reconciliation_errors'] and not summary['blocking_tool_records'],
            'Original collection has diagnostics')
    require(not summary['failure_waiver'] and not summary['baseline_matched_inferred_paths'],
            'Original results used an error baseline')
    failures = [row for row in summary['raw_observations'] if row['status'] == 'observed-failure']
    paths = {row['path'] for row in failures}
    require(paths == set(summary['observed_failure_paths']) and
            len(paths) == summary['counters']['failed_source_count'], 'Failure census changed')
    require(summary['raw_runner_returncode'] == (1 if paths else 0), 'Unexpected raw runner exit')
    actual = {(row['file'], row['mode']) for row in timings if row['status'] == 'fail'}
    expected = {(row['path'], 'module' if selection['expected_modes'][row['path']] == ['module']
                 else 'strict' if row['strict'] else 'non-strict') for row in failures}
    require(actual == expected, 'Original failure timings changed')
    require(set(selection['expected_modes']) == {row['file'] for row in timings},
            'Original timing coverage changed')
    return failures


def classify(failures, recipes):
    by_source = {recipe['source']: recipe for recipe in recipes}
    require(len(by_source) == len(recipes), 'Duplicate recipe source')
    justified, unresolved = [], []
    for failure in failures:
        recipe = by_source.get(failure['path'])
        observed = {key: failure[key] for key in ('line', 'signature', 'strict')}
        if recipe is not None and observed in recipe['observed_conflicts']:
            justified.append({'source': failure['path'], **observed,
                              'reason': recipe['reason'], 'spec': recipe['spec']})
        else:
            unresolved.append(failure)
    return justified, unresolved


def check_companions(summary, rows, modes):
    require(summary['raw_parser_complete'] and summary['lifecycle_closed'] and
            summary['identities_unchanged'], 'Companion collection is incomplete')
    require(summary['raw_runner_returncode'] == 0 and not summary['observed_failure_paths'] and
            not summary['reconciliation_errors'] and not summary['blocking_tool_records'],
            'A companion failed or produced diagnostics')
    require(all(row['status'] == 'pass' for row in rows), 'A companion failed or was skipped')
    expected = {(path, mode) for path, variants in modes.items() for mode in variants}
    actual = {(row['file'], row['mode']) for row in rows}
    require(len(actual) == len(rows) and actual == expected,
            'Every applicable companion mode must run exactly once')
    require(summary['counters']['executed_source_count'] == len(modes) and
            not summary['counters']['failed_source_count'] and not summary['counters']['skipped'],
            'Companion source census changed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--raw', type=Path, default=ROOT / 'ci-current-test262')
    parser.add_argument('--output', type=Path, default=ROOT / 'ci-current-conformance')
    parser.add_argument('--ledger', type=Path, default=ROOT / 'tests/test262-assertion-conflicts.json')
    args = parser.parse_args()
    require(os.environ.get('GITHUB_ACTIONS') == 'true', 'Test262 execution belongs in GitHub Actions')
    sys.path.insert(0, str(ROOT / 'tools/ci'))
    from current_test262 import census, configuration, identity, metadata_list, validate
    out = args.output.resolve()
    require(not out.exists(), 'Assessment output must be fresh')
    out.mkdir(parents=True)
    report = {'assessment_accepted': False}
    try:
        raw = args.raw.resolve()
        ledger = read_json(args.ledger)
        require(ledger['schema'] == 1, 'Unknown assertion ledger schema')
        before = identity()
        require(before['execution']['QJS_TEST262_BACKEND'] == ledger['backend'],
                'Assessment data belongs to a different backend')
        require(before['fixture_revision'] == ledger['fixture_revision'],
                'Test262 checkout differs from reviewed fixture revision')
        require(before == read_json(raw / 'identity-before.json'),
                'Raw results belong to a different source, fixture or executable')
        raw_summary = read_json(raw / 'summary.json')
        raw_rows = [json.loads(line) for line in (raw / 'timings.jsonl').read_text().splitlines()]
        selection = read_json(raw / 'selection.json')
        config = configuration()
        full_modes, full_excluded = census(config)
        require(selection == {'expected_modes': full_modes, 'excluded_sources': full_excluded,
                              'config': config}, 'Raw run omitted or changed configured sources')
        reparsed = validate((raw / 'stdout.log').read_text(), (raw / 'stderr.log').read_text(),
                            raw_summary['raw_runner_returncode'], raw_rows, full_modes, full_excluded)
        reparsed['conformance_accepted'] = raw_summary['raw_runner_returncode'] == 0
        require(all(raw_summary.get(key) == value for key, value in reparsed.items()),
                'Raw summary differs from its actual logs or timings')
        failures = check_raw(raw_summary, raw_rows, selection)
        for filename, expected in ledger['data_sha256'].items():
            require(sha((ROOT / filename).read_bytes()) == expected, 'Data identity changed: ' + filename)
        provenance = read_json(ROOT / 'src/intl/data/locale-metadata-provenance.json')
        require(provenance['input_digest_sha256'] == ledger['input_digest_sha256'],
                'Actual data input digest differs from reviewed data')
        justified, unresolved = classify(failures, ledger['recipes'])
        report.update(original_raw_failed_sources=raw_summary['counters']['failed_source_count'],
                      original_raw_failed_variants=len(failures),
                      individually_justified_conflicts=justified, unresolved_failures=unresolved,
                      original_raw_conformance_accepted=raw_summary['conformance_accepted'])
        inputs = {'ledger': sha(args.ledger.read_bytes()), 'raw': {}, 'companions': {},
                  'engine': before['qjs'], 'runner': before['runner'], 'checkout': before['checkout'],
                  'workflow_run': os.environ.get('GITHUB_RUN_ID'),
                  'workflow_attempt': os.environ.get('GITHUB_RUN_ATTEMPT')}
        for filename in ['summary.json', 'timings.jsonl', 'selection.json', 'identity-before.json',
                         'stdout.log', 'stderr.log']:
            inputs['raw'][filename] = sha((raw / filename).read_bytes())
        modes = {}
        for recipe in ledger['recipes']:
            source = recipe['source']
            require(source.startswith('test262/test/') and '..' not in Path(source).parts,
                    'Invalid original source path')
            original = (ROOT / source).read_bytes()
            generated = transform(original, recipe)
            require(metadata_list(original.decode(), 'flags') == metadata_list(generated.decode(), 'flags') and
                    metadata_list(original.decode(), 'includes') == metadata_list(generated.decode(), 'includes') and
                    metadata_list(original.decode(), 'features') == metadata_list(generated.decode(), 'features'),
                    'Companion changed applicable metadata')
            target = out / 'companions' / source.removeprefix('test262/test/')
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(generated)
            relative = target.relative_to(ROOT).as_posix()
            require(relative not in modes, 'Duplicate companion output')
            modes[relative] = selection['expected_modes'][source]
            require('none' not in modes[relative], 'A required companion is disabled')
            inputs['companions'][relative] = sha(generated)
        require(modes, 'No companion coverage requested')
        (out / 'inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')
        command = [sys.executable, str(ROOT / 'tools/ci/current_test262.py'),
                   '--output', str(out / 'companion-results'), '--sources', *modes]
        (out / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
        with (out / 'collector.stdout.log').open('w') as stdout, (out / 'collector.stderr.log').open('w') as stderr:
            result = subprocess.run(command, cwd=ROOT, stdout=stdout, stderr=stderr)
        require(identity() == before, 'Source, fixture or executable changed during companions')
        require(sha(args.ledger.read_bytes()) == inputs['ledger'], 'Assertion ledger changed')
        for filename, expected in inputs['raw'].items():
            require(sha((raw / filename).read_bytes()) == expected, 'Original evidence changed')
        for filename, expected in inputs['companions'].items():
            require(sha((ROOT / filename).read_bytes()) == expected, 'Companion changed during execution')
        require(result.returncode == 0, 'Companion execution failed; inspect companion logs')
        companion = out / 'companion-results'
        require(read_json(companion / 'identity-before.json') == before,
                'Companions used a different source, fixture or executable')
        summary = read_json(companion / 'summary.json')
        rows = [json.loads(line) for line in (companion / 'timings.jsonl').read_text().splitlines()]
        reparsed = validate((companion / 'stdout.log').read_text(),
                            (companion / 'stderr.log').read_text(),
                            summary['raw_runner_returncode'], rows, modes, [])
        reparsed['conformance_accepted'] = summary['raw_runner_returncode'] == 0
        require(all(summary.get(key) == value for key, value in reparsed.items()),
                'Companion summary differs from its actual logs or timings')
        check_companions(summary, rows, modes)
        report.update(companion_sources=len(modes), companion_variants=len(rows),
                      assessment_accepted=not unresolved)
    except (ValueError, KeyError, OSError, UnicodeError, subprocess.SubprocessError) as exc:
        report['blocking_error'] = str(exc)
    (out / 'summary.json').write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if report['assessment_accepted'] else 1


if __name__ == '__main__':
    sys.exit(main())
