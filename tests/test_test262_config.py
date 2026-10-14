#!/usr/bin/env python3
"""Exercise feature config generation and Make recipes without an engine or suite."""
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("test262_config", ROOT / "tools/ci/test262-config.py")
CONFIG = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CONFIG)

ORACLE_EXCLUSIONS = {
    "test262/test/intl402/DateTimeFormat/prototype/resolvedOptions/hourCycle-default.js",
    "test262/test/intl402/DateTimeFormat/prototype/format/numbering-system.js",
}


def section(text, name):
    headers = list(re.finditer(r"(?m)^\[([^\]\r\n]+)\][ \t]*$", text))
    for index, header in enumerate(headers):
        if header.group(1) == name:
            end = headers[index + 1].start() if index + 1 < len(headers) else len(text)
            return text[header.end():end]
    raise ValueError("missing config section: " + name)


def features(text):
    return dict((line.strip().split("=", 1) + ["yes"])[:2]
                for line in section(text, "features").splitlines()
                if line.strip() and not line.strip().startswith("#"))


class FeatureConfig(unittest.TestCase):
    def test_section_headers_ignore_comment_and_inline_occurrences(self):
        source = ("[config]\n"
                  "# exclude tests enumerated in this file (see also [exclude] section)\n"
                  "reportfile=fixture-report.txt\n"
                  "[features]\n# [exclude]\nFixtureUnknown=skip\n"
                  "[exclude]\n# [tests] is a comment\nslow-test.js\n"
                  "[tests]\nother-test.js\n")
        self.assertEqual(section(source, "exclude"),
                         "\n# [tests] is a comment\nslow-test.js\n")
        self.assertEqual(features(source), {"FixtureUnknown": "skip"})
        self.assertEqual(section(source, "tests"), "\nother-test.js\n")
        for reference in ("# see also [exclude] section", "[exclude] section"):
            with self.subTest(reference=reference):
                with self.assertRaises(ValueError):
                    section("[config]\n" + reference + "\n", "exclude")

    def test_profiles_preserve_unrelated_skips_and_exclusions(self):
        for name in ("test262.conf", "test262-intl.conf"):
            source = (ROOT / name).read_text()
            original = features(source)
            for intl, temporal in (("n", "n"), ("n", "y"), ("y", "n"), ("y", "y"), ("", "")):
                with self.subTest(source=name, intl=intl, temporal=temporal):
                    derived = CONFIG.derive(source, intl, temporal, preserve_reportfile=True)
                    active = features(derived)
                    self.assertEqual(active["Temporal"], "yes" if temporal == "y" else "skip")
                    self.assertEqual(active["Intl.Locale"], "yes" if intl == "y" else "skip")
                    self.assertEqual(active["Intl.Era-monthcode"],
                                     "yes" if "y" in (intl, temporal) else "skip")
                    for feature, value in original.items():
                        if not feature.startswith("Intl") and feature not in (
                                "Temporal", "canonical-tz", "intl-normative-optional"):
                            self.assertEqual(active[feature], value, feature)
                    expected = section(source, "exclude")
                    if intl == "y":
                        expected = expected.replace("# intl not supported\n", "").replace(
                            "test262/test/intl402/\n", "")
                    self.assertEqual(section(derived, "exclude"), expected)
                    self.assertEqual(section(derived, "config"), section(source, "config"))

    def test_hosted_report_defaults_and_preservation_option(self):
        source = "[config]\nreportfile=fixture-report.txt\nerrorfile=fixture-errors.txt\n"
        for intl, temporal, report in (("n", "n", "test262_report.txt"),
                                      ("n", "y", "test262_temporal_report.txt"),
                                      ("y", "n", "test262_intl_report.txt")):
            with self.subTest(intl=intl, temporal=temporal):
                default = CONFIG.derive(source, intl, temporal)
                self.assertIn("reportfile=" + report + "\n", default)
                self.assertIn("errorfile=fixture-errors.txt\n", default)
                preserved = CONFIG.derive(source, intl, temporal, preserve_reportfile=True)
                self.assertIn("reportfile=fixture-report.txt\n", preserved)


