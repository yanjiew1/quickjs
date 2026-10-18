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

fs = import('fs')

# Component declarations capture their selected fixture and compile context.
component_test_specs = []

# Stage-local test capabilities follow the selected runtime. Foundation tests
# remain available before their eventual build option is introduced.
calendar_enabled = get_option('temporal')
native_calendars = get_option('temporal')
native_timezones = get_option('temporal') and get_option('timezone_backend') == 'native'
calendar_test_sources = []
timezone_test_sources = []
system_timezone_test_sources = []
if calendar_enabled
  calendar_test_sources = files(
    '../src/calendar/iso.c',
  )
  timezone_test_sources = files(
    '../src/timezone/timezone.c',
  )
  if native_timezones
    timezone_test_sources += files(
      '../src/timezone/timezone-native.c',
      '../src/timezone/tzif.c',
    ) + [timezone_data]
  else
    timezone_test_sources += files(
      '../src/timezone/timezone-minimal.c',
    )
  endif
  if native_timezones
    system_timezone_test_sources = files(
      '../src/timezone/windows-zone.c',
    ) + [windows_timezone_data]
  endif
endif

fixture_manifest = files('../tools/build/test-fixtures.list')
fixture_inputs = []
foreach name : fs.read(fixture_manifest[0]).strip().split('\n')
  fixture_inputs += files('../' + name)
endforeach
fixtures = custom_target(
  'runtime-test-fixtures',
  input: fixture_inputs,
  output: 'test-fixtures.stamp',
  command: [
    python,
    files('../tools/build/prepare_test_tree.py'),
    meson.project_source_root(),
    meson.project_build_root(),
    fixture_manifest,
    '@OUTPUT@',
  ],
  build_by_default: true,
  build_always_stale: true,
)
test_workdir = meson.project_build_root()
if shared_modules
  bjson_module = shared_module(
    'bjson',
    'bjson.c',
    name_prefix: '',
    name_suffix: 'so',
    c_args: host_args + ['-DJS_SHARED_LIBRARY'],
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lundef=false'],
  )
endif

# Public consumers reuse the selected shared engine after its LTO link. Their
# small harnesses need no cross-DSO LTO; static consumers retain the selected LTO.
public_test_overrides = ['b_ndebug=false']
if shared_build
  public_test_overrides += 'b_lto=false'
endif
public_native_test_names = [
  'test_api',
  'test_builtin_brands',
  'test_embedding_helpers',
  'test_c_function_data',
  'test_c_closure',
  'test_regexp_oom',
  'test_host_loop',
  'test_fuzz_json',
  'test_fuzz_exception_ownership',
  'test_wait_async',
  'test_promise_oom',
  'test_qjsc_json_preload',
]
temporal_api_test = executable(
  'test_temporal_api',
  'test_temporal_api.c',
  c_args: host_args,
  include_directories: inc,
  dependencies: engine_dep,
  override_options: ['b_ndebug=false'],
)
test(
  'temporal-api',
  temporal_api_test,
  suite: ['repo', 'embedding', 'integration'],
  timeout: 120,
)
if get_option('temporal')
  foreach name : [
    'test_temporal_instant',
    'test_temporal_duration',
    'test_temporal_plain_time',
    'test_temporal_plain',
    'test_temporal_realm',
    'test_temporal_now',
    'test_temporal_zoned',
    'test_temporal_offset_wall_date_boundaries',
    'test_temporal_year_month_date_boundaries',
    'test_temporal_zero_zoned_duration_boundaries',
    'test_temporal_plain_date_duration_balance',
    'test_date_temporal_bridge',
  ]
    test(
      name,
      qjs,
      args: ['--std', 'tests/' + name + '.js'],
      suite: ['repo', 'temporal', 'integration'],
      workdir: test_workdir,
      depends: fixtures,
      timeout: 60,
    )
  endforeach
  if get_option('timezone_backend') == 'native'
    iana_args = ['--std', 'tests/test_temporal_iana.js']
    iana_env = []
    if host_machine.system() != 'windows'
      iana_args += '--default-ny'
      iana_env += 'TZ=:US/Eastern'
    endif
    test(
      'temporal-iana',
      qjs,
      args: iana_args,
      env: iana_env,
      suite: ['repo', 'temporal', 'integration'],
      workdir: test_workdir,
      depends: fixtures,
      timeout: 60,
    )
  endif
