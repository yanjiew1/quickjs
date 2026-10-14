"""Authenticate the user-requested eight-case full-suite DST exclusion."""
import copy
import hashlib
import json
from pathlib import Path
from core_baseline_transition import (CHECKPOINT_PATH, CHECKPOINT_SHA256,
                                      authenticate_transition)
from execution_threads import test262_workers

EXCLUSION_PATH = 'test262-slow.txt'
DST_PATHS = tuple('test262/test/staging/sm/Date/dst-offset-caching-{}-of-8.js'.format(n)
                  for n in range(1, 9))
_TOKEN = object()


def require(value, reason):
    if not value:
        raise ValueError(reason)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def canonical(value):
    return digest(json.dumps(value, sort_keys=True).encode())


class SlowTestSelection:
    def __init__(self, proof, baseline, token):
        require(token is _TOKEN, 'Unauthenticated slow selection')
        self._proof = copy.deepcopy(proof)
        self._baseline = baseline
        self._derived = None

    def receipt(self):
        return copy.deepcopy(self._proof)

    def check_context(self, context):
        require(all(context.get(key) == value for key, value in
                    self._proof['input_identity'].items()),
                'Slow selection source, fixture, config or engine changed')

    def derive_policy(self, policy, context):
        self.check_context(context)
        require(self._derived is None, 'Slow selection policy already derived')
        expected = dict(changed=0, excluded=19, executed_source_count=52742,
                        failed_source_count=123, fixed=0, new=109, skipped=856)
        if self._baseline is not None:
            expected['failed_source_count'] -= self._baseline.receipt()['removed_source_count']
        require(policy.get('postfix_raw_counters') == expected and
                policy.get('postfix_raw_returncode') == 1 and
                policy.get('no_new_skips_exclusions_expected_errors') is True and
                'authorized_slow_exclusions' not in policy,
                'Historical full-suite selection policy changed')
        result = copy.deepcopy(policy)
        result['postfix_raw_counters']['executed_source_count'] -= len(DST_PATHS)
        result['postfix_raw_counters']['excluded'] += len(DST_PATHS)
        result['no_new_skips_exclusions_expected_errors'] = False
        result['authorized_slow_exclusions'] = self.receipt()
        self._derived = copy.deepcopy(result)
        return result

    def check_policy(self, policy, context):
        self.check_context(context)
        require(self._derived is not None and policy == self._derived,
                'Policy differs from the exact authorized slow selection')


def authenticate_slow_selection(sources, load_bytes, context, command_bytes,
                                parsed, protected_paths, diagnostics):
    """Bind an exact closed full run; this never changes witness selections."""
    raw = load_bytes(EXCLUSION_PATH)
    require(raw == ('\n'.join(DST_PATHS) + '\n').encode() and
            sources.get(EXCLUSION_PATH) == digest(raw),
            'Slow exclusion file differs from the exact eight approved paths')
    checkpoint_bytes = load_bytes(CHECKPOINT_PATH)
    require(sources.get(CHECKPOINT_PATH) == digest(checkpoint_bytes) == CHECKPOINT_SHA256,
            'Historical full command changed')
    protected = set(protected_paths)
    manifest_path = 'tools/ci/native-prefix/selections.json'
    manifest_raw = load_bytes(manifest_path)
    require(sources.get(manifest_path) == digest(manifest_raw),
            'Required selection manifest changed')
    selections = json.loads(manifest_raw)
    require([item['distinct_source_count'] for item in selections] == [18, 2, 114],
            'Required selection census changed')
    for item in selections:
        name = 'tools/ci/native-prefix/' + item['path']
        require('..' not in name.split('/') and not item['path'].startswith('/'),
                'Unsafe required selection path')
        selected_raw = load_bytes(name)
        require(sources.get(name) == item['sha256'] == digest(selected_raw),
                'Required selection source changed')
        names = selected_raw.decode().splitlines()
        require(len(names) == len(set(names)) == item['distinct_source_count'],
                'Required selection cardinality changed')
        protected.update('test262/' + name for name in names)
    require(not set(DST_PATHS).intersection(protected),
            'Slow exclusion overlaps required or witness sources')
    require(isinstance(parsed.get('input_identity'), dict) and
            {'production_source_map_sha256', 'fixture_source_map_sha256',
             'config_sha256', 'baseline_sha256', 'qjs_sha256'} <=
            parsed['input_identity'].keys() and
            canonical(sources) == context['production_source_map_sha256'] and
            parsed.get('input_identity') == {key: context[key] for key in parsed['input_identity']} and
            parsed.get('raw_parser_complete') is True and
            parsed.get('raw_collection_complete') is True and
            parsed.get('sealed_inputs_unchanged') is True,
            'Slow selection lacks complete authenticated raw input')
    command = json.loads(command_bytes)
    expected = test262_workers(json.loads(checkpoint_bytes)['argv']) + [
        '--progress-log', str(Path(diagnostics) / 'test262-execution.jsonl'),
        '--timing-log', str(Path(diagnostics) / 'test262-timings.jsonl'),
        '-x', EXCLUSION_PATH]
    require(parsed.get('command_sha256') == digest(command_bytes) and
            command.get('argv') == expected and
            command.get('returncode') == parsed.get('raw_runner_returncode') and
            command.get('returncode') in (0, 1) and
            command.get('lifecycle_closed') is True and
            command.get('timed_out') is False and command.get('interrupted') is False,
            'Missing or altered closed full-suite exclusion command')
    baseline = authenticate_transition(sources, load_bytes, context)
    proof = dict(schema='native-user-directed-slow-selection-v1',
                 reason='User requested restoring slow DST stress exclusions on 2026-10-14',
                 exclusions=list(DST_PATHS), exclusion_file=EXCLUSION_PATH,
                 exclusion_file_sha256=digest(raw), command_sha256=digest(command_bytes),
                 input_identity=copy.deepcopy(parsed['input_identity']),
                 protected_source_count=len(protected),
                 protected_sources_sha256=canonical(sorted(protected)),
                 full_suite_only=True, required_and_witness_selections_unchanged=True,
                 conformance_accepted=False, failure_waiver=False)
    return SlowTestSelection(proof, baseline, _TOKEN)
