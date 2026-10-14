"""Approved current runner transport view for an immutable premise packet.

The raw packet and review remain historical. Only the explicitly approved reconstruction source pins and
its two effective canonical hashes change in emitted premise data. Current
source maps stay actual; ephemeral legacy inputs never become capture evidence.
"""
import copy
import hashlib
import importlib
import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
APPROVAL = 'tools/ci/native-prefix/premise-runner-compatibility.json'
ADAPTER = 'tools/ci/native-prefix/premise_source_compatibility.py'
LAUNCHER = 'tools/ci/native-prefix/launch_current_premise_witness.py'
SUBJECT = 'tools/run-test262.c'
THREADS = 'tools/ci/native-prefix/execution_threads.py'


def require(value, reason):
    if not value:
        raise ValueError(reason)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def canonical(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=True,
        separators=(',', ':')).encode()).hexdigest()


def source_map_digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def authenticated_receipt(files, packet, review_sha):
    root = HERE.parents[2]
    packet = Path(packet).resolve()
    require(packet == (HERE / 'original-assertions').resolve(), 'Unapproved premise packet')
    approval = json.loads((root / APPROVAL).read_bytes())
    require(approval.get('schema') == 'native-premise-reconstructed-source-compatibility-v2' and
            approval.get('subject') == SUBJECT and approval.get('source_review_complete') is True and
            approval.get('engine_algorithms_changed') is False and
            approval.get('semantic_adjudication_complete') is False and
            approval.get('conformance_accepted') is False,
            'Unapproved current runner compatibility')
    require(digest(packet / 'SEAL.json') == approval['packet_seal_sha256'] and
            review_sha == approval['root_source_review_sha256'] ==
            digest(HERE / 'premise-source-guard-review.json'),
            'Historical premise review or packet changed')
    pins = json.loads((packet / 'PREMISE-SOURCE-PINS.json').read_bytes())
    subjects = approval.get('subjects')
    dependencies = approval.get('additional_dependencies')
    require(isinstance(subjects, dict) and subjects and SUBJECT in subjects and
            isinstance(dependencies, dict), 'Missing reconstruction source derivation')
    require(subjects[SUBJECT]['historical_sha256'] == approval['historical_sha256'] and
            subjects[SUBJECT]['current_sha256'] == approval['current_sha256'],
            'Runner compatibility summary differs from exact subject map')
    require(all(name in pins and pins[name] == row['historical_sha256'] and
                files.get(name) == row['current_sha256'] == digest(root / name)
                for name, row in subjects.items()),
            'Unapproved reconstructed premise subject')
    require(all(files.get(name) == expected for name, expected in pins.items()
                if name not in subjects), 'Unapproved premise source changed')
    require(all(name not in pins and files.get(name) == row['current_sha256'] ==
                digest(root / name) for name, row in dependencies.items()),
            'Unapproved extracted premise dependency')
    sources = {name: digest(root / name) for name in (APPROVAL, ADAPTER, LAUNCHER, THREADS)}
    require(all(files.get(name) == pin for name, pin in sources.items()),
            'Current compatibility adapter is outside authenticated sources')
    return dict(schema='native-premise-reconstructed-source-compatibility-receipt-v2',
        source_tree_sha256=source_map_digest(files), source_bindings=sources,
        historical_packet_seal_sha256=approval['packet_seal_sha256'],
        historical_source_review_sha256=review_sha, subject=SUBJECT,
        historical_sha256=approval['historical_sha256'], current_sha256=approval['current_sha256'],
        subjects=subjects, additional_dependencies=dependencies,
        conformance_accepted=False, semantic_adjudication_complete=False)


def validate_bindings(bindings, packet):
    files = bindings.get('current_source_files')
    require(isinstance(files, dict) and files and
            source_map_digest(files) == bindings.get('source_tree_sha256'),
            'Compatibility complete actual source map differs')
    receipt = authenticated_receipt(files, packet,
        digest(HERE / 'premise-source-guard-review.json'))
    require(bindings.get('premise_source_compatibility') == receipt,
            'Missing exact current compatibility receipt')
    return receipt