endif

# Tests that include an implementation need archive extraction, not link_whole.
native_test_names = [
  'test_api',
  'test_temporal_oom',
  'test_builtin_brands',
  'test_embedding_helpers',
  'test_c_function_data',
  'test_c_closure',
  'test_regexp_oom',
  'test_bigint_oom',
  'test_bytecode',
  'test_typed_array',
  'test_allocator',
  'test_object',
  'test_iterator',
  'test_allocator_policy',
  'test_json_stringify_oom',
  'test_json_parse_oom',
  'test_string_buffer_oom',
  'test_std_interrupt',
  'test_std_oom',
  'test_typed_array_sort_oom',
  'test_bytecode_dump',
  'test_atomics_wait',
  'test_worker_context_failure',
  'test_host_loop',
  'test_host_poll_oom',
  'test_fuzz_json',
  'test_fuzz_exception_ownership',
  'test_wait_async',
  'test_native_jobs',
  'test_promise_oom',
  'test_qjsc_json_preload',
]
foreach name : native_test_names
  public_consumer = name in public_native_test_names
  if name == 'test_temporal_oom' and not get_option('temporal')
    continue
  endif
  if name == 'test_duration_record' and not calendar_enabled
    continue
  endif
  app = executable(
    name,
    name + '.c',
    c_args: host_args,
    include_directories: [inc, include_directories('..')],
    dependencies: public_consumer ? engine_dep : test_engine_dep,
    override_options: public_consumer ? public_test_overrides : ['b_ndebug=false'],
  )
  suites = ['repo', 'native']
  if name in ['test_api', 'test_allocator', 'test_native_jobs']
    suites += 'gc-stress'
  endif
  if name in [
    'test_atomics_wait',
    'test_wait_async',
    'test_native_jobs',
    'test_worker_context_failure',
  ]
    suites += 'threading'
  endif
  # Only file-backed native tests need the staged JavaScript fixtures.
  native_test_deps = []
  if name in ['test_api', 'test_atomics_wait', 'test_host_loop']
    native_test_deps += fixtures
  endif
  test(
    name,
    app,
    suite: suites + ['unit'],
    workdir: test_workdir,
    depends: native_test_deps,
    timeout: 120,
  )
endforeach

allocation_fuzz_test = executable(
  'test_fuzz_allocations',
  [
    'test_fuzz_allocations.c',
    '../fuzz/fuzz_common.c',
  ],
  c_args: host_args,
  include_directories: [inc, include_directories('..')],
  dependencies: engine_dep,
  override_options: public_test_overrides,
)
test(
  'test_fuzz_allocations',
  allocation_fuzz_test,
  suite: ['repo', 'native', 'unit'],
  workdir: test_workdir,
  timeout: 120,
)

# Reuse the selected support archives when assertions already match. A release
# build needs an assertion-enabled algorithm owner for these white-box tests;
# immutable tables still use the selected non-LTO data archive in either case.
component_support = normal_support_objects
if get_option('b_ndebug') == 'true' or (
  get_option('b_ndebug') == 'if-release' and
  get_option('buildtype') in ['release', 'plain']
)
  component_support = static_library(
    'test-support',
    support_library_sources,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_ndebug=false'],
    pic: false,
    install: false,
  )
