"""Authenticate a behavioral correction separately from source reconstruction.

Historical assertion packets and reviews remain immutable. Source approval
allows capture; qualification additionally requires independently reviewed
complete observation deltas and the exact subtraction of repaired failures.
"""
import copy
import hashlib
import json
from pathlib import Path

PREFIX = 'tools/ci/native-prefix/'
MANIFEST = PREFIX + 'hour-width-correction.json'
MODULE = PREFIX + 'native_correction_transition.py'
SUBJECT = 'src/intl/date-native.c'
POLICY = PREFIX + 'native-profile-diagnostic-policy.json'
SCOPE = PREFIX + 'native-normative-static-scope.json'
REVIEW = PREFIX + 'reviews/hour-width-correction-review.md'
REPAIRED_SOURCES = frozenset(
    'test262/test/intl402/Temporal/' + owner + '/prototype/toLocaleString/' + name
    for owner, name in (
        ('Instant', 'hourcycle.js'), ('PlainTime', 'hourcycle.js'),
        ('PlainDateTime', 'hourcycle.js'), ('ZonedDateTime', 'hourcycle.js'),
        ('PlainTime', 'resolved-time-zone.js'),
        ('PlainDateTime', 'resolved-time-zone.js')))
OBSERVATION_SOURCES = REPAIRED_SOURCES | frozenset(
    'test262/test/intl402/DateTimeFormat/prototype/resolvedOptions/' + name
    for name in ('hourCycle.js', 'hourCycle-default.js'))
_TOKEN = object()


def require(value, reason):
    if not value:
        raise ValueError(reason)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def canonical(value):
    return sha(json.dumps(value, sort_keys=True).encode())


def source_approval(files, root, load_bytes=None):
    if MANIFEST not in files:
        return None
    root = Path(root)
    read = load_bytes or (lambda path: (root / path).read_bytes())
    manifest = json.loads(read(MANIFEST))
    for path in (MANIFEST, MODULE, POLICY,
                 PREFIX + 'original-assertions/PREMISE-SOURCE-PINS.json'):
        require(files.get(path) == sha(read(path)),
                'Correction inputs differ from complete current source map')
    require(manifest.get('schema') == 'native-hour-width-semantic-correction-v1' and
            manifest.get('source_review_complete') is True and
            manifest.get('engine_algorithms_changed') is True and
            manifest.get('failure_waiver') is False and
            manifest.get('conformance_accepted') is False,
            'Unreviewed semantic correction source')
    subject = manifest['implementation_source']
    pins = json.loads(read(PREFIX + 'original-assertions/PREMISE-SOURCE-PINS.json'))
    require(subject['path'] == SUBJECT and
            subject['historical_sha256'] == pins[SUBJECT] and
            subject['current_sha256'] == files.get(SUBJECT) ==
            sha(read(SUBJECT)) and
            subject['historical_sha256'] != subject['current_sha256'],
            'Unapproved implementation correction')
    require(manifest['historical_policy_sha256'] == files[POLICY],
            'Historical diagnostic policy changed')
    policy = json.loads(read(POLICY))
    repaired = manifest['repaired_sources']
    require(isinstance(repaired, list) and len(repaired) == 6 and
            {row['path'] for row in repaired} == REPAIRED_SOURCES and
            all(set(row) == {'path', 'fixture_sha256'} for row in repaired),
            'Correction needs exactly six original fixture bindings')
    historical = {row['path']: row['fixture_sha256'] for row in
                  policy['adjudicated_diagnostic_entries']}
    require(all(historical.get(row['path']) == row['fixture_sha256']
                for row in repaired), 'Correction escapes historical diagnostic scope')
    require(type(manifest.get('semantic_review_complete')) is bool and
            (manifest['semantic_review_complete'] or
             manifest.get('reviewed_observation_changes') is None),
            'Pending capture cannot approve semantic observations')
    return dict(schema=manifest['schema'], manifest_sha256=files[MANIFEST],
        module_sha256=files[MODULE], subject=copy.deepcopy(subject),
        engine_algorithms_changed=True,
        semantic_review_complete=manifest['semantic_review_complete'],
        failure_waiver=False, conformance_accepted=False)


def pending_capture(bindings):
    correction = bindings.get('premise_source_compatibility', {}).get('algorithm_correction')
    return correction is not None and correction['semantic_review_complete'] is False


