# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# QuickJS JavaScript engine
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
# Copyright (c) 2026 Yan-Jie Wang
# SPDX-License-Identifier: MIT
project(
  'quickjs',
  'c',
  version: run_command(
    find_program('python3', 'python'),
    '-c',
    'import sys; print(open(sys.argv[1]).read().strip())',
    files('VERSION'),
    check: true,
  ).stdout().strip(),
  meson_version: '>=0.58.0',
  default_options: [
    'buildtype=debugoptimized',
    'c_std=gnu11',
    'warning_level=1',
    'b_ndebug=false',
    'b_staticpic=false',
    'default_library=static',
  ],
)

cc = meson.get_compiler('c')
if meson.is_cross_build()
  add_languages(
    'c',
    native: true,
    required: true,
  )
endif
build_cc = meson.get_compiler(
  'c',
  native: true,
)
python = find_program('python3', 'python')
inc = include_directories(
  '.',
  'include',
  'src/cutils',
  'src/dtoa',
  'src/unicode',
  'src/regexp',
)
version_arg = '-DCONFIG_VERSION="' + meson.project_version() + '"'
common_args = ['-D_GNU_SOURCE', version_arg, '-fwrapv']
intl_enabled = get_option('intl') != 'disabled'
if intl_enabled
  common_args += '-DCONFIG_INTL'
  warning('Intl is under development; only the documented completed APIs are available')
endif
if get_option('temporal')
  common_args += '-DCONFIG_TEMPORAL'
endif
if (get_option('temporal') or intl_enabled) and get_option('calendars') == 'native'
  common_args += '-DCONFIG_CALENDAR_NATIVE'
endif
if get_option('temporal') or intl_enabled
  if get_option('timezone_backend') == 'native'
    common_args += '-DCONFIG_TIMEZONE_NATIVE'
  endif
  if get_option('system_timezone_policy') == 'host-offset'
    common_args += '-DCONFIG_TIMEZONE_HOST_OFFSET'
  endif
endif
if get_option('gc_stress')
  common_args += '-DFORCE_GC_AT_MALLOC'
endif
if get_option('malloc_iter')
  common_args += '-DJS_MALLOC_USE_ITER'
endif
if get_option('b_sanitize') != 'none'
  common_args += '-fno-omit-frame-pointer'
endif

machine_args = []
machine_deps = []
foreach native : [false, true]
  compiler = native ? build_cc : cc
  system = native ? build_machine.system() : host_machine.system()
  args = common_args
  if compiler.get_id() == 'clang'
    args += compiler.get_supported_arguments(
      '-Wextra',
      '-Wno-sign-compare',
      '-Wundef',
      '-Wuninitialized',
      '-Wunused',
      '-Wno-unused-parameter',
      '-Wwrite-strings',
      '-Wchar-subscripts',
      '-funsigned-char',
    )
  endif
  if system == 'windows'
    args += '-D__USE_MINGW_ANSI_STDIO'
  elif compiler.has_function(
    'closefrom',
    prefix: '#define _GNU_SOURCE\n#include <unistd.h>',
  )
    args += '-DHAVE_CLOSEFROM'
  endif
  if compiler.sizeof('void *') == 4 and (
    native ? build_machine.cpu_family() : host_machine.cpu_family()
  ) in [
    'x86',
    'x86_64',
  ]
    args += compiler.get_supported_arguments('-msse2', '-mfpmath=sse')
  endif
  deps = [
    dependency(
      'threads',
      native: native,
    ),
  ]
  math = compiler.find_library(
    'm',
    required: false,
  )
  if math.found()
    deps += math
  endif
  if system != 'windows'
    dl = compiler.find_library(
      'dl',
      required: false,
    )
    if dl.found()
      deps += dl
    endif
  endif
  machine_args += [args]
  machine_deps += [deps]
endforeach
host_args = machine_args[0]
host_deps = machine_deps[0]
native_args = machine_args[1]
native_deps = machine_deps[1]