endif
foreach name : [
  'test_cutils',
  'test_dtoa',
  'test_unicode',
  'test_unicode_oom',
  'test_fuzz_regexp_timeout',
  'test_unicode_gen',
  'test_wait_queue',
]
  app = executable(
    name,
    name + '.c',
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [component_support, normal_data_objects],
    override_options: ['b_ndebug=false'],
  )
  suites = ['repo', 'native', 'unit']
  if name == 'test_wait_queue'
    suites += 'threading'
  endif
  test(
    name,
    app,
    suite: suites,
    workdir: test_workdir,
    timeout: 120,
  )
endforeach


# Build engine-independent library tests against their actual owners.
# Preserve authored synthetic providers; ordinary tests use the selected sources.
standalone = {
}
if calendar_enabled
  standalone += {
    'test_calendar_persian': files(
      'test_calendar_persian.c',
      '../src/calendar/data.c',
      '../src/calendar/iso.c',
      '../src/calendar/persian.c',
    ) + [calendar_data],
    'test_calendar_data': files(
      'test_calendar_data.c',
      '../src/calendar/data.c',
    ) + [calendar_data],
    'test_calendar_islamic': files(
      'test_calendar_islamic.c',
      '../src/calendar/data.c',
      '../src/calendar/iso.c',
      '../src/calendar/islamic.c',
    ) + [calendar_data],
    'test_calendar_hebrew': files(
      'test_calendar_hebrew.c',
      '../src/calendar/iso.c',
      '../src/calendar/hebrew.c',
    ),
    'test_calendar_solar': files(
      'test_calendar_solar.c',
      '../src/calendar/iso.c',
      '../src/calendar/solar.c',
    ),
    'test_system_timezone': files(
      'test_system_timezone.c',
      '../src/calendar/iso.c',
      '../src/timezone/system-zone.c',
    ) + timezone_test_sources + system_timezone_test_sources,
  }
endif

if native_timezones
  standalone += {
    'test_windows_timezone': files(
      'test_windows_timezone.c',
      '../src/timezone/windows-zone.c',
    ) + [windows_timezone_data],
    'test_native_timezone': files(
      'test_native_timezone.c',
      '../src/calendar/iso.c',
      '../src/timezone/timezone.c',
      '../src/timezone/timezone-native.c',
      '../src/timezone/tzif.c',
    ) + [timezone_data],
    'test_tzif_decoder': files(
      'test_tzif_decoder.c',
      '../src/calendar/iso.c',
      '../src/timezone/tzif.c',
    ),
  }
endif

if get_option('temporal')
  standalone += {
    'test_temporal_clock': files(
      'test_temporal_clock.c',
      '../src/temporal/epoch.c',
      '../src/temporal/clock.c',
    ),
    'test_temporal_zone_rules': files(
      'test_temporal_zone_rules.c',
      '../src/calendar/iso.c',
      '../src/temporal/epoch.c',
      '../src/temporal/iso.c',
      '../src/temporal/parse.c',
      '../src/temporal/format.c',
      '../src/temporal/time-zone.c',
      '../src/timezone/timezone.c',
    ),
    'test_temporal_relative': files(
      'test_temporal_relative.c',
      '../src/temporal/epoch.c',
      '../src/temporal/iso.c',
      '../src/temporal/civil.c',
      '../src/temporal/calendar.c',
      '../src/temporal/duration.c',
      '../src/temporal/options.c',
      '../src/temporal/relative.c',
      '../src/temporal/parse.c',
      '../src/temporal/format.c',
      '../src/temporal/time-zone.c',
    ) + calendar_test_sources + timezone_test_sources,
    'test_temporal_calendars': files(
      'test_temporal_calendars.c',
      '../src/temporal/epoch.c',
      '../src/temporal/iso.c',
      '../src/temporal/civil.c',
      '../src/temporal/calendar.c',
    ) + calendar_test_sources,
    'test_temporal_duration_math': files(
      'test_temporal_duration_math.c',
      '../src/calendar/iso.c',
      '../src/temporal/epoch.c',
      '../src/temporal/iso.c',
      '../src/temporal/options.c',
      '../src/temporal/duration.c',
      '../src/temporal/time.c',
    ),
    'test_temporal_format': files(
      'test_temporal_format.c',
      '../src/calendar/iso.c',
      '../src/temporal/epoch.c',
      '../src/temporal/iso.c',
      '../src/temporal/format.c',
      '../src/temporal/options.c',
    ),
    'test_temporal_parse': files(
      'test_temporal_parse.c',
      '../src/calendar/iso.c',
      '../src/temporal/epoch.c',
      '../src/temporal/iso.c',
      '../src/temporal/parse.c',
    ),
    'test_temporal_epoch': files(
      'test_temporal_epoch.c',
      '../src/temporal/epoch.c',
    ),
    'test_temporal_date_math': files(
      'test_temporal_date_math.c',
      '../src/calendar/iso.c',
      '../src/temporal/epoch.c',
      '../src/temporal/iso.c',
    ),
    'test_temporal_civil': files(
      'test_temporal_civil.c',
      '../src/calendar/iso.c',
      '../src/temporal/epoch.c',
      '../src/temporal/iso.c',
      '../src/temporal/civil.c',
    ),
  }
