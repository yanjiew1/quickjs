fs = import('fs')
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
test_deps = [fixtures]
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
  test_deps += [bjson_module, point_module]
endif

default_time_zone_api_test = executable(
  'test_default_time_zone_api',
  'test_default_time_zone_api.c',
  c_args: host_args,
  include_directories: inc,
  dependencies: engine_dep,
  override_options: ['b_ndebug=false'],
)
test(
  'default-time-zone-api',
  default_time_zone_api_test,
  suite: ['repo', 'embedding'],
  timeout: 30,
)
foreach backend : ['minimal', 'native']
  identifier_sources = files(
    'test_timezone_identifier.c',
    '../src/timezone/identifier.c',
    '../src/timezone/timezone.c',
  )
  if backend == 'native'
    identifier_sources += files(
      '../src/calendar/iso.c',
      '../src/timezone/timezone-native.c',
      '../src/timezone/tzif.c',
    ) + [timezone_data]
  else
    identifier_sources += files('../src/timezone/timezone-minimal.c')
  endif
  identifier_test = executable(
    'test_timezone_identifier_' + backend,
    identifier_sources,
    c_args: host_args + (backend == 'native' ? ['-DTEST_NATIVE_ZONE'] : []),
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_ndebug=false'],
  )
  test(
    'timezone-identifier-' + backend,
    identifier_test,
    suite: ['repo', 'native'],
    timeout: 30,
  )
endforeach

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
  suite: ['repo', 'embedding'],
  timeout: 120,
)
if get_option('temporal')
  test(
    'temporal-calendar-profile',
    qjs,
    args: ['--std', 'tests/test_temporal_calendars.js', get_option('calendars')],
    suite: ['repo', 'temporal'],
    workdir: test_workdir,
    depends: test_deps,
    timeout: 60,
  )
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
      suite: ['repo', 'temporal'],
      workdir: test_workdir,
      depends: test_deps,
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
      suite: ['repo', 'temporal'],
      workdir: test_workdir,
      depends: test_deps,
      timeout: 60,
    )
  endif
endif

