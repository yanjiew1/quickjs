#!/usr/bin/env python3
"""Verify split-05 source fidelity, without benchmarking or compiling."""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import subprocess
from split05_lib import functions, data_definitions

UPSTREAM = '04be246001599f5995fa2f2d8c91a0f198d3f34c'
ROOT = Path(__file__).resolve().parent.parent


def external_signature(sig):
    if re.search(r'\bstatic\b', sig):
        sig = re.sub(r'\bstatic\s+', 'QJS_INTERNAL ', sig, count=1)
        sig = re.sub(r'\b(?:inline|force_inline)\s+', '', sig)
    return sig


def body(item):
    return item['text'][item['body_start'] - item['start']:]


def sha(text):
    return hashlib.sha256(text.encode()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--upstream-dir', type=Path)
    ap.add_argument('--report', type=Path)
    ap.add_argument('--expect-manifest', type=Path)
    ap.add_argument('--expect-digest')
    args = ap.parse_args()

    def upstream(path):
        if args.upstream_dir:
            return (args.upstream_dir / path).read_text()
        return subprocess.check_output(['git', 'show', f'{UPSTREAM}:{path}'], cwd=ROOT).decode()

    reference = upstream('quickjs.c')
    original = defaultdict(list)
    for f in functions(reference):
        original[f['name']].append(f)
    original_data = {d['name']: d for d in data_definitions(reference)}
    seen, seen_data = Counter(), Counter()
    errors, rows, data_rows = [], [], []
    manifest = {}
    for path in sorted((ROOT / 'src/quickjs').glob('*.[ch]')):
        source = path.read_text()
        rel = str(path.relative_to(ROOT))
        manifest[rel] = sha(source)
        if re.search(r'\bqjs_\w*', source):
            errors.append(f'{rel}: remaining qjs_* identifier')
        definitions = functions(source)
        if path.suffix == '.h' and definitions:
            errors.append(f'{rel}: private header contains function definitions')
        for f in definitions:
            name = f['name']
            seen[name] += 1
            candidates = original.get(name, [])
            matches = [u for u in candidates if body(f) == body(u)
                       and f['sig'] in (u['sig'], external_signature(u['sig']))]
            if not matches:
                errors.append(f'{rel}:{f["line"]}: {name}: signature/body mismatch')
            rows.append({'name': name, 'file': rel, 'line': f['line'],
                         'body_sha256': sha(body(f)), 'upstream_exact_body': bool(matches),
                         'linkage_only_change': bool(matches and f['sig'] != matches[0]['sig'])})
        for d in data_definitions(source):
            name = d['name']
            seen_data[name] += 1
            same = name in original_data and body(d) == body(original_data[name])
            if not same:
                errors.append(f'{rel}: {name}: data initializer mismatch')
            data_rows.append({'name': name, 'file': rel, 'upstream_exact_initializer': same})
    if seen != Counter({k: len(v) for k, v in original.items()}):
        errors.append('upstream/current function definition multiplicities differ')
    if seen_data != Counter({k: 1 for k in original_data}):
        errors.append('upstream/current data definition multiplicities differ')
    if (ROOT / 'quickjs.h').read_text() != upstream('quickjs.h'):
        errors.append('public quickjs.h differs from upstream')
    for name in ['Makefile', 'quickjs.h']:
        manifest[name] = sha((ROOT / name).read_text())
    if args.expect_manifest:
        expected = json.loads(args.expect_manifest.read_text())
        if expected != manifest:
            errors.append('generated source hashes differ from the reviewed manifest')
    digest=sha(''.join(path+'\0'+value+'\n' for path,value in sorted(manifest.items())))
    if args.expect_digest and digest != args.expect_digest:
        errors.append('generated source digest differs from the reviewed local tree')
    report = {'source_digest':digest, 'upstream': UPSTREAM, 'function_definitions': sum(seen.values()),
              'unique_functions': len(seen), 'data_definitions': sum(seen_data.values()),
              'private_header_function_definitions': 0 if not errors else None,
              'functions': rows, 'data': data_rows, 'source_manifest': manifest,
              'errors': errors}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Functions: {report['function_definitions']} definitions / {report['unique_functions']} names")
    print(f"Data initializers: {report['data_definitions']}")
    print(f"Source fidelity: {'FAIL' if errors else 'PASS'}")
    print(f"Source digest: {digest}")
    for error in errors:
        print(error)
    raise SystemExit(bool(errors))


if __name__ == '__main__':
    main()
