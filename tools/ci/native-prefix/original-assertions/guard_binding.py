"""Pure source/profile guard binding; external root review supplies acceptance."""
import hashlib
import json
from selected_timezone_orchestration import validate_config


def canonical_sha(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=True, separators=(',', ':')).encode()).hexdigest()


def validate_premise_config(config):
    validate_config(config)
    guarded = {sid: site['premise_guard'] for sid, site in config['sites'].items() if site.get('premise_guard')}
    if not guarded:
        return
    binding = config.get('premise_bindings')
    data = config.get('premise_data')
    if not isinstance(binding, dict) or not isinstance(data, dict):
        raise ValueError('Fresh premise binding/data missing')
    review = binding.get('source_guard_review')
    if not isinstance(review, dict) or canonical_sha(review) != binding.get('root_guard_review_sha256'):
        raise ValueError('Premise root review digest differs')
    if review.get('schema') != 'native-premise-root-source-guard-review-v1' or review.get('source_review_complete') is not True or review.get('conformance_accepted') is not False or review.get('semantic_adjudication_complete') is not False:
        raise ValueError('Explicit root source guard review missing')
    for key in ('engine_sha256', 'source_tree_sha256', 'profile_sha256'):
        if not isinstance(config.get(key), str) or len(config[key]) != 64 or config[key] != binding.get(key) or config[key] != review.get(key):
            raise ValueError('Premise current identity differs: ' + key)
    if review.get('profile_canonical_sha256') != canonical_sha(config.get('profile')):
        raise ValueError('Premise effective profile differs')
    expected = data.get('source_files')
    current = binding.get('current_source_files')
    if not isinstance(expected, dict) or not expected or not isinstance(current, dict) or any(current.get(k) != v for k, v in expected.items()):
        raise ValueError('Premise current source files differ')
    if binding.get('source_pins_sha256') != review.get('source_pins_sha256') or canonical_sha(expected) != binding.get('source_pins_canonical_sha256'):
        raise ValueError('Premise source pin authentication differs')
    if canonical_sha(data) != binding.get('premise_data_canonical_sha256') or binding.get('premise_data_sha256') != review.get('premise_data_sha256'):
        raise ValueError('Premise data digest differs')
    for key in ('guard_policy_sha256', 'runtime_sha256', 'trace_proof_sha256'):
        if not isinstance(binding.get(key), str) or len(binding[key]) != 64 or binding[key] != review.get(key):
            raise ValueError('Premise guard policy/runtime review differs')
    helpers = binding.get('helper_sha256')
    if not isinstance(helpers, dict) or set(helpers) != {'render_practical_witness.py','validate_practical_witness.py','guard_binding.py','premise_trace_proof.py'} or review.get('helper_sha256') != helpers or helpers['premise_trace_proof.py'] != binding['trace_proof_sha256']:
        raise ValueError('Exact premise helper review missing')
    reviewed = review.get('reviewed_guard_sites')
    if not isinstance(reviewed, dict) or any(reviewed.get(sid) != guard for sid, guard in guarded.items()):
        raise ValueError('Exact premise source site review missing')


def prepare_premise_binding(packet, bindings, plan):
    raw = (packet / 'PREMISE-DATA.json').read_bytes()
    data = json.loads(raw)
    pins_raw = (packet / 'PREMISE-SOURCE-PINS.json').read_bytes()
    pins = json.loads(pins_raw)
    if pins != data.get('source_files'):
        raise ValueError('Source data/pin map differs')
    runtime_sha = hashlib.sha256((packet / 'witness-runtime.js').read_bytes()).hexdigest()
    if runtime_sha != plan['runtime_sha256']:
        raise ValueError('Premise renderer runtime differs')
    helper_sha256 = {name:hashlib.sha256((packet / name).read_bytes()).hexdigest() for name in ('render_practical_witness.py','validate_practical_witness.py','guard_binding.py','premise_trace_proof.py')}
    if helper_sha256 != plan.get('helper_sha256'):
        raise ValueError('Premise reviewed helper map differs')
    trace_proof_sha = hashlib.sha256((packet / 'premise_trace_proof.py').read_bytes()).hexdigest()
    if trace_proof_sha != plan.get('trace_proof_sha256'):
        raise ValueError('Premise branch proof helper differs')
    policy_raw = (packet / 'PREMISE-GUARD-POLICY.json').read_bytes()
    policy = json.loads(policy_raw)
    if policy['guarded_sites'] != plan['premise_guard_sites']:
        raise ValueError('Premise guard policy site map differs')
    review = bindings.get('premise_source_guard_review')
    binding = dict(engine_sha256=bindings['engine_sha256'], source_tree_sha256=bindings['source_tree_sha256'],
        profile_sha256=bindings['profile_sha256'], current_source_files=bindings.get('current_source_files'),
        source_guard_review=review, root_guard_review_sha256=bindings.get('premise_source_guard_review_sha256'),
        premise_data_sha256=hashlib.sha256(raw).hexdigest(), premise_data_canonical_sha256=canonical_sha(data),
        source_pins_sha256=hashlib.sha256(pins_raw).hexdigest(), source_pins_canonical_sha256=canonical_sha(pins),
        guard_policy_sha256=hashlib.sha256(policy_raw).hexdigest(), runtime_sha256=runtime_sha, trace_proof_sha256=trace_proof_sha, helper_sha256=helper_sha256)
    validate_premise_config(dict(sites={sid:dict(premise_guard=g) for sid,g in plan['premise_guard_sites'].items()},
        premise_bindings=binding, premise_data=data, engine_sha256=bindings['engine_sha256'],
        source_tree_sha256=bindings['source_tree_sha256'],profile_sha256=bindings['profile_sha256'],profile=bindings.get('profile')))
    return binding, data
