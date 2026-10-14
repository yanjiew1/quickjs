"""Prepared pure diagnostic parser; no execution or acceptance authority.

parse_result and observation_evidence are factored from the sealed v3 collector.
Only parse_result's fixed global denominator becomes an explicit argument.
Root must execute mocked-fixture tests and independently review before integration.
"""
from collections import Counter
import re

SANITIZER_RECORDS = (
    ('sanitizer-report', re.compile(r'^(?:==[0-9]+==\s*)?(?:ERROR|WARNING|FATAL): (?:AddressSanitizer|LeakSanitizer|MemorySanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer):')),
    ('sanitizer-summary', re.compile(r'^(?:==[0-9]+==\s*)?SUMMARY: (?:AddressSanitizer|LeakSanitizer|MemorySanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer):')),
    ('sanitizer-fatal', re.compile(r'^(?:==[0-9]+==\s*)?(?:AddressSanitizer|LeakSanitizer|MemorySanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer):(?:DEADLYSIGNAL| CHECK failed| failed| fatal)')),
    ('ubsan-runtime-error', re.compile(r'^(?!test262/test/)[^\n:]+\.(?:c|cc|cpp|cxx|h|hpp|inc):[0-9]+(?::[0-9]+)?: runtime error:')),
    ('runner-infrastructure', re.compile(r'^run-test262: ')),
    ('runtime-infrastructure', re.compile(r'^(?:Fatal glibc error:|LLVM ERROR:|Segmentation fault(?: \(core dumped\))?$|Aborted(?: \(core dumped\))?$)')),
)

def parse_result(stdout, stderr, distinct_source_count):
    # Exactly the production print sequence: optional nonzero fields in this order.
    unsigned = r'(0|[1-9][0-9]*)'
    positive = r'([1-9][0-9]*)'
    grammar = r'Result: ' + unsigned + '/' + unsigned + r' (error|errors)'
    for field in ('excluded', 'skipped', 'new', 'changed', 'fixed'):
        grammar += r'(?:, ' + positive + ' ' + field + r')?'
    result_lines = [(stream, number, literal)
                    for stream, content in (('stdout', stdout), ('stderr', stderr))
                    for number, literal in enumerate(content.splitlines(), 1)
                    if literal.startswith('Result:')]
    errors = []
    counters = None
    if len(result_lines) != 1:
        errors.append('Exactly one Result record required across original streams')
    else:
        stream, number, literal = result_lines[0]
        match = re.fullmatch(grammar, literal)
        if stream != 'stderr' or match is None:
            errors.append('Result does not match exact production stderr grammar')
        else:
            failed, executed, noun, *optional = match.groups()
            counters = dict(failed_source_count=int(failed), executed_source_count=int(executed))
            counters.update({key: int(value or 0) for key, value in
                             zip(('excluded', 'skipped', 'new', 'changed', 'fixed'), optional)})
            if noun != ('error' if int(executed) == 1 else 'errors'):
                errors.append('Result singular/plural differs from production test_count')
            if (counters['failed_source_count'] > counters['executed_source_count'] or
                    any(counters[k] > distinct_source_count for k in
                        ('failed_source_count', 'executed_source_count', 'excluded', 'skipped')) or
                    any(counters[k] > 2 * counters['executed_source_count'] for k in ('new', 'changed', 'fixed')) or
                    counters['new'] + counters['changed'] > counters['failed_source_count']):
                errors.append('Result fields exceed production source/variant ranges')
    return counters, errors, [dict(stream=x[0], raw_log_line=x[1], literal=x[2]) for x in result_lines]