# qjsc discovers build-tree headers beside its executable.
configure_file(
  input: 'include/quickjs.h',
  output: 'quickjs.h',
  copy: true,
)
configure_file(
  input: 'include/quickjs-libc.h',
  output: 'quickjs-libc.h',
  copy: true,
)
install_headers(
  'include/quickjs.h',
  'include/quickjs-libc.h',
  subdir: 'quickjs',
)
# Native generators remain runnable during a cross build.
unicode_gen = executable(
  'unicode_gen',
  [
    'tools/unicode_gen.c',
    'src/cutils/cutils.c',
  ],
  native: true,
  c_args: native_args,
  include_directories: inc,
  dependencies: native_deps,
  override_options: ['b_lto=false', 'b_sanitize=none'],
  build_by_default: false,
)
unicode_tables = files('src/unicode/libunicode-table.c')
unicode_table_headers = []
if get_option('unicode_data') != ''
  ucd = meson.project_source_root() / get_option('unicode_data')
  generated_unicode_tables = custom_target(
    'unicode-tables',
    input: files(
      ucd / 'UnicodeData.txt',
      ucd / 'SpecialCasing.txt',
      ucd / 'CaseFolding.txt',
      ucd / 'CompositionExclusions.txt',
      ucd / 'DerivedNormalizationProps.txt',
      ucd / 'Scripts.txt',
      ucd / 'ScriptExtensions.txt',
      ucd / 'emoji-data.txt',
      ucd / 'emoji-sequences.txt',
      ucd / 'emoji-zwj-sequences.txt',
      ucd / 'PropList.txt',
      ucd / 'DerivedCoreProperties.txt',
    ),
    output: [
      'libunicode-table.c',
      'libunicode-table.h',
    ],
    command: [unicode_gen, ucd, '@OUTPUT0@'],
  )
  unicode_tables = [generated_unicode_tables[0]]
  unicode_table_headers = [generated_unicode_tables[1]]
endif
# Object owners wait for the generated declarations without cycling through the generator.
unicode_headers = declare_dependency(sources: unicode_table_headers)
host_deps += [unicode_headers]
native_deps += [unicode_headers]
executable(
  'unicode_gen_test',
  [
    'tools/unicode_gen.c',
    'src/unicode/libunicode.c',
    'src/cutils/cutils.c',
    unicode_tables,
  ],
  native: true,
  c_args: native_args + ['-DUSE_TEST'],
  include_directories: inc,
  dependencies: native_deps,
  override_options: ['b_lto=false', 'b_sanitize=none'],
  build_by_default: false,
)
# Selected calendar repository tests execute this build-machine tool.
calendar_boundary = executable(
  'calendar-boundary',
  [
    'tools/calendar/boundary.c',
    'src/calendar/astronomy.c',
    'src/calendar/iso.c',
    'src/calendar/lunisolar.c',
  ],
  native: true,
  c_args: native_args,
  include_directories: inc,
  dependencies: native_deps,
  override_options: ['b_lto=false', 'b_sanitize=none'],
  build_by_default: get_option('tests') and ((get_option('temporal') or intl_enabled) and get_option('calendars') == 'native'),
)

# A validated byte pack has one C owner, generated without target execution.
calendar_data = custom_target(
  'native-calendar-data',
  input: files('data/calendar/calendar-data.bin'),
  output: 'calendar-data.c',
  command: [
    python,
    files('tools/calendar/embed.py'),
    '--input', '@INPUT@',
    '--output', '@OUTPUT@',
  ],
  depend_files: files(
    'tools/calendar/acquire.py',
    'tools/data/_support/io.py',
    'tools/calendar/bundle.py',
  ),
  build_by_default: false,
)

