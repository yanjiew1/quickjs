"""Pure adversarial streaming cases. Source prepared; only root executes."""
from pathlib import Path
import copy
import hashlib
import json
import sqlite3
import sys
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from premise_trace_proof import UNDEFINED
from trace_stream_test_fixtures import trace, config_for
from validate_practical_witness import WitnessInvalid, config_digest, validate_fixture
from trace_stream import (DATABASE_LIMIT, DEFAULT_OUTPUT_LIMIT, EventLines,
                          EventSet, LUNISOLAR_FIXTURE, LUNISOLAR_SHA256,
                          LUNISOLAR_OUTPUT_LIMIT, LUNISOLAR_RECORD_LIMIT,
                          MAX_EVENTS, ReceiptSet, ReceiptStore,
                          TraceBoundInvalid, hash_file, trace_limits)


def bound(case):
    records, config, command, raw = copy.deepcopy(case)
    config['_config_sha256'] = config_digest(config)
    command['config_sha256'] = config['_config_sha256']
    for index, event in enumerate(records, 1):
        sid = event['site']
        event.update(sequence=index, schema='native-original-assertion-trace-v2',
            site_source_sha256=config['sites'][sid]['source_sha256'] if sid else None,
            config_sha256=config['_config_sha256'], conformance_accepted=False,
            semantic_adjudication_complete=False,
            **{k: config[k] for k in ('fixture', 'fixture_sha256', 'variant',
                                      'engine_sha256', 'source_tree_sha256', 'profile_sha256')})
    data = ''.join(json.dumps(event) + '\n' for event in records).encode('utf-8')
    command['stdout_sha256'] = hashlib.sha256(data).hexdigest()
    return data, config, command, raw