endif

if get_option('temporal') and not native_timezones
  standalone += {
    'test_temporal_minimal_zones': files(
      'test_temporal_minimal_zones.c',
      '../src/calendar/iso.c',
      '../src/temporal/epoch.c',
      '../src/temporal/iso.c',
      '../src/temporal/parse.c',
      '../src/temporal/format.c',
      '../src/temporal/time-zone.c',
      '../src/timezone/timezone.c',
      '../src/timezone/timezone-minimal.c',
    ),
  }
endif


foreach name, sources : standalone
  app = {
    'name': name,
    'sources': sources,
    'c_args': host_args,
    'override_options': ['b_ndebug=false'],
  }
  suites = ['repo', 'native']
  if name == 'test_wait_queue'
    suites += 'threading'
  endif
  component_test_specs += [{
    'target': app,
    'name': name,
    'options': {
      'suite': suites,
      'workdir': test_workdir,
      'timeout': 120,
    },
  }]
endforeach

context_c = custom_target(
  'qjsc-context-source',
  depends: generator_runtime_deps,
  input: 'fixture_qjsc_context.js',
  output: 'qjsc-context-generated.h',
  command: [
    python,
    codegen,
    meson.project_source_root(),
    generator_qjsc,
    '@OUTPUT@',
    '-e',
    '-M', 'probe-module,probe',
    'tests/fixture_qjsc_context.js',
  ],
)
context_test = executable(
  'test_qjsc_context_failures',
  ['test_qjsc_context_failures.c', context_c],
  c_args: host_args,
  include_directories: [inc, include_directories('.')],
  dependencies: engine_dep,
  override_options: public_test_overrides,
)
test(
  'test_qjsc_context_failures',
  context_test,
  suite: ['repo', 'native', 'unit'],
  workdir: test_workdir,
  timeout: 120,
)

probe = static_library(
  'qjsc-json-probe',
  '../tools/qjsc.c',
  c_args: target_qjsc_args + ['-Dmain=qjsc_json_probe_tool_main'],
  include_directories: inc,
  dependencies: host_deps,
  build_by_default: false,
)
probe_test = executable(
  'test_qjsc_json_probe',
  'test_qjsc_json_probe.c',
  c_args: host_args,
  include_directories: inc,
  link_with: probe,
  dependencies: engine_dep,
  override_options: public_test_overrides,
)
test(
  'test_qjsc_json_probe',
  probe_test,
  suite: ['repo', 'native', 'unit'],
  workdir: test_workdir,
  timeout: 120,
)

