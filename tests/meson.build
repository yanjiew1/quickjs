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

if intl_enabled
  test(
    'data-source-pins',
    python,
    args: files('test_source_pins.py'),
    suite: ['repo', 'unit', 'generator'],
    timeout: 30,
  )

  test(
    'data-regeneration',
    python,
    args: files('test_data_regeneration.py'),
    suite: ['repo', 'unit', 'generator'],
    timeout: 30,
  )
endif

# Stage-local test capabilities follow the selected runtime. Foundation tests
# remain available before their eventual build option is introduced.
calendar_enabled = (get_option('temporal') or intl_enabled)
native_calendars = (get_option('temporal') or intl_enabled) and get_option('calendars') == 'native'
native_timezones = (get_option('temporal') or intl_enabled) and get_option('timezone_backend') == 'native'
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
  'test_temporal_oom',
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

# Reuse the selected support archives when assertions already match. A release
# build needs an assertion-enabled algorithm owner for these white-box tests;
# immutable tables still use the selected non-LTO data archive in either case.
component_support = normal_support_objects
if get_option('b_ndebug') == 'true' or (
  get_option('b_ndebug') == 'if-release' and
  get_option('buildtype') in ['release', 'plain']
)
  component_sources = []
  foreach source : normal_support_sources
    if source not in host_service_sources
      component_sources += source
    endif
  endforeach
  component_support = static_library(
    'test-support',
    component_sources,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_ndebug=false'],
    pic: false,
    install: false,
  )
endif

default_time_zone_api_test = executable(
  'test_default_time_zone_api',
  'test_default_time_zone_api.c',
  c_args: host_args,
  include_directories: inc,
  dependencies: engine_dep,
  override_options: public_test_overrides,
)
test(
  'data-io',
  python,
  args: files('test_data_io.py'),
  suite: ['repo', 'generator', 'unit'],
  timeout: 30,
)

test(
  'default-time-zone-api',
  default_time_zone_api_test,
  suite: ['repo', 'embedding', 'integration'],
  timeout: 30,
)
if calendar_enabled
  backend = get_option('timezone_backend')
  identifier_test = executable(
    'test_timezone_identifier_' + backend,
    'test_timezone_identifier.c',
    c_args: host_args + (native_timezones ? ['-DTEST_NATIVE_ZONE'] : []),
    include_directories: inc,
    dependencies: host_deps,
    link_with: [component_support, normal_data_objects],
    override_options: ['b_ndebug=false'],
  )
  test(
    'timezone-identifier-' + backend,
    identifier_test,
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )
endif

