#!/usr/bin/env python3
"""Check actual Make-selected static metadata without compiling an engine.

Windows is a simulated Make configuration. These tool tests make no claim
about Windows ICU development files or an enabled Windows runtime.
"""
import errno
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock


def check_make_call(command, *, cwd, env):
    try:
        return subprocess.check_call(command, cwd=cwd, env=env)
    except OSError as error:
        if os.name != "posix" or error.errno != errno.ENOEXEC:
            raise
    # POSIX shells can launch APE executables after exec returns ENOEXEC.
    # Pass every argument separately, including Make variable assignments.
    return subprocess.check_call(
        ["/bin/sh", "-c", 'exec "$@"', "qjs-test-make"] + command,
        cwd=cwd, env=env)


class MakeLauncher(unittest.TestCase):
    def setUp(self):
        self.command = ["/fixture/make with spaces", "-s",
                        "ICU_LIBS=-L/fixture/ICU library -licuuc",
                        "EXTRA_LIBS=$(touch unexpected); 'quoted'", ""]
        self.options = {"cwd": "/fixture/scratch", "env": {"PATH": "/fixture/bin"}}

    def test_native_launch_preserves_arguments_and_options(self):
        with mock.patch.object(subprocess, "check_call", return_value=0) as launch:
            self.assertEqual(check_make_call(self.command, **self.options), 0)
        launch.assert_called_once_with(self.command, **self.options)

    def test_posix_enoexec_retries_through_shell_without_interpolation(self):
        original = list(self.command)
        error = OSError(errno.ENOEXEC, "Exec format error")
        with mock.patch.object(os, "name", "posix"), mock.patch.object(
                subprocess, "check_call", side_effect=[error, 0]) as launch:
            self.assertEqual(check_make_call(self.command, **self.options), 0)
        self.assertEqual(launch.call_args_list, [
            mock.call(original, **self.options),
            mock.call(["/bin/sh", "-c", 'exec "$@"', "qjs-test-make"] + original,
                      **self.options)])
        self.assertEqual(self.command, original)

    def test_other_launch_errors_are_not_retried(self):
        for code in (errno.EACCES, errno.ENOENT):
            with self.subTest(errno=code):
                error = OSError(code, "Cannot launch Make")
                with mock.patch.object(os, "name", "posix"), mock.patch.object(
                        subprocess, "check_call", side_effect=error) as launch:
                    with self.assertRaises(OSError) as raised:
                        check_make_call(self.command, **self.options)
                self.assertIs(raised.exception, error)
                launch.assert_called_once_with(self.command, **self.options)

    def test_windows_executable_is_not_retried_through_posix_shell(self):
        command = [r"C:\Program Files\Make\make.exe", "-s"]
        error = OSError(errno.ENOEXEC, "Exec format error")
        with mock.patch.object(os, "name", "nt"), mock.patch.object(
                subprocess, "check_call", side_effect=error) as launch:
            with self.assertRaises(OSError) as raised:
                check_make_call(command, **self.options)
        self.assertIs(raised.exception, error)
        launch.assert_called_once_with(command, **self.options)

    def test_make_failure_is_not_retried(self):
        error = subprocess.CalledProcessError(2, self.command)
        with mock.patch.object(subprocess, "check_call", side_effect=error) as launch:
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                check_make_call(self.command, **self.options)
        self.assertIs(raised.exception, error)
        launch.assert_called_once_with(self.command, **self.options)

    def test_shell_failure_remains_a_failure(self):
        error = subprocess.CalledProcessError(2, self.command)
        with mock.patch.object(os, "name", "posix"), mock.patch.object(
                subprocess, "check_call", side_effect=[
                    OSError(errno.ENOEXEC, "Exec format error"), error]) as launch:
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                check_make_call(self.command, **self.options)
        self.assertIs(raised.exception, error)
        self.assertEqual(launch.call_count, 2)


