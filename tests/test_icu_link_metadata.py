#!/usr/bin/env python3
"""Check actual Make-selected static metadata without compiling an engine.

Windows is a simulated Make configuration. These tool tests make no claim
about Windows ICU development files or an enabled Windows runtime.
"""
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
        self.icu = ["-L/fixture/ICU library", "-licui18n", "-licuuc",
                    "-licudata", "-lstdc++"]

    def metadata(self, windows, extra):
        env = os.environ.copy()
        # This is an independent synthetic Make profile, not a recursive
        # jobserver client. subprocess closes the parent jobserver descriptors.
        for name in ("MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES"):
            env.pop(name, None)
        env.pop("MSYSTEM", None)
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
                   "HOST_ICU_LIBS=-lfixture_host_icu", "OBJDIR=metadata",
                   "EXTRA_LIBS=" + " ".join(shlex.quote(arg) for arg in extra),
                   "metadata/quickjs.pc"]
        subprocess.check_call(command, cwd=str(self.scratch), env=env)
        text = (self.scratch / "metadata/quickjs.pc").read_text()
        private = [line for line in text.splitlines()
                   if line.startswith("Libs.private:")]
        self.assertEqual(len(private), 1)
        self.assertIn("Libs: -L${libdir} -lquickjs\n", text)
        return shlex.split(private[0].split(":", 1)[1])

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


if __name__ == "__main__":
    unittest.main()