trace_reader = static_library(
  'quickjs-tracing-reader',
  '../src/quickjs/serialization/reader.c',
  c_args: host_args + ['-DDUMP_READ_OBJECT'],
  include_directories: inc,
  dependencies: host_deps,
  build_by_default: false,
)
trace_test = executable(
  'test_bytecode_trace',
  'test_bytecode.c',
  objects: trace_reader.extract_all_objects(
    recursive: false,
  ),
  c_args: host_args,
  include_directories: [inc, include_directories('..')],
  dependencies: test_engine_dep,
  override_options: ['b_ndebug=false'],
)
test(
  'test_bytecode_trace',
  trace_test,
  suite: ['repo', 'native', 'unit'],
  workdir: test_workdir,
  timeout: 120,
)

fuzz_support = executable(
  'test_fuzz_support',
  [
    'test_fuzz_support.c',
    '../fuzz/fuzz_common.c',
  ],
  c_args: host_args,
  include_directories: [inc, include_directories('..')],
  dependencies: engine_dep,
  override_options: public_test_overrides,
)
test(
  'test_fuzz_support',
  fuzz_support,
  suite: ['repo', 'native', 'unit'],
  workdir: test_workdir,
  depends: fixtures,
  timeout: 120,
)

# Meson supplies MESON_EXE_WRAPPER to drivers with a target binary argument.
json_c = custom_target(
  'qjsc-json-attributes-source',
  depends: generator_runtime_deps,
  input: files(
    'prepare_qjsc_json_attributes.py',
    'qjsc-json-attributes/main.js',
    'qjsc-json-attributes/invalid-main.js',
    'qjsc-json-attributes/native-std.js',
    'qjsc-json-attributes/payload.so',
    'qjsc-json-attributes/invalid.so',
    'qjsc-json-attributes/std',
    'qjsc-json-attributes/extended.so',
    'qjsc-json-attributes/plain.json',
  ),
  output: 'qjsc-json-attributes.c',
  command: [
    python,
    files('prepare_qjsc_json_attributes.py'),
    generator_qjsc,
    '@OUTPUT@',
  ],
)
json_exec = executable(
  'test_qjsc_json_exec',
  json_c,
  c_args: host_args,
  include_directories: inc,
  dependencies: engine_dep,
)
test(
  'qjsc-json-attributes',
  python,
  args: [
    files('run_qjsc_executable.py'),
    json_exec,
    'qjsc-json-attribute-payload-ok',
  ],
  suite: ['repo', 'compiler', 'integration'],
  workdir: test_workdir,
  timeout: 120,
)
native_std_c = custom_target(
  'qjsc-native-std-source',
  depends: generator_runtime_deps,
  input: 'qjsc-json-attributes/native-std.js',
  output: 'qjsc-native-std.c',
  command: [
    python,
    codegen,
    meson.project_source_root(),
    generator_qjsc,
    '@OUTPUT@',
    '-e',
    'tests/qjsc-json-attributes/native-std.js',
  ],
)
native_std_exec = executable(
  'test_qjsc_native_std_exec',
  native_std_c,
  c_args: host_args,
  include_directories: inc,
  dependencies: engine_dep,
)
test(
  'qjsc-native-std',
  python,
  args: [
    files('run_qjsc_executable.py'),
    native_std_exec,
    'qjsc-native-std-ok',
  ],
  suite: ['repo', 'compiler', 'integration'],
  workdir: test_workdir,
  timeout: 120,
)
collision_inputs = files(
  'prepare_qjsc_json_collisions.py',
  'qjsc-json-collisions/a.json',
  'qjsc-json-collisions/a_attributes.js',
  'qjsc-json-collisions/a_attributes_size.js',
  'qjsc-json-collisions/a_1_attributes.js',
  'qjsc-json-collisions/a_1_attributes_size.js',
  'qjsc-json-collisions/attributes-first.js',
  'qjsc-json-collisions/attributes-last.js',
  'qjsc-json-collisions/size-first.js',
  'qjsc-json-collisions/size-last.js',
)
foreach name : ['attributes-first', 'attributes-last', 'size-first', 'size-last']
  generated = custom_target(
    'qjsc-json-collision-' + name,
    depends: generator_runtime_deps,
    input: collision_inputs,
    output: 'qjsc-json-collision-' + name + '.c',
    command: [
      python,
      files('prepare_qjsc_json_collisions.py'),
      generator_qjsc,
      '@OUTPUT@',
      name,
    ],
  )
  app = executable(
    'test_qjsc_json_collision_' + name,
    generated,
    c_args: host_args,
    include_directories: inc,
    dependencies: engine_dep,
  )
  test(
    'qjsc-json-collision-' + name,
    python,
    args: [
      files('run_qjsc_executable.py'),
      app,
      'qjsc-json-collision-ok',
    ],
    suite: ['repo', 'compiler', 'integration'],
    workdir: test_workdir,
    timeout: 120,
  )