class PrivateLibraryMetadata(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="qjs-icu-metadata-")
        self.addCleanup(self.temporary.cleanup)
        self.scratch = Path(self.temporary.name)
        self.source = Path(__file__).resolve().parents[1]
        shutil.copy2(str(self.source / "Makefile"), str(self.scratch / "Makefile"))
        shutil.copy2(str(self.source / "VERSION"), str(self.scratch / "VERSION"))
        (self.scratch / "tools").mkdir()
        shutil.copy2(str(self.source / "tools/intl-link-config.py"),
                     str(self.scratch / "tools/intl-link-config.py"))
        # Make's compiler capability probes may run this script. It rejects
        # every probe and never invokes a compiler or produces an object.
        self.compiler = self.scratch / "reject-compiler"
        self.compiler.write_text("#!/bin/sh\nexit 1\n")
        self.compiler.chmod(0o755)
        # A text-only dependent target checks config invalidation without
        # invoking a compiler, even when every output timestamp is equal.
        with (self.scratch / "Makefile").open("a") as fixture:
            fixture.write("\nmetadata/config-consumer: .obj/intl-build-config\n"
                          "\t@mkdir -p $(@D)\n"
                          "\t@printf x >> $@\n")
        # These are text-only stand-ins for object outputs. Use the actual
        # Make compile rules, including late prepared Temporal units and an
        # explicit native support recipe, without invoking a C compiler.
        self.intl_enabled = True
        self.object_sources = ["tests/test_temporal.c",
                               "tests/test_temporal_calendars.c",
                               "tests/test_temporal_civil.c",
                               "tests/test_temporal_duration_math.c",
                               "tests/test_temporal_zones.c",
                               "tests/config_native_consumer.c",
                               "tests/test_fuzz_support.c"]
        for name in self.object_sources + ["fuzz/fuzz_common.c",
                                            "fuzz/fuzz_common.h"]:
            path = self.scratch / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("/* text-only configuration fixture */\n")
        self.object_targets = ["metadata/" + name[:-2] + ".o"
                               for name in self.object_sources]
        self.compiler.write_text(
            "#!/usr/bin/env python3\n"
            "import json, pathlib, sys\n"
            "args = sys.argv[1:]\n"
            "if '-o' not in args: raise SystemExit(1)\n"
            "out = pathlib.Path(args[args.index('-o') + 1])\n"
            "if not str(out).startswith('metadata/') or '-c' not in args:\n"
            "    raise SystemExit(1)\n"
            "source = args[-1]\n"
            "if not source.endswith('.c'): raise SystemExit(2)\n"
            "previous = json.loads(out.read_text()) if out.exists() else {}\n"
            "out.parent.mkdir(parents=True, exist_ok=True)\n"
            "out.write_text(json.dumps({'source': source,\n"
            "    'icu': '-DCONFIG_ICU' in args,\n"
            "    'builds': previous.get('builds', 0) + 1}))\n")
        self.host_icu = ["-lfixture_host_icu"]
        self.icu = ["-L/fixture/ICU library", "-licui18n", "-licuuc",
                    "-licudata", "-lstdc++"]

    def metadata(self, windows, extra, objects_only=False):
        env = os.environ.copy()
        # This is an independent synthetic Make profile, not a recursive
        # jobserver client. subprocess closes the parent jobserver descriptors.
        for name in ("MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES",
                     "CONFIG_INTL_BACKEND"):
            env.pop(name, None)
        env.pop("MSYSTEM", None)
        if hasattr(self, "coarse_bin"):
            env["PATH"] = str(self.coarse_bin) + os.pathsep + env["PATH"]
        command = [os.environ.get("QJS_TEST_MAKE", "make"),
                   "--no-print-directory", "-s", "CONFIG_CLANG=",
                   "CONFIG_COSMO=", "CONFIG_DARWIN=", "CONFIG_FREEBSD=",
                   "CONFIG_LTO=", "CONFIG_M32=", "CONFIG_PROFILE=",
                   "CONFIG_ASAN=", "CONFIG_UBSAN=", "CONFIG_MSAN=",
                   "CONFIG_TSAN=", "CONFIG_TEMPORAL=n", "CONFIG_INTL=" + ("y" if self.intl_enabled else "n"),
                   "CONFIG_INTL_BACKEND=icu",
                   "CONFIG_WIN32=" + ("y" if windows else ""),
                   "CROSS_PREFIX=" + ("fixture-target-" if windows else ""),
                   "CC=" + str(self.compiler), "HOST_CC=" + str(self.compiler),
                   "ICU_CFLAGS=-I/fixture/include", "HOST_ICU_CFLAGS=",
                   "ICU_LIBS=" + " ".join(shlex.quote(arg) for arg in self.icu),
                   "HOST_ICU_LIBS=" + " ".join(shlex.quote(arg) for arg in self.host_icu),
                   "OBJDIR=metadata",
                   "EXTRA_LIBS=" + " ".join(shlex.quote(arg) for arg in extra),
                   "metadata/quickjs.pc", "metadata/qjsc-intl-link.h",
                   "metadata/qjsc-intl-host-link.h", "metadata/config-consumer"] + self.object_targets
        if objects_only:
            command = command[:-4 - len(self.object_targets)] + self.object_targets
        check_make_call(command, cwd=str(self.scratch), env=env)
        if objects_only:
            return
        for name, expected in (("qjsc-intl-link.h", self.icu),
                               ("qjsc-intl-host-link.h", self.host_icu)):
            lines = (self.scratch / "metadata" / name).read_text().splitlines()
            actual = [json.loads(line.strip().rstrip(",")) for line in lines
                      if line.startswith('    "')]
            self.assertEqual(actual, expected)
        text = (self.scratch / "metadata/quickjs.pc").read_text()
        private = [line for line in text.splitlines()
                   if line.startswith("Libs.private:")]
        self.assertEqual(len(private), 1)
        self.assertIn("Libs: -L${libdir} -lquickjs\n", text)
        return shlex.split(private[0].split(":", 1)[1])

    def build_outputs(self):
        return [self.scratch / name for name in
                (".obj/intl-build-config", "metadata/quickjs.pc",
                 "metadata/qjsc-intl-link.h", "metadata/qjsc-intl-host-link.h",
                 "metadata/config-consumer") + tuple(self.object_targets)]

    def retain_equal_stamp_mtime(self):
        # Model a coarse timestamp filesystem: stamp replacement retains
        # the previous timestamp. Every consumer starts at that same time.
        stamp = self.scratch / ".obj/intl-build-config"
        self.fixed_mtime_ns = stamp.stat().st_mtime_ns // 1000000000 * 1000000000
        for path in self.build_outputs():
            os.utime(str(path), ns=(self.fixed_mtime_ns, self.fixed_mtime_ns))
        generator = self.scratch / "tools/intl-link-config.py"
        older = self.fixed_mtime_ns - 1000000000
        os.utime(str(generator), ns=(older, older))
        for name in self.object_sources + ["fuzz/fuzz_common.c",
                                            "fuzz/fuzz_common.h"]:
            os.utime(str(self.scratch / name), ns=(older, older))
        real_mv = shutil.which("mv")
        self.assertIsNotNone(real_mv)
        self.coarse_bin = self.scratch / "coarse-bin"
        self.coarse_bin.mkdir()
        wrapper = self.coarse_bin / "mv"
        wrapper.write_text(
            "#!/bin/sh\n"
            'if [ "$#" = 2 ] && [ "$2" = ".obj/intl-build-config" ] && [ -e "$2" ]; then\n'
            '    touch -r "$2" "$1" || exit 1\n'
            "fi\n"
            "exec " + shlex.quote(real_mv) + ' "$@"\n')
        wrapper.chmod(0o755)

    def expected(self, windows, extra):
        return ["-lm", "-lpthread"] + ([] if windows else ["-ldl"]) + extra + self.icu

    def test_windows_has_no_linux_dl_dependency(self):
        extra = ["/fixture/private library.a", "-lextra"]
        actual = self.metadata(True, extra)
        self.assertEqual(actual, self.expected(True, extra))
        self.assertNotIn("-ldl", actual)

    def test_posix_retains_dependencies_with_inherited_native_backend(self):
        extra = ["/fixture/private library.a", "-lextra"]
        # A parent native profile exports both the selector and Make overrides.
        # This fixture still exercises ICU metadata and private dependencies.
        with mock.patch.dict(os.environ, {
                "CONFIG_INTL_BACKEND": "native",
                "MAKEFLAGS": "-- CONFIG_INTL_BACKEND=native",
                "MFLAGS": "-s",
                "MAKEOVERRIDES": "CONFIG_INTL_BACKEND=native"}):
            self.assertEqual(self.metadata(False, extra),
                             self.expected(False, extra))

    def test_profile_and_extra_changes_regenerate_the_same_output(self):
        self.assertEqual(self.metadata(True, ["-lfirst"]),
                         self.expected(True, ["-lfirst"]))
        self.assertEqual(self.metadata(False, ["-lsecond"]),
                         self.expected(False, ["-lsecond"]))
        self.assertEqual(self.metadata(False, ["-lthird"]),
                         self.expected(False, ["-lthird"]))

    def test_changed_configuration_with_equal_timestamps(self):
        self.metadata(True, ["-lfirst"])
        self.retain_equal_stamp_mtime()
        self.assertEqual(self.metadata(False, ["-lsecond"]),
                         self.expected(False, ["-lsecond"]))
        self.icu = ["-lchanged_target_icu"]
        self.host_icu = ["-lchanged_host_icu"]
        self.assertEqual(self.metadata(False, ["-lthird"]),
                         self.expected(False, ["-lthird"]))
        self.assertEqual((self.scratch / ".obj/intl-build-config").stat().st_mtime_ns,
                         self.fixed_mtime_ns)
        self.assertEqual((self.scratch / "metadata/config-consumer").read_text(), "xxx")

    def test_late_and_native_objects_follow_equal_timestamp_config_changes(self):
        self.metadata(False, ["-linitial"])
        self.retain_equal_stamp_mtime()
        for target, source in zip(self.object_targets, self.object_sources):
            value = json.loads((self.scratch / target).read_text())
            self.assertEqual(value, {"source": source, "icu": True, "builds": 1})
        self.intl_enabled = False
        self.metadata(False, ["-linitial"], objects_only=True)
        for target, source in zip(self.object_targets, self.object_sources):
            value = json.loads((self.scratch / target).read_text())
            self.assertEqual(value, {"source": source, "icu": False, "builds": 2})
        self.intl_enabled = True
        self.metadata(False, ["-linitial"], objects_only=True)
        for target, source in zip(self.object_targets, self.object_sources):
            value = json.loads((self.scratch / target).read_text())
            self.assertEqual(value, {"source": source, "icu": True, "builds": 3})
        self.metadata(False, ["-linitial"], objects_only=True)
        for target in self.object_targets:
            self.assertEqual(json.loads((self.scratch / target).read_text())["builds"], 3)

    def test_unchanged_configuration_preserves_outputs(self):
        self.metadata(False, ["-lunchanged"])
        self.retain_equal_stamp_mtime()
        before = [(path.read_bytes(), path.stat().st_mtime_ns)
                  for path in self.build_outputs()]
        self.assertEqual(self.metadata(False, ["-lunchanged"]),
                         self.expected(False, ["-lunchanged"]))
        self.assertEqual(before, [(path.read_bytes(), path.stat().st_mtime_ns)
                                  for path in self.build_outputs()])


if __name__ == "__main__":
    unittest.main()
