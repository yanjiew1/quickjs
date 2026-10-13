"""Actual Make selection/stamp tests using a compiler that always rejects."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

class NativeSelector(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory(prefix='qjs-native-selector-');self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name);source=Path(__file__).resolve().parents[1]
        for name in ['Makefile','VERSION']:shutil.copy2(source/name,self.root/name)
        (self.root/'tools').mkdir()
        shutil.copy2(source/'tools/icu-build-config.mk',self.root/'tools/icu-build-config.mk')
        with (self.root/'Makefile').open('a') as output:
            output.write("\nqjs-selector-probe:\n\t@printf '%s\\n' '$(CONFIG_ICU)' '$(CONFIG_INTL_NATIVE)' '$(patsubst $(OBJDIR)/%.o,%.c,$(QJS_LIB_OBJS))' '$(LIBS)' '$(HOST_LIBS)'\n")
    def probe(self,intl='y',temporal='n',backend='native',success=True):
        env=os.environ.copy()
        for key in list(env):
            if key.startswith(('CONFIG_','ICU_','HOST_','PKG_CONFIG')) or key in ['MAKEFLAGS','MFLAGS','MAKEOVERRIDES']:env.pop(key,None)
        args=[os.environ.get('QJS_TEST_MAKE','make'),'--no-print-directory','-s','CONFIG_CLANG=','CONFIG_COSMO=','CONFIG_WIN32=','CONFIG_DARWIN=','CONFIG_LTO=','CC=/bin/false','HOST_CC=/bin/false','ICU_CFLAGS=','HOST_ICU_CFLAGS=','ICU_LIBS=-lfixture_target_icu','HOST_ICU_LIBS=-lfixture_host_icu','CONFIG_INTL='+intl,'CONFIG_TEMPORAL='+temporal]
        if backend is not None:args.append('CONFIG_INTL_BACKEND='+backend)
        args+=['qjs-selector-probe','.obj/temporal-build-config']
        result=subprocess.run(args,cwd=self.root,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.assertEqual(result.returncode==0,success,result.stderr)
        return result.stdout.splitlines()
    def test_native_has_one_plain_c_data_owner_and_no_icu(self):
        values=self.probe();self.assertEqual(values[:2],['n','y']);sources=values[2].split()
        self.assertEqual(sources.count('src/intl/data/locale-metadata.c'),1)
        self.assertEqual(sources.count('src/temporal/time-zone.c'),1)
        for name in ['src/intl/libintl.c','src/intl/plural-icu.c','src/quickjs/builtins/intl/values.c','src/quickjs/builtins/intl/list-format.c']:self.assertNotIn(name,sources)
        self.assertFalse(any(name.startswith('src/quickjs/builtins/temporal/') and name!='src/quickjs/builtins/temporal/common.c' for name in sources))
        self.assertNotIn('fixture_target_icu',values[3]);self.assertNotIn('fixture_host_icu',values[4])
    def test_temporal_remains_a_real_icu_consumer(self):
        values=self.probe(temporal='y',backend='icu');self.assertEqual(values[:2],['y','n']);self.assertIn('src/temporal/time-zone.c',values[2]);self.assertIn('fixture_target_icu',values[3]);self.assertIn('fixture_host_icu',values[4])
    def test_native_temporal_uses_native_time_zone_data_without_icu(self):
        values=self.probe(temporal='y');self.assertEqual(values[:2],['n','y'])
        self.assertIn('src/temporal/time-zone.c',values[2])
        self.assertIn('src/timezone/timezone.c',values[2])
        self.assertNotIn('fixture_target_icu',values[3]);self.assertNotIn('fixture_host_icu',values[4])
    def test_default_and_explicit_icu_preserve_source_selection(self):
        default=self.probe(backend=None);explicit=self.probe(backend='native');self.assertEqual(default,explicit);self.assertEqual(default[:2],['n','y']);self.assertNotIn('src/intl/libintl.c',default[2]);self.assertIn('src/intl/provider-native.c',default[2].split())
    def test_disabled_intl_derives_no_native_feature(self):
        self.assertEqual(self.probe(intl='n')[:2],['n','n'])
    def test_unknown_empty_wildcard_and_multiple_selectors_reject(self):
        for backend in ['unknown','','%','icu native']:self.probe(backend=backend,success=False)
    def test_backend_transition_changes_the_exact_common_stamp(self):
        self.probe(backend='icu');before=(self.root/'.obj/temporal-build-config').read_bytes()
        self.probe();after=(self.root/'.obj/temporal-build-config').read_bytes();self.assertNotEqual(before,after)
        self.probe();self.assertEqual(after,(self.root/'.obj/temporal-build-config').read_bytes())
if __name__=='__main__':unittest.main()
