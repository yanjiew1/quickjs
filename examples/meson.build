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