endforeach

js_tests = {
  'closure': ['tests/test_closure.js'],
  'realm': ['-m', 'tests/test_realm.js'],
  'iterator-realms': ['-m', 'tests/test_iterator_realms.js'],
  'disposal-realms': ['-m', 'tests/test_disposal_realms.js'],
  'resource-realms': ['-m', 'tests/test_resource_realms.js'],
  'language': ['tests/test_language.js'],
  'stripped-functions': ['--strip-source', '-m', 'tests/test_stripped_functions.js'],
  'module': ['-m', 'tests/test_module.js'],
  'module-names': ['-m', 'tests/test_module_names.js'],
  'builtin': ['--std', 'tests/test_builtin.js'],
  'unicode-property-grammar': ['tests/test_unicode_property_grammar.js'],
  'date-parse-boundaries': ['tests/test_date_parse_boundaries.js'],
  'date-setter-snapshot': ['tests/test_date_setter_snapshot.js'],
  'loop': ['tests/test_loop.js'],
  'bigint': ['tests/test_bigint.js'],
  'async': ['-m', 'tests/test_async.js'],
  'wait-async': ['-m', 'tests/test_wait_async.js'],
  'cyclic-import': ['tests/test_cyclic_import.js'],
  'worker': ['tests/test_worker.js'],
  'gsab-worker': ['tests/test_gsab_worker.js'],
}
if host_machine.system() == 'windows'
  js_tests += {
    'wait-worker-overflow': ['-m', 'tests/test_wait_async_worker_overflow.js'],
  }
else
  js_tests += {
    'std': ['tests/test_std.js'],
    'os-exec-wait-error': ['tests/test_os_exec_wait_error.js'],
    'var-ref': ['tests/test_var_ref.js'],
    'rw-handler': ['tests/test_rw_handler.js'],
  }
endif
if shared_modules
  js_tests += {
    'bjson': ['tests/test_bjson.js'],
    'point-module': ['examples/test_point.js'],
  }
endif
foreach name, args : js_tests
  js_test_deps = [fixtures]
  if name == 'bjson'
    js_test_deps += bjson_module
  elif name == 'point-module'
    js_test_deps += point_module
  endif
  suites = ['repo', 'javascript']
  if name in [
    'closure',
    'realm',
    'iterator-realms',
    'disposal-realms',
    'resource-realms',
    'stripped-functions',
    'async',
    'wait-async',
    'bjson',
  ]
    suites += 'gc-stress'
  endif
  if name in ['worker', 'gsab-worker', 'wait-async']
    suites += 'threading'
  endif
  test(
    name,
    qjs,
    args: args,
    suite: suites + ['integration'],
    workdir: test_workdir,
    depends: js_test_deps,
    timeout: name == 'os-exec-wait-error' ? 5 : 120,
  )
