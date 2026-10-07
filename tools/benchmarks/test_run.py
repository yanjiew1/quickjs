"""Verify report contracts without executing or timing a JavaScript engine."""

import json
from pathlib import Path
import signal
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import run


class BenchmarkToolTests(unittest.TestCase):
    def test_v8_requires_one_exact_positive_case(self):
        result = [{"name": "Crypto", "reference": 266181,
                   "results": [{"name": "Decrypt", "microseconds_per_iteration": 7.5}]}]
        text = "V8_DETAILS " + json.dumps(result) + "\n"
        self.assertEqual(run.parse_v8(text, "Crypto", "Decrypt"), {"Crypto/Decrypt": 7.5})
        for invalid in (text + text, "ERROR Crypto failed\n" + text,
                        text.replace('"Decrypt"', '"Encrypt"'),
                        text.replace("7.5", "0"), text.replace("7.5", "NaN")):
            with self.assertRaises(ValueError):
                run.parse_v8(invalid, "Crypto", "Decrypt")

    def test_micro_rejects_wrong_or_nonfinite_results(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result.json"
            for data in ({"array_read": 1.5}, {"array_read": 1.5, "array_write": 3},
                         {"array_read": False}, {"array_read": float("inf")}):
                path.write_text(json.dumps(data))
                if len(data) == 1 and data["array_read"] == 1.5:
                    self.assertEqual(run.parse_micro(path, "array_read", ""), data)
                else:
                    with self.assertRaises(ValueError):
                        run.parse_micro(path, "array_read", "")

    def test_sort_printed_failure_rejects_a_positive_saved_score(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result.json"
            path.write_text('{"sort_bench": 1.5}')
            self.assertEqual(run.parse_micro(path, "sort_bench", "sort_bench 1.5\n"),
                             {"sort_bench": 1.5})
            output = "sort_bench: out of order error for random at offset 4: 3 > 2\n"
            with self.assertRaisesRegex(ValueError, "out-of-order"):
                run.parse_micro(path, "sort_bench", output)

    def test_stale_build_rejected_before_make_without_deleting_files(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            old_object = source / ".obj" / "old-gcc-object.o"
            old_object.parent.mkdir()
            old_object.write_bytes(b"old compiler object")
            with patch.object(run.subprocess, "check_output", return_value=b""), \
                 patch.object(run, "checked_process") as execute:
                with self.assertRaisesRegex(ValueError, "Pristine build required"):
                    run.build(source, "clang", source, 2)
                execute.assert_not_called()
            self.assertEqual(old_object.read_bytes(), b"old compiler object")

    def test_tracked_upstream_headers_are_not_stale_products(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / "quickjs.h").write_text("upstream public header")
            with patch.object(run.subprocess, "check_output", return_value=b"quickjs.h\0"):
                run.require_pristine_build(source)
            with patch.object(run.subprocess, "check_output", return_value=b""):
                with self.assertRaisesRegex(ValueError, "quickjs.h"):
                    run.require_pristine_build(source)

    def test_sigterm_reaches_process_group_drainage(self):
        with self.assertRaises(run.BenchmarkCancelled):
            run.cancel_on_signal(signal.SIGTERM, None)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            process = unittest.mock.Mock(pid=12345)
            process.wait.side_effect = [run.BenchmarkCancelled("SIGTERM"), 0]
            with patch.object(run.subprocess, "Popen", return_value=process), \
                 patch.object(run.os, "name", "posix"), \
                 patch.object(run.os, "killpg", create=True) as kill_group:
                with self.assertRaises(run.BenchmarkCancelled):
                    run.checked_process(["fake-compiler"], source, source / "output.log", 1)
                kill_group.assert_called_once_with(12345, signal.SIGKILL)
                self.assertEqual(process.wait.call_count, 2)

    def test_timeout_still_drains_the_child_group(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            process = unittest.mock.Mock(pid=12346)
            process.wait.side_effect = [subprocess.TimeoutExpired("fake-compiler", 1), 0]
            with patch.object(run.subprocess, "Popen", return_value=process), \
                 patch.object(run.os, "name", "posix"), \
                 patch.object(run.os, "killpg", create=True) as kill_group:
                with self.assertRaises(subprocess.TimeoutExpired):
                    run.checked_process(["fake-compiler"], source, source / "output.log", 1)
                kill_group.assert_called_once_with(12346, signal.SIGKILL)
                self.assertEqual(process.wait.call_count, 2)

    def test_zoo_requires_all_scores_and_no_printed_errors(self):
        expected = ["Splay", "SplayLatency"]
        text = "Splay: 123\nSplayLatency: 4.5\n"
        self.assertEqual(run.parse_zoo(text, expected), {"Splay": 123, "SplayLatency": 4.5})
        for invalid in ("Splay: 123\n", text + "Splay: 123\n",
                        text.replace("4.5", "NaN"), text + "Splay: Error: bad result\n"):
            with self.assertRaises(ValueError):
                run.parse_zoo(invalid, expected)

    def test_direction_and_zoo_upper_middle_match_original(self):
        rows = []
        for suite, upstream, candidate in (
            ("microbench", [2, 8], [1, 4]),
            ("zoo", [1, 5], [2, 10]),
        ):
            for variant, samples in (("upstream", upstream), ("candidate", candidate)):
                for rep, value in enumerate(samples):
                    rows.append(dict(suite=suite, case="sample", variant=variant,
                                     round=rep + 1, value=value, unit="test"))
        report = run.summarize(rows, {"microbench": 2, "zoo": 2})
        self.assertEqual(report["microbench"]["cases"]["sample"]["upstream"], 5)
        self.assertEqual(report["zoo"]["cases"]["sample"]["upstream"], 5)
        for suite in report:
            self.assertAlmostEqual(report[suite]["candidate_change_percent"], 100)
        with self.assertRaises(ValueError):
            run.summarize(rows[:-1], {"microbench": 2, "zoo": 2})

    def test_geometric_mean_does_not_average_percentages(self):
        rows = [dict(suite="v8", case=case, variant=variant, round=1,
                     value=value, unit="us") for case, values in (
                         ("a", (4, 1)), ("b", (1, 4)))
                for variant, value in zip(("upstream", "candidate"), values)]
        report = run.summarize(rows, {"v8": 1})
        self.assertAlmostEqual(report["v8"]["candidate_change_percent"], 0)

    def test_readable_summary_contains_every_selected_individual_score(self):
        manifest = json.loads(run.MANIFEST.read_text())
        catalog = {
            "v8": ["/".join(case) for case in manifest["v8_cases"]],
            "microbench": manifest["micro_cases"],
            "zoo": [name for entry in manifest["zoo"]["files"].values()
                    for name in entry["scores"]],
        }
        rows = [dict(suite=suite, case=name, variant=variant, round=1,
                     value=value, unit="contract-unit")
                for suite, names in catalog.items() for name in names
                for variant, value in (("upstream", 100), ("candidate", 90))]
        report = run.summarize(rows, dict.fromkeys(catalog, 1))
        text = run.markdown(report, dict(status="completed", compiler="gcc",
                                        candidate_revision="a" * 40,
                                        upstream_revision="b" * 40))
        for suite, names in catalog.items():
            self.assertIn("## " + suite, text)
            for name in names:
                lines = [line for line in text.splitlines() if line.startswith("| " + name + " |")]
                self.assertEqual(len(lines), 1)
                self.assertIn("| 100 | 90 | contract-unit | 1 |", lines[0])
        self.assertEqual(sum(len(names) for names in catalog.values()), 98)

    def test_fixture_corruption_and_catalog_coverage(self):
        manifest = json.loads(run.MANIFEST.read_text())
        self.assertEqual(len(manifest["v8_cases"]), 10)
        self.assertEqual(len(manifest["micro_cases"]), 72)
        self.assertEqual(len(set(manifest["micro_cases"])), 72)
        self.assertEqual(len(manifest["zoo"]["files"]), 14)
        self.assertEqual(sum(len(value["scores"]) for value in manifest["zoo"]["files"].values()), 16)
        self.assertNotIn("zlib.js", manifest["zoo"]["files"])
        for name, expected in manifest["bundled_sha256"].items():
            run.verify_fixture(run.HERE / "fixtures" / name, expected)
        with tempfile.TemporaryDirectory() as directory:
            assembled = run.assemble_v8(manifest, run.HERE.parents[1],
                                       Path(directory) / "fresh-v8.js")
            run.verify_fixture(assembled, manifest["v8_provenance"]["assembled_sha256"])
            path = Path(directory) / "fixture"
            path.write_text("corrupt fixture")
            with self.assertRaises(ValueError):
                run.verify_fixture(path, "0" * 64)

    def test_affinity_does_not_claim_exclusive_cpu_or_modify_host(self):
        with patch.object(run.os, "sched_getaffinity", return_value={2, 4}, create=True), \
             patch.object(run.os, "sched_setaffinity", create=True) as set_affinity:
            report = run.select_affinity("auto")
            set_affinity.assert_called_once_with(0, {2})
            self.assertFalse(report["exclusive"])
            self.assertEqual(report["allowed_before"], [2, 4])
        with patch.object(run.os, "sched_getaffinity", return_value={2}, create=True), \
             patch.object(run.os, "sched_setaffinity", side_effect=PermissionError("denied"), create=True):
            self.assertEqual(run.select_affinity("auto")["status"], "Affinity unavailable")


if __name__ == "__main__":
    unittest.main()