# Native timezone views are derived by the checked C runtime reader.
# This executable always targets the build host, including in cross builds.
timezone_describe = executable(
  'timezone-describe',
  [
    'tools/timezone/describe.c',
    'src/timezone/tzif.c',
    'src/calendar/iso.c',
  ],
  native: true,
  c_args: native_args,
  include_directories: inc,
  dependencies: native_deps,
  override_options: ['b_lto=false', 'b_sanitize=none'],
  build_by_default: false,
)
timezone_data = custom_target(
  'native-timezone-data',
  input: files('data/timezone/timezone-data.bin'),
  output: 'timezone-data.c',
  command: [
    python,
    files('tools/timezone/embed.py'),
    '--input', '@INPUT@',
    '--output', '@OUTPUT@',
    '--describe', timezone_describe,
  ],
  depend_files: files(
    'tools/timezone/acquire.py',
    'tools/data/_support/io.py',
    'tools/timezone/bundle.py',
  ),
  build_by_default: false,
)
windows_timezone_data = custom_target(
  'windows-timezone-data',
  input: files(
    'data/timezone/sources/cldr/windowsZones.xml',
    'data/timezone/timezone-data.bin',
  ),
  output: 'windows-timezone-data.c',
  command: [
    python,
    files('tools/timezone/windows.py'),
    '--input', '@INPUT0@',
    '--zones', '@INPUT1@',
    '--output', '@OUTPUT@',
  ],
  depend_files: files(
    'tools/timezone/acquire.py',
    'tools/data/_support/io.py',
    'tools/timezone/bundle.py',
  ),
  build_by_default: false,
)
if intl_enabled
  intl_data = custom_target(
    'native-intl-data',
    input: files('data/intl/' + get_option('intl') + '.bin'),
    output: 'intl-data.c',
    command: [
      python,
      files('tools/intl/embed.py'),
      '--input', '@INPUT@',
      '--output', '@OUTPUT@',
    ],
    depend_files: files(
      'tools/intl/acquire.py',
      'tools/intl/bundle.py',
      'tools/intl/cldr_xml.py',
      'tools/intl/list_patterns.py',
      'tools/intl/locale_metadata.py',
      'tools/intl/locale_tables.py',
      'tools/intl/parents.py',
      'tools/intl/preferences.py',
      'tools/intl/preference_tables.py',
      'tools/intl/resolve.py',
      'tools/intl/tables.py',
    ),
  )
endif
subdir('src')

shared_build = get_option('default_library') != 'static'
static_install = get_option('default_library') != 'shared'
shared_link_args = []
library_install_dir = get_option('libdir') / 'quickjs'
shared_kwargs = {
  'install_dir': library_install_dir,
}
if host_machine.system() == 'windows'
  # Meson 0.58 installs DLLs to bindir and import libraries to libdir by default.
  library_install_dir = get_option('libdir')
endif
if shared_build
  export_style = 'elf'
  if host_machine.system() in ['windows', 'darwin']
    export_style = host_machine.system()
  endif
  export_name = export_style == 'windows' ? 'quickjs.def' : 'quickjs.exports'
  export_file = configure_file(
    input: files(
      'include/quickjs.h',
      'include/quickjs-libc.h',
    ),
    output: export_name,
    command: [python, files('tools/build/public_exports.py'), export_style, '@INPUT@'],
    capture: true,
  )
  if export_style == 'windows'
    shared_kwargs = {
      'vs_module_defs': export_file,
    }
  elif export_style == 'darwin'
    shared_link_args = [
      '-Wl,-exported_symbols_list,' + (
        meson.project_build_root() / export_name
      ),
    ]
  else
    shared_link_args = [
      '-Wl,--version-script=' + (
        meson.project_build_root() / export_name
      ),
    ]
  endif
endif
variants = []
if static_install or get_option('tests')
  variants += [['normal', false, get_option('b_lto')]]
endif
if shared_build
  variants += [['shared', false, get_option('b_lto')]]
endif
if meson.is_cross_build()
  variants += [['native', true, false]]
