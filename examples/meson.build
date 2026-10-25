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

if shared_modules and (get_option('examples') or get_option('tests'))
  point_module = shared_module(
    'point',
    'point.c',
    name_prefix: '',
    name_suffix: 'so',
    c_args: host_args + ['-DJS_SHARED_LIBRARY'],
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lundef=false'],
  )
  if get_option('examples')
    shared_module(
      'fib',
      'fib.c',
      name_prefix: '',
      name_suffix: 'so',
      c_args: host_args + ['-DJS_SHARED_LIBRARY'],
      include_directories: inc,
      dependencies: host_deps,
      override_options: ['b_lundef=false'],
    )
  endif
endif
if get_option('examples')
  hello_flags = [
    '-e',
    '-fno-string-normalize',
    '-fno-map',
    '-fno-promise',
    '-fno-typedarray',
    '-fno-regexp',
    '-fno-json',
    '-fno-eval',
    '-fno-proxy',
    '-fno-date',
    '-fno-module-loader',
    'examples/hello.js',
  ]
  hello_c = custom_target(
    'hello-source',
    depends: generator_runtime_deps,
    input: 'hello.js',
    output: 'hello.c',
    command: [python, codegen, meson.project_source_root(), generator_qjsc, '@OUTPUT@'] + hello_flags,
  )
  executable(
    'hello',
    hello_c,
    c_args: host_args,
    include_directories: inc,
    dependencies: engine_dep,
  )
  hello_module_c = custom_target(
    'hello-module-source',
    depends: generator_runtime_deps,
    input: [
      'hello_module.js',
      'fib_module.js',
      'message.json',
    ],
    output: 'hello_module.c',
    command: [
      python,
      codegen,
      meson.project_source_root(),
      generator_qjsc,
      '@OUTPUT@',
      '-e',
      '-m',
      'examples/hello_module.js',
    ],
  )
  executable(
    'hello_module',
    hello_module_c,
    c_args: host_args,
    include_directories: inc,
    dependencies: engine_dep,
  )
  fib_c = custom_target(
    'fib-source',
    depends: generator_runtime_deps,
    input: 'test_fib.js',
    output: 'test_fib.c',
    command: [
      python,
      codegen,
      meson.project_source_root(),
      generator_qjsc,
      '@OUTPUT@',
      '-e',
      '-M',
      'examples/fib.so,fib',
      '-m',
      'examples/test_fib.js',
    ],
  )
  executable(
    'test_fib',
    [fib_c, 'fib.c'],
    c_args: host_args,
    include_directories: inc,
    dependencies: engine_dep,
  )
endif
