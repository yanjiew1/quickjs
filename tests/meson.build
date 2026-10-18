fs = import('fs')

# Component declarations capture their selected fixture and compile context.
component_test_specs = []

# Stage-local test capabilities follow the selected runtime. Foundation tests
# remain available before their eventual build option is introduced.
calendar_enabled = true
native_calendars = true
native_timezones = false
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
  timezone_test_sources += files(
    '../src/timezone/timezone-minimal.c',
  )
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

# Tests that include an implementation need archive extraction, not link_whole.
native_test_names = [
  'test_api',
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
    suite: suites + ['unit'],
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
  suite: ['repo', 'native', 'unit'],
  workdir: test_workdir,
  depends: test_deps,
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
    depends: test_deps,
    timeout: 120,
  )
endforeach


# Build engine-independent library tests against their actual owners.
# Preserve authored synthetic providers; ordinary tests use the selected sources.
standalone = {
}
standalone += {
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