class StreamTests(unittest.TestCase):
    def test_lazy_generator_consumes_only_current_record(self):
        consumed = []
        def lines():
            for i in range(10000):
                consumed.append(i)
                yield ('{"i":%d}\n' % i).encode()
        stream = EventLines(lines(), {})
        iterator = iter(stream)
        self.assertEqual(next(iterator), {'i': 0})
        self.assertEqual(consumed, [0])

    def test_exact_crlf_and_non_ascii_hash(self):
        raw = '{"word":"月"}\r\n{"last":true}'.encode()
        stream = EventLines(raw.splitlines(keepends=True), {})
        self.assertEqual(list(stream), [{'word': '月'}, {'last': True}])
        stream.verify(hashlib.sha256(raw).hexdigest())
        with self.assertRaises(TraceBoundInvalid):
            stream.verify(hashlib.sha256(raw.replace(b'\r\n', b'\n')).hexdigest())

    def test_event_budget_rejects_without_loading_remainder(self):
        consumed = []
        def lines():
            for i in range(10000):
                consumed.append(i)
                yield b'{}\n'
        with self.assertRaises(TraceBoundInvalid):
            list(EventLines(lines(), {'max_events': 2}))
        self.assertEqual(consumed, [0, 1, 2])

    def test_global_event_ceiling_cannot_be_raised_by_config(self):
        for value in (True, 0, -1, 2000001, 2.0, '2000000'):
            with self.subTest(value=value), self.assertRaises(TraceBoundInvalid):
                EventLines([], {'max_events': value})

    def test_only_exact_lunisolar_hash_has_larger_allowance(self):
        config = dict(fixture=LUNISOLAR_FIXTURE, fixture_sha256=LUNISOLAR_SHA256)
        self.assertEqual(trace_limits(config), (LUNISOLAR_OUTPUT_LIMIT, LUNISOLAR_RECORD_LIMIT))
        self.assertLess(LUNISOLAR_OUTPUT_LIMIT, 2 * 1024**3)
        self.assertEqual(LUNISOLAR_OUTPUT_LIMIT, 3939 * 512 * 1024 + 16 * 1024**2)
        config['fixture_sha256'] = '0' * 64
        self.assertEqual(trace_limits(config)[0], DEFAULT_OUTPUT_LIMIT)
        config['fixture_sha256'] = LUNISOLAR_SHA256
        config['fixture'] = 'other.js'
        self.assertEqual(trace_limits(config)[0], DEFAULT_OUTPUT_LIMIT)

    def test_bounded_file_read_rejects_large_record(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'trace.log'
            path.write_bytes(b'{"text":"' + b'x' * 70000 + b'"}\n')
            stream = EventLines(path, dict(fixture=LUNISOLAR_FIXTURE,
                                          fixture_sha256=LUNISOLAR_SHA256))
            with self.assertRaises(TraceBoundInvalid):
                list(stream)
            self.assertEqual(stream.bytes_seen, LUNISOLAR_RECORD_LIMIT + 1)

    def test_total_byte_budget_is_enforced(self):
        stream = EventLines([b'{}\n', b'{}\n'], {})
        stream.byte_limit = 5
        with self.assertRaises(TraceBoundInvalid):
            list(stream)

    def test_blank_scalar_invalid_utf8_bom_and_partial_json_rejected(self):
        for raw in (b'\n', b'null\n', b'{oops}\n', b'{"v":"\xff"}\n',
                    b'\xef\xbb\xbf{}\n', b'{"v":'):
            with self.subTest(raw=raw), self.assertRaises(TraceBoundInvalid):
                list(EventLines([raw], {}))

    def test_hash_cannot_be_verified_before_eof(self):
        stream = EventLines([b'{}\n', b'{}\n'], {})
        iterator = iter(stream)
        next(iterator)
        with self.assertRaises(TraceBoundInvalid):
            stream.verify(hashlib.sha256(b'{}\n').hexdigest())

    def test_stream_cannot_be_replayed(self):
        stream = EventLines([b'{}\n'], {})
        list(stream)
        with self.assertRaises(TraceBoundInvalid):
            list(stream)

    def test_hash_file_reads_bounded_chunks(self):
        raw = b'x' * (2 * 1024 * 1024 + 7)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'trace'
            path.write_bytes(raw)
            with patch.object(Path, 'read_bytes', side_effect=AssertionError('whole-file read')):
                self.assertEqual(hash_file(path), hashlib.sha256(raw).hexdigest())


class IndexTests(unittest.TestCase):
    def test_event_ids_use_bounded_bit_storage_and_preserve_references(self):
        ids = EventSet(MAX_EVENTS)
        for key in (1, 8, MAX_EVENTS):
            ids.add(key)
            ids.add(key)
        self.assertEqual(len(ids), 3)
        self.assertLessEqual(len(ids.bits), 250001)
        self.assertEqual(list(ids), [1, 8, MAX_EVENTS])
        for key in (True, None, 0, MAX_EVENTS + 1, 1.0):
            self.assertNotIn(key, ids)
            with self.assertRaises(TraceBoundInvalid):
                ids.add(key)

    def test_receipts_survive_cache_eviction_and_preserve_private_types(self):
        with ReceiptStore(UNDEFINED) as store:
            receipts = store.mapping('native')
            for i in range(40):
                receipts[i] = {'value': [i, UNDEFINED], 'invariants': {'a', 'b'},
                               'tuple': (i, 'field'), 'user': {'undefined': ['scalar', 'fake']}}
            self.assertEqual(len(receipts.cache), 8)
            self.assertEqual(len(receipts), 40)
            value = receipts[0]
            self.assertIs(value['value'][1], UNDEFINED)
            self.assertEqual(value['invariants'], {'a', 'b'})
            self.assertEqual(value['tuple'], (0, 'field'))
            self.assertEqual(value['user'], {'undefined': ['scalar', 'fake']})
            self.assertEqual(set(receipts), set(range(40)))
            value['updated'] = True
            receipts[0] = value
            for i in range(20, 40):
                receipts[i]
            self.assertTrue(receipts[0]['updated'])

    def test_receipt_namespaces_and_totalization_keys_do_not_alias(self):
        with ReceiptStore(UNDEFINED) as store:
            a = store.mapping('a')
            b = store.mapping('b')
            a[1] = 'a'
            b[1] = 'b'
            keys = ReceiptSet(store, 'keys')
            key = (1, 'site', 3, 'value')
            keys.add(key)
            keys.add(key)
            self.assertEqual(a[1], 'a')
            self.assertEqual(b[1], 'b')
            self.assertEqual(len(keys), 1)
            self.assertEqual(list(keys), [key])
            keys.discard(key)
            self.assertNotIn(key, keys)

    def test_private_database_cleanup_on_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                with ReceiptStore(UNDEFINED, directory) as store:
                    private_path = Path(store.temporary.name)
                    store.mapping('x')[1] = {'large': 'x' * 10000}
                    raise ValueError('mandatory rejection')
            self.assertFalse(private_path.exists())
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_private_database_has_finite_page_and_cache_limits(self):
        with ReceiptStore(UNDEFINED) as store:
            con = store.connection
            self.assertEqual(con.execute('PRAGMA max_page_count').fetchone()[0] * 4096, DATABASE_LIMIT)
            self.assertEqual(con.execute('PRAGMA cache_size').fetchone()[0], -2048)
            self.assertEqual(con.execute('PRAGMA mmap_size').fetchone()[0], 0)

    def test_database_exhaustion_fails_closed(self):
        with ReceiptStore(UNDEFINED, database_limit=16384) as store:
            with self.assertRaises(sqlite3.Error):
                store.mapping('large')[1] = 'x' * 1000000


class WholeFixtureTests(unittest.TestCase):
    def streamed(self, case):
        data, config, command, raw = bound(case)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'trace.log'
            path.write_bytes(data)
            with (patch.object(Path, 'read_text', side_effect=AssertionError('whole-file text')),
                  patch.object(Path, 'read_bytes', side_effect=AssertionError('whole-file bytes'))):
                result = validate_fixture(path, config, command, raw)
            self.assertEqual(list(Path(directory).iterdir()), [path])
            return result

    def test_streamed_guarded_unavailable_trace_matches_text_summary(self):
        case = trace()
        data, config, command, raw = bound(case)
        self.assertEqual(self.streamed(case), validate_fixture(data.decode(), config, command, raw))

    def test_guarded_failure_mandatory_obligations_and_raw_score_preserved(self):
        result = self.streamed(trace(unavailable=False))
        self.assertEqual(result['data_mismatches'], 1)
        self.assertFalse(result['semantic_adjudication_complete'])
        self.assertFalse(result['conformance_accepted'])

    def test_old_original_receipt_and_guard_survive_many_later_native_results(self):
        case = trace()
        records = case[0]
        at = next(i for i, e in enumerate(records) if e['kind'] == 'invariant')
        original_guard = next(i + 1 for i, e in enumerate(records) if e['kind'] == 'premise-guard')
        formatter = next(i + 1 for i, e in enumerate(records) if e.get('role') == 'native-result' and e['site'] == 'f')
        original_parts = next(e['value'] for e in records if e.get('role') == 'native-result' and e['site'] == 'p')
        extra = []
        for i in range(40):
            entry = at + 1 + 3 * i
            extra.extend([
                dict(kind='enter', site='p', operation='call', parent=2, arguments=[]),
                dict(kind='value', site='p', operation_entry=entry, role='native-result',
                     value=copy.deepcopy(original_parts)),
                dict(kind='exit', site='p', operation='call', entry=entry, completion='normal',
                     result_event=entry + 1, arguments=[{'kind': 'number', 'value': '0'}],
                     receiver={'kind': 'call-reference', 'call_event': formatter}),
            ])
        records[at:at] = extra
        next(e for e in records if e['kind'] == 'unavailable-obligation')['guard_event'] = original_guard + len(extra)
        records[-1]['site_counts']['p'] += 40
        self.assertEqual(self.streamed(case)['site_counts']['p'], 41)

    def test_sequence_tampering_rejected(self):
        data, config, command, raw = bound(trace())
        lines = data.splitlines(keepends=True)
        lines[1], lines[2] = lines[2], lines[1]
        command['stdout_sha256'] = hashlib.sha256(b''.join(lines)).hexdigest()
        with self.assertRaises(WitnessInvalid):
            validate_fixture(iter(lines), config, command, raw)

    def test_completion_must_be_last_not_just_present(self):
        data, config, command, raw = bound(trace())
        extra = json.loads(data.splitlines()[-1])
        extra['sequence'] += 1
        data += (json.dumps(extra) + '\n').encode()
        command['stdout_sha256'] = hashlib.sha256(data).hexdigest()
        with self.assertRaises(WitnessInvalid):
            validate_fixture(iter(data.splitlines(keepends=True)), config, command, raw)

    def test_truncation_and_hash_tampering_rejected(self):
        data, config, command, raw = bound(trace())
        with self.assertRaises(WitnessInvalid):
            validate_fixture(iter(data.splitlines(keepends=True)[:-1]), config, command, raw)
        command['stdout_sha256'] = '0' * 64
        with self.assertRaises(WitnessInvalid):
            validate_fixture(iter(data.splitlines(keepends=True)), config, command, raw)

    def test_missing_mandatory_branch_invariant_is_not_waived(self):
        case = trace()
        case[0][:] = [e for e in case[0] if e.get('name') != 'premise-original-parts-origin']
        with self.assertRaises(WitnessInvalid):
            self.streamed(case)

    def test_duplicate_guard_consumption_rejected(self):
        case = trace()
        original = next(e for e in case[0] if e['kind'] == 'unavailable-obligation')
        case[0].insert(-2, copy.deepcopy(original))
        with self.assertRaises(WitnessInvalid):
            self.streamed(case)

    def expected_throw_case(self, passed):
        _, config, command, raw = trace()
        config['sites']['t'] = dict(kind='assertion', source='assert.throws(TypeError, fn)',
            source_sha256='9' * 64, assertion_method='throws', continue_exact_data_assertion=True)
        records = [
            dict(kind='start', site=None, original_entry_reached=True),
            dict(kind='enter', site='t', operation='assertion', parent=None, arguments=[]),
            dict(kind='enter', site='p', operation='call', parent=2, arguments=[]),
            dict(kind='exit', site='p', operation='call', entry=3, completion='throw'),
            dict(kind='exit', site='t', operation='assertion', entry=2,
                 completion='normal' if passed else 'throw', original_assertion_passed=passed,
                 continued=not passed, error={'test262_error': True}),
            dict(kind='completion', site=None, original_eof_reached=True,
                 fixture_capture_complete=True, site_counts={'p': 1, 't': 1},
                 data_mismatches=int(not passed), mandatory_failed_site_ids=[],
                 continued_data_site_ids=[] if passed else ['t'],
                 totalized_original_data_obligations=0, all_original_expressions_evaluated=True),
        ]
        return records, config, command, raw

    def test_expected_native_throw_requires_subsequent_original_assertion_pass(self):
        self.assertEqual(self.streamed(self.expected_throw_case(True))['data_mismatches'], 0)
        # A data-comparison continuation cannot substitute for assert.throws
        # actually passing; the deferred expected-throw obligation still rejects.
        with self.assertRaisesRegex(WitnessInvalid, 'expected throw assertion did not pass'):
            self.streamed(self.expected_throw_case(False))

    def test_unexpected_native_throw_is_not_waived(self):
        case = self.expected_throw_case(True)
        case[1]['sites']['t']['assertion_method'] = 'sameValue'
        with self.assertRaisesRegex(WitnessInvalid, 'Unexpected native/helper throw'):
            self.streamed(case)

    def test_excessive_active_stack_fails_before_unbounded_retention(self):
        _, config, command, raw = trace()
        config['max_events'] = 1000
        records = [dict(kind='start', site=None, original_entry_reached=True)]
        for i in range(257):
            records.append(dict(kind='enter', site='h', operation='helper',
                                parent=(i + 1) if i else None, arguments=[]))
        with self.assertRaisesRegex(WitnessInvalid, 'Finite operation stack bound'):
            self.streamed((records, config, command, raw))


if __name__ == '__main__':
    unittest.main()
