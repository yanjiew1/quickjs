"""Verify report contracts without executing or timing a JavaScript engine."""

from contextlib import ExitStack
import json
from pathlib import Path
import signal
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import run
import select_benchmark


class BenchmarkToolTests(unittest.TestCase):
    def test_build_features_follow_actual_defines(self):
        self.assertEqual(run.compiled_feature_config([
            "cc -DCONFIG_TEMPORAL -c engine.c",
            "cc -DCONFIG_TEMPORAL -c builtin.c",
        ]), {"CONFIG_INTL": "n", "CONFIG_TEMPORAL": "y", "CONFIG_ICU": "n"})
        self.assertEqual(run.compiled_feature_config([
            "cc -DCONFIG_TEMPORAL_STUB -DCONFIG_INTL=0 -D CONFIG_ICU=1 "
            "-DCONFIG_TEMPORAL -U CONFIG_TEMPORAL -c engine.c",
        ]), {"CONFIG_INTL": "y", "CONFIG_TEMPORAL": "n", "CONFIG_ICU": "y"})
        self.assertEqual(run.compiled_feature_config([
            "cc -DCONFIG_ICU -UCONFIG_ICU -c engine.c",
        ]), {"CONFIG_INTL": "n", "CONFIG_TEMPORAL": "n", "CONFIG_ICU": "n"})

    def test_actual_parent_contract_rejects_other_revision(self):
        with patch.object(run.subprocess, "run", return_value=
                          subprocess.CompletedProcess([], 0, "b" * 40 + "\n", "")) as git:
            self.assertEqual(run.checked_parent_revision(Path("candidate"), "b" * 40), "b" * 40)
            git.assert_called_once_with(["git", "-C", "candidate", "rev-parse", "HEAD^1"],
                                        text=True, capture_output=True, check=True)
            with self.assertRaisesRegex(ValueError, "actual immediate first parent"):
                run.checked_parent_revision(Path("candidate"), "c" * 40)

    def test_cadence_likely_impact_and_dispatch_selection(self):
        self.assertTrue(select_benchmark.selection("push", 26, ["doc/quickjs.texi"], 24)[0])
        self.assertTrue(select_benchmark.selection("push", 27, ["src/intl/date-native-bank.c"], 26)[0])
        self.assertTrue(select_benchmark.selection("push", 27, ["tests/microbench.js"], 26)[0])
        self.assertTrue(select_benchmark.selection("push", 27, ["tools/build/qjsc_codegen.py"], 26)[0])
        self.assertFalse(select_benchmark.selection("push", 27, ["doc/quickjs.texi"], 26)[0])
        self.assertTrue(select_benchmark.selection("workflow_dispatch", 26, [])[0])
        for before_count in (None, 0, 26, 27):
            self.assertTrue(select_benchmark.selection("push", 26, [], before_count)[0])
        with self.assertRaises(ValueError):
            select_benchmark.selection("push", 0, [], 0)

    def select_mock_push(self, before, count=26, previous=24, paths=b"doc/quickjs.texi\0",
                         history=None, unavailable=False, event="push", branch=""):
        candidate, parent, expected_before = "a" * 40, "b" * 40, "c" * 40
        if history is None:
            history = [candidate, parent, expected_before]
        def fake_git(command):
            self.assertEqual(command[:3], ["git", "-C", "candidate"])
            arguments = command[3:]
            if arguments == ["rev-parse", "HEAD"]:
                return (candidate + "\n").encode()
            if arguments == ["rev-parse", "HEAD^1"]:
                return (parent + "\n").encode()
            if arguments == ["rev-list", "--first-parent", "--count", "HEAD"]:
                return str(count).encode()
            if arguments == ["rev-parse", "--verify", expected_before + "^{commit}"]:
                if unavailable:
                    raise subprocess.CalledProcessError(128, command)
                return (expected_before + "\n").encode()
            if arguments == ["rev-list", "--first-parent", "HEAD"]:
                return ("\n".join(history) + "\n").encode()
            if arguments == ["rev-list", "--first-parent", "--count", expected_before]:
                return str(previous).encode()
            if arguments in (["diff", "--name-only", "--no-renames", "-z", expected_before, "HEAD", "--"],
                              ["diff", "--name-only", "--no-renames", "-z", parent, "HEAD", "--"]):
                return paths
            self.fail("Unexpected git command: " + repr(command))
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "github-output"
            argv = ["select_benchmark.py", "--candidate", "candidate", "--candidate-revision",
                    candidate, "--event", event, "--branch", branch, "--github-output", str(output)]
            if before is not None:
                argv += ["--before-revision", before]
            with patch("sys.argv", argv), \
                 patch.object(select_benchmark.subprocess, "check_output", side_effect=fake_git) as git:
                select_benchmark.main()
            fields = dict(line.split("=", 1) for line in output.read_text().splitlines())
            self.assertEqual(fields["parent_revision"], parent)
            return fields, [call[0][0][3:] for call in git.call_args_list]

    def test_push_range_crossing_and_earlier_impact_preserve_actual_parent(self):
        before = "c" * 40
        fields, commands = self.select_mock_push(before, paths=b"src/quickjs/runtime.c\0doc/quickjs.texi\0")
        self.assertEqual(fields["run"], "true")
        self.assertIn("Routine", fields["reason"])
        self.assertIn(["diff", "--name-only", "--no-renames", "-z", before, "HEAD", "--"], commands)
        fields, _ = self.select_mock_push(before, count=28, previous=26,
                                          paths=b"src/quickjs/runtime.c\0doc/quickjs.texi\0")
        self.assertEqual(fields["run"], "true")
        self.assertIn("Likely", fields["reason"])
        fields, _ = self.select_mock_push(before, count=27, previous=26)
        self.assertEqual(fields["run"], "false")

    def test_unavailable_nonancestor_and_rewritten_pushes_select_conservatively(self):
        scenarios = [dict(before="0" * 40), dict(before=None), dict(before=""),
                     dict(before="c" * 40, unavailable=True),
                     dict(before="c" * 40, history=["a" * 40, "b" * 40]),
                     dict(before="c" * 40, previous=27),
                     dict(before="c" * 40, previous=26)]
        for scenario in scenarios:
            with self.subTest(scenario=scenario):
                fields, commands = self.select_mock_push(**scenario)
                self.assertEqual(fields["run"], "true")
                self.assertIn("conservatively", fields["reason"])
                self.assertFalse(any(command[0] == "diff" for command in commands))

    def test_dispatch_ignores_push_range_and_preserves_actual_parent(self):
        fields, commands = self.select_mock_push("0" * 40, event="workflow_dispatch")
        self.assertEqual(fields["run"], "true")
        self.assertEqual(fields["reason"], "Explicit dispatch")
        self.assertEqual(commands, [["rev-parse", "HEAD"], ["rev-parse", "HEAD^1"],
                                    ["rev-list", "--first-parent", "--count", "HEAD"]])

    def test_latest_formal_branch_tips_always_measure(self):
        for branch in ("main", "main-new", "temporal", "intl"):
            with self.subTest(branch=branch):
                selected, reason = select_benchmark.selection("push", 27, [], 26, branch)
                self.assertTrue(selected)
                self.assertEqual(reason, "Latest formal branch tip")

    def test_new_scratch_branch_uses_tree_impact_and_cadence(self):
        scenarios = [(27, b"doc/quickjs.texi\0", "false"),
                     (27, b"src/quickjs/runtime.c\0", "true"),
                     (25, b"doc/quickjs.texi\0", "true"),
                     (27, b"", "false")]
        for count, paths, expected in scenarios:
            with self.subTest(count=count, paths=paths):
                fields, commands = self.select_mock_push("0" * 40, count=count, paths=paths,
                                                         branch="scratch-verify-ci-test")
                self.assertEqual(fields["run"], expected)
                self.assertIn("immediate-parent policy", fields["reason"])
                self.assertIn(["diff", "--name-only", "--no-renames", "-z",
                               "b" * 40, "HEAD", "--"], commands)

    def test_bellard_baseline_requires_exact_revision_and_original_origin(self):
        for origin in (run.BELLARD_REPOSITORY, "https://github.com/bellard/quickjs",
                       "git@github.com:bellard/quickjs.git"):
            with self.subTest(origin=origin), \
                 patch.object(run, "checked_revision") as revision, \
                 patch.object(run, "checked_parent_revision") as parent, \
                 patch.object(run.subprocess, "run", return_value=
                              subprocess.CompletedProcess([], 0, origin + "\n", "")):
                run.checked_baseline(Path("candidate"), Path("baseline"), "b" * 40, "bellard")
                revision.assert_called_once_with(Path("baseline"), "b" * 40)
                parent.assert_not_called()
        with patch.object(run, "checked_revision"), \
             patch.object(run.subprocess, "run", return_value=
                          subprocess.CompletedProcess([], 0, "https://github.com/yanjiew1/quickjs.git\n", "")):
            with self.assertRaisesRegex(ValueError, "bellard/quickjs"):
                run.checked_baseline(Path("candidate"), Path("baseline"), "b" * 40, "bellard")

    def test_parent_baseline_still_requires_actual_first_parent(self):
        with patch.object(run, "checked_revision") as revision, \
             patch.object(run, "checked_parent_revision") as parent:
            run.checked_baseline(Path("candidate"), Path("baseline"), "b" * 40, "parent")
            revision.assert_called_once_with(Path("baseline"), "b" * 40)
            parent.assert_called_once_with(Path("candidate"), "b" * 40)

    def test_bellard_summary_labels_provenance_and_scores(self):
        report = run.summarize([
            dict(suite="v8", case="sample", variant=variant, round=1, value=value, unit="us")
            for variant, value in (("baseline", 100), ("candidate", 90))], {"v8": 1})
        text = run.markdown(report, dict(status="completed", compiler="clang",
                                        candidate_revision="a" * 40, baseline_revision="b" * 40,
                                        baseline_kind="bellard", baseline_repository=run.BELLARD_REPOSITORY))
        self.assertIn("Bellard QuickJS: `" + "b" * 40 + "`", text)
        self.assertIn(run.BELLARD_REPOSITORY, text)
        self.assertIn("no latest-version claim", text)
        self.assertIn("| Case | Bellard QuickJS | Candidate |", text)
        self.assertIn("Geometric mean speed change:", text)
        self.assertIn("| sample | 100 | 90 | us | 1 |", text)

    def test_runner_keeps_fresh_alternating_ten_round_zoo_pairs_for_both_baselines(self):
        for kind in ("parent", "bellard"):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                output = root / "report"
                fixtures = root / "fixtures"
                fixtures.mkdir()
                fixture = fixtures / "fixture.js"
                fixture.write_text("fixture")
                manifest = root / "manifest.json"
                manifest.write_text(json.dumps({"bundled_sha256": {}, "zoo": {"files": {
                    fixture.name: {"scores": ["First", "Second"], "sha256": run.sha256(fixture)}
                }}}))
                invocations = []
                build_policies = []

                def build(source, compiler, destination, jobs, suite, baseline_release=False):
                    build_policies.append((destination.name, baseline_release))
                    binary = destination / "qjs"
                    binary.write_bytes(b"mock runtime")
                    return {"binary": str(binary), "binary_sha256": run.sha256(binary),
                            "feature_config": {}}

                def execute(command, cwd, log, timeout):
                    variant = Path(command[0]).parent.name
                    invocations.append((variant, cwd))
                    log.write_text("First: 110\nSecond: 220\n" if variant == "candidate" else
                                   "First: 100\nSecond: 200\n")
                    return 0.01

                # These mocks emit fixture scores only: no compiler, engine,
                # affinity change, network request or throughput measurement.
                option = "--parent" if kind == "parent" else "--baseline"
                argv = ["run.py", "--candidate", str(root / "candidate"),
                        "--candidate-revision", "a" * 40, option, str(root / "baseline"),
                        option + "-revision", "b" * 40, "--baseline-kind", kind,
                        "--compiler", "gcc", "--suite", "zoo", "--rounds", "3",
                        "--output", str(output)]
                with ExitStack() as stack:
                    stack.enter_context(patch("sys.argv", argv))
                    stack.enter_context(patch.object(run, "MANIFEST", manifest))
                    baseline = stack.enter_context(patch.object(run, "checked_baseline"))
                    for name, replacement in (
                        ("checked_revision", lambda *args: None), ("build", build),
                        ("checked_process", execute), ("fetch_zoo", lambda *args: fixtures),
                        ("select_affinity", lambda *args: {}), ("environment", lambda *args: {}),
                    ):
                        stack.enter_context(patch.object(run, name, replacement))
                    run.main()
                self.assertEqual(build_policies, [("baseline", kind == "bellard"),
                                                  ("candidate", False)])
                self.assertEqual(baseline.call_count, 3)
                self.assertTrue(all(call[0][-1] == kind for call in baseline.call_args_list))
                expected = [variant for rep in range(10) for variant in
                            (("baseline", "candidate") if rep % 2 == 0 else ("candidate", "baseline"))]
                self.assertEqual([variant for variant, cwd in invocations], expected)
                self.assertEqual(len({cwd for variant, cwd in invocations}), 20)
                state = json.loads((output / "state.json").read_text())
                self.assertEqual(state["status"], "completed")
                self.assertEqual(state["baseline_kind"], kind)
                self.assertEqual(state["rounds"]["zoo"], 10)
                self.assertEqual(state["report_schema_version"], 2)
                rows = json.loads((output / "runs.json").read_text())
                self.assertEqual(len(rows), 40)
                self.assertTrue(all(row["fresh_process"] for row in rows))
                analysis = json.loads((output / "analysis.json").read_text())["zoo"]
                self.assertEqual(analysis["case_count"], 2)
                self.assertAlmostEqual(analysis["candidate_change_percent"], 10)

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

    def test_meson_build_checks_produced_objects_and_ignores_unbuilt_commands(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "source", root / "output"
            source.mkdir()
            output.mkdir()
            (source / "meson.build").write_text("project('fixture', 'c')")

            def execute(command, cwd, log, timeout):
                log.write_text("fixture build\n")
                if command[1] == "setup":
                    build = output / "meson-build"
                    build.mkdir()
                    (build / "engine.o").write_bytes(b"object")
                    (build / "qjs").write_bytes(b"executable")
                    commands = [dict(directory=str(build), file="engine.c",
                                     command="cc -O2 -DNDEBUG -c engine.c -o engine.o"),
                                dict(directory=str(build), file="unbuilt.c",
                                     command="cc -O0 -c unbuilt.c -o unbuilt.o")]
                    (build / "compile_commands.json").write_text(json.dumps(commands))
                return 0.1

            with patch.object(run.subprocess, "check_output",
                              side_effect=lambda command: b"cc -o qjs engine.o\n" if command[0] == "ninja" else b""), \
                 patch.object(run, "checked_process", side_effect=execute) as process:
                result = run.build(source, "gcc", output, 2)
            self.assertEqual(result["backend"], "meson")
            self.assertEqual(len(result["actual_compile_commands"]), 1)
            self.assertEqual(process.call_count, 2)
            self.assertIn("-Db_sanitize=none", result["configure_command"])
            self.assertIn("-Db_ndebug=true", result["configure_command"])
            self.assertFalse(result["assertions_enabled"])
            self.assertEqual(result["actual_build_commands"], ["cc -o qjs engine.o"])
            self.assertTrue(Path(result["binary"]).is_file())

    def test_benchmark_rejects_assertion_enabled_compile_commands(self):
        run.require_assertions_disabled(["cc -O2 -DNDEBUG -c engine.c"])
        for command in ["cc -O2 -c engine.c",
                        "cc -O2 -DNDEBUG -UNDEBUG -c engine.c"]:
            with self.subTest(command=command):
                with self.assertRaisesRegex(ValueError, "disable assertions"):
                    run.require_assertions_disabled([command])

    def test_make_build_preserves_flags_and_disables_assertions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "source", root / "output"
            source.mkdir()
            output.mkdir()

            def execute(command, cwd, log, timeout, env):
                self.assertEqual(env["CFLAGS"], "-g -DNDEBUG")
                log.write_text("gcc -g -DNDEBUG -O2 -c engine.c -o engine.o\n")
                (source / "qjs").write_bytes(b"executable")
                return 0.1

            with patch.dict(run.os.environ, {"CFLAGS": "-g"}), \
                 patch.object(run.subprocess, "check_output", return_value=b""), \
                 patch.object(run, "checked_process", side_effect=execute):
                result = run.build(source, "gcc", output, 2)
            self.assertFalse(result["assertions_enabled"])
            self.assertEqual(result["build_environment"]["CFLAGS"], "-g -DNDEBUG")

    def test_meson_link_only_instrumentation_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for flag in ["-fsanitize=address", "-flto=auto"]:
                with patch.object(run, "checked_process", return_value=0), \
                     patch.object(run.subprocess, "check_output",
                                  return_value=("cc -o qjs engine.o " + flag).encode()):
                    with self.assertRaisesRegex(ValueError, "link/build command"):
                        run.build_meson(root, "gcc", root, 2, "core")

    def test_tracked_parent_headers_are_not_stale_products(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / "quickjs.h").write_text("parent public header")
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
        for suite, parent, candidate in (
            ("microbench", [2, 8], [1, 4]),
            ("zoo", [1, 5], [2, 10]),
        ):
            for variant, samples in (("baseline", parent), ("candidate", candidate)):
                for rep, value in enumerate(samples):
                    rows.append(dict(suite=suite, case="sample", variant=variant,
                                     round=rep + 1, value=value, unit="test"))
        report = run.summarize(rows, {"microbench": 2, "zoo": 2})
        self.assertEqual(report["microbench"]["cases"]["sample"]["baseline"], 5)
        self.assertEqual(report["zoo"]["cases"]["sample"]["baseline"], 5)
        for suite in report:
            self.assertAlmostEqual(report[suite]["candidate_change_percent"], 100)
        with self.assertRaises(ValueError):
            run.summarize(rows[:-1], {"microbench": 2, "zoo": 2})

    def test_geometric_mean_does_not_average_percentages(self):
        rows = [dict(suite="v8", case=case, variant=variant, round=1,
                     value=value, unit="us") for case, values in (
                         ("a", (4, 1)), ("b", (1, 4)))
                for variant, value in zip(("baseline", "candidate"), values)]
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
                for variant, value in (("baseline", 100), ("candidate", 90))]
        report = run.summarize(rows, dict.fromkeys(catalog, 1))
        text = run.markdown(report, dict(status="completed", compiler="gcc",
                                        candidate_revision="a" * 40,
                                        baseline_revision="b" * 40))
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