endif
foreach variant : variants
  tag = variant[0]
  native = variant[1]
  lto = variant[2]
  args = native ? native_args : host_args
  if tag == 'shared'
    args += '-DJS_BUILD_SHARED_LIBRARY'
  endif
  deps = native ? native_deps : host_deps
  compiler = native ? build_cc : cc
  overrides = ['b_lto=' + (lto ? 'true' : 'false')]
  if native
    overrides += ['b_sanitize=none', 'b_pie=false']
  endif
  core_objects = static_library(
    'quickjs-core-' + tag,
    engine_sources,
    native: native,
    c_args: args,
    include_directories: inc,
    dependencies: deps,
    pic: tag == 'shared',
    build_by_default: false,
    gnu_symbol_visibility: 'hidden',
    override_options: overrides,
  )
  vm_args = []
  if compiler.get_id() == 'clang' and compiler.has_multi_arguments(
    '-mllvm',
    '-tail-dup-succ-size=256',
  )
    vm_args = ['-mllvm', '-tail-dup-succ-size=256']
  endif
  vm_objects = static_library(
    'quickjs-vm-' + tag,
    vm_sources,
    native: native,
    c_args: args + vm_args,
    include_directories: inc,
    dependencies: deps,
    pic: tag == 'shared',
    build_by_default: false,
    gnu_symbol_visibility: 'hidden',
    override_options: overrides,
  )
  if not native
    host_vm_args = vm_args
  endif
  compiler_objects = static_library(
    'quickjs-compiler-' + tag,
    compiler_sources,
    native: native,
    c_args: args,
    include_directories: inc,
    dependencies: deps,
    pic: tag == 'shared',
    build_by_default: false,
    gnu_symbol_visibility: 'hidden',
    override_options: overrides,
  )
  variant_support_sources = support_sources
  variant_data_sources = data_sources
  variant_system = native ? build_machine.system() : host_machine.system()
  if variant_system == 'windows'
    variant_support_sources += windows_timezone_sources
    variant_data_sources += windows_timezone_data_sources
  endif
  support_objects = static_library(
    'quickjs-support-' + tag,
    variant_support_sources,
    native: native,
    c_args: args,
    include_directories: inc,
    dependencies: deps,
    pic: tag == 'shared',
    build_by_default: false,
    gnu_symbol_visibility: 'hidden',
    override_options: overrides,
  )
  # Tables have no engine instructions to optimize at each LTO link. Keep
  # their architecture, instrumentation, visibility and PIC with this variant.
  data_overrides = ['b_lto=false']
  if native
    data_overrides += ['b_sanitize=none', 'b_pie=false']
  endif
  data_objects = static_library(
    'quickjs-data-' + tag,
    variant_data_sources,
    native: native,
    c_args: args,
    include_directories: inc,
    dependencies: deps,
    pic: tag == 'shared',
    build_by_default: false,
    gnu_symbol_visibility: 'hidden',
    override_options: data_overrides,
  )
  objects = [
    core_objects.extract_all_objects(
      recursive: false,
    ),
    vm_objects.extract_all_objects(
      recursive: false,
    ),
    compiler_objects.extract_all_objects(
      recursive: false,
    ),
    support_objects.extract_all_objects(
      recursive: false,
    ),
    data_objects.extract_all_objects(
      recursive: false,
    ),
  ]
  libname = tag == 'normal' ? 'quickjs' : 'quickjs.' + tag
  if tag == 'shared'
    shared_overrides = []
    if compiler.get_id() == 'clang' and get_option('b_sanitize') != 'none'
      # Clang resolves sanitizer hooks from the instrumented executable.
      shared_overrides += 'b_lundef=false'
    endif
    lib = shared_library(
      'quickjs',
      objects: objects,
      dependencies: deps,
      link_args: shared_link_args,
      link_depends: export_file,
      kwargs: shared_kwargs,
      override_options: shared_overrides,
      install: true,
    )
  else
    lib = static_library(
      libname,
      objects: objects,
      native: native,
      install: not native and static_install,
      install_dir: library_install_dir,
      build_by_default: not native and (static_install or get_option('tests')),
      override_options: overrides,
    )
  endif
  if tag == 'normal'
    normal_engine = lib
    normal_support_objects = support_objects
    normal_support_sources = variant_support_sources
    normal_data_objects = data_objects
  elif tag == 'shared'
    shared_engine = lib
  else
    native_engine = lib
  endif
endforeach
engine = shared_build ? shared_engine : normal_engine
engine_link = shared_build ? [engine] : []
engine_whole = shared_build ? [] : [engine]
tool_support = shared_build ? files('src/cutils/cutils.c') : []
tool_rpath = shared_build ? get_option('prefix') / library_install_dir : ''
if get_option('tests')
  test_engine_dep = declare_dependency(
    link_with: normal_engine,
    include_directories: include_directories('include'),
    dependencies: host_deps,
  )