# Tests that include an implementation need archive extraction, not link_whole.
native_test_names = [
  'test_duration_record',
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
  app = executable(
    name,
    name + '.c',
    c_args: host_args,
    include_directories: [inc, include_directories('..')],
    dependencies: test_engine_dep,
    override_options: ['b_ndebug=false'],
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
  test(
    name,
    app,
    suite: suites,
    workdir: test_workdir,
    depends: test_deps,
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
  dependencies: test_engine_dep,
  override_options: ['b_ndebug=false'],
)
test(
  'test_fuzz_allocations',
  allocation_fuzz_test,
  suite: ['repo', 'native'],
  workdir: test_workdir,
  depends: test_deps,
  timeout: 120,
)

# Build engine-independent library tests against their actual owners.
standalone = {
  'test_intl_locale_grammar': [
    'test_intl_locale_grammar.c',
    '../src/intl/common/locale-grammar.c',
  ],
  'test_intl_number_decimal': [
    'test_intl_number_decimal.c',
    '../src/intl/common/decimal.c',
    '../src/intl/common/number-decimal.c',
    '../src/dtoa/dtoa.c',
    '../src/cutils/cutils.c',
  ],
  'test_intl_decimal': [
    'test_intl_decimal.c',
    '../src/intl/common/decimal.c',
  ],
  'test_temporal_noniso_calendars': [
    'test_temporal_noniso_calendars.c',
    '../src/calendar/astronomy.c',
    '../src/calendar/calendar.c',
    '../src/calendar/calendar-native.c',
    '../src/calendar/data.c',
    '../src/calendar/eras.c',
    '../src/calendar/hebrew.c',
    '../src/calendar/islamic.c',
    '../src/calendar/iso.c',
    '../src/calendar/lunisolar.c',
    '../src/calendar/native-months.c',
    '../src/calendar/persian.c',
    '../src/calendar/solar.c',
    '../src/temporal/calendar.c',
    '../src/temporal/civil.c',
    '../src/temporal/epoch.c',
    '../src/temporal/iso.c',
    '../src/temporal/noniso-calendar.c',
    '../src/temporal/noniso-arithmetic.c',
    calendar_data,
  ],
  'test_calendar_native': [
    'test_calendar_native.c',
    '../src/calendar/astronomy.c',
    '../src/calendar/calendar.c',
    '../src/calendar/calendar-native.c',
    '../src/calendar/data.c',
    '../src/calendar/eras.c',
    '../src/calendar/hebrew.c',
    '../src/calendar/islamic.c',
    '../src/calendar/iso.c',
    '../src/calendar/lunisolar.c',
    '../src/calendar/native-months.c',
    '../src/calendar/persian.c',
    '../src/calendar/solar.c',
    calendar_data,
  ],
  'test_calendar_eras_future': [
    'test_calendar_eras_future.c',
    '../src/calendar/data.c',
    '../src/calendar/eras.c',
    '../src/calendar/iso.c',
  ],
  'test_calendar_eras': [
    'test_calendar_eras.c',
    '../src/calendar/data.c',
    '../src/calendar/eras.c',
    '../src/calendar/iso.c',
    calendar_data,
  ],
  'test_calendar_core': [
    'test_calendar_core.c',
    '../src/calendar/calendar.c',
    '../src/calendar/calendar-minimal.c',
    '../src/calendar/iso.c',
  ],
  'test_calendar_lunisolar': [
    'test_calendar_lunisolar.c',
    '../src/calendar/astronomy.c',
    '../src/calendar/iso.c',
    '../src/calendar/lunisolar.c',
  ],
  'test_calendar_astronomy': [
    'test_calendar_astronomy.c',
    '../src/calendar/astronomy.c',
    '../src/calendar/iso.c',
  ],
  'test_calendar_persian': [
    'test_calendar_persian.c',
    '../src/calendar/data.c',
    '../src/calendar/iso.c',
    '../src/calendar/persian.c',
    calendar_data,
  ],
  'test_calendar_data': [
    'test_calendar_data.c',
    '../src/calendar/data.c',
    calendar_data,
  ],
  'test_calendar_islamic': [
    'test_calendar_islamic.c',
    '../src/calendar/data.c',
    '../src/calendar/iso.c',
    '../src/calendar/islamic.c',
    calendar_data,
  ],
  'test_calendar_hebrew': [
    'test_calendar_hebrew.c',
    '../src/calendar/iso.c',
    '../src/calendar/hebrew.c',
  ],
  'test_calendar_solar': [
    'test_calendar_solar.c',
    '../src/calendar/iso.c',
    '../src/calendar/solar.c',
  ],
  'test_system_timezone': [
    'test_system_timezone.c',
    '../src/calendar/iso.c',
    '../src/timezone/system-zone.c',
    '../src/timezone/timezone.c',
    '../src/timezone/timezone-minimal.c',
    '../src/timezone/windows-zone.c',
    windows_timezone_data,
  ],
  'test_windows_timezone': [
    'test_windows_timezone.c',
    '../src/timezone/windows-zone.c',
    windows_timezone_data,
  ],
  'test_native_timezone': [
    'test_native_timezone.c',
    '../src/calendar/iso.c',
    '../src/timezone/timezone.c',
    '../src/timezone/timezone-native.c',
    '../src/timezone/tzif.c',
    timezone_data,
  ],
  'test_tzif_decoder': [
    'test_tzif_decoder.c',
    '../src/calendar/iso.c',
    '../src/timezone/tzif.c',
  ],
  'test_temporal_clock': [
    'test_temporal_clock.c',
    '../src/temporal/epoch.c',
    '../src/temporal/clock.c',
  ],
  'test_temporal_zone_rules': [
    'test_temporal_zone_rules.c',
    '../src/calendar/iso.c',
    '../src/temporal/epoch.c',
    '../src/temporal/iso.c',
    '../src/temporal/parse.c',
    '../src/temporal/format.c',
    '../src/temporal/time-zone.c',
    '../src/timezone/identifier.c',
    '../src/timezone/timezone.c',
  ],
  'test_temporal_relative': [
    'test_temporal_relative.c',
    '../src/calendar/calendar.c',
    '../src/calendar/calendar-minimal.c',
    '../src/calendar/iso.c',
    '../src/temporal/epoch.c',
    '../src/temporal/iso.c',
    '../src/temporal/civil.c',
    '../src/temporal/calendar.c',
    '../src/temporal/noniso-calendar.c',
    '../src/temporal/noniso-arithmetic.c',
    '../src/temporal/duration.c',
    '../src/temporal/options.c',
    '../src/temporal/relative.c',
    '../src/temporal/parse.c',
    '../src/temporal/format.c',
    '../src/temporal/time-zone.c',
    '../src/timezone/identifier.c',
    '../src/timezone/timezone.c',
    '../src/timezone/timezone-minimal.c',
  ],
  'test_temporal_minimal_zones': [
    'test_temporal_minimal_zones.c',
    '../src/calendar/iso.c',
    '../src/temporal/epoch.c',
    '../src/temporal/iso.c',
    '../src/temporal/parse.c',
    '../src/temporal/format.c',
    '../src/temporal/time-zone.c',
    '../src/timezone/identifier.c',
    '../src/timezone/timezone.c',
    '../src/timezone/timezone-minimal.c',
  ],
  'test_temporal_calendars': [
    'test_temporal_calendars.c',
    '../src/calendar/calendar.c',
    '../src/calendar/calendar-minimal.c',
    '../src/calendar/iso.c',
    '../src/temporal/epoch.c',
    '../src/temporal/iso.c',
    '../src/temporal/civil.c',
    '../src/temporal/calendar.c',
    '../src/temporal/noniso-calendar.c',
    '../src/temporal/noniso-arithmetic.c',
  ],
  'test_temporal_duration_math': [
    'test_temporal_duration_math.c',
    '../src/calendar/iso.c',
    '../src/temporal/epoch.c',
    '../src/temporal/iso.c',
    '../src/temporal/options.c',
    '../src/temporal/duration.c',
    '../src/temporal/time.c',
  ],
  'test_temporal_format': [
    'test_temporal_format.c',
    '../src/calendar/iso.c',
    '../src/temporal/epoch.c',
    '../src/temporal/iso.c',
    '../src/temporal/format.c',
    '../src/temporal/options.c',
  ],
  'test_temporal_parse': [
    'test_temporal_parse.c',
    '../src/calendar/iso.c',
    '../src/temporal/epoch.c',
    '../src/temporal/iso.c',
    '../src/temporal/parse.c',
  ],
  'test_temporal_epoch': [
    'test_temporal_epoch.c',
    '../src/temporal/epoch.c',
  ],
  'test_temporal_date_math': [
    'test_temporal_date_math.c',
    '../src/calendar/iso.c',
    '../src/temporal/epoch.c',
    '../src/temporal/iso.c',
  ],
  'test_temporal_civil': [
    'test_temporal_civil.c',
    '../src/calendar/iso.c',
    '../src/temporal/epoch.c',
    '../src/temporal/iso.c',
    '../src/temporal/civil.c',
  ],
  'test_cutils': [
    'test_cutils.c',
    '../src/cutils/cutils.c',
  ],
  'test_dtoa': [
    'test_dtoa.c',
    '../src/dtoa/dtoa.c',
    '../src/cutils/cutils.c',
  ],
  'test_unicode': [
    'test_unicode.c',
    '../src/unicode/libunicode.c',
    unicode_tables,
    '../src/cutils/cutils.c',
  ],
  'test_unicode_oom': [
    'test_unicode_oom.c',
    '../src/regexp/compile.c',
    '../src/regexp/exec.c',
    '../src/unicode/libunicode.c',
    unicode_tables,
    '../src/cutils/cutils.c',
  ],
  'test_fuzz_regexp_timeout': [
    'test_fuzz_regexp_timeout.c',
    '../src/regexp/compile.c',
    '../src/regexp/exec.c',
    '../src/unicode/libunicode.c',
    unicode_tables,
    '../src/cutils/cutils.c',
  ],
  'test_unicode_gen': [
    'test_unicode_gen.c',
    '../src/cutils/cutils.c',
  ],
  'test_wait_queue': [
    'test_wait_queue.c',
    '../src/libwait/wait-queue.c',
  ],
}
foreach name, sources : standalone
  app = executable(
    name,
    sources,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_ndebug=false'],
  )
  suites = ['repo', 'native']
  if name == 'test_wait_queue'
    suites += 'threading'
  endif
  test(
    name,
    app,
    suite: suites,
    workdir: test_workdir,
    depends: test_deps,
    timeout: 120,
  )
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
    'tests/fixture_qjsc_context.js',
  ],
)
context_test = executable(
  'test_qjsc_context_failures',
  ['test_qjsc_context_failures.c', context_c],
  c_args: host_args,
  include_directories: [inc, include_directories('.')],
  dependencies: test_engine_dep,
  override_options: ['b_ndebug=false'],
)
test(
  'test_qjsc_context_failures',
  context_test,
  suite: ['repo', 'native'],
  workdir: test_workdir,
  depends: test_deps,
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
  dependencies: test_engine_dep,
  override_options: ['b_ndebug=false'],
)
test(
  'test_qjsc_json_probe',
  probe_test,
  suite: ['repo', 'native'],
  workdir: test_workdir,
  depends: test_deps,
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
  suite: ['repo', 'native'],
  workdir: test_workdir,
  depends: test_deps,
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
  dependencies: test_engine_dep,
  override_options: ['b_ndebug=false'],
)
test(
  'test_fuzz_support',
  fuzz_support,
  suite: ['repo', 'native'],
  workdir: test_workdir,
  depends: test_deps,
  timeout: 120,
)