class MakeFeatureConfig(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qjs-test262-config-")
        self.addCleanup(temporary.cleanup)
        self.scratch = Path(temporary.name)
        for name in ("Makefile", "VERSION", "test262.conf", "test262-intl.conf",
                     "tools/ci/test262-config.py", "tools/icu-build-config.mk"):
            target = self.scratch / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / name, target)
        (self.scratch / "test262").mkdir()
        (self.scratch / "test262/features.txt").write_text("Temporal\nIntl.Locale\n")
        self.compiler = self.scratch / "reject-compiler"
        self.compiler.write_text("#!/bin/sh\nprintf '%s\\n' \"$*\" >> compiler-invocations\nexit 1\n")
        self.compiler.chmod(0o755)
        # The suite runner is a recording fixture. Make treats it as an old
        # target so none of the engine object prerequisites can be built.
        runner = self.scratch / "run-test262"
        runner.write_text("#!" + sys.executable + "\n"
                          "import json, pathlib, sys\n"
                          "pathlib.Path('runner-args.json').write_text(json.dumps(sys.argv[1:]))\n")
        runner.chmod(0o755)

    def make(self, target, *switches):
        compiler_log = self.scratch / "compiler-invocations"
        if compiler_log.exists():
            compiler_log.unlink()
        environment = os.environ.copy()
        for name in ("MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES", "MSYSTEM",
                     "TEST262_CONFIG", "TEST262_FEATURE_CONFIG",
                     "CONFIG_INTL", "CONFIG_TEMPORAL", "CONFIG_INTL_LEGACY",
                     "CONFIG_INTL_BACKEND", "CONFIG_INTL_NATIVE", "CONFIG_ICU",
                     "ICU_CFLAGS", "ICU_LIBS", "HOST_ICU_CFLAGS", "HOST_ICU_LIBS"):
            environment.pop(name, None)
        command = [os.environ.get("QJS_TEST_MAKE", "make"), "--no-print-directory", "-s",
                   "-o", "run-test262", "CONFIG_CLANG=", "CONFIG_COSMO=", "CONFIG_LTO=",
                   "CONFIG_WIN32=", "EXE=", "TEST262_THREADS=3", "CONFIG_DARWIN=", "CONFIG_FREEBSD=", "CROSS_PREFIX=",
                   "CONFIG_ASAN=", "CONFIG_UBSAN=", "CONFIG_MSAN=", "CONFIG_TSAN=",
                   "CC=" + str(self.compiler), "HOST_CC=" + str(self.compiler),
                   "CONFIG_INTL_BACKEND=icu", "ICU_CFLAGS=", "ICU_LIBS=-lfixture",
                   "HOST_ICU_CFLAGS=", "HOST_ICU_LIBS=-lfixture", "PYTHON=" + sys.executable]
        subprocess.run(command + list(switches) + [target], cwd=self.scratch,
                       env=environment, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        # Make probes closefrom during parsing; the fixture rejects that probe.
        # Any attempt to compile an engine or runner object also fails Make.
        self.assertEqual(compiler_log.read_text().splitlines(),
                         ["-o /dev/null compat/test-closefrom.c"])

    def recorded_config(self):
        args = json.loads((self.scratch / "runner-args.json").read_text())
        config = Path(args[args.index("-c") + 1])
        return args, config, (self.scratch / config).read_text()

    def test_default_and_explicit_feature_switches(self):
        profiles = (((), "skip", "yes"),
                    (("CONFIG_INTL=n", "CONFIG_TEMPORAL=n"), "skip", "skip"),
                    (("CONFIG_INTL=y", "CONFIG_TEMPORAL=n"), "yes", "skip"),
                    (("CONFIG_INTL=y", "CONFIG_TEMPORAL=y"), "yes", "yes"),
                    (("CONFIG_INTL=y", "CONFIG_TEMPORAL=y", "CONFIG_INTL_LEGACY=n"), "yes", "yes"),
                    (("CONFIG_INTL=y", "CONFIG_TEMPORAL=y", "CONFIG_INTL_BACKEND=native",
                      "CONFIG_ICU=n"), "yes", "yes"),
                    (("CONFIG_INTL=n", "CONFIG_TEMPORAL=n", "CONFIG_ICU=y"), "skip", "skip"))
        for switches, intl, temporal in profiles:
            with self.subTest(switches=switches):
                self.make("test2", *switches)
                args, path, text = self.recorded_config()
                active = features(text)
                self.assertEqual(active["Intl.Locale"], intl)
                self.assertEqual(active["Temporal"], temporal)
                self.assertEqual(active["intl-normative-optional"], intl)
                self.assertEqual(active["Intl.Era-monthcode"],
                                 "yes" if "yes" in (intl, temporal) else "skip")
                source_name = "test262-intl.conf" if intl == "yes" else "test262.conf"
                source = (self.scratch / source_name).read_text()
                self.assertEqual(path, Path(Path(source_name).stem + "_intl-" +
                                           ("y" if intl == "yes" else "n") + "_temporal-" +
                                           ("y" if temporal == "yes" else "n") + ".txt"))
                self.assertEqual(section(text, "config"), section(source, "config"))
                expected = section(source, "exclude")
                if intl == "yes":
                    expected = expected.replace("# intl not supported\n", "")
                    excluded = [line.strip() for line in section(text, "exclude").splitlines()
                                if line.strip().startswith("test262/test/intl402/")]
                    self.assertEqual(set(excluded), ORACLE_EXCLUSIONS)
                    self.assertEqual(len(excluded), 2)
                    self.assertIn("#test262/test/built-ins/RegExp/CharacterClassEscapes/\n", text)
                    self.assertIn("#test262/test/built-ins/RegExp/property-escapes/\n", text)
                else:
                    self.assertIn("test262/test/intl402/\n", expected)
                self.assertEqual(section(text, "exclude"), expected)
                for feature, value in features(source).items():
                    if not feature.startswith("Intl") and feature not in (
                            "Temporal", "canonical-tz", "intl-normative-optional"):
                        self.assertEqual(active[feature], value, feature)
                self.assertEqual(args, ["-T", "3", "-t", "-m", "-c", str(path), "-a"])

    def test_empty_build_switches_are_rejected_before_runner_execution(self):
        for switches in (("CONFIG_INTL=",), ("CONFIG_TEMPORAL=",)):
            with self.subTest(switches=switches):
                with self.assertRaises(subprocess.CalledProcessError) as failure:
                    self.make("test2", *switches)
                self.assertIn(b"must be exactly y or n", failure.exception.stderr)
                self.assertFalse((self.scratch / "runner-args.json").exists())

    def test_target_modes_keep_options_after_config_loading(self):
        for target, before, after in (("test2-default", ["-t", "-m"], []),
                                      ("test2", ["-t", "-m"], ["-a"]),
                                      ("test2-update", ["-t", "-u"], ["-a"]),
                                      ("test2-check", ["-t", "-m"], ["-E", "-a"])):
            for switches in ((), ("CONFIG_INTL=y", "CONFIG_TEMPORAL=n"),
                             ("CONFIG_INTL=y", "CONFIG_TEMPORAL=y")):
                with self.subTest(target=target, switches=switches):
                    self.make(target, *switches)
                    args, path, text = self.recorded_config()
                    self.assertEqual(args, ["-T", "3"] + before + ["-c", str(path)] + after)
                    self.assertIn("mode=default\n", text)

    def test_fixture_paths_and_changed_source_regenerate(self):
        fixture = self.scratch / "fixtures/custom.conf"
        fixture.parent.mkdir()
        fixture.write_text("[config]\nharnessdir=harness\nerrorfile=errors.txt\n"
                           "reportfile=report.txt\ntestdir=tests\nmode=default\n"
                           "[features]\nIntl.Locale=skip\nTemporal=skip\n[exclude]\nslow-test.js\n")
        for intl, temporal in (("n", "n"), ("n", "y"), ("y", "n"), ("y", "y")):
            with self.subTest(intl=intl, temporal=temporal):
                self.make("test2-update", "TEST262_CONFIG=fixtures/custom.conf",
                          "CONFIG_INTL=" + intl, "CONFIG_TEMPORAL=" + temporal)
                args, path, text = self.recorded_config()
                self.assertEqual(path, Path("fixtures/custom_intl-" + intl +
                                            "_temporal-" + temporal + ".txt"))
                self.assertEqual(section(text, "config"), section(fixture.read_text(), "config"))
                self.assertEqual(section(text, "exclude"), "\nslow-test.js\n")
                self.assertEqual(features(text)["Intl.Locale"], "yes" if intl == "y" else "skip")
                self.assertEqual(features(text)["Temporal"], "yes" if temporal == "y" else "skip")
                self.assertEqual(args, ["-T", "3", "-t", "-u", "-c", str(path), "-a"])
        fixture.write_text(fixture.read_text().replace("slow-test.js", "another-slow-test.js"))
        next_time = (self.scratch / path).stat().st_mtime + 1
        os.utime(fixture, (next_time, next_time))
        self.make("test2-config", "TEST262_CONFIG=fixtures/custom.conf",
                  "CONFIG_INTL=y", "CONFIG_TEMPORAL=y")
        self.assertIn("another-slow-test.js\n", (self.scratch / path).read_text())

    def test_generation_does_not_require_an_installed_suite(self):
        shutil.rmtree(self.scratch / "test262")
        for intl, temporal in (("n", "n"), ("n", "y"), ("y", "n"), ("y", "y")):
            with self.subTest(intl=intl, temporal=temporal):
                self.make("test2-config", "CONFIG_INTL=" + intl, "CONFIG_TEMPORAL=" + temporal)
                basename = "test262-intl" if intl == "y" else "test262"
                text = (self.scratch / (basename + "_intl-" + intl +
                                       "_temporal-" + temporal + ".txt")).read_text()
                self.assertEqual(features(text)["Temporal"], "yes" if temporal == "y" else "skip")
        self.assertFalse((self.scratch / "runner-args.json").exists())
        historical = self.scratch / "test262-temporal_intl-n_temporal-y.txt"
        historical.write_text("historical derived config\n")
        custom = self.scratch / "custom_intl-y_temporal-y.txt"
        custom.write_text("custom derived config\n")
        self.make("clean")
        for basename in ("test262", "test262-intl", "test262-temporal"):
            self.assertEqual(list(self.scratch.glob(basename + "_intl-*_temporal-*.txt")), [])
        self.assertTrue(custom.exists())
        self.assertTrue((self.scratch / "test262.conf").exists())
        self.assertTrue((self.scratch / "test262-intl.conf").exists())


if __name__ == "__main__":
    unittest.main()
