"""Authenticate removal of the reviewed assignment-reference baseline records.

Historical checkpoint and native assertion reviews stay unchanged. This narrow
transition changes neither exclusions nor native semantic expectations.
"""
import copy
import hashlib
import json

CHECKPOINT_PATH = 'tools/ci/native-prefix/integration-checkpoint.json'
CHECKPOINT_SHA256 = '0f1afeb1ce984a03bfdb331b755fa0985b888164112649d0136e83ac5c2a8e6d'
TRANSITION_PATH = 'tools/ci/native-prefix/core-baseline-transition.json'
BASELINE_PATH = 'test262_errors.txt'
GLOBAL_SOURCE = 'test262/test/language/identifier-resolution/assign-to-global-undefined.js'
_FIELDS = {'schema', 'mode', 'historical_checkpoint_sha256',
           'historical_baseline_sha256', 'current_baseline_sha256',
           'removed_records', 'remaining_source_count'}
_TOKEN = object()


def require(value, reason):
    if not value:
        raise ValueError(reason)


def digest(raw):
    require(type(raw) is bytes, 'Baseline evidence must contain bytes')
    return hashlib.sha256(raw).hexdigest()


def canonical(sources):
    return hashlib.sha256(json.dumps(sources, sort_keys=True).encode()).hexdigest()


class CoreBaselineTransition:
    def __init__(self, proof, token):
        require(token is _TOKEN, 'Baseline transition needs authenticated source')
        self._proof = copy.deepcopy(proof)
        self._token = token

    def __deepcopy__(self, memo):
        return self

    def receipt(self):
        require(self._token is _TOKEN, 'Unauthenticated baseline transition')
        return copy.deepcopy(self._proof)

    def check_context(self, context):
        require(self._token is _TOKEN and
                context['baseline_sha256'] == self._proof['current_baseline_sha256'] and
                context['production_source_map_sha256'] == self._proof['source_map_sha256'],
                'Current baseline transition source/context differs')

    def review_matches(self, baseline, context):
        self.check_context(context)
        return baseline == self._proof['historical_baseline_sha256']

    def derive_document(self, document, role, context):
        self.check_context(context)
        result = copy.deepcopy(document)
        if 'baseline_sha256' in result:
            require(result['baseline_sha256'] == self._proof['historical_baseline_sha256'],
                    'Historical normative baseline changed')
            result['baseline_sha256'] = self._proof['current_baseline_sha256']
        if role == 'diagnostic-policy':
            counts = result['postfix_raw_counters']
            require(counts == dict(changed=0, excluded=19,
                    executed_source_count=52742, failed_source_count=123,
                    fixed=0, new=109, skipped=856),
                    'Historical native counters changed')
            counts['failed_source_count'] -= self._proof['removed_source_count']
        return result


def authenticate_transition(sources, load_bytes, context=None):
    if TRANSITION_PATH not in sources:
        return None

    def authenticated(path):
        raw = load_bytes(path)
        require(sources.get(path) == digest(raw), 'Changed baseline transition source bytes')
        return raw

    checkpoint_raw = authenticated(CHECKPOINT_PATH)
    require(digest(checkpoint_raw) == CHECKPOINT_SHA256,
            'Historical checkpoint changed')
    checkpoint = json.loads(checkpoint_raw)
    historical = checkpoint['known_error_baseline_lines']
    require(len(historical) == 14 and len(set(historical)) == 14 and
            checkpoint['known_failing_source_count'] == 14,
            'Historical assignment baseline count changed')
    historical_raw = ('\n'.join(historical) + '\n').encode()
    require(digest(historical_raw) == checkpoint['known_error_baseline_sha256'],
            'Historical baseline bytes changed')
    transition_raw = authenticated(TRANSITION_PATH)
    transition = json.loads(transition_raw)
    require(type(transition) is dict and set(transition) == _FIELDS and
            transition['schema'] == 'assignment-core-baseline-transition-v1' and
            transition['mode'] in ('rhs-eval-13', 'assignment-reference-14') and
            transition['historical_checkpoint_sha256'] == CHECKPOINT_SHA256 and
            transition['historical_baseline_sha256'] == digest(historical_raw),
            'Unsupported baseline transition or historical identity')
    global_records = [line for line in historical if line.startswith(GLOBAL_SOURCE + ':')]
    require(len(global_records) == 1, 'Historical strict-global record changed')
    removed = (historical[:-1] if transition['mode'] == 'rhs-eval-13' else historical)
    require(historical[-1] == global_records[0] and transition['removed_records'] == removed,
            'Baseline transition removes an unapproved identity, variant or signature')
    remaining = [line for line in historical if line not in removed]
    raw = ('\n'.join(remaining) + '\n').encode() if remaining else b''
    current_raw = authenticated(BASELINE_PATH)
    require(current_raw == raw and
            transition['current_baseline_sha256'] == digest(raw) and
            type(transition['remaining_source_count']) is int and
            transition['remaining_source_count'] == len(remaining),
            'Current baseline is not the exact approved historical subtraction')
    removed_sources = {line.split(':', 1)[0] for line in removed}
    require(len(removed_sources) == len(removed), 'Ambiguous removed source identities')
    proof = dict(transition, source_map_sha256=canonical(sources),
        transition_sha256=digest(transition_raw), removed_source_count=len(removed_sources),
        current_baseline_lines=remaining, conformance_accepted=False, failure_waiver=False)
    result = CoreBaselineTransition(proof, _TOKEN)
    if context is not None:
        result.check_context(context)
    return result


def review_baseline_matches(expected, context):
    transition = context.get('core_baseline_transition')
    if transition is None:
        return expected == context['baseline_sha256']
    require(type(transition) is CoreBaselineTransition, 'Untrusted baseline transition object')
    return transition.review_matches(expected, context)