# Meson supplies MESON_EXE_WRAPPER to drivers with a target binary argument.
runner_prefix = ''
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
    '',
  ],
)
json_exec = executable(
  'test_qjsc_json_exec',
  json_c,
  c_args: host_args,
  include_directories: inc,
  dependencies: test_engine_dep,
)
test(
  'qjsc-json-attributes',
  python,
  args: [
    files('run_qjsc_json_attributes.py'),
    runner_prefix,
    json_exec,
    'qjsc-json-attribute-payload-ok',
  ],
  suite: ['repo', 'compiler'],
  workdir: test_workdir,
  depends: test_deps,
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
  dependencies: test_engine_dep,
)
test(
  'qjsc-native-std',
  python,
  args: [
    files('run_qjsc_json_attributes.py'),
    runner_prefix,
    native_std_exec,
    'qjsc-native-std-ok',
  ],
  suite: ['repo', 'compiler'],
  workdir: test_workdir,
  depends: test_deps,
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
      '',
    ],
  )
  app = executable(
    'test_qjsc_json_collision_' + name,
    generated,
    c_args: host_args,
    include_directories: inc,
    dependencies: test_engine_dep,
  )
  test(
    'qjsc-json-collision-' + name,
    python,
    args: [
      files('run_qjsc_json_collisions.py'),
      runner_prefix,
      app,
      'qjsc-json-collision-ok',
    ],
    suite: ['repo', 'compiler'],
    workdir: test_workdir,
    depends: test_deps,
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
  suites = ['repo', 'javascript']
  if name in [
    'closure',
    'realm',
    'iterator-realms',
    'disposal-realms',
    'resource-realms',
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
    suite: suites,
    workdir: test_workdir,
    depends: test_deps,
    timeout: 120,
  )
endforeach
test(
  'wait-async-agents',
  run_test262,
  args: ['-N', 'tests/test_wait_async_agents.js'],
  suite: ['repo', 'threading'],
  workdir: test_workdir,
  depends: test_deps,
  timeout: 120,
)
sh = find_program('sh')
regexp_test = executable(
  'regexp_test',
  [
    'regexp_test.c',
    '../src/regexp/compile.c',
    '../src/regexp/exec.c',
    '../src/unicode/libunicode.c',
    unicode_tables,
    '../src/cutils/cutils.c',
  ],
  c_args: host_args + ['-DTEST'],
  include_directories: inc,
  dependencies: host_deps,
)
test(
  'regexp-cli',
  sh,
  args: [files('test_regexp.sh'), runner_prefix, regexp_test],
  suite: ['repo', 'tools'],
  workdir: test_workdir,
  timeout: 120,
)
test(
  'qjs-cli',
  sh,
  args: [files('test_qjs_cli.sh'), runner_prefix, qjs],
  suite: ['repo', 'tools'],
  workdir: test_workdir,
  timeout: 120,
)
test(
  'test262-runner',
  sh,
  args: [
    files('test_run_test262.sh'),
    runner_prefix,
    run_test262,
    get_option('temporal') ? 'y' : 'n',
  ],
  suite: ['repo', 'tools'],
  workdir: test_workdir,
  depends: test_deps,
  timeout: 120,
)
test(
  'test262-timing-reports',
  python,
  args: files('test_test262_timings.py'),
  suite: ['repo', 'tools'],
  workdir: test_workdir,
  timeout: 120,
)
test(
  'test262-calendar-selection',
  python,
  args: files('test_temporal_test262_selection.py'),
  suite: ['repo', 'tools'],
  timeout: 120,
)

test(
  'calendar-boundary-export',
  python,
  args: [files('test_calendar_boundary.py'), calendar_boundary],
  depends: calendar_boundary,
  suite: ['repo', 'native'],
  timeout: 60,
)

test(
  'calendar-generator',
  python,
  args: files('test_calendar_generator.py'),
  suite: ['repo', 'native'],
  timeout: 30,
)

test(
  'timezone-generator',
  python,
  args: files('test_timezone_generator.py'),
  suite: ['repo', 'native'],
  timeout: 30,
)
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
      '',
      ' '.join(expected_flags),
      'n',
      get_option('temporal') ? 'y' : 'n',
      get_option('b_lto') ? lto_flag : '',
    ],
    suite: ['repo', 'compiler'],
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
  suite: ['repo', 'build'],
  timeout: 30,
)

promise_api_test = executable(
  'test_promise_api',
  'test_promise_api.c',
  c_args: host_args,
  include_directories: include_directories('../include'),
  dependencies: engine_dep,
  override_options: ['b_ndebug=false'],
)
test(
  'promise-api',
  promise_api_test,
  suite: ['repo', 'embedding'],
  timeout: 120,
)

public_api_test = executable(
  'test_shared_api',
  'test_shared_api.c',
  c_args: host_args,
  include_directories: include_directories('../include'),
  dependencies: engine_dep,
  override_options: ['b_ndebug=false'],
)
test(
  'public-embedding-api',
  public_api_test,
  suite: ['repo', 'embedding'],
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
    suite: ['repo', 'embedding'],
    timeout: 30,
  )
endif

test(
  'export-report-parser',
  python,
  args: files('test_export_report.py'),
  suite: ['repo', 'build'],
  timeout: 30,
)
