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
      '-Wno-missing-field-initializers',
      '-Wundef',
      '-Wuninitialized',
      '-Wunused',
      '-Wno-unused-parameter',
      '-Wwrite-strings',
      '-Wchar-subscripts',
      '-funsigned-char',
    )
  else
    args += compiler.get_supported_arguments(
      '-Wno-array-bounds',
      '-Wno-format-truncation',
      '-Wno-infinite-recursion',
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
subdir('src')

variants = [['normal', false, false]]
if get_option('b_lto')
  variants += [['lto', false, true]]
endif
if meson.is_cross_build()
  variants += [['native', true, false]]
endif
foreach variant : variants
  tag = variant[0]
  native = variant[1]
  lto = variant[2]
  args = native ? native_args : host_args
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
    pic: false,
    build_by_default: false,
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
    pic: false,
    build_by_default: false,
    override_options: overrides,
  )
  if tag == 'normal'
    host_vm_args = vm_args
  endif
  compiler_objects = static_library(
    'quickjs-compiler-' + tag,
    compiler_sources,
    native: native,
    c_args: args,
    include_directories: inc,
    dependencies: deps,
    pic: false,
    build_by_default: false,
    override_options: overrides,
  )
  support_objects = static_library(
    'quickjs-support-' + tag,
    support_sources,
    native: native,
    c_args: args,
    include_directories: inc,
    dependencies: deps,
    pic: false,
    build_by_default: false,
    override_options: overrides,
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
  ]
  libname = tag == 'normal' ? 'quickjs' : 'quickjs.' + tag
  lib = static_library(
    libname,
    objects: objects,
    native: native,
    install: not native,
    install_dir: get_option('libdir') / 'quickjs',
    build_by_default: not native,
    override_options: overrides,
  )
  if tag == 'normal'
    normal_engine = lib
  elif tag == 'lto'
    lto_engine = lib
  else
    native_engine = lib
  endif
endforeach
engine = get_option('b_lto') ? lto_engine : normal_engine
engine_dep = declare_dependency(
  link_with: engine,
  include_directories: include_directories('include'),
  dependencies: host_deps,
)
# Private unit tests use the same archive until shared linkage is selected.
test_engine_dep = engine_dep
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
  config.set_quoted(
    'CONFIG_LIB_DIR',
    get_option('prefix') / get_option('libdir') / 'quickjs',
  )
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
    'tools/qjsc.c',
    native: native,
    c_args: qjsc_args,
    include_directories: inc,
    dependencies: native ? native_deps : host_deps,
    link_whole: native ? native_engine : engine,
    override_options: overrides,
    install: not native,
  )
  if native
    generator_qjsc = app
  else
    qjsc = app
    target_qjsc_args = qjsc_args
  endif
endforeach
if not meson.is_cross_build()
  generator_qjsc = qjsc
endif
codegen = files('tools/build/qjsc_codegen.py')
repl = custom_target(
  'repl',
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
  ['tools/qjs.c', repl],
  c_args: host_args,
  include_directories: inc,
  dependencies: host_deps,
  link_whole: engine,
  export_dynamic: host_machine.system() != 'windows',
  install: true,
)
run_test262 = executable(
  'run-test262',
  'tools/run-test262.c',
  c_args: host_args,
  include_directories: inc,
  dependencies: host_deps,
  link_whole: engine,
)

# This diagnostic uses a separate, non-runnable value representation.
if get_option('check_jsvalue')
  # These are compile checks only; the multiple tool main definitions are not linked.
  check_sources = engine_sources + vm_sources + compiler_sources + files(
    'src/quickjs-libc/event-loop.c',
    'src/quickjs-libc/host.c',
    'src/quickjs-libc/module-loader.c',
    'src/quickjs-libc/os.c',
    'src/quickjs-libc/std.c',
    'src/quickjs-libc/worker.c',
    'tools/qjs.c',
    'tools/qjsc.c',
    'tools/run-test262.c',
  )
  static_library(
    'quickjs-value-check',
    check_sources,
    c_args: host_args + ['-DCONFIG_CHECK_JSVALUE'],
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['optimization=0', 'b_lto=false'],
    install: false,
  )
endif

pkg = import('pkgconfig')
pkg.generate(
  normal_engine,
  name: 'QuickJS',
  filebase: 'quickjs',
  description: 'Embeddable JavaScript engine',
  subdirs: 'quickjs',
  libraries_private: host_deps,
)
shared_modules = host_machine.system() not in ['windows', 'darwin']
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