def effective_data(packet, receipt):
    data = json.loads((Path(packet) / 'PREMISE-DATA.json').read_bytes())
    for name, row in receipt['subjects'].items():
        require(data.get('source_files', {}).get(name) == row['historical_sha256'],
                'Historical premise subject differs: ' + name)
        data['source_files'][name] = row['current_sha256']
    return data


def install(packet):
    """Install before either sealed renderer import, without editing its bytes."""
    packet = Path(packet).resolve()
    require(packet == (HERE / 'original-assertions').resolve(), 'Unapproved adapter packet')
    approval = json.loads((HERE.parents[2] / APPROVAL).read_bytes())
    require(digest(packet / 'SEAL.json') == approval['packet_seal_sha256'],
            'Changed premise packet seal before import')
    for name, expected in json.loads((packet / 'SEAL.json').read_bytes())['files'].items():
        path = packet / name
        require(not Path(name).is_absolute() and '..' not in Path(name).parts and
                path.is_file() and not path.is_symlink() and
                packet in path.resolve().parents and digest(path) == expected,
                'Changed or unsafe sealed premise source before import: ' + name)
    sys.path.insert(0, str(packet))
    guard = importlib.import_module('guard_binding')
    marker = digest(HERE / 'premise_source_compatibility.py')
    if getattr(guard.prepare_premise_binding, '_current_runner_adapter', None) == marker:
        return
    require('render_practical_witness' not in sys.modules,
            'Premise adapter must precede renderer import')
    original_prepare = guard.prepare_premise_binding

    def prepare(packet, bindings, plan):
        receipt = validate_bindings(bindings, packet)
        legacy = copy.deepcopy(bindings)
        for name, row in receipt['subjects'].items():
            legacy['current_source_files'][name] = row['historical_sha256']
        binding, data = original_prepare(packet, legacy, plan)
        historical = json.loads((Path(packet) / 'PREMISE-DATA.json').read_bytes())
        require(data == historical, 'Sealed prepare changed historical premise data')
        binding = copy.deepcopy(binding)
        data = effective_data(packet, receipt)
        binding['current_source_files'] = copy.deepcopy(bindings['current_source_files'])
        binding['source_pins_canonical_sha256'] = canonical(data['source_files'])
        binding['premise_data_canonical_sha256'] = canonical(data)
        return binding, data

    prepare._current_runner_adapter = marker
    guard.prepare_premise_binding = prepare


def stable_effective_config(config, bindings):
    """Inverse-normalize only an authenticated reconstruction source derivation."""
    if not any(site.get('premise_guard') for site in config.get('sites', {}).values()):
        return config
    # Existing mocked/historical callers without the subject retain their view.
    data = config.get('premise_data')
    if not isinstance(data, dict) or SUBJECT not in data.get('source_files', {}):
        require(bindings.get('premise_source_compatibility') is None,
                'Current compatibility data missing')
        return config
    packet = HERE / 'original-assertions'
    receipt = validate_bindings(bindings, packet)
    require(data == effective_data(packet, receipt), 'Effective premise data changed outside approved pin')
    binding = config.get('premise_bindings')
    require(isinstance(binding, dict) and
            binding.get('current_source_files') == bindings['current_source_files'] and
            binding.get('source_pins_canonical_sha256') == canonical(data['source_files']) and
            binding.get('premise_data_canonical_sha256') == canonical(data),
            'Effective premise canonical authentication differs')
    historical = json.loads((packet / 'PREMISE-DATA.json').read_bytes())
    result = copy.deepcopy(config)
    result['premise_data'] = historical
    result['premise_bindings']['source_pins_canonical_sha256'] = canonical(historical['source_files'])
    result['premise_bindings']['premise_data_canonical_sha256'] = canonical(historical)
    return result
