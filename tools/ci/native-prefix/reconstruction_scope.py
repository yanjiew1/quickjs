"""Explicit current path derivation from the immutable historical manifest."""
import hashlib
import json

HISTORICAL_MAP = 'c71443f857120b4ece57b2d4c2ce635e9b54bacf97f33f5eb4b6177c839a3b70'


def require(value, reason):
    if not value:
        raise RuntimeError(reason)


def safe_path(value):
    return (isinstance(value, str) and value and not value.startswith('/') and
            '\\' not in value and all(p not in ('', '.', '..') for p in value.split('/')))


def required_paths(historical, derivation):
    actual = hashlib.sha256(json.dumps(historical, sort_keys=True).encode()).hexdigest()
    require(actual == HISTORICAL_MAP and isinstance(derivation, dict) and
            set(derivation) == {'schema', 'historical_map_sha256', 'replacements',
                               'omissions', 'additional_sources'} and
            derivation['schema'] == 'quickjs-history-source-path-derivation-v1' and
            derivation['historical_map_sha256'] == HISTORICAL_MAP,
            'Changed historical source map or unreviewed reconstruction derivation')
    replacements, omissions = derivation['replacements'], derivation['omissions']
    additional = derivation['additional_sources']
    require(isinstance(replacements, dict) and isinstance(omissions, dict) and
            not set(replacements) & set(omissions) and
            (set(replacements) | set(omissions)) <= set(historical),
            'Reconstruction changes an unknown historical source path')
    require(isinstance(additional, list) and len(additional) == len(set(additional))
            and all(safe_path(path) for path in additional),
            'Unsafe or duplicate additional required source')
    result = set(historical) - set(replacements) - set(omissions)
    renamed = set()
    for old, entry in replacements.items():
        require(isinstance(entry, dict) and set(entry) == {'path', 'reason'} and
                safe_path(entry['path']) and isinstance(entry['reason'], str) and
                entry['reason'].strip() and entry['path'] not in renamed,
                'Missing exact replacement path or review reason: ' + old)
        renamed.add(entry['path'])
    for old, entry in omissions.items():
        require(isinstance(entry, dict) and set(entry) == {'reason', 'covered_by'} and
                isinstance(entry['reason'], str) and entry['reason'].strip() and
                isinstance(entry['covered_by'], list) and entry['covered_by'] and
                all(safe_path(path) for path in entry['covered_by']),
                'Missing omission review or replacement coverage: ' + old)
        result.update(entry['covered_by'])
    result.update(renamed)
    result.update(additional)
    require(not result & (set(replacements) | set(omissions)),
            'Retired historical source reintroduced into required path set')
    return result