endforeach
test(
  'wait-async-agents',
  run_test262,
  args: ['-N', 'tests/test_wait_async_agents.js'],
  suite: ['repo', 'threading', 'integration'],
  workdir: test_workdir,
  depends: fixtures,
  timeout: 120,
)
sh = find_program('sh')
regexp_test = executable(
  'regexp_test',
  [
    'regexp_test.c',
    '../src/regexp/compile.c',
    '../src/regexp/exec.c',
  ],
  # TEST changes the RegExp opcode metadata, but not Unicode or C utilities.
  c_args: host_args + ['-DTEST'],
  include_directories: inc,
  dependencies: host_deps,
  link_with: [component_support, normal_data_objects],
  override_options: ['b_ndebug=false'],
)
test(
  'regexp-cli',
  sh,
  args: [files('test_regexp.sh'), regexp_test],
  suite: ['repo', 'tools', 'unit'],
  workdir: test_workdir,
  timeout: 120,
)
test(
  'qjs-cli',
  sh,
  args: [files('test_qjs_cli.sh'), qjs],
  suite: ['repo', 'tools', 'integration'],
  workdir: test_workdir,
  timeout: 120,
)
test(
  'test262-runner',
  sh,
  args: [
    files('test_run_test262.sh'),
    run_test262,
    get_option('temporal') ? 'y' : 'n',
  ],
  suite: ['repo', 'tools', 'integration'],
  workdir: test_workdir,
  depends: fixtures,
  timeout: 120,
)
test(
  'test262-timing-reports',
  python,
  args: files('test_test262_timings.py'),
  suite: ['repo', 'tools', 'unit'],
  workdir: test_workdir,
  timeout: 120,
)

