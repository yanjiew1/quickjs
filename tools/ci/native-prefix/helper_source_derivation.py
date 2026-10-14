"""Authenticate exact helper identity changes without changing observations."""
import copy
import hashlib
import json
import re

PREFIX = 'tools/ci/native-prefix/'
MANIFEST = PREFIX + 'reconstructed-helper-bindings.json'
MODULE = PREFIX + 'helper_source_derivation.py'
HELPERS = frozenset(('run.py', 'native_profile_gate.py',
    'project_original_assertions.py', 'premise_source_compatibility.py',
    'premise-runner-compatibility.json', 'launch_current_premise_witness.py'))


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def map_digest(value):
    return digest(json.dumps(value, sort_keys=True).encode())


def valid_pin(value):
    return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{64}', value) is not None


class HelperSourceDerivation:
    def __init__(self, manifest, binding, source_map_sha256):
        self._manifest = copy.deepcopy(manifest)
        self._binding = copy.deepcopy(binding)
        self._source_map_sha256 = source_map_sha256

    def receipt(self):
        return dict(schema='quickjs-current-observation-helper-derivation-v1',
            manifest_binding=copy.deepcopy(self._binding),
            source_map_sha256=self._source_map_sha256,
            current_helper_sources=copy.deepcopy(self._manifest['current_helper_sources']),
            identity_only=True, observations_changed=False, conformance_accepted=False)

    def derive_observation(self, observation):
        require(isinstance(observation, dict) and isinstance(
            observation.get('source_bindings'), dict), 'Missing reviewed helper identity map')
        old = observation['source_bindings']
        generation = self._manifest['historical_generations'].get(map_digest(old))
        require(generation is not None and old == generation['source_bindings'],
                'Unknown historical helper identity generation or changed non-helper source')
        derived = copy.deepcopy(observation)
        for name, pin in self._manifest['current_helper_sources'].items():
            derived['source_bindings'][name] = pin
        return derived

    def derive_review(self, review):
        derived = copy.deepcopy(review)
        if 'stable_observation_expectation' in derived:
            derived['stable_observation_expectation'] = self.derive_observation(
                derived['stable_observation_expectation'])
        return derived


def authenticate_helper_derivation(sources, expected_source_map, load_bytes):
    # Legacy exact-current fixtures remain supported. Real checkout identities
    # authenticate the complete tracked map, including this manifest.
    if MANIFEST not in sources:
        return None
    require(map_digest(sources) == expected_source_map,
            'Helper derivation differs from authenticated complete current source map')
    raw = load_bytes(MANIFEST)
    require(digest(raw) == sources[MANIFEST], 'Changed helper derivation manifest')
    manifest = json.loads(raw)
    require(isinstance(manifest, dict) and set(manifest) == {
        'schema', 'identity_only', 'observations_changed', 'conformance_accepted',
        'current_helper_sources', 'historical_generations'} and
        manifest['schema'] == 'quickjs-reconstructed-observation-helper-bindings-v1' and
        manifest['identity_only'] is True and manifest['observations_changed'] is False and
        manifest['conformance_accepted'] is False, 'Unreviewed helper identity derivation')
    current = manifest['current_helper_sources']
    require(isinstance(current, dict) and set(current) == HELPERS,
            'Unknown or missing current helper substitution')
    require(MODULE in sources and digest(load_bytes(MODULE)) == sources[MODULE],
            'Helper derivation implementation is outside current authenticated sources')
    for name, pin in current.items():
        require(valid_pin(pin) and sources.get(PREFIX + name) == pin and
                digest(load_bytes(PREFIX + name)) == pin,
                'Current helper bytes differ from reviewed derivation: ' + name)
    generations = manifest['historical_generations']
    require(isinstance(generations, dict) and generations, 'Missing historical helper generations')
    for identity, generation in generations.items():
        require(isinstance(generation, dict) and set(generation) == {
            'source_bindings', 'origin_review'}, 'Unreviewed historical helper generation')
        old, origin = generation['source_bindings'], generation['origin_review']
        require(isinstance(old, dict) and HELPERS <= set(old) and
                all(isinstance(name, str) and valid_pin(pin) for name, pin in old.items()) and
                map_digest(old) == identity, 'Changed exact historical helper source map')
        require(isinstance(origin, dict) and set(origin) == {'path', 'sha256'} and
                isinstance(origin['path'], str) and origin['path'].startswith(PREFIX + 'reviews/') and
                all(part not in ('', '.', '..') for part in origin['path'].split('/')) and
                valid_pin(origin['sha256']) and sources.get(origin['path']) == origin['sha256'],
                'Historical helper generation is outside authenticated original reviews')
        original = load_bytes(origin['path'])
        require(digest(original) == origin['sha256'] and json.loads(original).get(
            'stable_observation_expectation', {}).get('source_bindings') == old,
            'Historical helper generation differs from its immutable review')
    return HelperSourceDerivation(manifest, dict(path=MANIFEST, sha256=sources[MANIFEST]),
                                  expected_source_map)
