"""Exact native diagnostic qualification. Raw failing outcomes are preserved.

This module is a proposed policy verifier, not a source triage allowlist.
The committed draft authorizes nothing. Root must bind final actual observations,
review every remaining file, and integrate the independent witness validator.
"""
import hashlib
import copy
import json
import re
from native_normative_scope import current_mandatory_review
from core_baseline_transition import review_baseline_matches
from slow_test_selection import SlowTestSelection

IDENTITY_FIELDS = ("path", "strict", "line", "signature")
BINDING_FIELDS = ("production_source_map_sha256", "fixture_source_map_sha256",
                  "config_sha256", "baseline_sha256", "effective_profile_sha256")
MANDATORY_OBLIGATIONS = ("mandatory-repairs-L1-T1-F1-F2", "host-default-H1",
                         "exact-equal-range-collapse", "complete-original-assertions")


def demand(value, reason):
    if not value:
        raise ValueError(reason)


def identity(record):
    demand(type(record.get("strict")) is bool and type(record.get("line")) is int,
           "Invalid original variant/line")
    demand(record["line"] > 0 and isinstance(record.get("signature"), str),
           "Missing original failure signature")
    demand(isinstance(record.get("path"), str) and
           re.fullmatch(r"test262/test/.+\.js", record["path"]) and
           ".." not in record["path"].split("/"), "Unsafe original fixture path")
    return tuple(record[key] for key in IDENTITY_FIELDS)


def bound_json(binding, load_bytes):
    demand(isinstance(binding, dict) and set(binding) == {"path", "sha256"},
           "Missing exact evidence binding")
    demand(isinstance(binding["path"], str) and not binding["path"].startswith("/")
           and ".." not in binding["path"].split("/"), "Unsafe evidence path")
    demand(isinstance(binding["sha256"], str) and
           re.fullmatch(r"[0-9a-f]{64}", binding["sha256"]), "Missing evidence SHA256")
    raw = load_bytes(binding["path"])
    demand(hashlib.sha256(raw).hexdigest() == binding["sha256"], "Changed evidence bytes")
    return json.loads(raw)


def same_bindings(document, context):
    demand(all(document.get(key) == context[key] for key in BINDING_FIELDS),
           "Evidence does not bind the actual production source/config/profile/fixture")


def expected_trace_engine(trace, context):
    """The one source-bound links diagnostic never replaces ordinary raw identity."""
    fixture = 'test262/test/intl402/Temporal/ZonedDateTime/links.js'
    role = trace.get('selected_provider_diagnostic_role')
    if trace.get('fixture') != fixture:
        demand(role is None, 'Provider diagnostic role attached to another fixture')
        return context['qjs_sha256']
    diagnostic = context.get('selected_provider_diagnostic')
    demand(isinstance(diagnostic, dict) and isinstance(role, dict) and
           diagnostic.get('schema') == 'qjs-original-selected-provider-diagnostic-v1' and
           role.get('schema') == 'qjs-selected-provider-trace-role-v1' and
           role.get('fixture') == fixture and
           role.get('fixture_sha256') == trace.get('fixture_sha256') == diagnostic.get('fixture_sha256') and
           diagnostic.get('normal_policy') == 'system-first' and
           diagnostic.get('normal_engine_sha256') == role.get('ordinary_engine_sha256') == context['qjs_sha256'] and
           diagnostic.get('engine_sha256') == role.get('diagnostic_engine_sha256') and
           diagnostic.get('engine_sha256') != context['qjs_sha256'] and
           diagnostic.get('source_tree_sha256') == role.get('source_tree_sha256') == context['production_source_map_sha256'] and
           diagnostic.get('profile_sha256') == role.get('profile_sha256') == context['effective_profile_sha256'] and
           diagnostic.get('compiler_configuration_sha256') == role.get('compiler_configuration_sha256') and
           diagnostic.get('preflight_input_proof_sha256') == role.get('preflight_input_proof_sha256') and
           isinstance(role.get('actual_comparison_canonical_sha256'), str) and
           re.fullmatch(r'[0-9a-f]{64}', role['actual_comparison_canonical_sha256']) and
           all(record.get('conformance_accepted') is False and
               record.get('semantic_adjudication_complete') is False for record in (role, diagnostic)),
           'Missing authenticated original selected-provider diagnostic engine role')
    return diagnostic['engine_sha256']