if calendar_enabled
  test(
    'calendar-generator',
    python,
    args: files('test_calendar_generator.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )
endif

if native_timezones
  test(
    'timezone-generator',
    python,
    args: files('test_timezone_generator.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )
endif
if not meson.is_cross_build() and host_machine.system() != 'windows'
  expected_flags = []
  if cc.sizeof('void *') == 4 and host_machine.cpu_family() in ['x86', 'x86_64']
    expected_flags += '-m32'
  endif
  foreach pair : [
    ['address', '-fsanitize=address'],
    ['memory', '-fsanitize=memory'],
    ['undefined', '-fsanitize=undefined'],
    ['thread', '-fsanitize=thread'],
  ]
    if pair[0] in get_option('b_sanitize').split(',')
      expected_flags += pair[1]
    endif
  endforeach
  test(
    'qjsc-executable',
    python,
    args: [
      files('test_qjsc_executable.py'),
      qjsc,
      ' '.join(expected_flags),
      'n',
      get_option('temporal') ? 'y' : 'n',
      get_option('b_lto') ? lto_flag : '',
      'libquickjs' + library_suffix,
    ],
    suite: ['repo', 'compiler', 'integration'],
    workdir: test_workdir,
    timeout: 120,
  )
endif
test(
  'build-dependencies',
  python,
  args: [
    files('test_meson_dependencies.py'),
    meson.project_source_root(),
    meson.project_build_root(),
  ],
  suite: ['repo', 'build', 'unit'],
  timeout: 30,
)

promise_api_test = executable(
  'test_promise_api',
  'test_promise_api.c',
  c_args: host_args,
  include_directories: include_directories('../include'),
  dependencies: engine_dep,
  override_options: public_test_overrides,
)
test(
  'promise-api',
  promise_api_test,
  suite: ['repo', 'embedding', 'integration'],
  timeout: 120,
)

runtime_finalizers_test = executable(
  'test_runtime_finalizers',
  'test_runtime_finalizers.c',
  c_args: host_args,
  include_directories: include_directories('../include'),
  dependencies: engine_dep,
  override_options: public_test_overrides,
)
test(
  'runtime-finalizers',
  runtime_finalizers_test,
  suite: ['repo', 'embedding', 'integration'],
  timeout: 120,
)

error_api_test = executable(
  'test_error_api',
  'test_error_api.c',
  c_args: host_args,
  include_directories: include_directories('../include'),
  dependencies: engine_dep,
  override_options: public_test_overrides,
)
test(
  'error-api',
  error_api_test,
  suite: ['repo', 'embedding', 'integration'],
  timeout: 120,
)

public_api_test = executable(
  'test_shared_api',
  'test_shared_api.c',
  c_args: host_args,
  include_directories: include_directories('../include'),
  dependencies: engine_dep,
  override_options: public_test_overrides,
)
test(
  'public-embedding-api',
  public_api_test,
  suite: ['repo', 'embedding', 'integration'],
  workdir: test_workdir,
  timeout: 120,
)
if shared_build
  test(
    'shared-library-exports',
    python,
    args: [
      files('test_shared_exports.py'),
      meson.project_source_root(),
      shared_engine,
      host_machine.system(),
    ],
    suite: ['repo', 'embedding', 'integration'],
    timeout: 30,
  )
endif

test(
  'export-report-parser',
  python,
  args: files('test_export_report.py'),
  suite: ['repo', 'build', 'unit'],
  timeout: 30,
)

# Share only identical component translation units within this configuration.
# Component declarations use the same host compiler, include directories and
# dependencies; source identity, ordered flags and overrides distinguish keys.
component_object_keys = []
component_object_libraries = []
component_objects_by_context = {}
component_target_builds = []
component_target_names = []
component_targets = []
foreach spec : component_test_specs
  build = spec['target']
  target_index = -1
  foreach index : range(component_target_names.length())
    if component_target_names[index] == build['name']
      assert(component_target_builds[index] == build, 'Conflicting component target: ' + build['name'])
      target_index = index
      break
    endif
  endforeach
  if target_index == -1
    compile_args = build.get('c_args', [])
    overrides = build.get('override_options', [])
    # A context bucket avoids a quadratic interpreter loop over unrelated
    # compile settings. Full keys are still checked, so this hint cannot merge
    # sources or flag lists even if two joined strings happen to coincide.
    context = '\n'.join(compile_args + overrides)
    objects = []
    selected_objects = []
    foreach source : build['sources']
      key = [source, compile_args, overrides]
      object_index = -1
      candidates = component_objects_by_context.get(context, [])
      foreach index : candidates
        if component_object_keys[index] == key
          object_index = index
          break
        endif
      endforeach
      if object_index == -1
        object_index = component_object_keys.length()
        owner = static_library(
          'test-object-' + object_index.to_string(),
          source,
          c_args: compile_args,
          include_directories: inc,
          dependencies: host_deps,
          override_options: overrides,
          pic: false,
          install: false,
          build_by_default: false,
        )
        component_object_keys += [key]
        component_object_libraries += [owner]
        component_objects_by_context += {context: candidates + [object_index]}
      endif
      if object_index not in selected_objects
        objects += component_object_libraries[object_index].extract_all_objects(recursive: false)
        selected_objects += [object_index]
      endif
    endforeach
    target = executable(
      build['name'],
      objects: objects,
      c_args: compile_args,
      include_directories: inc,
      dependencies: host_deps,
      override_options: overrides,
      link_with: build.get('link_with', []),
      link_language: 'c',
    )
    component_target_builds += [build]
    component_target_names += [build['name']]
    component_targets += [target]
    target_index = component_targets.length() - 1
  endif
  options = spec['options']
  suites = options.get('suite', [])
  if 'unit' not in suites
    options += {'suite': suites + ['unit']}
  endif
  test(spec['name'], component_targets[target_index], kwargs: options)
endforeach

# Check that separate CI phases partition the selected repository suite.
test(
  'build-configuration',
  python,
  args: [files('test_build_configuration.py'), meson.project_source_root(), meson.project_build_root()],
  suite: ['repo', 'unit', 'build'],
  timeout: 30,
)