endif
engine_dep = declare_dependency(
  link_with: engine,
  include_directories: include_directories('include'),
  dependencies: host_deps,
)
meson.override_dependency('quickjs', engine_dep)

qjsc_variants = [['target', false]]
if meson.is_cross_build()
  qjsc_variants += [['native', true]]
endif
foreach variant : qjsc_variants
  native = variant[1]
  args = native ? native_args : host_args
  compiler = native ? build_cc : cc
  cmd = compiler.cmd_array()
  if not native
    if get_option('qjsc_cc').length() != 0
      cmd = get_option('qjsc_cc')
    elif meson.is_cross_build()
      cmd = [compiler.get_id() == 'clang' ? 'clang' : 'gcc']
    endif
  endif
  quoted_cmd = []
  foreach part : cmd
    quoted_cmd += '"' + part.replace('\\', '\\\\').replace('"', '\\"') + '"'
  endforeach
  config = configuration_data()
  config.set_quoted('CONFIG_CC', cmd[0])
  config.set('CONFIG_CC_ARGS', ', '.join(quoted_cmd))
  config.set_quoted('CONFIG_PREFIX', get_option('prefix'))
  config.set_quoted(
    'CONFIG_INCLUDE_DIR',
    get_option('prefix') / get_option('includedir') / 'quickjs',
  )
  config.set('CONFIG_SHARED_LIBRARY', not static_install and not native)
  library_suffix = '.a'
  if not static_install and not native
    library_suffix = host_machine.system() == 'darwin' ? '.dylib' : '.so'
  endif
  config.set_quoted('CONFIG_LIBRARY_SUFFIX', library_suffix)
  config.set_quoted('CONFIG_LIB_DIR', get_option('prefix') / library_install_dir)
  config.set('CONFIG_LTO', get_option('b_lto') and not native)
  lto_flag = '-flto'
  if get_option('b_lto_mode') == 'thin' and compiler.get_id() == 'clang'
    lto_flag = '-flto=thin'
  endif
  config.set_quoted('CONFIG_LTO_FLAG', lto_flag)
  config.set(
    'CONFIG_M32',
    compiler.sizeof('void *') == 4 and (
      native ? build_machine.cpu_family() : host_machine.cpu_family()
    ) in ['x86', 'x86_64'] and '-m32' not in cmd,
  )
  sanitizers = native ? [] : get_option('b_sanitize').split(',')
  foreach pair : [
    ['ASAN', 'address'],
    ['MSAN', 'memory'],
    ['UBSAN', 'undefined'],
    ['TSAN', 'thread'],
  ]
    config.set('CONFIG_' + pair[0], pair[1] in sanitizers)
  endforeach
  config_name = native ? 'qjsc-native-config.h' : 'qjsc-config.h'
  configure_file(
    input: 'tools/qjsc-config.h.in',
    output: config_name,
    configuration: config,
  )
  qjsc_args = args + ['-DQJSC_CONFIG_FILE="' + config_name + '"']
  overrides = native ? ['b_lto=false', 'b_sanitize=none'] : []
  app = executable(
    native ? 'host-qjsc' : 'qjsc',
    ['tools/qjsc.c', native ? [] : tool_support],
    native: native,
    c_args: qjsc_args,
    include_directories: inc,
    dependencies: native ? native_deps : host_deps,
    link_whole: native ? [native_engine] : engine_whole,
    link_with: native ? [] : engine_link,
    install_rpath: native ? '' : tool_rpath,
    override_options: overrides,
    gnu_symbol_visibility: 'hidden',
    install: not native,
  )
  if native
    generator_qjsc = app
  else
    qjsc = app
    target_qjsc_args = qjsc_args
    target_lto_flag = lto_flag
  endif
endforeach
# Generated bytecode depends on engine behavior even when the shared export
# list, and consequently the qjsc executable timestamp, does not change.
if meson.is_cross_build()
  generator_runtime_deps = [native_engine]
else
  generator_qjsc = qjsc
  generator_runtime_deps = engine_link + engine_whole