def exact_current_capture(trace, context):
    """Recompute script bindings and aggregate fields from both current variants."""
    binding = trace.get('current_capture_binding')
    demand(isinstance(binding, dict), 'Missing exact current capture binding')
    fields = ('engine_sha256', 'trace_sha256', 'generated_sha256', 'config_sha256')
    if binding.get('capture_kind') == 'script-variants':
        variants = trace.get('variant_summaries')
        demand(isinstance(variants, dict) and set(variants) == {'strict', 'nonstrict'},
               'Missing both exact current original variants')
        for name, item in variants.items():
            demand(item.get('variant') == name and item.get('fixture_capture_complete') is True and
                   item.get('original_eof_reached') is True and item.get('mandatory_failed_site_ids') == [],
                   'Current original variant incomplete or mandatory failure')
            demand(item.get('engine_sha256') == expected_trace_engine(item, context) and
                   item.get('source_tree_sha256') == context['production_source_map_sha256'] and
                   item.get('profile_sha256') == context['effective_profile_sha256'] and
                   item.get('fixture_sha256') == trace['fixture_sha256'],
                   'Current variant source/engine/profile/fixture differs')
            demand(all(isinstance(item.get(key), str) and re.fullmatch(r'[0-9a-f]{64}', item[key])
                       for key in fields), 'Missing exact current variant capture hashes')
            demand(type(item.get('totalized_original_data_obligations')) is int and
                   item['totalized_original_data_obligations'] >= 0 and
                   type(item.get('all_original_expressions_evaluated')) is bool,
                   'Missing current variant completeness/totalization')
        expected = dict(capture_kind='script-variants', variant_bindings={
            name: {key: item[key] for key in fields} for name, item in sorted(variants.items())})
        demand(binding == expected, 'Current variant capture binding differs')
        demand(trace.get('continued_data_site_ids') == sorted({site for item in variants.values()
                   for site in item['continued_data_site_ids']}) and
               trace.get('totalized_original_data_obligations') == sorted({site for item in variants.values()
                   for site in item['totalized_original_data_site_ids']}) and
               trace.get('totalized_original_data_obligation_count') == sum(
                   item['totalized_original_data_obligations'] for item in variants.values()) and
               trace.get('all_original_expressions_evaluated') is all(
                   item['all_original_expressions_evaluated'] is True for item in variants.values()),
               'Current aggregate omits an original variant obligation')
        demand('raw_failure_identity' in trace, 'Missing original raw failure identity field')
        raw_identity = trace['raw_failure_identity']
        if raw_identity is None:
            selected = 'nonstrict'
        else:
            demand(isinstance(raw_identity, dict), 'Invalid original raw failure identity')
            identity(raw_identity)
            selected = 'strict' if raw_identity['strict'] else 'nonstrict'
        demand(trace.get('variant') == selected and all(trace.get(key) == variants[selected][key]
                   for key in fields), 'Raw-selected current variant identity differs')
    else:
        required = {'capture_kind', 'engine_sha256', 'trace_sha256', 'sampler_sha256',
                    'command_sha256', 'current_bindings_sha256'}
        demand(set(binding) == required and binding['capture_kind'] == 'rtf-variant-neutral' and
               trace.get('strict_variant_neutral_capture') is True,
               'Missing fresh complete current RTF capture binding')
        demand(all(isinstance(binding[key], str) and re.fullmatch(r'[0-9a-f]{64}', binding[key])
                   for key in required - {'capture_kind'}), 'Missing current RTF hashes')
        demand(binding['engine_sha256'] == trace['engine_sha256'] == context['qjs_sha256'] and
               binding['trace_sha256'] == trace['trace_sha256'] and
               binding['sampler_sha256'] == trace['generated_sha256'],
               'Fresh current RTF engine/trace/sampler binding differs')
    return binding



