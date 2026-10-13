#!/usr/bin/env python3
"""Exercise ICU Make selection using text fixtures and no engine/compiler.

These fixtures check target/host metadata selection without requiring an
installed ICU SDK or advertising an enabled runtime provider.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class ICUSelection(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="qjs-icu-selection-")
        self.addCleanup(temporary.cleanup)
        self.scratch = Path(temporary.name)
        source = Path(__file__).resolve().parents[1]
        shutil.copy2(source / "tools/icu-build-config.mk",
                     self.scratch / "icu-build-config.mk")
        self.fixture = self.scratch / "Makefile"
        self.fixture.write_text(
            "include icu-build-config.mk\n"
            "$(info TARGET_CFLAGS=$(ICU_CFLAGS))\n"
            "$(info TARGET_LIBS=$(ICU_LIBS))\n"
            "$(info HOST_CFLAGS=$(HOST_ICU_CFLAGS))\n"
            "$(info HOST_LIBS=$(HOST_ICU_LIBS))\n"
            "fixture:\n\t@:\n")
        self.pkg = self.scratch / "fixture-pkg-config"
        self.pkg.write_text(
            "#!/usr/bin/env python3\n"
            "import json, os, pathlib, sys\n"
            "args = sys.argv[1:]\n"
            "with pathlib.Path(os.environ['ICU_FIXTURE_LOG']).open('a') as log:\n"
            "    log.write(json.dumps(args) + '\\n')\n"
            "role = 'host' if args and args[0] == '--fixture-host' else 'target'\n"
            "if role == 'host': args.pop(0)\n"
            "version = os.environ.get('ICU_FIXTURE_' + role.upper() + '_VERSION', '74.1')\n"
            "for arg in args:\n"
            "    if arg.startswith('--atleast-version='):\n"
            "        parse = lambda v: tuple(map(int, v.split('.')))\n"
            "        raise SystemExit(0 if parse(version) >= parse(arg.split('=', 1)[1]) else 1)\n"
            "if '--libs' in args:\n"
            "    if not os.environ.get('ICU_FIXTURE_EMPTY_LIBS'):\n"
            "        print('-L/fixture/' + role + ' -licui18n -licuuc -licudata' +\n"
            "              (' -lprivate' if '--static' in args else ''))\n"
            "elif '--cflags' in args: print('-I/fixture/' + role)\n"
            "else: raise SystemExit(2)\n")
        self.pkg.chmod(0o755)
        self.log = self.scratch / "pkg-log.jsonl"

    def select(self, *, version="74.1", host_version="74.1", extra=(),
               cross=False, default_target=False, default_host=False,
               target_environment=False, empty_libs=False):
        env = os.environ.copy()
        for name in ("MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES", "PKG_CONFIG",
                     "HOST_PKG_CONFIG", "PKG_CONFIG_PATH", "PKG_CONFIG_LIBDIR",
                     "PKG_CONFIG_SYSROOT_DIR", "ICU_CFLAGS", "ICU_LIBS",
                     "HOST_ICU_CFLAGS", "HOST_ICU_LIBS", "ICU_STATIC"):
            env.pop(name, None)
        env.update(ICU_FIXTURE_LOG=str(self.log),
                   ICU_FIXTURE_TARGET_VERSION=version,
                   ICU_FIXTURE_HOST_VERSION=host_version)
        if target_environment:
            env["PKG_CONFIG_LIBDIR"] = "/fixture/target/pkgconfig"
        if empty_libs:
            env["ICU_FIXTURE_EMPTY_LIBS"] = "y"
        command = [os.environ.get("QJS_TEST_MAKE", "make"),
                   "--no-print-directory", "-s", "CONFIG_ICU=y",
                   "CROSS_PREFIX=" + ("fixture-target-" if cross else "")]
        if not default_target:
            command.append("PKG_CONFIG=" + str(self.pkg))
        if not default_host:
            command.append("HOST_PKG_CONFIG=" + str(self.pkg) + " --fixture-host")
        command.extend(extra)
        command.append("fixture")
        return subprocess.run(command, cwd=self.scratch, env=env, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def calls(self):
        if not self.log.exists():
            return []
        return [json.loads(line) for line in self.log.read_text().splitlines()]

    def test_api_floor_and_validated_sdk_are_accepted(self):
        for version in ("74", "74.1", "78.3", "79.1"):
            result = self.select(version=version)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("TARGET_LIBS=-L/fixture/target", result.stdout)

    def test_old_system_and_below_floor_fail_clearly(self):
        for version in ("60.3", "73.2"):
            result = self.select(version=version)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("ICU4C >=74", result.stderr)
        self.assertFalse(any("--libs" in args for args in self.calls()))

    def test_explicit_target_and_host_flags_never_query_pkg_config(self):
        result = self.select(cross=True, extra=(
            "ICU_CFLAGS=-I/explicit/target", "ICU_LIBS=/explicit/target.a",
            "HOST_ICU_CFLAGS=-I/explicit/host", "HOST_ICU_LIBS=/explicit/host.a"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("TARGET_LIBS=/explicit/target.a", result.stdout)
        self.assertIn("HOST_LIBS=/explicit/host.a", result.stdout)
        self.assertEqual(self.calls(), [])

    def test_host_sdk_version_is_checked_separately(self):
        result = self.select(cross=True, host_version="60.3")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("host ICU4C >=74", result.stderr)

    def test_cross_target_needs_explicit_selection(self):
        result = self.select(cross=True, default_target=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("explicitly target-scoped", result.stderr)
        self.assertEqual(self.calls(), [])

    def test_host_does_not_inherit_target_sysroot_implicitly(self):
        result = self.select(cross=True, default_host=True,
                             target_environment=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("explicitly host-scoped", result.stderr)

    def test_cross_wrappers_keep_target_and_host_distinct(self):
        result = self.select(cross=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("TARGET_CFLAGS=-I/fixture/target", result.stdout)
        self.assertIn("HOST_CFLAGS=-I/fixture/host", result.stdout)

    def test_static_metadata_expands_private_dependencies(self):
        result = self.select(cross=True, extra=("ICU_STATIC=y",))
        self.assertEqual(result.returncode, 0, result.stderr)
        calls = [args for args in self.calls() if "--libs" in args]
        self.assertEqual(len(calls), 2)
        self.assertTrue(all("--static" in args for args in calls))
        self.assertIn("-lprivate", result.stdout)

    def test_failed_library_discovery_cannot_become_an_empty_backend(self):
        result = self.select(empty_libs=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("nonempty ICU_LIBS", result.stderr)


if __name__ == "__main__":
    unittest.main()
