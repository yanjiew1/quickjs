"""Current sampler capture versus complete authenticated previously reviewed actual trace."""
from pathlib import Path
import hashlib
import json
from collections import defaultdict


def validate_current_rtf(packet, text, command, bindings, raw_full_run):
    if command.get('returncode') != 0 or command.get('timed_out') is not False or command.get('interrupted') is not False or command.get('lifecycle_closed') is not True:
        raise ValueError('Current RTF command incomplete')
    if raw_full_run.get('raw_preserved') is not True:
        raise ValueError('Normal raw full run not retained')
    for key in ('engine_sha256', 'source_tree_sha256', 'profile_sha256'):
        if command.get(key) != bindings.get(key):
            raise ValueError('Current RTF command identity differs')
    if hashlib.sha256(text.encode()).hexdigest() != command.get('stdout_sha256'):
        raise ValueError('Current RTF complete stdout differs')
    packet = Path(packet)
    sampler = (packet / 'rtf-reviewed-sampler' / 'capture-relative-diagnostic.js').read_bytes()
    if command.get('generated_sha256') != hashlib.sha256(sampler).hexdigest():
        raise ValueError('RTF sampler source differs')
    current = json.loads(text)
    historical = json.loads((packet / 'rtf-original-actual-reference' / 'capture.log').read_text())
    reviewed = json.loads((packet / 'rtf-original-actual-reference' / 'review-summary.json').read_text())
    inventory = json.loads((packet / 'rtf-reviewed-sampler' / 'expected-source-inventory.json').read_text())
    if current != historical:
        raise ValueError('Current RTF complete observations changed; fresh review required')
    if reviewed.get('original_calls_compared') != 1008 or reviewed.get('ANY_UNEXPECTED') != []:
        raise ValueError('Authenticated historical complete review missing')
    calls = inventory['original_api_calls']
    if len(calls) != 1008 or len({c['id'] for c in calls}) != 1008 or current['counts']['originalFixtureApiCalls'] != 1008:
        raise ValueError('Complete exact original RTF call identities differ')
    fields = defaultdict(list)
    differences = json.loads((packet / 'rtf-original-actual-reference' / 'review-actual-original-assertion-differences.json').read_text())
    difference_ids = {d['id'] for d in differences} if isinstance(differences, list) else {d['id'] for d in differences.get('differences', []) + differences.get('constructor_original_assertion_differences', [])}
    for c in calls:
        path = 'test262/' + c['fixture']['path']
        fields[path].append(c)
    constructor = 'test262/test/intl402/RelativeTimeFormat/constructor/constructor/locales-valid.js'
    fields[constructor] = inventory['valid_constructor_contexts'] + inventory['invalid_constructor_contexts']
    summaries = {}
    for path, source_calls in fields.items():
        pin = next((c['fixture']['sha256'] for c in source_calls if 'fixture' in c), None)
        if pin is None:
            pin = hashlib.sha256((packet / 'immutable-original-inputs' / path.removeprefix('test262/')).read_bytes()).hexdigest()
        summaries[path] = dict(schema='native-original-assertion-trace-v2', fixture=path, fixture_sha256=pin,
            variant='reviewed-variant-neutral-native-input-matrix', strict_variant_neutral_capture=True,
            fixture_capture_complete=True, original_eof_reached=True,
            capture_basis='Current unchanged sampler normal EOF; exact complete actual observations equal authenticated fully reviewed prior trace. Original source assertions are captured by sampler mapping, not executed as a Test262 script.',
            mandatory_failed_site_ids=[], continued_data_site_ids=sorted(c['id'] for c in source_calls if c['id'] in difference_ids),
            executed_site_ids=sorted(c['id'] for c in source_calls), original_api_call_count=len(source_calls),
            totalized_original_data_obligations=0, totalized_original_data_site_ids=[], all_original_expressions_evaluated=True,
            engine_sha256=bindings['engine_sha256'], source_tree_sha256=bindings['source_tree_sha256'], profile_sha256=bindings['profile_sha256'],
            trace_sha256=command['stdout_sha256'], generated_sha256=command['generated_sha256'],
            semantic_adjudication_complete=False, conformance_accepted=False,
            embedded_metadata_is_historical_provenance=True, current_execution_identity_source='current command receipt')
    if len(summaries) != 13:
        raise ValueError('Exact RTF original file scope differs')
    return summaries
