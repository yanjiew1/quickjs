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

# Keep runtime objects before compiler objects in non-LTO links.
engine_sources = files(
  'quickjs/allocator.c',
  'quickjs/atom.c',
  'quickjs/bigint.c',
  'quickjs/builtins/array-buffer.c',
  'quickjs/builtins/array-from-async.c',
  'quickjs/builtins/array.c',
  'quickjs/builtins/async-from-sync-iterator.c',
  'quickjs/builtins/async-resource-management.c',
  'quickjs/builtins/async.c',
  'quickjs/builtins/atomics.c',
  'quickjs/builtins/bigint.c',
  'quickjs/builtins/boolean.c',
  'quickjs/builtins/collections.c',
  'quickjs/builtins/data-view.c',
  'quickjs/builtins/date.c',
  'quickjs/builtins/error.c',
  'quickjs/builtins/finalization-registry.c',
  'quickjs/builtins/function.c',
  'quickjs/builtins/global.c',
  'quickjs/builtins/intrinsics.c',
  'quickjs/builtins/iterator.c',
  'quickjs/builtins/json-stringify.c',
  'quickjs/builtins/json.c',
  'quickjs/builtins/math.c',
  'quickjs/builtins/number.c',
  'quickjs/builtins/object.c',
  'quickjs/builtins/promise.c',
  'quickjs/builtins/proxy.c',
  'quickjs/builtins/reflect.c',
  'quickjs/builtins/regexp.c',
  'quickjs/builtins/resource-management.c',
  'quickjs/builtins/string.c',
  'quickjs/builtins/symbol.c',
  'quickjs/builtins/typed-array.c',
  'quickjs/builtins/uint8array-encoding.c',
  'quickjs/builtins/weakref.c',
  'quickjs/bytecode-format.c',
  'quickjs/c-function.c',
  'quickjs/class.c',
  'quickjs/error-support.c',
  'quickjs/function-list.c',
  'quickjs/function.c',
  'quickjs/gc.c',
  'quickjs/generator.c',
  'quickjs/global-environment.c',
  'quickjs/disposal.c',
  'quickjs/iterator-protocol.c',
  'quickjs/memory-usage.c',
  'quickjs/module-evaluation.c',
  'quickjs/module.c',
  'quickjs/native-jobs.c',
  'quickjs/number.c',
  'quickjs/object.c',
  'quickjs/parse-state.c',
  'quickjs/runtime.c',
  'quickjs/serialization/reader.c',
  'quickjs/serialization/writer.c',
  'quickjs/string-buffer.c',
  'quickjs/string.c',
  'quickjs/value/compare.c',
  'quickjs/value/conversion.c',
  'quickjs/value/print.c',
)
vm_sources = files('quickjs/vm.c')
compiler_sources = files(
  'quickjs/compiler/backend.c',
  'quickjs/compiler/bytecode-dump.c',
  'quickjs/compiler/eval.c',
  'quickjs/compiler/lexer.c',
  'quickjs/compiler/parser.c',
  'quickjs/compiler/stack-analysis.c',
)
support_library_sources = files(
  'libwait/wait-queue.c',
  'dtoa/dtoa.c',
  'regexp/compile.c',
  'regexp/exec.c',
  'unicode/libunicode.c',
  'cutils/cutils.c',
)
host_service_sources = files(
  'quickjs-libc/event-loop.c',
  'quickjs-libc/environment.c',
  'quickjs-libc/host.c',
  'quickjs-libc/module-loader.c',
  'quickjs-libc/os.c',
  'quickjs-libc/std.c',
  'quickjs-libc/worker.c',
)

support_sources = support_library_sources + host_service_sources

# Immutable generated tables have one object owner outside LTO.
data_sources = []
data_sources += unicode_tables
windows_timezone_sources = []
windows_timezone_data_sources = []

# The coordinator also provides disabled-build public API stubs.
engine_sources += files(
  'quickjs/builtins/temporal/common.c',
  'quickjs/default-time-zone.c',
)
if get_option('temporal')
  engine_sources += files('quickjs/builtins/duration-value.c')
  engine_sources += files(
    'quickjs/builtins/temporal/calendar-fields.c',
    'quickjs/builtins/temporal/duration.c',
    'quickjs/builtins/temporal/instant.c',
    'quickjs/builtins/temporal/now.c',
    'quickjs/builtins/temporal/options.c',
    'quickjs/builtins/temporal/plain-time.c',
    'quickjs/builtins/temporal/plain.c',
    'quickjs/builtins/temporal/zoned-arithmetic.c',
    'quickjs/builtins/temporal/zoned-date-time.c',
  )
  support_sources += files(
    'calendar/calendar.c',
    'calendar/iso.c',
    'temporal/calendar.c',
    'temporal/civil.c',
    'temporal/clock.c',
    'temporal/duration.c',
    'temporal/epoch.c',
    'temporal/format.c',
    'temporal/iso.c',
    'temporal/noniso-arithmetic.c',
    'temporal/noniso-calendar.c',
    'temporal/options.c',
    'temporal/parse.c',
    'temporal/relative.c',
    'temporal/time.c',
    'temporal/time-zone.c',
    'timezone/identifier.c',
    'timezone/system-zone.c',
    'timezone/timezone.c',
  )
  if get_option('calendars') == 'native'
    support_sources += files(
      'calendar/astronomy.c',
      'calendar/calendar-native.c',
      'calendar/data.c',
      'calendar/eras.c',
      'calendar/hebrew.c',
      'calendar/islamic.c',
      'calendar/lunisolar.c',
      'calendar/native-months.c',
      'calendar/persian.c',
      'calendar/solar.c',
    )
    data_sources += calendar_data
  else
    support_sources += files('calendar/calendar-minimal.c')
  endif
  if get_option('timezone_backend') == 'native'
    support_sources += files(
      'timezone/timezone-native.c',
      'timezone/tzif.c',
    )
    data_sources += timezone_data
    windows_timezone_sources = files('timezone/windows-zone.c')
    windows_timezone_data_sources += windows_timezone_data
  else
    support_sources += files('timezone/timezone-minimal.c')
  endif
endif
