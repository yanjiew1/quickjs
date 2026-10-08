#!/usr/bin/env python3
"""Check actual Make-selected static metadata without compiling an engine.

Windows is a simulated Make configuration. These tool tests make no claim
about Windows ICU development files or an enabled Windows runtime.
"""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


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
        self.host_icu = ["-lfixture_host_icu"]
        self.icu = ["-L/fixture/ICU library", "-licui18n", "-licuuc",
                    "-licudata", "-lstdc++"]

    def metadata(self, windows, extra):
        env = os.environ.copy()
        # This is an independent synthetic Make profile, not a recursive
        # jobserver client. subprocess closes the parent jobserver descriptors.
        for name in ("MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES"):
            env.pop(name, None)
        env.pop("MSYSTEM", None)
        if hasattr(self, "coarse_bin"):
            env["PATH"] = str(self.coarse_bin) + os.pathsep + env["PATH"]
        command = [os.environ.get("QJS_TEST_MAKE", "make"),
                   "--no-print-directory", "-s", "CONFIG_CLANG=",
                   "CONFIG_COSMO=", "CONFIG_DARWIN=", "CONFIG_FREEBSD=",
                   "CONFIG_LTO=", "CONFIG_M32=", "CONFIG_PROFILE=",
                   "CONFIG_ASAN=", "CONFIG_UBSAN=", "CONFIG_MSAN=",
                   "CONFIG_TSAN=", "CONFIG_TEMPORAL=n", "CONFIG_INTL=y",
                   "CONFIG_WIN32=" + ("y" if windows else ""),
                   "CROSS_PREFIX=" + ("fixture-target-" if windows else ""),
                   "CC=" + str(self.compiler), "HOST_CC=" + str(self.compiler),
                   "ICU_CFLAGS=-I/fixture/include", "HOST_ICU_CFLAGS=",
                   "ICU_LIBS=" + " ".join(shlex.quote(arg) for arg in self.icu),
                   "HOST_ICU_LIBS=" + " ".join(shlex.quote(arg) for arg in self.host_icu),
                   "OBJDIR=metadata",
                   "EXTRA_LIBS=" + " ".join(shlex.quote(arg) for arg in extra),
                   "metadata/quickjs.pc", "metadata/qjsc-intl-link.h",
                   "metadata/qjsc-intl-host-link.h", "metadata/config-consumer"]
        subprocess.check_call(command, cwd=str(self.scratch), env=env)
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
                 "metadata/config-consumer")]

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

    def test_posix_retains_platform_and_extra_dependencies(self):
        extra = ["/fixture/private library.a", "-lextra"]
        self.assertEqual(self.metadata(False, extra), self.expected(False, extra))

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