endif
codegen = files('tools/build/qjsc_codegen.py')
repl = custom_target(
  'repl',
  depends: generator_runtime_deps,
  input: 'tools/repl.js',
  output: 'repl.c',
  command: [
    python,
    codegen,
    meson.current_source_dir(),
    generator_qjsc,
    '@OUTPUT@',
    '-s',
    '-c',
    '-m',
    'tools/repl.js',
  ],
)
qjs = executable(
  'qjs',
  ['tools/qjs.c', repl, tool_support],
  c_args: host_args,
  include_directories: inc,
  dependencies: host_deps,
  link_whole: engine_whole,
  link_with: engine_link,
  install_rpath: tool_rpath,
  gnu_symbol_visibility: 'hidden',
  export_dynamic: host_machine.system() != 'windows',
  install: true,
)
run_test262 = executable(
  'run-test262',
  ['tools/run-test262.c', tool_support],
  c_args: host_args,
  include_directories: inc,
  dependencies: host_deps,
  link_whole: engine_whole,
  link_with: engine_link,
  gnu_symbol_visibility: 'hidden',
)

# This diagnostic uses a separate, non-runnable value representation.
if get_option('check_jsvalue')
  # These are compile checks only; the multiple tool main definitions are not linked.
  check_sources = engine_sources + vm_sources + compiler_sources + files(
    'src/quickjs-libc/event-loop.c',
    'src/quickjs-libc/host.c',
    'src/quickjs-libc/environment.c',
    'src/quickjs-libc/module-loader.c',
    'src/quickjs-libc/os.c',
    'src/quickjs-libc/std.c',
    'src/quickjs-libc/worker.c',
    'tools/qjs.c',
    'tools/qjsc.c',
    'tools/run-test262.c',
  )
  # The non-runnable pointer-tag model makes negative-tag base cases unreachable.
  check_args = ['-DCONFIG_CHECK_JSVALUE']
  if cc.get_id() == 'gcc' and cc.has_argument('-Winfinite-recursion')
    check_args += '-Wno-infinite-recursion'
  endif
  static_library(
    'quickjs-value-check',
    check_sources,
    c_args: host_args + check_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['optimization=0', 'b_lto=false'],
    install: false,
  )
endif

pkg = import('pkgconfig')
pkg_private = host_deps
if static_install and get_option('b_lto')
  # Static bitcode consumers need the target compiler's plugin and LTO mode.
  pkg_private += [target_lto_flag]
endif
pkg.generate(
  name: 'QuickJS',
  filebase: 'quickjs',
  libraries: [
    host_machine.system() == 'windows' ? '-L${libdir}' : '-L${libdir}/quickjs',
    '-lquickjs',
  ],
  install_dir: get_option('libdir') / 'pkgconfig',
  description: 'Embeddable JavaScript engine',
  subdirs: 'quickjs',
  libraries_private: pkg_private,
)
shared_modules = host_machine.system() not in ['windows', 'darwin']
subdir('doc')
subdir('examples')
subdir('tools/build')
if get_option('tests')
  subdir('tests')
endif
summary(
  {
    'Cross compilation': meson.is_cross_build(),
    'Library type': get_option('default_library'),
    'LTO': get_option('b_lto'),
    'Sanitizers': get_option('b_sanitize'),
    'Tests': get_option('tests'),
    'Examples': get_option('examples'),
    'Unicode tables': get_option('unicode_data') == '' ? 'bundled' : 'generated',
  },
  section: 'Build',
  bool_yn: true,
)

features = {
  'Temporal API': get_option('temporal'),
  'Intl profile': get_option('intl'),
}
if get_option('temporal') or intl_enabled
  features += {
    'Calendar arithmetic': get_option('calendars'),
    'Named-zone backend': get_option('timezone_backend'),
    'Default-zone fallback': get_option('system_timezone_policy'),
  }
endif
summary(features, section: 'Features', bool_yn: true)
summary(
  {
    'GC stress': get_option('gc_stress'),
    'Allocation iteration': get_option('malloc_iter'),
    'JSValue type checks': get_option('check_jsvalue'),
    'Fuzzing targets': get_option('fuzzing'),
  },
  section: 'Diagnostics',
  bool_yn: true,
)