class NativeCorrectionTransition:
    def __init__(self, manifest, proof, token):
        require(token is _TOKEN, 'Correction requires authenticated reviewed sources')
        self._manifest = copy.deepcopy(manifest)
        self._proof = copy.deepcopy(proof)

    def receipt(self):
        return copy.deepcopy(self._proof)

    def removed_source_count(self):
        return len(self._manifest['repaired_sources'])

    def check_context(self, context):
        require(context.get('production_source_map_sha256') ==
                self._proof['source_map_sha256'], 'Correction source identity changed')

    def derive_observation(self, path, observation):
        result = copy.deepcopy(observation)
        change = self._manifest['reviewed_observation_changes'].get(path)
        if change is not None:
            require(result['observations'] == change['historical'],
                    'Historical correction observation differs from reviewed delta')
            result['observations'] = copy.deepcopy(change['corrected'])
        return result

    def derive_policy(self, policy):
        result = copy.deepcopy(policy)
        repaired = {row['path']: row['fixture_sha256'] for row in
                    self._manifest['repaired_sources']}
        entries = result['adjudicated_diagnostic_entries']
        require(len(entries) == 109 and
                {row['path']: row['fixture_sha256'] for row in entries
                 if row['path'] in repaired} == repaired and
                result['postfix_raw_counters'] == dict(changed=0, excluded=19,
                    executed_source_count=52742, failed_source_count=109,
                    fixed=0, new=109, skipped=856),
                'Correction is not the exact historical six-source subtraction')
        require(not any(row['path'] in repaired for item in
                        result['required_selections'].values()
                        for row in item['failure_identities']),
                'Correction cannot alter required source selections')
        result['adjudicated_diagnostic_entries'] = [row for row in entries
                                                  if row['path'] not in repaired]
        result['postfix_raw_counters']['failed_source_count'] -= len(repaired)
        result['postfix_raw_counters']['new'] -= len(repaired)
        result['semantic_correction'] = self.receipt()
        return result


def authenticate_correction(sources, load_bytes, fixture_hashes=None):
    if MANIFEST not in sources:
        return None
    proof = source_approval(sources, '.', load_bytes)
    manifest = json.loads(load_bytes(MANIFEST))
    require(proof['semantic_review_complete'] is True,
            'Source-only correction cannot qualify semantic observations')
    binding = manifest.get('independent_review_binding')
    require(isinstance(binding, dict) and set(binding) == {'path', 'sha256'} and
            binding['path'] == REVIEW and
            binding['sha256'] == sources.get(REVIEW) == sha(load_bytes(REVIEW)),
            'Correction independent review is not authenticated')
    require(sources.get(SCOPE) == sha(load_bytes(SCOPE)),
            'Correction historical observation scope is not authenticated')
    scope = json.loads(load_bytes(SCOPE))
    historical = scope['stable_observation_expectations']
    require(len(historical) == 118 and canonical(historical) ==
            manifest['historical_observations_sha256'],
            'Historical complete observation expectations changed')
    changes = manifest['reviewed_observation_changes']
    require(isinstance(changes, dict) and set(changes) == OBSERVATION_SOURCES and
            set(changes) <= set(historical), 'Unknown or missing corrected observations')
    for path, change in changes.items():
        require(set(change) == {'historical', 'corrected', 'review_basis'} and
                change['historical'] == historical[path]['observations'] and
                change['historical'] != change['corrected'] and
                set(change['corrected']) == {'strict', 'nonstrict'} and
                isinstance(change['review_basis'], str) and change['review_basis'].strip(),
                'Unreviewed or mismatched complete observation delta')
        for variant in ('strict', 'nonstrict'):
            old, new = change['historical'][variant], change['corrected'][variant]
            require(set(new) == set(old) == {'events_sha256', 'event_count',
                    'stable_configuration_sha256'} and new['event_count'] == old['event_count'],
                    'Correction cannot remove assertion events or change observation shape')
    if fixture_hashes is not None:
        require(all(fixture_hashes.get(row['path']) == row['fixture_sha256'] for row in
                    manifest['repaired_sources']), 'Corrected fixture bytes changed')
    proof.update(source_map_sha256=canonical(sources),
                 independent_review_binding=copy.deepcopy(binding),
                 repaired_sources=copy.deepcopy(manifest['repaired_sources']),
                 reviewed_observation_changes=copy.deepcopy(changes))
    return NativeCorrectionTransition(manifest, proof, _TOKEN)