def semantic_row_evidence(trace, review_binding, review, helper_derivation=None):
    """Consume fingerprints recomputed by the authenticated current projector.

    validated_witness is a trusted output of the pinned projector invocation,
    never an arbitrary caller document. The historical review stays immutable.
    """
    if helper_derivation is not None:
        from helper_source_derivation import HelperSourceDerivation
        demand(type(helper_derivation) is HelperSourceDerivation,
               'Untrusted helper source derivation')
        review = helper_derivation.derive_review(review)
    actual = trace.get('stable_observation')
    demand(isinstance(actual, dict) and set(actual) == {
        'schema', 'capture_kind', 'source_bindings', 'observations'} and
        actual['schema'] == 'native-complete-stable-observation-v2' and
        actual['capture_kind'] == trace['current_capture_binding']['capture_kind'],
        'Missing complete current semantic fingerprint')
    sources = actual['source_bindings']
    demand(isinstance(sources, dict) and sources and all(
        isinstance(name, str) and isinstance(pin, str) and re.fullmatch(r'[0-9a-f]{64}', pin)
        for name, pin in sources.items()), 'Missing stable witness/source bindings')
    observations = actual['observations']
    if actual['capture_kind'] == 'script-variants':
        demand(isinstance(observations, dict) and set(observations) == {'strict', 'nonstrict'},
               'Missing both complete stable original variants')
        for item in observations.values():
            demand(isinstance(item, dict) and set(item) == {
                'events_sha256', 'event_count', 'stable_configuration_sha256'} and
                type(item['event_count']) is int and item['event_count'] > 0 and all(
                    isinstance(item[key], str) and re.fullmatch(r'[0-9a-f]{64}', item[key])
                    for key in ('events_sha256', 'stable_configuration_sha256')),
                'Incomplete ordered events or stable configuration fingerprint')
    else:
        demand(actual['capture_kind'] == 'rtf-variant-neutral' and
               isinstance(observations, dict) and set(observations) == {'whole_parsed_rtf_sha256'} and
               isinstance(observations['whole_parsed_rtf_sha256'], str) and
               re.fullmatch(r'[0-9a-f]{64}', observations['whole_parsed_rtf_sha256']),
               'Missing whole parsed current RTF fingerprint')
    demand(review.get('stable_observation_expectation') == actual,
           'Complete ordered semantic observations/configuration/source changed; bounded review required')
    demand(isinstance(review.get('current_capture_binding'), dict),
           'Missing historical first qualification capture identity')
    result = dict(schema='native-current-semantic-row-evidence-v1',
        origin='mechanically-derived-current-regression-evidence',
        stable_review_binding=copy.deepcopy(review_binding),
        historical_capture_binding=copy.deepcopy(review['current_capture_binding']),
        stable_observation=copy.deepcopy(actual),
        current_capture_binding=copy.deepcopy(trace['current_capture_binding']))
    if helper_derivation is not None:
        result['helper_source_derivation'] = helper_derivation.receipt()
    return result


