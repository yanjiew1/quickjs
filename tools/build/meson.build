# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
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

# Explicit developer tasks; expensive external suites are never default tests.
foreach entry : [
  ['stats', ['-qd']],
  ['microbench', ['--std', meson.project_source_root() / 'tests/microbench.js']],
]
  run_target(
    entry[0],
    command: [qjs] + entry[1],
  )
endforeach
# Downloading test data does not require a compiled engine or runner.
run_target(
  'test262-bootstrap',
  command: [
    python,
    files('test262.py'),
    'bootstrap',
    '--source',
    meson.project_source_root(),
  ],
)
foreach mode : ['run', 'default', 'check', 'update', 'legacy']
  run_target(
    'test262-' + mode,
    command: [
      python,
      files('test262.py'),
      mode,
      '--source',
      meson.project_source_root(),
      '--runner',
      run_test262,
    ],
  )
endforeach
v8_files = files(
  '../../tests/bench-v8/base.js',
  '../../tests/bench-v8/richards.js',
  '../../tests/bench-v8/deltablue.js',
  '../../tests/bench-v8/crypto.js',
  '../../tests/bench-v8/raytrace.js',
  '../../tests/bench-v8/earley-boyer.js',
  '../../tests/bench-v8/regexp.js',
  '../../tests/bench-v8/splay.js',
  '../../tests/bench-v8/navier-stokes.js',
  '../../tests/bench-v8/run_harness.js',
)
v8 = custom_target(
  'v8-source',
  input: v8_files,
  output: 'v8-combined.js',
  command: [python, files('concat.py'), '@OUTPUT@', '@INPUT@'],
  build_by_default: false,
)
run_target(
  'bench-v8',
  command: [qjs, '-d', v8],
)
if get_option('fuzzing')
  if cc.get_id() != 'clang'
    error('The fuzzing targets require a Clang compiler with libFuzzer.')
  endif
  fuzz_args = host_args + ['-fsanitize=fuzzer-no-link']
  fuzz_sources = engine_sources + compiler_sources + support_sources
  fuzz_data_sources = data_sources
  if host_machine.system() == 'windows'
    fuzz_sources += windows_timezone_sources
    fuzz_data_sources += windows_timezone_data_sources
  endif
  fuzz_engine = static_library(
    'quickjs-fuzz',
    fuzz_sources,
    c_args: fuzz_args,
    include_directories: inc,
    dependencies: host_deps,
    build_by_default: false,
  )
  # Immutable tables keep one selected owner outside LTO.
  fuzz_data = static_library(
    'quickjs-fuzz-data',
    fuzz_data_sources,
    c_args: fuzz_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    build_by_default: false,
  )
  fuzz_vm = static_library(
    'quickjs-fuzz-vm',
    vm_sources,
    c_args: fuzz_args + host_vm_args,
    include_directories: inc,
    dependencies: host_deps,
    build_by_default: false,
  )
  foreach name : ['eval', 'compile', 'regexp']
    sources = files('../../fuzz/fuzz_' + name + '.c')
    if name != 'regexp'
      sources += files('../../fuzz/fuzz_common.c')
    endif
    executable(
      'fuzz_' + name,
      sources,
      c_args: fuzz_args,
      link_args: ['-fsanitize=fuzzer'],
      link_with: [fuzz_vm, fuzz_engine, fuzz_data],
      include_directories: [inc, include_directories('../..')],
      dependencies: host_deps,
    )
  endforeach
endif
