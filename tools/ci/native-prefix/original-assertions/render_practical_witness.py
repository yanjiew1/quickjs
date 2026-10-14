"""Authenticated source renderer. Root owns invocation; no engine launch here."""
from pathlib import Path
import argparse
import hashlib
import json
from guard_binding import prepare_premise_binding
from selected_timezone_orchestration import TZ_FIXTURE, fixture_bindings, authenticated_input


def digest(data):
    return hashlib.sha256(data).hexdigest()


def config_digest(config):
    return digest(json.dumps({k: v for k, v in config.items() if k != '_config_sha256'}, sort_keys=True, ensure_ascii=True, separators=(',', ':')).encode())


def authenticate_packet(packet, expected_seal):
    packet = Path(packet).resolve()
    raw = (packet / 'SEAL.json').read_bytes()
    if digest(raw) != expected_seal:
        raise ValueError('Packet seal differs from root binding')
    seal = json.loads(raw)
    for rel, expected in seal['files'].items():
        path = (packet / rel).resolve()
        if not path.is_relative_to(packet) or not path.is_file() or digest(path.read_bytes()) != expected:
            raise ValueError('Packet file changed: ' + rel)
    return packet


def compose_nested(original, nodes):
    """Exact span wrappers preserve original operand/receiver evaluation and nest calls."""
    ordered = sorted(nodes, key=lambda n: (n['start'], -n['end'], n.get('priority', 0)))
    roots = []
    stack = []
    for node in ordered:
        node = dict(node, children=[])
        if original[node['start']:node['end']] != node['original']:
            raise ValueError('Nested edit preimage differs')
        if digest(node['original'].encode()) != node['preimage_sha256']:
            raise ValueError('Nested edit seal differs')
        while stack and node['start'] >= stack[-1]['end']:
            stack.pop()
        if stack:
            if node['end'] > stack[-1]['end']:
                raise ValueError('Crossing source transform spans')
            stack[-1]['children'].append(node)
        else:
            roots.append(node)
        stack.append(node)

    def region(start, end, children):
        parts = []
        cursor = start
        for child in children:
            if child['start'] < cursor:
                raise ValueError('Source transform siblings overlap')
            parts.append(original[cursor:child['start']])
            inner = region(child['start'], child['end'], child['children'])
            if child['kind'] == 'replacement':
                if child['children']:
                    raise ValueError('Replacement cannot erase nested original obligations')
                parts.append(child['replacement'])
            elif child['kind'] == 'wrapper':
                parts.append(child['prefix'] + inner + child['suffix'])
            else:
                raise ValueError('Unknown source transformation kind')
            cursor = child['end']
        parts.append(original[cursor:end])
        return ''.join(parts)

    return region(0, len(original), roots)


def render(packet, bindings, output_root, selected_paths=None):
    packet = authenticate_packet(packet, bindings['packet_seal_sha256'])
    for key in ('engine_sha256', 'source_tree_sha256', 'profile_sha256'):
        if not isinstance(bindings.get(key), str) or len(bindings[key]) != 64:
            raise ValueError('Actual root identity required: ' + key)
    if not set(('L1', 'T1', 'F1', 'F2', 'H1')) <= set(bindings.get('integrated_source_fix_ids', [])):
        raise ValueError('Required independent fixes have not been authenticated by root')
    plan = json.loads((packet / 'HOSTED-RENDER-PLAN.json').read_text())
    premise_binding, premise_data = prepare_premise_binding(packet, bindings, plan)
    fixtures = plan['fixtures'] + plan.get('oracle_fixtures', [])
    all_paths = {f['path'] for f in fixtures}
    selected = set(selected_paths) if selected_paths is not None else {f['path'] for f in plan['fixtures']}
    if not selected or selected - all_paths:
        raise ValueError('Empty or newly unclassified selection: ' + str(sorted(selected - all_paths)))
    output_root = Path(output_root)
    if output_root.exists():
        raise ValueError('Output must be a fresh isolated directory')
    output_root.mkdir(parents=True)
    runtime = (packet / 'witness-runtime.js').read_text()
    manifest = []
    for fixture in fixtures:
        if fixture['path'] not in selected:
            continue
        selected_bindings = fixture_bindings(bindings, fixture['path'])
        selected_premise_binding = premise_binding
        if fixture['path'] == TZ_FIXTURE:
            selected_premise_binding, _ = prepare_premise_binding(packet, selected_bindings, plan)
        source_path = packet / 'immutable-original-inputs' / fixture['path'].removeprefix('test262/')
        raw = source_path.read_bytes()
        if digest(raw) != fixture['official_original_sha256']:
            raise ValueError('Official source preimage differs')
        prelude = []
        for h in fixture['harness']:
            hb = (packet / 'immutable-original-inputs' / 'harness' / h['name']).read_bytes()
            if digest(hb) != h['sha256']:
                raise ValueError('Original harness preimage differs')
            if h.get('edits'):
                raise ValueError('Unknown harness edit recipe')
            prelude.append(compose_nested(hb.decode(), h['nested_nodes']) if h.get('nested_nodes') else hb.decode())
        body = compose_nested(raw.decode(), fixture['nested_nodes'])
        for variant in fixture.get('variants', ['nonstrict', 'strict']):
            config = dict(fixture['runtime_config'], variant=variant,
                profile_sha256=bindings['profile_sha256'], profile=bindings.get('profile'),
                engine_sha256=selected_bindings['engine_sha256'], source_tree_sha256=bindings['source_tree_sha256'])
            if any(site.get('premise_guard') for site in config['sites'].values()):
                config['premise_bindings'] = selected_premise_binding
                config['premise_data'] = premise_data
            if fixture['path'] == TZ_FIXTURE:
                config['selected_provider_input'] = authenticated_input(packet, bindings)
            config['_config_sha256'] = config_digest(config)
            # Direct script eval keeps unchanged harness flag operations in their original sloppy realm.
            # Both original default Test262 variants are captured; neither is a conformance run.
            evaluated = ('"use strict";\n' if variant == 'strict' else '') + body + '\n__w.finish();\n'
            generated = '\n'.join(prelude) + '\n' + runtime + '\nconst __w = createOriginalWitness(' + json.dumps(config, ensure_ascii=True, separators=(',', ':')) + ', line => print(line));\n' + 'eval(' + json.dumps(evaluated, ensure_ascii=True) + ');\n'
            rel = Path(variant) / fixture['path'].removeprefix('test262/')
            dest = output_root / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(generated)
            manifest.append(dict(path=fixture['path'], variant=variant, generated_path=str(rel),
                generated_sha256=digest(generated.encode()), fixture_sha256=digest(raw), config=config))
    (output_root / 'GENERATED-MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--packet', required=True)
    parser.add_argument('--bindings', required=True)
    parser.add_argument('--output-root', required=True)
    parser.add_argument('--selection')
    args = parser.parse_args()
    render(args.packet, json.loads(Path(args.bindings).read_text()), args.output_root,
        json.loads(Path(args.selection).read_text()) if args.selection else None)


if __name__ == '__main__':
    main()