def reviewed_entry(entry, context, fixture_hashes, witness, load_bytes):
    key = identity(entry)
    demand(entry.get("disposition") == "EXACT_PROFILE_DIAGNOSTIC_REVIEWED" and
           entry.get("classification") == "NORMATIVE_DATA_PROFILE_DIFFERENCE" and
           entry.get("raw_outcome") == "FAIL", "Mandatory or unclassified failure")
    demand(entry.get("fixture_sha256") == fixture_hashes.get(entry["path"]),
           "Changed original fixture")
    review = bound_json(entry.get("review_binding"), load_bytes)
    stable = 'stable_observation_expectation' in review
    if stable:
        demand(all(review_baseline_matches(review.get(field), context)
                   if field == 'baseline_sha256' else review.get(field) == context[field]
                   for field in BINDING_FIELDS if field != 'production_source_map_sha256'),
               'Stable review fixture/config/baseline/profile differs')
        demand(isinstance(review.get('production_source_map_sha256'), str) and
               re.fullmatch(r'[0-9a-f]{64}', review['production_source_map_sha256']),
               'Missing unchanged historical production identity')
    else:
        same_bindings(review, context)
    demand(review.get("schema") == "native-original-assertion-review-v2" and
           identity(review) == key and review.get("fixture_sha256") == entry["fixture_sha256"],
           "Wrong per-file original assertion review")
    demand(review.get("complete_original_assertions_reviewed") is True and
           review.get("mandatory_semantics_satisfied") is True and
           review.get("unclassified_assertion_ids") == [],
           "Original assertions are incomplete or mandatory semantics unresolved")
    proofs = review.get("normative_proofs")
    demand(isinstance(proofs, list) and proofs, "Missing per-file normative proof")
    for proof in proofs:
        demand(isinstance(proof.get("revision"), str) and
               re.fullmatch(r"[0-9a-f]{40}", proof["revision"]) and proof.get("anchor"),
               "Unpinned normative revision/anchor")
        excerpt = proof.get("excerpt_binding")
        demand(isinstance(excerpt, dict) and set(excerpt) == {"path", "sha256"},
               "Missing normative excerpt binding")
        demand(not excerpt["path"].startswith("/") and ".." not in excerpt["path"].split("/"),
               "Unsafe normative excerpt path")
        demand(hashlib.sha256(load_bytes(excerpt["path"])).hexdigest() == excerpt["sha256"],
               "Changed normative excerpt")
    trace = witness.get(entry["path"])
    demand(isinstance(trace, dict) and trace.get("schema") == "native-original-assertion-trace-v2",
           "Missing independently revalidated original assertion trace")
    demand(trace.get("fixture_capture_complete") is True and
           trace.get("original_eof_reached") is True and trace.get("mandatory_failures") == [] and
           trace.get("mandatory_failed_site_ids") == [],
           "Missing original EOF or failing mandatory assertion")
    demand(isinstance(trace.get("raw_failure_identity"), dict) and
           identity(trace["raw_failure_identity"]) == key, "Wrong original strict variant or raw failure identity")
    demand(trace.get("fixture_sha256") == entry["fixture_sha256"] and
           trace.get("source_tree_sha256") == context["production_source_map_sha256"] and
           trace.get("engine_sha256") == expected_trace_engine(trace, context) and
           trace.get("profile_sha256") == context["effective_profile_sha256"],
           "Original assertion witness is for another source/engine/profile")
    current_capture = exact_current_capture(trace, context)
    if stable:
        scopes = context.get('native_normative_scopes')
        expected = semantic_row_evidence(trace, entry['review_binding'], review,
            scopes.helper_derivation() if scopes is not None else None)
        demand(trace.get('current_row_evidence') == expected,
               'Derived current row evidence differs from authenticated stable review and exact current capture')
    else:
        demand(review.get("current_capture_binding") == current_capture,
               "Per-file normative review does not bind exact current engine/trace/generated/config capture")
    actual_sites = trace.get("continued_data_site_ids")
    demand(isinstance(actual_sites, list), "Missing actual continued data assertion sites")
    allowed = review.get("reviewed_data_mismatch_site_ids")
    demand(isinstance(allowed, list) and len(allowed) == len(set(allowed)),
           "Missing unique reviewed data assertion sites")
    demand(set(actual_sites) == set(allowed),
           "New, disappeared or unclassified original data assertion mismatch")
    totalized = trace.get("totalized_original_data_obligations", [])
    if trace.get("all_original_expressions_evaluated") is not True:
        demand(review.get("data_only_array_totalization_reviewed") is True and totalized,
               "Original expressions incomplete without exact reviewed pure-data adapter")
        demand(set(totalized) == set(review.get("approved_totalized_original_data_obligation_site_ids", [])),
               "Unreviewed totalized original data site")
        adapters = review.get("approved_array_adapter_bindings")
        demand(isinstance(adapters, list) and adapters, "Unbound original-array adapter")
        for binding in adapters:
            demand(set(binding) == {"path", "sha256"} and not binding["path"].startswith("/")
                   and ".." not in binding["path"].split("/"), "Unsafe data adapter binding")
            demand(hashlib.sha256(load_bytes(binding["path"])).hexdigest() == binding["sha256"],
                   "Changed approved pure-data array adapter")
    else:
        demand(totalized == [], "Expression-complete trace unexpectedly totalized original data")
    return key