def observation_evidence(stdout, stderr, baseline_records, runnable):
    observations, errors, unknown = [], [], []
    baseline_by_variant = {(p['path'], p['strict']): p for p in baseline_records}
    events = {'failure': Counter(), 'previous': Counter(), 'fixed': Counter()}
    for stream, content in (('stdout', stdout), ('stderr', stderr)):
        for number, literal in enumerate(content.splitlines(), 1):
            match = re.fullmatch(r'(test262/test/.*?\.js):([0-9]+): (.*)', literal)
            if not match:
                if literal.startswith('test262/test/') and '.js:' in literal:
                    errors.append('Malformed path/line observation at {}:{}'.format(stream, number))
                continue
            path, line, signature = match.groups()
            strict = signature.startswith('strict mode: ')
            body = signature[len('strict mode: '):] if strict else signature
            item = dict(path=path, line=int(line), signature=signature, strict=strict,
                        stream=stream, raw_log_line=number, literal=literal)
            key = (path, strict)
            if path not in runnable or int(line) < 1 or int(line) > 2147483647:
                errors.append('Observation path/line outside reviewed runnable fixture')
            if re.fullmatch(r'unknown feature: [^\s]+', body) and not strict and int(line) == 1:
                item['status'] = 'unknown-feature-skip-warning'
                unknown.append(item)
            elif body.startswith('previous error: '):
                item['status'] = 'previous'
                events['previous'][key] += 1
                baseline = baseline_by_variant.get(key)
                if baseline is None or item['line'] != baseline['line'] or body[len('previous error: '):] != baseline['body']:
                    errors.append('Previous error does not match unchanged baseline variant/signature/line')
            elif re.fullmatch(r'OK, (?:fixed error: .+|now has error .+)', body):
                item['status'] = 'fixed'
                events['fixed'][key] += 1
                baseline = baseline_by_variant.get(key)
                if baseline is None:
                    errors.append('Fixed event has no unchanged baseline variant')
                elif body.startswith('OK, fixed error: ') and (
                        item['line'] != baseline['line'] or body[len('OK, fixed error: '):] != baseline['body']):
                    errors.append('Fixed error reference differs from unchanged baseline')
                elif body.startswith('OK, now has error ') and not (
                        baseline['body'] == 'expected error' or
                        baseline['body'].startswith('unexpected error type:') or
                        baseline['body'] == body[len('OK, now has error '):]):
                    errors.append('Now-has-error event cannot match the unchanged negative baseline')
            elif (body == 'expected error' or
                  re.fullmatch(r'(?:unexpected error|unexpected error type): .+', body)):
                item['status'] = 'observed-failure'
                events['failure'][key] += 1
            else:
                item['status'] = 'unrecognized-path-record'
                errors.append('Unrecognized production path record at {}:{}'.format(stream, number))
            observations.append(item)
    for kind, multiset in events.items():
        if any(n != 1 for n in multiset.values()):
            errors.append('Duplicate {} observation for one path/variant'.format(kind))
    if any(n > 1 for n in Counter(key[0] for key in events['failure']).values()):
        errors.append('More than one failing variant for a source; production stops strict after nonstrict failure')
    if any(events['previous'][key] != events['failure'][key] for key in events['previous']):
        errors.append('Changed previous records lack exactly one failure on the same path/variant')
    if any(events['previous'][key] != 1 for key in events['failure'] if key in baseline_by_variant):
        errors.append('Baseline-bearing failure lacks exactly one previous error on the same path/variant')
    new_variants = set(events['failure']) - set(events['previous'])
    if new_variants & set(baseline_by_variant):
        errors.append('New event occurs on an unchanged baseline path/variant')
    if any(not (p['signature'][len('strict mode: '):] if p['strict'] else p['signature']).startswith('unexpected error: ')
           for p in observations if p['status'] == 'observed-failure' and
           (p['path'], p['strict']) in events['previous']):
        errors.append('Changed pairing requires the production unexpected-error record')
    if set(events['fixed']) & set(events['failure']):
        errors.append('Fixed and failure records coexist for the same variant')
    changed = sum(events['previous'].values())
    new = sum(events['failure'].values()) - changed
    fixed = sum(events['fixed'].values())
    # Baseline variants are references only; matching runtime messages are suppressed.
    inferred_variants = set(baseline_by_variant) - set(events['fixed']) - set(events['failure'])
    observed_paths = {key[0] for key in events['failure']}
    inferred_paths = {key[0] for key in inferred_variants}
    counts = dict(new=new, changed=changed, fixed=fixed)
    multiset = {kind: [dict(path=k[0], strict=k[1], occurrences=n)
                      for k, n in sorted(value.items())] for kind, value in events.items()}
    return observations, unknown, errors, counts, multiset, observed_paths, inferred_paths

