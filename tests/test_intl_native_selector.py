"""Make selection and deliberate rejection before native frontend activation."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class NativeSelector(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='qjs-native-selector-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        source = Path(__file__).resolve().parents[1]
        for name in ('Makefile', 'VERSION'):
            shutil.copy2(source/name, self.root/name)
        with (self.root/'Makefile').open('a') as output:
            output.write("\nqjs-selector-probe:\n\t@printf '%s\\n' '$(CONFIG_ICU)' '$(CONFIG_INTL_NATIVE)' '$(QUICKJS_SRCS)' '$(LIBS)' '$(HOST_LIBS)'\n")

    def probe(self, intl='y', temporal='n', backend='native', success=True):
        env = os.environ.copy()
        for key in list(env):
            if key.startswith(('CONFIG_', 'ICU_', 'HOST_', 'PKG_CONFIG')) or key in ('MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES'):
                env.pop(key, None)
        args = [os.environ.get('QJS_TEST_MAKE', 'make'), '--no-print-directory', '-s',
                'CONFIG_CLANG=', 'CONFIG_COSMO=', 'CONFIG_WIN32=', 'CONFIG_DARWIN=',
                'CONFIG_LTO=', 'CC=/bin/false', 'HOST_CC=/bin/false',
                'ICU_CFLAGS=', 'HOST_ICU_CFLAGS=', 'ICU_LIBS=-lfixture_target_icu',
                'HOST_ICU_LIBS=-lfixture_host_icu', 'CONFIG_INTL='+intl,
                'CONFIG_TEMPORAL='+temporal]
        if backend is not None:
            args.append('CONFIG_INTL_BACKEND='+backend)
        args += ['qjs-selector-probe', '.obj/intl-build-config']
        result = subprocess.run(args, cwd=self.root, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(result.returncode == 0, success, result.stderr)
        return result

    def test_native_frontends_require_the_separate_activation_commit(self):
        for intl, temporal in (('y', 'n'), ('n', 'y'), ('y', 'y')):
            result = self.probe(intl=intl, temporal=temporal, success=False)
            self.assertIn('native Intl/Temporal frontends require the separate activation commit', result.stderr)

    def test_frontend_disabled_native_selection_has_no_icu(self):
        values = self.probe(intl='n').stdout.splitlines()
        self.assertEqual(values[:2], ['n', 'n'])
        self.assertNotIn('src/intl/provider-native.c', values[2].split())
        self.assertNotIn('fixture_target_icu', values[3])
        self.assertNotIn('fixture_host_icu', values[4])

    def test_temporal_remains_a_real_icu_consumer(self):
        values = self.probe(temporal='y', backend='icu').stdout.splitlines()
        self.assertEqual(values[:2], ['y', 'n'])
        self.assertIn('src/temporal/time-zone.c', values[2])
        self.assertIn('fixture_target_icu', values[3])
        self.assertIn('fixture_host_icu', values[4])

    def test_default_and_explicit_icu_preserve_source_selection(self):
        default = self.probe(backend=None).stdout.splitlines()
        explicit = self.probe(backend='icu').stdout.splitlines()
        self.assertEqual(default, explicit)
        self.assertEqual(default[:2], ['y', 'n'])
        self.assertIn('src/intl/libintl.c', default[2])
        self.assertNotIn('provider-native.c', default[2])

    def test_unknown_empty_wildcard_and_multiple_selectors_reject(self):
        for backend in ('unknown', '', '%', 'icu native'):
            self.probe(backend=backend, success=False)

    def test_backend_transition_changes_the_exact_common_stamp(self):
        self.probe(backend='icu')
        before = (self.root/'.obj/intl-build-config').read_bytes()
        self.probe(intl='n')
        after = (self.root/'.obj/intl-build-config').read_bytes()
        self.assertNotEqual(before, after)
        self.probe(intl='n')
        self.assertEqual(after, (self.root/'.obj/intl-build-config').read_bytes())


if __name__ == '__main__':
    unittest.main()