temporal_api_test = executable(
  'test_temporal_api',
  'test_temporal_api.c',
  c_args: host_args,
  include_directories: inc,
  dependencies: engine_dep,
  override_options: public_test_overrides,
)
test(
  'temporal-api',
  temporal_api_test,
  suite: ['repo', 'embedding', 'integration'],
  timeout: 120,
)
if get_option('temporal')
  test(
    'temporal-calendar-profile',
    qjs,
    args: ['--std', 'tests/test_temporal_calendars.js', get_option('calendars')],
    suite: ['repo', 'temporal', 'integration'],
    workdir: test_workdir,
    depends: fixtures,
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
  'test_duration_record',
  'test_intl_api',
  'test_intl_supported_values_oom',
  'test_intl_case_oom',
  'test_intl_defaults',
  'test_intl_list_oom',
  'test_intl_segmenter_oom',
  'test_intl_plural_oom',
  'test_intl_number_oom',
  'test_intl_collator_oom',
  'test_intl_date_time_oom',
  'test_intl_relative_oom',
  'test_intl_duration_oom',
  'test_intl_display_oom',
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
  if name.startswith('test_intl_') and name not in ['test_intl_api', 'test_intl_defaults'] and not intl_enabled
    continue
  endif
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

# Drivers use ordinary archive extraction: synthetic era data and complete
# synthetic zone backends in the driver keep their production peers unextracted.
component_tests = []
if get_option('temporal') and native_calendars
  component_tests += 'test_temporal_noniso_calendars'
endif
if native_calendars
  component_tests += [
    'test_calendar_native',
    'test_calendar_eras_future',
    'test_calendar_eras',
    'test_calendar_lunisolar',
    'test_calendar_astronomy',
    'test_calendar_persian',
    'test_calendar_data',
    'test_calendar_islamic',
    'test_calendar_hebrew',
    'test_calendar_solar',
  ]
endif
if calendar_enabled and not native_calendars
  component_tests += 'test_calendar_core'
endif
if calendar_enabled
  component_tests += 'test_system_timezone'
endif
if native_timezones
  component_tests += [
    'test_windows_timezone',
    'test_native_timezone',
    'test_tzif_decoder',
  ]
endif
if get_option('temporal')
  component_tests += [
    'test_temporal_clock',
    'test_temporal_zone_rules',
    'test_temporal_calendars',
    'test_temporal_duration_math',
    'test_temporal_format',
    'test_temporal_parse',
    'test_temporal_epoch',
    'test_temporal_date_math',
    'test_temporal_civil',
  ]
  if not native_timezones
    component_tests += 'test_temporal_minimal_zones'
  endif
endif

# The portable Windows-mapping oracle also runs on non-Windows hosts, whose
# selected engine correctly omits these objects. Keep its data outside LTO.
if native_timezones and host_machine.system() != 'windows'
  windows_test_data = static_library(
    'test-windows-timezone-data',
    windows_timezone_data,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
  )
endif
foreach name : component_tests
  extra_sources = []
  libraries = [component_support, normal_data_objects]
  if name == 'test_windows_timezone' and host_machine.system() != 'windows'
    extra_sources += files('../src/timezone/windows-zone.c')
    libraries += windows_test_data
  endif
  app = executable(
    name,
    [name + '.c', extra_sources],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    name,
    app,
    suite: ['repo', 'native', 'unit'],
    workdir: test_workdir,
    timeout: 120,
  )
endforeach

# Build engine-independent library tests against their actual owners.
# Drivers borrow selected algorithms; only independent oracles and tool/test
# helpers need their own target-machine, assertion-enabled compilation owner.
component_libraries = [component_support, normal_data_objects]
component_helpers = static_library(
  'test-boundary-helpers',
  'unicode_boundary_test.c',
  c_args: host_args,
  include_directories: inc,
  dependencies: host_deps,
  override_options: ['b_ndebug=false'],
  install: false,
)
if intl_enabled
  intl_test_tools = static_library(
    'test-intl-tools',
    files(
      'intl_collation_ordered.c',
      '../src/intl/collation/compare.c',
      '../tools/intl/collation-archive.c',
      '../tools/intl/collation-builder.c',
      '../tools/intl/collation-ir.c',
      '../tools/intl/collation-map.c',
      '../tools/intl/collation-program.c',
      '../tools/intl/collation-relations.c',
      '../tools/intl/collation-reorder.c',
      '../tools/intl/collation-root-input.c',
      '../tools/intl/collation-settings.c',
      '../tools/intl/collation-snapshot.c',
      '../tools/intl/collation-text.c',
      '../tools/intl/collation-trie.c',
      '../tools/intl/collation-weights.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_ndebug=false'],
    install: false,
  )
endif
if get_option('intl') == 'native'
  segment_property_test_library = static_library(
    'test-segment-property',
    '../tools/intl/segment-property.c',
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_ndebug=false'],
    install: false,
  )
endif

if intl_enabled
  intl_number_format_options_test = executable(
    'test_intl_number_format_options',
    'test_intl_number_format_options.c',
    c_args: host_args,
    include_directories: inc,
    dependencies: test_engine_dep,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-number-format-options',
    intl_number_format_options_test,
    args: files('test_intl_number_format_options.js'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )
  intl_locale_options_test = executable(
    'test_intl_locale_options',
    'test_intl_locale_options.c',
    c_args: host_args,
    include_directories: inc,
    dependencies: test_engine_dep,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-locale-options',
    intl_locale_options_test,
    args: files('test_intl_locale_options.js'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )
  intl_number_value_test = executable(
    'test_intl_number_value',
    'test_intl_number_value.c',
    c_args: host_args,
    include_directories: inc,
    dependencies: test_engine_dep,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-number-value',
    intl_number_value_test,
    args: files('test_intl_number_value.js'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )
  intl_digit_options_test = executable(
    'test_intl_digit_options',
    'test_intl_digit_options.c',
    c_args: host_args,
    include_directories: inc,
    dependencies: test_engine_dep,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-digit-options',
    intl_digit_options_test,
    args: files('test_intl_digit_options.js'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )
endif

if intl_enabled
  intl_plural_rules_test = executable(
    'test_intl_plural_rules',
    files(
      'test_intl_plural_rules.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-plural-rules',
    intl_plural_rules_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )
  intl_plural_owner_sources = files(
    'test_intl_plural_data.c',
  )
  intl_plural_fixture = custom_target(
    'intl-plural-authored-fixture',
    output: 'intl-plural-authored-fixture.c',
    command: [python, files('intl_plural_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_plural_fixture_objects = static_library(
    ('intl-plural-authored-fixture') + '-objects',
    intl_plural_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_plural_authored = executable(
    'test_intl_plural_authored',
    intl_plural_owner_sources,
    objects: [
      intl_plural_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1', '-DTEST_INTL_PROFILE=0']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-plural-authored',
    intl_plural_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )
  intl_locale_fixture = custom_target(
    'intl-locale-authored-fixture',
    output: 'intl-locale-authored-fixture.c',
    command: [python, files('intl_locale_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_locale_fixture_objects = static_library(
    ('intl-locale-authored-fixture') + '-objects',
    intl_locale_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_collation_fixture = custom_target(
    'intl-collation-authored-fixture',
    output: 'intl-collation-authored-fixture.c',
    command: [python, files('intl_collation_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_collation_fixture_objects = static_library(
    ('intl-collation-authored-fixture') + '-objects',
    intl_collation_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_collation_context_fixture = custom_target(
    'intl-collation-context-fixture',
    output: 'intl-collation-context-fixture.c',
    command: [python, files('intl_collation_fixture.py'), '--contexts', '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_collation_context_fixture_objects = static_library(
    ('intl-collation-context-fixture') + '-objects',
    intl_collation_context_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_collation_elements_fixture = custom_target(
    'intl-collation-elements-fixture',
    output: 'intl-collation-elements-fixture.c',
    command: [python, files('intl_collation_elements_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files('intl_collation_fixture.py'),
  )
  intl_collation_elements_fixture_objects = static_library(
    ('intl-collation-elements-fixture') + '-objects',
    intl_collation_elements_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )

  intl_collation_ir_fixture = custom_target(
    'intl-collation-ir-fixture',
    output: 'intl-collation-ir-fixture.c',
    command: [python, files('intl_collation_ir_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_collation_ir.py',
      '../tools/intl/collation_ir.py',
      '../tools/intl/collation_rules.py',
      '../tools/intl/collation_programs.py',
    ),
  )
  intl_collation_ir_fixture_objects = static_library(
    ('intl-collation-ir-fixture') + '-objects',
    intl_collation_ir_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_collation_program_fixture = custom_target(
    'intl-collation-program-fixture',
    output: 'intl-collation-program-fixture.c',
    command: [python, files('intl_collation_program_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_collation_ir.py',
      'intl_collation_root_input_fixture.py',
      'test_intl_collation_root_input.py',
      'test_intl_collation_ordinals.py',
      'test_intl_collation_fractional.py',
    ),
  )
  intl_collation_program_fixture_objects = static_library(
    ('intl-collation-program-fixture') + '-objects',
    intl_collation_program_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_collation_root_input_fixture = custom_target(
    'intl-collation-root-input-fixture',
    output: 'intl-collation-root-input-fixture.c',
    command: [python, files('intl_collation_root_input_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_collation_root_input.py',
      'test_intl_collation_ordinals.py',
      'test_intl_collation_fractional.py',
      '../tools/intl/collation_root_input.py',
      '../tools/intl/collation_ordinals.py',
      '../tools/intl/collation_fractional.py',
      '../tools/intl/collation_groups.py',
    ),
  )
  intl_collation_root_input_fixture_objects = static_library(
    ('intl-collation-root-input-fixture') + '-objects',
    intl_collation_root_input_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )

  intl_numbering_owner_sources = files(
    'test_intl_numbering_data.c',
  )
  intl_numbering_fixture = custom_target(
    'intl-numbering-authored-fixture',
    output: 'intl-numbering-authored-fixture.c',
    command: [python, files('intl_numbering_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_numbering_fixture_objects = static_library(
    ('intl-numbering-authored-fixture') + '-objects',
    intl_numbering_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_numbering_authored = executable(
    'test_intl_numbering_authored',
    intl_numbering_owner_sources,
    objects: [
      intl_numbering_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-numbering-authored',
    intl_numbering_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )
  intl_number_symbol_owner_sources = files(
    'test_intl_number_symbols.c',
  )
  intl_number_symbol_fixture = custom_target(
    'intl-number-symbol-authored-fixture',
    output: 'intl-number-symbol-authored-fixture.c',
    command: [python, files('intl_number_symbol_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_number_symbol_fixture_objects = static_library(
    ('intl-number-symbol-authored-fixture') + '-objects',
    intl_number_symbol_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_number_symbol_authored = executable(
    'test_intl_number_symbols_authored',
    intl_number_symbol_owner_sources,
    objects: [
      intl_number_symbol_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1', '-DTEST_INTL_PROFILE=0']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-number-symbols-authored',
    intl_number_symbol_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )
  intl_number_pattern_owner_sources = files(
    'test_intl_number_patterns.c',
  )
  intl_number_pattern_fixture = custom_target(
    'intl-number-pattern-authored-fixture',
    output: 'intl-number-pattern-authored-fixture.c',
    command: [python, files('intl_number_pattern_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_number_pattern_fixture_objects = static_library(
    ('intl-number-pattern-authored-fixture') + '-objects',
    intl_number_pattern_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_number_pattern_authored = executable(
    'test_intl_number_patterns_authored',
    intl_number_pattern_owner_sources,
    objects: [
      intl_number_pattern_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1', '-DTEST_INTL_PROFILE=0']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-number-patterns-authored',
    intl_number_pattern_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_compact_owner_sources = files(
    'test_intl_compact_data.c',
  )
  intl_compact_fixture = custom_target(
    'intl-compact-authored-fixture',
    output: 'intl-compact-authored-fixture.c',
    command: [python, files('intl_compact_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_compact_fixture_objects = static_library(
    ('intl-compact-authored-fixture') + '-objects',
    intl_compact_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_compact_authored = executable(
    'test_intl_compact_authored',
    intl_compact_owner_sources,
    objects: [
      intl_compact_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1', '-DTEST_INTL_PROFILE=0']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-compact-authored',
    intl_compact_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_number_compact_test = executable(
    'test_intl_number_compact',
    files(
      'test_intl_number_compact.c',
    ),
    objects: [
      intl_compact_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-number-compact',
    intl_number_compact_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_currency_owner_sources = files(
    'test_intl_currency_data.c',
  )
  intl_currency_fixture = custom_target(
    'intl-currency-authored-fixture',
    output: 'intl-currency-authored-fixture.c',
    command: [python, files('intl_currency_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_currencies.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_currency_fixture_objects = static_library(
    ('intl-currency-authored-fixture') + '-objects',
    intl_currency_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_currency_authored = executable(
    'test_intl_currency_authored',
    intl_currency_owner_sources,
    objects: [
      intl_currency_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1', '-DTEST_INTL_PROFILE=0']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-currency-authored',
    intl_currency_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_currency_display_test = executable(
    'test_intl_currency_display',
    files(
      'test_intl_currency_display.c',
    ),
    objects: [
      intl_currency_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-currency-display',
    intl_currency_display_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_number_layout_owner_sources = files(
    'test_intl_number_layout_data.c',
  )
  intl_number_layout_fixture = custom_target(
    'intl-number-layout-authored-fixture',
    output: 'intl-number-layout-authored-fixture.c',
    command: [python, files('intl_number_layout_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_list_patterns.py',
      'test_intl_numbering.py',
      'test_intl_number_patterns.py',
      'test_intl_number_layout.py',
    ),
  )
  intl_number_layout_fixture_objects = static_library(
    ('intl-number-layout-authored-fixture') + '-objects',
    intl_number_layout_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_number_layout_authored = executable(
    'test_intl_number_layout_authored',
    intl_number_layout_owner_sources,
    objects: [
      intl_number_layout_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1', '-DTEST_INTL_PROFILE=0']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-number-layout-authored',
    intl_number_layout_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_relative_format_test = executable(
    'test_intl_relative_format',
    files(
      'test_intl_relative_format.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-relative-format',
    intl_relative_format_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_duration_options_test = executable(
    'test_intl_duration_options',
    files(
      'test_intl_duration_options.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-duration-options',
    intl_duration_options_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_duration_decimal_test = executable(
    'test_intl_duration_decimal',
    files(
      'test_intl_duration_decimal.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-duration-decimal',
    intl_duration_decimal_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_duration_digital_test = executable(
    'test_intl_duration_digital',
    files(
      'test_intl_duration_digital.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-duration-digital',
    intl_duration_digital_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_duration_format_test = executable(
    'test_intl_duration_format',
    files(
      'test_intl_duration_format.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-duration-format',
    intl_duration_format_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_language_match_sources = files(
    'test_intl_language_match.c',
  )
  intl_language_match_fixture = custom_target(
    'intl-language-match-fixture',
    output: 'intl-language-match-fixture.c',
    command: [python, files('intl_display_fixture.py'), '--language', '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_display_names.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_language_match_fixture_objects = static_library(
    ('intl-language-match-fixture') + '-objects',
    intl_language_match_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_language_match_test = executable(
    'test_intl_language_match',
    intl_language_match_sources,
    objects: [
      intl_language_match_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_LANGUAGE_MATCH_PREPARED=0']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-language-match',
    intl_language_match_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_language_format_sources = files(
    'test_intl_language_format.c',
  )
  intl_language_format_fixture = custom_target(
    'intl-language-format-fixture',
    output: 'intl-language-format-fixture.c',
    command: [python, files('intl_display_fixture.py'), '--render', '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_display_names.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_language_format_fixture_objects = static_library(
    ('intl-language-format-fixture') + '-objects',
    intl_language_format_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_language_format_test = executable(
    'test_intl_language_format',
    intl_language_format_sources,
    objects: [
      intl_language_format_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_LANGUAGE_FORMAT_PREPARED=0']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-language-format',
    intl_language_format_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_calendar_name_sources = files(
    'test_intl_calendar_names.c',
  )
  intl_calendar_name_modes = {
    'regular': '0',
    'ambiguous': '1',
    'conflicting': '2',
    'empty': '3',
  }
  foreach mode, number : intl_calendar_name_modes
    intl_calendar_name_fixture = custom_target(
      'intl-calendar-names-' + mode + '-fixture',
      output: 'intl-calendar-names-' + mode + '-fixture.c',
      command: [python, files('intl_calendar_name_fixture.py'), '--mode', mode, '--output', '@OUTPUT@'],
      depend_files: intl_tool_dependencies + files(
        'test_intl_display_names.py',
        'test_intl_list_patterns.py',
      ),
    )
    intl_calendar_name_fixture_objects = static_library(
      ('intl-calendar-names-' + mode + '-fixture') + '-objects',
      intl_calendar_name_fixture,
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      override_options: ['b_lto=false'],
      install: false,
      build_by_default: false,
    )
    intl_calendar_name_test = executable(
      ('test_intl_calendar_names_' + mode),
      intl_calendar_name_sources,
      objects: [
        intl_calendar_name_fixture_objects.extract_all_objects(recursive: false),
      ],
      c_args: (host_args + [
        ('-DTEST_INTL_CALENDAR_NAME_MODE=' + number)
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-calendar-names-' + mode),
      intl_calendar_name_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
  endforeach

  intl_display_selection_fixture = custom_target(
    'intl-display-selection-fixture',
    output: 'intl-display-selection-fixture.c',
    command: [python, files('intl_display_fixture.py'), '--selection', '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_display_names.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_display_selection_fixture_objects = static_library(
    ('intl-display-selection-fixture') + '-objects',
    intl_display_selection_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_display_selection_test = executable(
    'test_intl_display_selection',
    files(
      'test_intl_display_selection.c',
    ),
    objects: [
      intl_display_selection_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-display-selection',
    intl_display_selection_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_display_owner_sources = files(
    'test_intl_display_data.c',
  )
  foreach fixture : ['authored', 'empty']
    fixture_args = []
    if fixture == 'empty'
      fixture_args = ['--empty']
    endif
    intl_display_fixture = custom_target(
      'intl-display-' + fixture + '-fixture',
      output: 'intl-display-' + fixture + '-fixture.c',
      command: [python, files('intl_display_fixture.py'), '--output', '@OUTPUT@'] + fixture_args,
      depend_files: intl_tool_dependencies + files(
        'test_intl_display_names.py',
        'test_intl_list_patterns.py',
      ),
    )
    intl_display_fixture_objects = static_library(
      ('intl-display-' + fixture + '-fixture') + '-objects',
      intl_display_fixture,
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      override_options: ['b_lto=false'],
      install: false,
      build_by_default: false,
    )
    intl_display_test = executable(
      ('test_intl_display_' + fixture),
      intl_display_owner_sources,
      objects: [
        intl_display_fixture_objects.extract_all_objects(recursive: false),
      ],
      c_args: (host_args + [
        '-DTEST_INTL_PREPARED=0',
        ('-DTEST_INTL_EMPTY=' + ((fixture == 'empty') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-display-' + fixture),
      intl_display_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
  endforeach

  intl_date_available_fixture = custom_target(
    'intl-date-available-authored-fixture',
    output: 'intl-date-available-authored-fixture.c',
    command: [python, files('intl_date_available_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_date_available_formats.py',
      'test_intl_date_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_available_fixture_objects = static_library(
    ('intl-date-available-authored-fixture') + '-objects',
    intl_date_available_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_available_source_fixture = custom_target(
    'intl-date-available-source-fixture',
    output: 'intl-date-available-source-fixture.c',
    command: [python, files('intl_date_available_source_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_date_available_formats.py',
      'test_intl_date_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_available_source_fixture_objects = static_library(
    ('intl-date-available-source-fixture') + '-objects',
    intl_date_available_source_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_available_source_test = executable(
    'test_intl_date_available_source',
    files(
      'test_intl_date_available_source.c',
    ),
    objects: [
      intl_date_available_source_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-available-source',
    intl_date_available_source_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_available_authored = executable(
    'test_intl_date_available_authored',
    files(
      'test_intl_date_available_data.c',
    ),
    objects: [
      intl_date_available_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-available-authored',
    intl_date_available_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_calendar_name_data_fixture = custom_target(
    'intl-calendar-name-authored-fixture',
    output: 'intl-calendar-name-authored-fixture.c',
    command: [python, files('intl_calendar_name_data_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_calendar_names.py',
      'test_intl_date_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_calendar_name_data_fixture_objects = static_library(
    ('intl-calendar-name-authored-fixture') + '-objects',
    intl_calendar_name_data_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_calendar_name_authored = executable(
    'test_intl_calendar_name_authored',
    files(
      'test_intl_calendar_name_data.c',
    ),
    objects: [
      intl_calendar_name_data_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-calendar-name-authored',
    intl_calendar_name_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_name_select_fixture = custom_target(
    'intl-date-name-select-authored-fixture',
    output: 'intl-date-name-select-authored-fixture.c',
    command: [python, files('intl_date_name_select_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_calendar_names.py',
      'test_intl_date_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_name_select_fixture_objects = static_library(
    ('intl-date-name-select-authored-fixture') + '-objects',
    intl_date_name_select_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_name_select_test = executable(
    'test_intl_date_name_select',
    files(
      'test_intl_date_name_select.c',
    ),
    objects: [
      intl_date_name_select_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-name-select',
    intl_date_name_select_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_combination_fixture = custom_target(
    'intl-date-combination-authored-fixture',
    output: 'intl-date-combination-authored-fixture.c',
    command: [python, files('intl_date_combination_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_date_combinations.py',
      'test_intl_date_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_combination_fixture_objects = static_library(
    ('intl-date-combination-authored-fixture') + '-objects',
    intl_date_combination_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_combination_authored = executable(
    'test_intl_date_combination_authored',
    files(
      'test_intl_date_combination_data.c',
    ),
    objects: [
      intl_date_combination_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-combination-authored',
    intl_date_combination_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_style_fixture = custom_target(
    'intl-date-style-authored-fixture',
    output: 'intl-date-style-authored-fixture.c',
    command: [python, files('intl_date_style_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_date_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_style_fixture_objects = static_library(
    ('intl-date-style-authored-fixture') + '-objects',
    intl_date_style_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_style_authored = executable(
    'test_intl_date_style_authored',
    files(
      'test_intl_date_style_data.c',
    ),
    objects: [
      intl_date_style_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-style-authored',
    intl_date_style_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_field_owner_sources = files(
    'test_intl_date_field_data.c',
  )
  intl_date_field_fixture = custom_target(
    'intl-date-field-authored-fixture',
    output: 'intl-date-field-authored-fixture.c',
    command: [python, files('intl_date_field_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_date_field_names.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_field_fixture_objects = static_library(
    ('intl-date-field-authored-fixture') + '-objects',
    intl_date_field_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_field_authored = executable(
    'test_intl_date_field_authored',
    intl_date_field_owner_sources,
    objects: [
      intl_date_field_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-field-authored',
    intl_date_field_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_duration_owner_sources = files(
    'test_intl_duration_data.c',
  )
  intl_duration_fixture = custom_target(
    'intl-duration-authored-fixture',
    output: 'intl-duration-authored-fixture.c',
    command: [python, files('intl_duration_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_duration_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_duration_fixture_objects = static_library(
    ('intl-duration-authored-fixture') + '-objects',
    intl_duration_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_duration_authored = executable(
    'test_intl_duration_authored',
    intl_duration_owner_sources,
    objects: [
      intl_duration_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-duration-authored',
    intl_duration_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_datetime_basic_match = executable(
    'test_intl_datetime_basic_match',
    files(
      'test_intl_datetime_basic_match.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-datetime-basic-match',
    intl_datetime_basic_match,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_calendar_identity_fixture = custom_target(
    'intl-date-calendar-identity-fixture',
    output: 'intl-date-calendar-identity-fixture.c',
    command: [python, files('intl_date_calendar_identity_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_date_calendar_identity_fixture_objects = static_library(
    ('intl-date-calendar-identity-fixture') + '-objects',
    intl_date_calendar_identity_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_calendar_identity_test = executable(
    'test_intl_date_calendar_identity',
    files(
      'test_intl_date_calendar_identity.c',
    ),
    objects: [
      intl_date_calendar_identity_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-calendar-identity',
    intl_date_calendar_identity_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_source_candidate_fixture = custom_target(
    'intl-date-source-candidate-fixture',
    output: 'intl-date-source-candidate-fixture.c',
    command: [python, files('intl_date_source_candidate_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_date_source_candidate_fixture_objects = static_library(
    ('intl-date-source-candidate-fixture') + '-objects',
    intl_date_source_candidate_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_source_candidate_test = executable(
    'test_intl_date_source_candidate',
    files(
      'test_intl_date_source_candidate.c',
    ),
    objects: [
      intl_date_source_candidate_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-source-candidate',
    intl_date_source_candidate_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_style_date_fixture = custom_target(
    'intl-date-style-date-authored-fixture',
    output: 'intl-date-style-date-authored-fixture.c',
    command: [python, files('intl_date_style_date_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_date_available_formats.py',
      'test_intl_date_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_style_date_fixture_objects = static_library(
    ('intl-date-style-date-authored-fixture') + '-objects',
    intl_date_style_date_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_style_date_test = executable(
    'test_intl_date_style_date',
    files(
      'test_intl_date_style_date.c',
    ),
    objects: [
      intl_date_style_date_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-style-date',
    intl_date_style_date_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_style_hour_fixture = custom_target(
    'intl-date-style-hour-authored-fixture',
    output: 'intl-date-style-hour-authored-fixture.c',
    command: [python, files('intl_date_style_hour_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_date_available_formats.py',
      'test_intl_date_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_style_hour_fixture_objects = static_library(
    ('intl-date-style-hour-authored-fixture') + '-objects',
    intl_date_style_hour_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_style_hour_test = executable(
    'test_intl_date_style_hour',
    files(
      'test_intl_date_style_hour.c',
    ),
    objects: [
      intl_date_style_hour_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-style-hour',
    intl_date_style_hour_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_style_program_fixture = custom_target(
    'intl-date-style-program-authored-fixture',
    output: 'intl-date-style-program-authored-fixture.c',
    command: [python, files('intl_date_style_program_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_date_combinations.py',
      'test_intl_date_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_style_program_fixture_objects = static_library(
    ('intl-date-style-program-authored-fixture') + '-objects',
    intl_date_style_program_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_style_program_test = executable(
    'test_intl_date_style_program',
    files(
      'test_intl_date_style_program.c',
    ),
    objects: [
      intl_date_style_program_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-style-program',
    intl_date_style_program_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_ordinary_fixture = custom_target(
    'intl-date-ordinary-authored-fixture',
    output: 'intl-date-ordinary-authored-fixture.c',
    command: [python, files('intl_date_ordinary_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_date_ordinary_fixture_objects = static_library(
    ('intl-date-ordinary-authored-fixture') + '-objects',
    intl_date_ordinary_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_ordinary_sources = files(
    'test_intl_date_ordinary.c',
  )
  intl_date_ordinary_test = executable(
    'test_intl_date_ordinary',
    intl_date_ordinary_sources,
    objects: [
      intl_date_ordinary_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-ordinary',
    intl_date_ordinary_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_single_program_test = executable(
    'test_intl_date_single_program',
    files(
      'test_intl_date_single_program.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-single-program',
    intl_date_single_program_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_format_source_test = executable(
    'test_intl_date_format_source',
    files(
      'test_intl_date_format_source.c',
    ),
    objects: [
      intl_date_source_candidate_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-format-source',
    intl_date_format_source_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_pattern_compose_test = executable(
    'test_intl_date_pattern_compose',
    files(
      'test_intl_date_pattern_compose.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-pattern-compose',
    intl_date_pattern_compose_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_period_spacing_test = executable(
    'test_intl_date_period_spacing',
    files(
      'test_intl_date_period_spacing.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-period-spacing',
    intl_date_period_spacing_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_pattern_info_test = executable(
    'test_intl_date_pattern_info',
    files(
      'test_intl_date_pattern_info.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-pattern-info',
    intl_date_pattern_info_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_interval_program_test = executable(
    'test_intl_date_interval_program',
    files(
      'test_intl_date_interval_program.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-interval-program',
    intl_date_interval_program_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_pattern_test = executable(
    'test_intl_date_pattern',
    files(
      'test_intl_date_pattern.c',
    ),
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-pattern',
    intl_date_pattern_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_calendar_era_sources = files(
    'test_intl_calendar_era_data.c',
  )
  intl_calendar_era_fixture = custom_target(
    'intl-calendar-era-authored-fixture',
    output: 'intl-calendar-era-authored-fixture.c',
    command: [python, files('intl_calendar_era_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files('test_intl_calendar_eras.py'),
  )
  intl_calendar_era_fixture_objects = static_library(
    ('intl-calendar-era-authored-fixture') + '-objects',
    intl_calendar_era_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_calendar_era_authored = executable(
    'test_intl_calendar_era_authored',
    intl_calendar_era_sources,
    objects: [
      intl_calendar_era_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_ERA_AUTHORED=1']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-calendar-era-authored',
    intl_calendar_era_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_dayperiod_fixture = custom_target(
    'intl-dayperiod-authored-fixture',
    output: 'intl-dayperiod-authored-fixture.c',
    command: [python, files('intl_dayperiod_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files('test_intl_dayperiod_rules.py'),
  )
  intl_dayperiod_fixture_objects = static_library(
    ('intl-dayperiod-authored-fixture') + '-objects',
    intl_dayperiod_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_dayperiod_authored = executable(
    'test_intl_dayperiod_authored',
    files(
      'test_intl_dayperiod_data.c',
    ),
    objects: [
      intl_dayperiod_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-dayperiod-authored',
    intl_dayperiod_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_dayperiod_select_test = executable(
    'test_intl_dayperiod_select',
    files(
      'test_intl_dayperiod_select.c',
    ),
    objects: [
      intl_dayperiod_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-dayperiod-select',
    intl_dayperiod_select_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_timezone_presentation_fixture = custom_target(
    'intl-timezone-presentation-authored-fixture',
    output: 'intl-timezone-presentation-authored-fixture.c',
    command: [python, files('intl_timezone_presentation_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_timezone_presentation_fixture_objects = static_library(
    ('intl-timezone-presentation-authored-fixture') + '-objects',
    intl_timezone_presentation_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_timezone_presentation_authored = executable(
    'test_intl_timezone_presentation_authored',
    files(
      'test_intl_timezone_presentation_data.c',
    ),
    objects: [
      intl_timezone_presentation_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-timezone-presentation-authored',
    intl_timezone_presentation_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_timezone_name_sources = files(
    'test_intl_timezone_name_data.c',
  )
  intl_timezone_name_fixture = custom_target(
    'intl-timezone-name-authored-fixture',
    output: 'intl-timezone-name-authored-fixture.c',
    command: [python, files('intl_timezone_name_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_timezone_name_tables.py',
      'test_intl_timezone_names.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_timezone_name_fixture_objects = static_library(
    ('intl-timezone-name-authored-fixture') + '-objects',
    intl_timezone_name_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_timezone_name_authored = executable(
    'test_intl_timezone_name_authored',
    intl_timezone_name_sources,
    objects: [
      intl_timezone_name_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-timezone-name-authored',
    intl_timezone_name_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_interval_sources = files(
    'test_intl_date_interval_data.c',
  )
  intl_date_interval_fixture = custom_target(
    'intl-date-interval-authored-fixture',
    output: 'intl-date-interval-authored-fixture.c',
    command: [python, files('intl_date_interval_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_date_intervals.py',
      'test_intl_date_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_interval_fixture_objects = static_library(
    ('intl-date-interval-authored-fixture') + '-objects',
    intl_date_interval_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_interval_selector_fixture = custom_target(
    'intl-date-interval-selector-fixture',
    output: 'intl-date-interval-selector-fixture.c',
    command: [
      python,
      files('intl_date_interval_fixture.py'),
      '--output', '@OUTPUT@',
      '--prefix', 'qjs_intl_interval_selector_fixture',
    ],
    depend_files: intl_tool_dependencies + files(
      'test_intl_date_intervals.py',
      'test_intl_date_patterns.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_interval_selector_fixture_objects = static_library(
    ('intl-date-interval-selector-fixture') + '-objects',
    intl_date_interval_selector_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_date_interval_authored = executable(
    'test_intl_date_interval_authored',
    intl_date_interval_sources,
    objects: [
      intl_date_interval_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-date-interval-authored',
    intl_date_interval_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_relative_owner_sources = files(
    'test_intl_relative_data.c',
  )
  intl_relative_fixture = custom_target(
    'intl-relative-authored-fixture',
    output: 'intl-relative-authored-fixture.c',
    command: [python, files('intl_relative_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_list_patterns.py',
      'test_intl_relative_patterns.py',
    ),
  )
  intl_relative_fixture_objects = static_library(
    ('intl-relative-authored-fixture') + '-objects',
    intl_relative_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_relative_authored = executable(
    'test_intl_relative_authored',
    intl_relative_owner_sources,
    objects: [
      intl_relative_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-relative-authored',
    intl_relative_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_unit_owner_sources = files(
    'test_intl_unit_data.c',
  )
  intl_unit_grammar_sources = files(
    'test_intl_unit_grammar_data.c',
  )
  intl_unit_grammar_fixture = custom_target(
    'intl-unit-grammar-authored-fixture',
    output: 'intl-unit-grammar-authored-fixture.c',
    command: [python, files('intl_unit_grammar_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_unit_grammar.py',
    ),
  )
  intl_unit_grammar_fixture_objects = static_library(
    ('intl-unit-grammar-authored-fixture') + '-objects',
    intl_unit_grammar_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_unit_grammar_authored = executable(
    'test_intl_unit_grammar_authored',
    intl_unit_grammar_sources,
    objects: [
      intl_unit_grammar_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1', '-DTEST_INTL_PROFILE=0']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-unit-grammar-authored',
    intl_unit_grammar_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )
  intl_unit_fixture = custom_target(
    'intl-unit-authored-fixture',
    output: 'intl-unit-authored-fixture.c',
    command: [python, files('intl_unit_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_list_patterns.py',
      'test_intl_numbering.py',
      'test_intl_number_patterns.py',
      'test_intl_number_layout.py',
      'test_intl_unit_patterns.py',
    ),
  )
  intl_unit_fixture_objects = static_library(
    ('intl-unit-authored-fixture') + '-objects',
    intl_unit_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_unit_authored = executable(
    'test_intl_unit_authored',
    intl_unit_owner_sources,
    objects: [
      intl_unit_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: (host_args + ['-DTEST_INTL_AUTHORED=1', '-DTEST_INTL_PROFILE=0']),
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-unit-authored',
    intl_unit_authored,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_unit_pattern_test = executable(
    'test_intl_unit_pattern',
    files(
      'test_intl_unit_pattern.c',
    ),
    objects: [
      intl_unit_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-unit-pattern',
    intl_unit_pattern_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_number_unit_test = executable(
    'test_intl_number_unit',
    files(
      'test_intl_number_unit.c',
    ),
    objects: [
      intl_unit_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-number-unit',
    intl_number_unit_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_date_zone_fixture = custom_target(
    'intl-date-zone-authored-fixture',
    output: 'intl-date-zone-authored-fixture.c',
    command: [python, files('intl_date_zone_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files('test_intl_timezone_names.py', 'test_intl_list_patterns.py'),
  )
  intl_date_zone_fixture_objects = static_library(
    ('intl-date-zone-authored-fixture') + '-objects',
    intl_date_zone_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )

  intl_date_text_fixture = custom_target(
    'intl-date-text-authored-fixture',
    output: 'intl-date-text-authored-fixture.c',
    command: [python, files('intl_date_text_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_calendar_names.py',
      'test_intl_list_patterns.py',
    ),
  )
  intl_date_text_fixture_objects = static_library(
    ('intl-date-text-authored-fixture') + '-objects',
    intl_date_text_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )

  foreach profile : [get_option('intl')]
    intl_collation_profile_test = executable(
      ('test_intl_collation_' + profile),
      files(
        'test_intl_collation_profiles.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: [intl_test_tools] + component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-collation-data-' + profile),
      intl_collation_profile_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_collation_prepared_test = executable(
      ('test_intl_collation_prepared_' + profile),
      files(
        'test_intl_collation_prepared.c',
      ),
      c_args: (host_args + [
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: [intl_test_tools] + component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-collation-prepared-' + profile),
      intl_collation_prepared_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_collation_service_test = executable(
      ('test_intl_collation_service_' + profile),
      files(
        'test_intl_collation_service.c',
      ),
      c_args: (host_args + [
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: [intl_test_tools] + component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-collation-service-' + profile),
      intl_collation_service_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_calendar_era_test = executable(
      ('test_intl_calendar_era_' + profile),
      intl_calendar_era_sources,
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-calendar-era-' + profile),
      intl_calendar_era_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_data_test = executable(
      ('test_intl_data_' + profile),
      files(
        'test_intl_data.c',
      ),
      c_args: (host_args + [
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    intl_numbering_test = executable(
      ('test_intl_numbering_' + profile),
      intl_numbering_owner_sources,
      c_args: (host_args + ['-DTEST_INTL_AUTHORED=0']),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-numbering-' + profile),
      intl_numbering_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_number_symbol_test = executable(
      ('test_intl_number_symbols_' + profile),
      intl_number_symbol_owner_sources,
      c_args: (host_args + [
        '-DTEST_INTL_AUTHORED=0',
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-number-symbols-' + profile),
      intl_number_symbol_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_number_pattern_test = executable(
      ('test_intl_number_patterns_' + profile),
      intl_number_pattern_owner_sources,
      c_args: (host_args + [
        '-DTEST_INTL_AUTHORED=0',
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-number-patterns-' + profile),
      intl_number_pattern_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_compact_test = executable(
      ('test_intl_compact_' + profile),
      intl_compact_owner_sources,
      c_args: (host_args + [
        '-DTEST_INTL_AUTHORED=0',
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-compact-' + profile),
      intl_compact_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_currency_test = executable(
      ('test_intl_currency_' + profile),
      intl_currency_owner_sources,
      c_args: (host_args + [
        '-DTEST_INTL_AUTHORED=0',
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-currency-' + profile),
      intl_currency_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_number_layout_test = executable(
      ('test_intl_number_layout_' + profile),
      intl_number_layout_owner_sources,
      c_args: (host_args + [
        '-DTEST_INTL_AUTHORED=0',
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-number-layout-' + profile),
      intl_number_layout_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_plural_service_test = executable(
      ('test_intl_plural_service_' + profile),
      files(
        'test_intl_plural_service.c',
      ),
      c_args: (host_args + [
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-plural-service-' + profile),
      intl_plural_service_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_plural_data_test = executable(
      ('test_intl_plural_data_' + profile),
      intl_plural_owner_sources,
      c_args: (host_args + [
        '-DTEST_INTL_AUTHORED=0',
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-plural-data-' + profile),
      intl_plural_data_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    test(
      ('intl-prepared-' + profile),
      intl_data_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_list_data_test = executable(
      ('test_intl_list_data_' + profile),
      files(
        'test_intl_list_data.c',
      ),
      c_args: (host_args + [
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-list-data-' + profile),
      intl_list_data_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_preference_test = executable(
      ('test_intl_preferences_' + profile),
      files(
        'test_intl_preferences.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-preferences-' + profile),
      intl_preference_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_locale_resolve_test = executable(
      ('test_intl_locale_resolve_' + profile),
      files(
        'test_intl_locale_resolve.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-locale-resolve-' + profile),
      intl_locale_resolve_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_locale_test = executable(
      ('test_intl_locale_id_' + profile),
      files(
        'test_intl_native_locale_id.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-locale-id-' + profile),
      intl_locale_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_list_test = executable(
      ('test_intl_list_service_' + profile),
      files(
        'test_intl_list_service.c',
      ),
      c_args: (host_args + [
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-list-service-' + profile),
      intl_list_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_ordinary_profile_test = executable(
      ('test_intl_date_ordinary_' + profile),
      intl_date_ordinary_sources,
      objects: [
        intl_date_ordinary_fixture_objects.extract_all_objects(recursive: false),
      ],
      c_args: (host_args + [
        ('-DTEST_INTL_ORDINARY_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-ordinary-' + profile),
      intl_date_ordinary_profile_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_zone_test = executable(
      ('test_intl_date_zone_' + profile),
      files(
        'test_intl_date_zone.c',
      ) + files(
      ),
      objects: [
        intl_date_zone_fixture_objects.extract_all_objects(recursive: false),
      ],
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-zone-' + profile),
      intl_date_zone_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_offset_test = executable(
      ('test_intl_date_offset_' + profile),
      files(
        'test_intl_date_offset.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-offset-' + profile),
      intl_date_offset_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_text_test = executable(
      ('test_intl_date_text_' + profile),
      files(
        'test_intl_date_text.c',
      ),
      objects: [
        intl_date_text_fixture_objects.extract_all_objects(recursive: false),
      ],
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-text-' + profile),
      intl_date_text_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_format_test = executable(
      ('test_intl_date_format_' + profile),
      files(
        'test_intl_date_format.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-format-' + profile),
      intl_date_format_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_service_test = executable(
      ('test_intl_date_service_' + profile),
      files(
        'test_intl_date_service.c',
      ),
      objects: [
        intl_date_interval_selector_fixture_objects.extract_all_objects(recursive: false),
      ],
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-service-' + profile),
      intl_date_service_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_numeric_test = executable(
      ('test_intl_date_numeric_' + profile),
      files(
        'test_intl_date_numeric.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-numeric-' + profile),
      intl_date_numeric_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_number_service_test = executable(
      ('test_intl_number_service_' + profile),
      files(
        'test_intl_number_service.c',
      ),
      c_args: (host_args + [
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-number-service-' + profile),
      intl_number_service_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_duration_service_test = executable(
      ('test_intl_duration_service_' + profile),
      files(
        'test_intl_duration_service.c',
      ),
      c_args: (host_args + [
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-duration-service-' + profile),
      intl_duration_service_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_display_service_test = executable(
      ('test_intl_display_service_' + profile),
      files(
        'test_intl_display_service.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-display-service-' + profile),
      intl_display_service_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 60,
    )
    intl_relative_service_test = executable(
      ('test_intl_relative_service_' + profile),
      files(
        'test_intl_relative_service.c',
      ),
      c_args: (host_args + [
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-relative-service-' + profile),
      intl_relative_service_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_native_number_test = executable(
      ('test_intl_native_number_' + profile),
      files(
        'test_intl_native_number.c',
      ),
      c_args: (host_args + [
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-native-number-' + profile),
      intl_native_number_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_relative_test = executable(
      ('test_intl_relative_data_' + profile),
      intl_relative_owner_sources,
      c_args: (host_args + ['-DTEST_INTL_AUTHORED=0']),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-relative-data-' + profile),
      intl_relative_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_dayperiod_data_test = executable(
      ('test_intl_dayperiod_data_' + profile),
      files(
        'test_intl_dayperiod_profiles.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-dayperiod-data-' + profile),
      intl_dayperiod_data_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_timezone_presentation_data_test = executable(
      ('test_intl_timezone_presentation_data_' + profile),
      files(
        'test_intl_timezone_presentation_profiles.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-timezone-presentation-data-' + profile),
      intl_timezone_presentation_data_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_timezone_name_data_test = executable(
      ('test_intl_timezone_name_data_' + profile),
      files(
        'test_intl_timezone_name_profiles.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-timezone-name-data-' + profile),
      intl_timezone_name_data_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_interval_data_test = executable(
      ('test_intl_date_interval_data_' + profile),
      files(
        'test_intl_date_interval_profiles.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-interval-data-' + profile),
      intl_date_interval_data_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_available_data_test = executable(
      ('test_intl_date_available_data_' + profile),
      files(
        'test_intl_date_available_profiles.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-available-data-' + profile),
      intl_date_available_data_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_calendar_name_data_test = executable(
      ('test_intl_calendar_name_data_' + profile),
      files(
        'test_intl_calendar_name_profiles.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-calendar-name-data-' + profile),
      intl_calendar_name_data_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_combination_test = executable(
      ('test_intl_date_combination_data_' + profile),
      files(
        'test_intl_date_combination_data.c',
      ),
      c_args: (host_args + ['-DTEST_INTL_AUTHORED=0']),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-combination-data-' + profile),
      intl_date_combination_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_style_test = executable(
      ('test_intl_date_style_data_' + profile),
      files(
        'test_intl_date_style_data.c',
      ),
      c_args: (host_args + ['-DTEST_INTL_AUTHORED=0']),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-style-data-' + profile),
      intl_date_style_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_date_field_test = executable(
      ('test_intl_date_field_data_' + profile),
      intl_date_field_owner_sources,
      c_args: (host_args + ['-DTEST_INTL_AUTHORED=0']),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-date-field-data-' + profile),
      intl_date_field_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_display_test = executable(
      ('test_intl_display_data_' + profile),
      intl_display_owner_sources,
      c_args: (host_args + ['-DTEST_INTL_PREPARED=1', '-DTEST_INTL_EMPTY=0']),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-display-data-' + profile),
      intl_display_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_calendar_name_test = executable(
      ('test_intl_calendar_names_' + profile),
      intl_calendar_name_sources,
      c_args: (host_args + ['-DTEST_INTL_CALENDAR_NAME_MODE=4']),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-calendar-names-' + profile),
      intl_calendar_name_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_language_match_test = executable(
      ('test_intl_language_match_' + profile),
      intl_language_match_sources,
      c_args: (host_args + ['-DTEST_INTL_LANGUAGE_MATCH_PREPARED=1']),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-language-match-' + profile),
      intl_language_match_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_language_format_test = executable(
      ('test_intl_language_format_' + profile),
      intl_language_format_sources,
      c_args: (host_args + ['-DTEST_INTL_LANGUAGE_FORMAT_PREPARED=1']),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-language-format-' + profile),
      intl_language_format_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_duration_test = executable(
      ('test_intl_duration_data_' + profile),
      intl_duration_owner_sources,
      c_args: (host_args + ['-DTEST_INTL_AUTHORED=0']),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-duration-data-' + profile),
      intl_duration_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_unit_test = executable(
      ('test_intl_unit_data_' + profile),
      intl_unit_owner_sources,
      c_args: (host_args + [
        '-DTEST_INTL_AUTHORED=0',
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-unit-data-' + profile),
      intl_unit_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_unit_grammar_test = executable(
      ('test_intl_unit_grammar_data_' + profile),
      intl_unit_grammar_sources,
      c_args: (host_args + [
        '-DTEST_INTL_AUTHORED=0',
        ('-DTEST_INTL_PROFILE=' + ((profile == 'native') ? '1' : '0'))
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-unit-grammar-data-' + profile),
      intl_unit_grammar_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
    intl_inventory_test = executable(
      ('test_intl_locale_inventory_' + profile),
      files(
        'test_intl_locale_inventory.c',
      ),
      c_args: host_args,
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    intl_inventory_env = environment()
    if host_machine.cpu_family() == 'aarch64' and cc.get_id() == 'gcc' and get_option('b_sanitize').contains('address')
      # GCC ARM64 fake-stack scanning dominates this exhaustive data-only pass.
      # Functional and allocation-failure tests retain use-after-return checks,
      # as do Clang and the other platforms' full inventory checks.
      intl_inventory_env.append('ASAN_OPTIONS', 'detect_stack_use_after_return=0', separator: ':')
    endif
    test(
      ('intl-locale-inventory-' + profile),
      intl_inventory_test,
      suite: ['repo', 'native'] + ['unit'],
      env: intl_inventory_env,
      timeout: 30,
    )
  endforeach

  intl_locale_info_fixture = custom_target(
    'intl-locale-info-authored-fixture',
    output: 'intl-locale-info-authored-fixture.c',
    command: [
      python,
      files('intl_locale_info_fixture.py'),
      '--output', '@OUTPUT@',
    ],
    depend_files: intl_tool_dependencies,
  )
  intl_locale_info_fixture_objects = static_library(
    ('intl-locale-info-authored-fixture') + '-objects',
    intl_locale_info_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_locale_info_test = executable(
    'test_intl_locale_info',
    files(
      'test_intl_locale_info.c',
    ),
    objects: [
      intl_locale_info_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-locale-info-sources',
    intl_locale_info_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )
endif

# Preserve authored synthetic providers; ordinary tests use the selected sources.
standalone = {
}
if intl_enabled
  standalone += {
    'test_intl_date_local': files(
      'test_intl_date_local.c',
    ),
    'test_intl_date_calendar': files(
      'test_intl_date_calendar.c',
    ),
    'test_intl_currency_boundary': files(
      'test_intl_currency_boundary.c',
    ) + files(
    ),
    'test_intl_number_affix': files(
      'test_intl_number_affix.c',
    ),
    'test_intl_number_round': files(
      'test_intl_number_round.c',
    ),
    'test_intl_number_body': files(
      'test_intl_number_body.c',
    ),
    'test_intl_number_range': files(
      'test_intl_number_range.c',
    ),
    'test_intl_date_range_layout': files(
      'test_intl_date_range_layout.c',
    ),
    'test_intl_range_compose': files(
      'test_intl_range_compose.c',
    ),
    'test_intl_unit_identifier': files(
      'test_intl_unit_identifier.c',
    ),
    'test_intl_unit_format': files(
      'test_intl_unit_format.c',
    ),
    'test_intl_locale_system': files(
      'test_intl_locale_system.c',
    ),
    'test_intl_list_engine': files(
      'test_intl_list_engine.c',
    ),
    'test_intl_locale_matching': files(
      'test_intl_locale_matching.c',
    ),
    'test_intl_locale_identifier': files(
      'test_intl_locale_identifier.c',
    ),
    'test_intl_pack': files(
      'test_intl_pack.c',
    ),
    'test_intl_segment_routing': files(
      'test_intl_segment_routing.c',
    ),
    'test_intl_collation_weights': files(
      'test_intl_collation_weights.c',
    ),
    'test_intl_collation_weights_graph': files(
      'test_intl_collation_weights_graph.c',
    ),
    'test_intl_collation_relations': files(
      'test_intl_collation_relations.c',
    ),
    'test_intl_collation_settings': files(
      'test_intl_collation_settings.c',
    ),
    'test_intl_collation_logical': files(
      'test_intl_collation_logical.c',
    ),
    'test_intl_locale_grammar': files(
      'test_intl_locale_grammar.c',
    ),
    'test_intl_number_decimal': files(
      'test_intl_number_decimal.c',
    ),
    'test_intl_decimal': files(
      'test_intl_decimal.c',
    ),
  }
  test_intl_locale_data = executable(
    'test_intl_locale_data',
    files(
      'test_intl_locale_data.c',
    ),
    objects: [
      intl_locale_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_locale_data',
    test_intl_locale_data,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_compare = executable(
    'test_intl_collation_compare',
    files(
      'test_intl_collation_compare.c',
    ),
    objects: [
      intl_collation_elements_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_compare',
    test_intl_collation_compare,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_elements = executable(
    'test_intl_collation_elements',
    files(
      'test_intl_collation_elements.c',
    ),
    objects: [
      intl_collation_elements_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_elements',
    test_intl_collation_elements,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_overlay = executable(
    'test_intl_collation_overlay',
    files(
      'test_intl_collation_overlay.c',
    ),
    objects: [
      intl_collation_elements_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_overlay',
    test_intl_collation_overlay,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_contexts = executable(
    'test_intl_collation_contexts',
    files(
      'test_intl_collation_contexts.c',
    ),
    objects: [
      intl_collation_context_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_contexts',
    test_intl_collation_contexts,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_map = executable(
    'test_intl_collation_map',
    files(
      'test_intl_collation_map.c',
    ),
    objects: [
      intl_collation_elements_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_map',
    test_intl_collation_map,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_base = executable(
    'test_intl_collation_base',
    files(
      'test_intl_collation_base.c',
    ),
    objects: [
      intl_collation_root_input_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_base',
    test_intl_collation_base,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_root_input = executable(
    'test_intl_collation_root_input',
    files(
      'test_intl_collation_root_input.c',
    ),
    objects: [
      intl_collation_root_input_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_root_input',
    test_intl_collation_root_input,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_reorder = executable(
    'test_intl_collation_reorder',
    files(
      'test_intl_collation_reorder.c',
    ),
    objects: [
      intl_collation_root_input_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_reorder',
    test_intl_collation_reorder,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_ir = executable(
    'test_intl_collation_ir',
    files(
      'test_intl_collation_ir.c',
    ),
    objects: [
      intl_collation_ir_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_ir',
    test_intl_collation_ir,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_compiled = executable(
    'test_intl_collation_compiled',
    files(
      'test_intl_collation_compiled.c',
    ),
    objects: [
      intl_collation_root_input_fixture_objects.extract_all_objects(recursive: false),
      intl_collation_program_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_compiled',
    test_intl_collation_compiled,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_archive = executable(
    'test_intl_collation_archive',
    files(
      'test_intl_collation_archive.c',
    ),
    objects: [
      intl_collation_root_input_fixture_objects.extract_all_objects(recursive: false),
      intl_collation_program_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_archive',
    test_intl_collation_archive,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_trie = executable(
    'test_intl_collation_trie',
    files(
      'test_intl_collation_trie.c',
    ),
    objects: [
      intl_collation_root_input_fixture_objects.extract_all_objects(recursive: false),
      intl_collation_program_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_trie',
    test_intl_collation_trie,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_snapshot = executable(
    'test_intl_collation_snapshot',
    files(
      'test_intl_collation_snapshot.c',
    ),
    objects: [
      intl_collation_root_input_fixture_objects.extract_all_objects(recursive: false),
      intl_collation_program_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_snapshot',
    test_intl_collation_snapshot,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_program = executable(
    'test_intl_collation_program',
    files(
      'test_intl_collation_program.c',
    ),
    objects: [
      intl_collation_root_input_fixture_objects.extract_all_objects(recursive: false),
      intl_collation_program_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_program',
    test_intl_collation_program,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_builder = executable(
    'test_intl_collation_builder',
    files(
      'test_intl_collation_builder.c',
    ),
    objects: [
      intl_collation_root_input_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_builder',
    test_intl_collation_builder,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )
  test_intl_collation_data = executable(
    'test_intl_collation_data',
    files(
      'test_intl_collation_data.c',
    ),
    objects: [
      intl_collation_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: [intl_test_tools] + component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'test_intl_collation_data',
    test_intl_collation_data,
    suite: ['repo', 'native'] + ['unit'],
    workdir: test_workdir,
    timeout: 120,
  )

endif
if get_option('intl') == 'minimal'
  standalone += {
    'test_intl_segment_minimal': files(
      'test_intl_segment_minimal.c',
    ),
  }
endif
if get_option('intl') == 'native'
  standalone += {
    'test_intl_segment_property': files(
      'test_intl_segment_property.c',
    ),
  }
endif
standalone += {
  'test_unicode_sentence': files(
    'test_unicode_sentence.c',
  ),
  'test_unicode_word': files(
    'test_unicode_word.c',
  ),
  'test_unicode_grapheme': files(
    'test_unicode_grapheme.c',
  ),
  'test_unicode_break_properties': files(
    'test_unicode_break_properties.c',
  ),
  'test_unicode_normalization_boundary': files(
    'test_unicode_normalization_boundary.c',
  ),
}

foreach name, sources : standalone
  libraries = component_libraries
  if name.startswith('test_intl_collation_')
    libraries = [intl_test_tools] + libraries
  elif name == 'test_intl_segment_property'
    libraries = [segment_property_test_library] + libraries
  elif name in ['test_unicode_sentence', 'test_unicode_word', 'test_unicode_grapheme']
    libraries = [component_helpers] + libraries
  endif
  app = executable(
    name,
    sources,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    name,
    app,
    suite: ['repo', 'native', 'unit'],
    workdir: test_workdir,
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
    'std-large-file': ['tests/test_std_large_file.js'],
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
if intl_enabled
  js_tests += {
    'intl-locale': ['-m', 'tests/test_intl_locale.js'],
    'intl-locale-object': ['-m', 'tests/test_intl_locale_object.js'],
    'intl-locale-info': ['-m', 'tests/test_intl_locale_preferences.js'],
    'intl-supported-values': ['-m', 'tests/test_intl_supported_values.js', get_option('calendars'), get_option('timezone_backend')],
    'intl-string-case': ['-m', 'tests/test_intl_string_case.js'],
    'intl-to-locale-string': ['-m', 'tests/test_intl_to_locale_string.js'],
    'intl-list-format': ['-m', 'tests/test_intl_list_format.js'],
    'intl-plural-rules': ['-m', 'tests/test_intl_plural_rules.js'],
    'intl-number-format': ['-m', 'tests/test_intl_number_format.js'],
    'intl-collator': ['-m', 'tests/test_intl_collator.js'],
    'intl-date-time-format': ['-m', 'tests/test_intl_date_time_format.js'],
    'intl-date-time-range': ['-m', 'tests/test_intl_date_time_range.js', get_option('calendars'), get_option('timezone_backend')],
    'intl-relative-time-format': ['-m', 'tests/test_intl_relative_time_format.js'],
    'intl-duration-format': ['-m', 'tests/test_intl_duration_format.js'],
    'intl-display-names-api': ['-m', 'tests/test_intl_display_names.js'],
  }
endif
if intl_enabled
  js_tests += {
    'intl-segmenter': ['-m', 'tests/test_intl_segmenter.js'],
    'intl-segmenter-profile': ['-m', 'tests/test_intl_segmenter_profile.js', get_option('intl')],
  }
endif
if intl_enabled
  test(
    'intl-locale-numbering-fallback',
    qjs,
    args: ['-m', 'tests/test_intl_locale_preferences.js'],
    env: ['LC_ALL=bn_BD.UTF-8'],
    suite: ['repo', 'javascript', 'integration'],
    workdir: test_workdir,
    depends: fixtures,
    timeout: 60,
  )
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
test(
  'test262-calendar-selection',
  python,
  args: files('test_temporal_test262_selection.py'),
  suite: ['repo', 'tools', 'unit'],
  timeout: 120,
)

if native_calendars
  test(
    'calendar-boundary-export',
    python,
    args: [files('test_calendar_boundary.py'), calendar_boundary],
    depends: calendar_boundary,
    suite: ['repo', 'native', 'unit'],
    timeout: 60,
  )
endif

if native_calendars
  test(
    'calendar-generator',
    python,
    args: files('test_calendar_generator.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )
endif

if intl_enabled
  test(
    'intl-number-symbols',
    python,
    args: files('test_intl_number_symbols.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-currencies',
    python,
    args: files('test_intl_currencies.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-currency-tables',
    python,
    args: files('test_intl_currency_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-number-layout',
    python,
    args: files('test_intl_number_layout.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-number-layout-tables',
    python,
    args: files('test_intl_number_layout_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-sparse-maps',
    python,
    args: files('test_intl_sparse_maps.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-unit-grammar',
    python,
    args: files('test_intl_unit_grammar.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-unit-grammar-tables',
    python,
    args: files('test_intl_unit_grammar_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-unit-patterns',
    python,
    args: files('test_intl_unit_patterns.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-duration-tables',
    python,
    args: files('test_intl_duration_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-date-field-tables',
    python,
    args: files('test_intl_date_field_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-calendar-eras',
    python,
    args: files('test_intl_calendar_eras.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-dayperiod-rules',
    python,
    args: files('test_intl_dayperiod_rules.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )
endif

if get_option('intl') == 'native'
  test(
    'intl-segment-sources',
    python,
    args: files('test_intl_segment_sources.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-inheritance',
    python,
    args: files('test_intl_segment_inheritance.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )
endif

if intl_enabled
  test(
    'intl-scalar-trie',
    python,
    args: files('test_intl_scalar_trie.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )
endif

if get_option('intl') == 'native'
  test(
    'intl-segment-suppressions',
    python,
    args: files('test_intl_segment_suppressions.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-suppression-tables',
    python,
    args: files('test_intl_segment_suppression_tables.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-catalog',
    python,
    args: files('test_intl_segment_catalog.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-bundle',
    python,
    args: files('test_intl_segment_bundle.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-expression',
    python,
    args: files('test_intl_segment_expression.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-rules',
    python,
    args: files('test_intl_segment_rules.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-compat',
    python,
    args: files('test_intl_segment_compat.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-lower',
    python,
    args: files('test_intl_segment_lower.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-nfa',
    python,
    args: files('test_intl_segment_nfa.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-alphabet',
    python,
    args: files('test_intl_segment_alphabet.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-dfa',
    python,
    args: files('test_intl_segment_dfa.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-program',
    python,
    args: files('test_intl_segment_program.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-program-tables',
    python,
    args: files('test_intl_segment_program_tables.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  intl_segment_program_fixture = custom_target(
    'intl-segment-program-authored-fixture',
    output: 'intl-segment-program-authored-fixture.c',
    command: [python, files('intl_segment_program_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'test_intl_segment_program_tables.py',
      'intl_segment_language_fixture.py',
    ),
  )
  intl_segment_program_fixture_objects = static_library(
    ('intl-segment-program-authored-fixture') + '-objects',
    intl_segment_program_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_segment_program_test = executable(
    'test_intl_segment_program',
    files(
      'test_intl_segment_program.c',
    ),
    objects: [
      intl_segment_program_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-segment-program-reader',
    intl_segment_program_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_segment_scan_test = executable(
    'test_intl_segment_scan',
    files(
      'test_intl_segment_scan.c',
    ),
    objects: [
      intl_segment_program_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-segment-scan',
    intl_segment_scan_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  test(
    'intl-segment-profile',
    python,
    args: files('test_intl_segment_profile.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-sets',
    python,
    args: files('test_intl_segment_sets.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-dictionary-sources',
    python,
    args: files('test_intl_segment_dictionary_sources.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-dictionary-parser',
    python,
    args: files('test_intl_segment_dictionary.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-dictionary-trie',
    python,
    args: files('test_intl_segment_dictionary_trie.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-dictionary-tables',
    python,
    args: files('test_intl_segment_dictionary_tables.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  intl_segment_dictionary_fixture = custom_target(
    'intl-segment-dictionary-authored-fixture',
    output: 'intl-segment-dictionary-authored-fixture.c',
    command: [python, files('intl_segment_dictionary_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_segment_dictionary_fixture_objects = static_library(
    ('intl-segment-dictionary-authored-fixture') + '-objects',
    intl_segment_dictionary_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_segment_dictionary_test = executable(
    'test_intl_segment_dictionary',
    files(
      'test_intl_segment_dictionary.c',
    ),
    objects: [
      intl_segment_dictionary_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-segment-dictionary',
    intl_segment_dictionary_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_segment_suppression_fixture = custom_target(
    'intl-segment-suppression-authored-fixture',
    output: 'intl-segment-suppression-authored-fixture.c',
    command: [python, files('intl_segment_suppression_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files('test_intl_segment_suppression_tables.py'),
  )
  intl_segment_suppression_fixture_objects = static_library(
    ('intl-segment-suppression-authored-fixture') + '-objects',
    intl_segment_suppression_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_segment_suppression_test = executable(
    'test_intl_segment_suppression',
    files(
      'test_intl_segment_suppression.c',
    ),
    objects: [
      intl_segment_suppression_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-segment-suppression-reader',
    intl_segment_suppression_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_segment_suppression_runtime_fixture = custom_target(
    'intl-segment-suppression-runtime-fixture',
    output: 'intl-segment-suppression-runtime-fixture.c',
    command: [python, files('intl_segment_suppression_runtime_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files('intl_segment_language_fixture.py'),
  )
  intl_segment_suppression_runtime_fixture_objects = static_library(
    ('intl-segment-suppression-runtime-fixture') + '-objects',
    intl_segment_suppression_runtime_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_segment_suppression_runtime_test = executable(
    'test_intl_segment_suppression_runtime',
    files(
      'test_intl_segment_suppression_runtime.c',
    ),
    objects: [
      intl_segment_suppression_runtime_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-segment-suppression-runtime',
    intl_segment_suppression_runtime_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  intl_segment_dictionary_refine_fixture = custom_target(
    'intl-segment-dictionary-refine-fixture',
    output: 'intl-segment-dictionary-refine-fixture.c',
    command: [python, files('intl_segment_dictionary_refine_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'intl_segment_language_fixture.py',
      'test_intl_segment_program_tables.py',
    ),
  )
  intl_segment_dictionary_refine_fixture_objects = static_library(
    ('intl-segment-dictionary-refine-fixture') + '-objects',
    intl_segment_dictionary_refine_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_segment_native_input_test = executable(
    'test_intl_segment_native_input',
    files(
      'test_intl_segment_native_input.c',
    ),
    objects: [
      intl_segment_dictionary_refine_fixture_objects.extract_all_objects(recursive: false),
      intl_segment_suppression_runtime_fixture_objects.extract_all_objects(recursive: false),
      intl_segment_program_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-segment-native-input',
    intl_segment_native_input_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )
endif

# Compile the provider once; small independent packs exercise admission without
# a test-only mutable global data source or production dictionary OOM sweeps.
if get_option('intl') == 'native'
  intl_segment_native_fixture = custom_target(
    'intl-segment-native-fixture',
    output: 'intl-segment-native-fixture.c',
    command: [python, files('intl_segment_native_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies + files(
      'intl_segment_language_fixture.py',
      'test_intl_segment_program_tables.py',
    ),
  )

  foreach label, case : {
    'complete': 0,
    'fingerprint': 1,
    'program-coverage': 2,
    'dictionaries': 3,
    'root': 4,
    'profile': 5,
    'list-coverage': 6,
  }
    native_segment_fixture_objects = static_library(
      'intl-segment-case-data-' + label,
      intl_segment_native_fixture,
      c_args: host_args + ['-DQJS_INTL_TEST_SEGMENT_CASE=' + case.to_string()],
      include_directories: inc,
      dependencies: host_deps,
      override_options: ['b_lto=false'],
      install: false,
    )
    native_segment_test = executable(
      ('test_intl_segment_native_' + label),
      files(
        'test_intl_segment_native.c',
      ),
      objects: [
        native_segment_fixture_objects.extract_all_objects(recursive: false),
      ],
      c_args: (host_args + [
        ('-DQJS_INTL_TEST_SEGMENT_CASE=' + case.to_string())
      ]),
      include_directories: inc,
      dependencies: host_deps,
      link_with: component_libraries,
      override_options: ['b_ndebug=false'],
    )
    test(
      ('intl-segment-native-' + label),
      native_segment_test,
      suite: ['repo', 'native'] + ['unit'],
      timeout: 30,
    )
  endforeach

  intl_segment_dictionary_refine_test = executable(
    'test_intl_segment_dictionary_refine',
    files(
      'test_intl_segment_dictionary_refine.c',
    ),
    objects: [
      intl_segment_dictionary_refine_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-segment-dictionary-refine',
    intl_segment_dictionary_refine_test,
    suite: ['repo', 'native'] + ['unit'],
    timeout: 30,
  )

  test(
    'intl-segment-property-tool',
    python,
    args: [files('test_intl_segment_property_tool.py'), '--tool', segment_property],
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-segment-property-resolver',
    python,
    args: [
      files('test_intl_segment_property_resolver.py'),
      '--tool', segment_property,
      '--workdir', meson.current_build_dir(),
    ],
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )
endif

if intl_enabled
  test(
    'intl-dayperiod-tables',
    python,
    args: files('test_intl_dayperiod_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-date-style-tables',
    python,
    args: files('test_intl_date_style_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-date-combination-tables',
    python,
    args: files('test_intl_date_combination_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-date-available-tables',
    python,
    args: files('test_intl_date_available_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-calendar-name-tables',
    python,
    args: files('test_intl_calendar_name_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-calendar-names',
    python,
    args: files('test_intl_calendar_names.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-date-interval-tables',
    python,
    args: files('test_intl_date_interval_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-timezone-name-tables',
    python,
    args: files('test_intl_timezone_name_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-timezone-presentation-tables',
    python,
    args: files('test_intl_timezone_presentation_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-timezone-presentation',
    python,
    args: files('test_intl_timezone_presentation.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-timezone-names',
    python,
    args: files('test_intl_timezone_names.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-zone-pattern-syntax',
    python,
    args: files('test_intl_zone_pattern_syntax.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-date-intervals',
    python,
    args: files('test_intl_date_intervals.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-date-available-formats',
    python,
    args: files('test_intl_date_available_formats.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-date-combinations',
    python,
    args: files('test_intl_date_combinations.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-date-patterns',
    python,
    args: files('test_intl_date_patterns.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-date-pattern-syntax',
    python,
    args: files('test_intl_date_pattern_syntax.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-date-field-names',
    python,
    args: files('test_intl_date_field_names.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-display-tables',
    python,
    args: files('test_intl_display_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-display-names',
    python,
    args: files('test_intl_display_names.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-duration-patterns',
    python,
    args: files('test_intl_duration_patterns.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-relative-patterns',
    python,
    args: files('test_intl_relative_patterns.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-relative-tables',
    python,
    args: files('test_intl_relative_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-unit-tables',
    python,
    args: files('test_intl_unit_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-number-symbol-tables',
    python,
    args: files('test_intl_number_symbol_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-compact-tables',
    python,
    args: files('test_intl_compact_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-compact-pattern-syntax',
    python,
    args: files('test_intl_compact_pattern_syntax.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-compact-patterns',
    python,
    args: files('test_intl_compact_patterns.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-number-pattern-tables',
    python,
    args: files('test_intl_number_pattern_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-number-pattern-syntax',
    python,
    args: files('test_intl_number_pattern_syntax.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-number-patterns',
    python,
    args: files('test_intl_number_patterns.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-compiler-input',
    python,
    args: [files('test_intl_collation_ir.py'), '--compiler', collation_compile],
    depends: collation_compile,
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-profile-preparation',
    python,
    args: [files('test_intl_collation_prepare.py'), '--compiler', collation_compile],
    depends: collation_compile,
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-bundle',
    python,
    args: [files('test_intl_collation_bundle.py'), '--compiler', collation_compile],
    depends: collation_compile,
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-archive',
    python,
    args: [files('test_intl_collation_archive.py'), '--compiler', collation_compile],
    depends: collation_compile,
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-groups',
    python,
    args: files('test_intl_collation_groups.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-fractional-source',
    python,
    args: files('test_intl_collation_fractional.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-root-ordinals',
    python,
    args: files('test_intl_collation_ordinals.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-root-cli',
    python,
    args: [files('test_intl_collation_root_reader.py'), '--compiler', collation_compile],
    depends: collation_compile,
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-root-input',
    python,
    args: files('test_intl_collation_root_input.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-bindings',
    python,
    args: files('test_intl_collation_bindings.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-programs',
    python,
    args: files('test_intl_collation_programs.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-rules',
    python,
    args: files('test_intl_collation_rules.py'),
    suite: ['repo', 'generator', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-tables',
    python,
    args: files('test_intl_collation_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-collation-contexts',
    python,
    args: files('test_intl_collation_contexts.py'),
    suite: ['repo', 'generator', 'unit'],
  )
  test(
    'intl-collation-root',
    python,
    args: files('test_intl_collation_root.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-plural-sources',
    python,
    args: files('test_intl_plural_rules.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-plural-tables',
    python,
    args: files('test_intl_plural_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-numbering-tables',
    python,
    args: files('test_intl_numbering_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-numbering-metadata',
    python,
    args: files('test_intl_numbering.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-locale-preferences',
    python,
    args: files('test_intl_preferences.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-locale-metadata',
    python,
    args: files('test_intl_locale_metadata.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-typed-tables',
    python,
    args: files('test_intl_tables.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-available-locales',
    python,
    args: files('test_intl_availability.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-binary-envelope',
    python,
    args: files('test_intl_bundle.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-list-patterns',
    python,
    args: files('test_intl_list_patterns.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-record-resolution',
    python,
    args: files('test_intl_resolution.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-locale-inheritance',
    python,
    args: files('test_intl_parents.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-xml-schema',
    python,
    args: files('test_intl_xml.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-source-acquisition',
    python,
    args: files('test_intl_sources.py'),
    suite: ['repo', 'native', 'unit'],
    timeout: 30,
  )

  test(
    'intl-git-source',
    python,
    args: files('test_intl_git_source.py'),
    suite: ['repo', 'generator', 'unit'],
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
      intl_enabled ? 'y' : 'n',
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

# Exercise the actual C compiler's initialization of opaque prepared bytes.
if intl_enabled
  intl_embedding_fixture = custom_target(
    'intl-embedding-fixture',
    output: 'intl-embedding-fixture.c',
    command: [python, files('intl_embed_fixture.py'), '--output', '@OUTPUT@'],
    depend_files: intl_tool_dependencies,
  )
  intl_embedding_fixture_objects = static_library(
    ('intl-embedding-fixture') + '-objects',
    intl_embedding_fixture,
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    override_options: ['b_lto=false'],
    install: false,
    build_by_default: false,
  )
  intl_embedding_test = executable(
    'test_intl_embedding',
    files(
      'test_intl_embedding.c',
    ),
    objects: [
      intl_embedding_fixture_objects.extract_all_objects(recursive: false),
    ],
    c_args: host_args,
    include_directories: inc,
    dependencies: host_deps,
    link_with: component_libraries,
    override_options: ['b_ndebug=false'],
  )
  test(
    'intl-embedding',
    intl_embedding_test,
    suite: ['repo', 'generator'] + ['unit'],
    timeout: 30,
  )
endif

# Check that separate CI phases partition the selected repository suite.
test(
  'build-configuration',
  python,
  args: [files('test_build_configuration.py'), meson.project_source_root(), meson.project_build_root()],
  suite: ['repo', 'unit', 'build'],
  timeout: 30,
)