def classify_tool_records(stdout, stderr):
    records = []
    for stream, content in (("stdout", stdout), ("stderr", stderr)):
        for number, literal in enumerate(content.splitlines(), 1):
            for kind, pattern in SANITIZER_RECORDS:
                if pattern.search(literal):
                    records.append(dict(kind=kind, stream=stream,
                                        raw_log_line=number, literal=literal))
                    break
    return records


def reconcile(stdout, stderr, returncode, baseline_records, runnable,
              distinct_source_count):
    counters, errors, results = parse_result(stdout, stderr, distinct_source_count)
    observations, unknown, event_errors, events, multisets, actual, inferred = \
        observation_evidence(stdout, stderr, baseline_records, runnable)
    errors.extend(event_errors)
    keys = [(p["path"], p["strict"]) for p in baseline_records]
    if len(keys) != len(set(keys)):
        errors.append("Duplicate baseline path/variant reference")
    if unknown:
        errors.append("Unreviewed unknown-feature warning")
    expected_rc = int(bool(events["new"] or events["changed"] or events["fixed"]))
    if type(returncode) is not int or returncode != expected_rc:
        errors.append("Actual returncode differs from production event status")
    failed_paths = actual | inferred
    if counters is not None:
        if counters["executed_source_count"] + counters["excluded"] + counters["skipped"] != distinct_source_count:
            errors.append("Source denominator does not reconcile")
        if counters["failed_source_count"] != len(failed_paths):
            errors.append("Failure-path count does not reconcile")
        if any(counters[key] != events[key] for key in events):
            errors.append("Result events differ from actual observation occurrences")
    blocking = classify_tool_records(stdout, stderr)
    return dict(counters=counters, raw_runner_returncode=returncode,
                expected_event_returncode=expected_rc, result_records=results,
                raw_observations=observations, unknown_feature_records=unknown,
                variant_observation_multisets=multisets,
                observed_failure_paths=sorted(actual),
                baseline_matched_inferred_paths=sorted(inferred),
                baseline_references_are_fresh_runtime_observations=False,
                reconciliation_errors=errors, blocking_tool_records=blocking,
                raw_parser_complete=counters is not None and not errors and not blocking,
                raw_collection_complete=False, sealed_inputs_unchanged=False,
                runtime_accepted=False, hosted_accepted=False,
                conformance_accepted=False, failure_waiver=False)


def match_source_triage(parsed, triage_records, fixture_hashes):
    """Compare source audit identities only; a match conveys no adjudication."""
    index = {r["path"]: r for r in triage_records}
    errors = []
    if len(index) != len(triage_records):
        errors.append("Duplicate source-triage path")
    matches = []
    for observation in parsed["raw_observations"]:
        if observation["status"] != "observed-failure":
            continue
        entry = index.get(observation["path"])
        if entry is None:
            errors.append("Untriaged failure: " + observation["path"])
            continue
        expected = entry["observed_first_failure"]
        if any(observation[key] != expected[key] for key in ("path", "line", "strict", "signature")):
            errors.append("Changed first-failure identity: " + observation["path"])
            continue
        if fixture_hashes.get(observation["path"]) != entry["fixture_identity"]["reviewed_overlay_sha256"]:
            errors.append("Changed fixture identity: " + observation["path"])
            continue
        matches.append(observation["path"])
    return dict(source_triage_matched_paths=sorted(matches), source_triage_errors=errors,
                final_profile_diagnostic_adjudicated=False, runtime_accepted=False,
                hosted_accepted=False, conformance_accepted=False, failure_waiver=False)