def qualify_native(parsed, policy, context, fixture_hashes, validated_witness,
                   load_bytes, selection=None):
    """Require current raw identities + normative reviews + validated full trace.

    validated_witness must come from the authenticated practical validator's
    current execution, including the dependency-authenticated RTF proof.
    Returning qualification never rewrites Test262 rc, counters or FAIL statuses.
    """
    demand(parsed.get("raw_parser_complete") is True and
           parsed.get("raw_collection_complete") is True and
           parsed.get("sealed_inputs_unchanged") is True,
           "Missing raw collection/parser/input-integrity proof")
    demand(not parsed.get("blocking_tool_records") and
           not parsed.get("unknown_feature_records"), "SAN/infra/unknown-feature record")
    counters = parsed["counters"]
    demand(counters["changed"] == 0 and counters["fixed"] == 0,
           "Baseline signature changed or fixed; independent review required")
    demand(policy.get("schema") == "native-exact-profile-policy-v2" and
           policy.get("profile_diagnostic_authorized") is True,
           "Native policy remains unreviewed REQUIRED draft")
    demand(policy.get("failure_waiver") is False and policy.get("conformance_accepted") is False,
           "Failure waiver or conformance claim")
    slow_selection = context.get("slow_test_selection")
    if slow_selection is None:
        demand(policy.get("no_new_skips_exclusions_expected_errors") is True and
               "authorized_slow_exclusions" not in policy,
               "Changed skip policy without an authenticated transition")
    else:
        demand(type(slow_selection) is SlowTestSelection and
               policy.get("no_new_skips_exclusions_expected_errors") is False,
               "Untrusted slow selection transition")
        slow_selection.check_policy(policy, context)
    same_bindings(policy, context)
    demand(context.get("mandatory_repository_gates_passed") is True,
           "Mandatory repository/API checks failed or missing")
    bindings = policy.get("witness_source_bindings")
    demand(isinstance(bindings, list) and bindings, "Unbound witness implementation/helpers")
    for binding in bindings:
        # Implementations are raw source, not JSON, but paths/hash rules are identical.
        demand(set(binding) == {"path", "sha256"} and not binding["path"].startswith("/")
               and ".." not in binding["path"].split("/"), "Unsafe witness source binding")
        demand(hashlib.sha256(load_bytes(binding["path"])).hexdigest() == binding["sha256"],
               "Changed witness helper/configuration")
    obligations = policy.get("mandatory_obligations", [])
    demand({r.get("id") for r in obligations} == set(MANDATORY_OBLIGATIONS) and
           len(obligations) == len(MANDATORY_OBLIGATIONS), "Missing/duplicate mandatory obligation")
    for obligation in obligations:
        demand(obligation.get("status") == "SATISFIED" and obligation.get("proof_bindings"),
               "Unresolved mandatory obligation")
        for binding in obligation["proof_bindings"]:
            proof = current_mandatory_review(binding, context, load_bytes)
            same_bindings(proof, context)
            demand(proof.get("mandatory_semantics_satisfied") is True,
                   "Mandatory obligation review does not satisfy semantics")
    demand(validated_witness.get("schema") == "native-original-assertion-capture-summary-v2"
           and validated_witness.get("full_capture_complete") is True
           and validated_witness.get("independent_revalidation_complete") is True,
           "Incomplete or unvalidated current original assertion capture")
    demand(validated_witness.get("conformance_accepted") is False,
           "Assertion capture cannot grant conformance authority")
    witness = validated_witness.get("fixture_summaries", {})
    demand(isinstance(witness, dict), "Missing per-file validated summaries")
    entries = policy.get("adjudicated_diagnostic_entries", [])
    index = {identity(entry): entry for entry in entries}
    demand(len(index) == len(entries), "Duplicate reviewed original path/variant/signature")
    if selection is None:
        expected_counts = policy.get("postfix_raw_counters")
        expected_rc = policy.get("postfix_raw_returncode")
        expected_keys = set(index)
    else:
        item = policy.get("required_selections", {}).get(selection)
        demand(isinstance(item, dict), "Required source selection lacks exact review")
        expected_counts, expected_rc = item.get("raw_counters"), item.get("raw_returncode")
        expected_keys = {identity(record) for record in item.get("failure_identities", [])}
        demand(expected_keys <= set(index), "Selection admits a nonreviewed failure")
    demand(counters == expected_counts and parsed["raw_runner_returncode"] == expected_rc,
           "Actual post-fix raw counters/status changed; re-review required")
    actual = {identity(record) for record in parsed["raw_observations"]
              if record["status"] == "observed-failure"}
    demand(actual == expected_keys, "Missing/new/changed/unclassified failure identity")
    for key in actual:
        reviewed_entry(index[key], context, fixture_hashes, witness, load_bytes)
    result = dict(schema="native-exact-profile-qualification-v2",
                profile_diagnostic_qualified=True, raw_runner_returncode=parsed["raw_runner_returncode"],
                raw_counters=counters, raw_failures_preserved=True,
                all_original_expressions_evaluated=all(witness[path].get("all_original_expressions_evaluated") is True
                                                       for path in {key[0] for key in actual}),
                exact_reviewed_failed_source_count=len(actual),
                conformance_accepted=False, failure_waiver=False,
                basis="Current exact observations, complete original assertions and per-file normative reviews")
    if slow_selection is not None and selection is None:
        result["authorized_slow_exclusions"] = slow_selection.receipt()
    return result

