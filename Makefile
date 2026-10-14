#
# QuickJS Javascript Engine
#
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

ifeq ($(shell uname -s),Darwin)
CONFIG_DARWIN=y
endif
ifeq ($(shell uname -s),FreeBSD)
CONFIG_FREEBSD=y
endif
# Windows cross compilation from Linux
# May need to have libwinpthread*.dll alongside the executable
# (On Ubuntu/Debian may be installed with mingw-w64-x86-64-dev
# to /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll)
#CONFIG_WIN32=y
# use link time optimization (smaller and faster executables but slower build)
#CONFIG_LTO=y
# consider warnings as errors (for development)
#CONFIG_WERROR=y
# force 32 bit build on x86_64
#CONFIG_M32=y
# cosmopolitan build (see https://github.com/jart/cosmopolitan)
#CONFIG_COSMO=y

# Temporal defaults on. Native Intl is an incomplete development profile.
# ICU is an explicit backend selection on this branch.
CONFIG_INTL?=n
CONFIG_TEMPORAL?=y
CONFIG_INTL_BACKEND?=native
ifeq ($(filter icu native,$(CONFIG_INTL_BACKEND)),)
$(error CONFIG_INTL_BACKEND must be exactly icu or native)
endif
ifneq ($(words $(CONFIG_INTL_BACKEND)),1)
$(error CONFIG_INTL_BACKEND must be exactly icu or native)
endif
override CONFIG_INTL_NATIVE:=n
# Core Unicode ICU selection is explicit even with both frontends disabled.
CONFIG_ICU?=$(if $(filter native,$(CONFIG_INTL_BACKEND)),n,$(if $(filter y,$(CONFIG_TEMPORAL) $(CONFIG_INTL)),y,n))
ifeq ($(filter y n,$(CONFIG_ICU)),)
$(error CONFIG_ICU must be exactly y or n)
endif
ifneq ($(words $(CONFIG_ICU)),1)
$(error CONFIG_ICU must be exactly y or n)
endif
ifeq ($(CONFIG_INTL_BACKEND),native)
ifeq ($(CONFIG_ICU),y)
$(error CONFIG_INTL_BACKEND=native requires CONFIG_ICU=n)
endif
ifeq ($(CONFIG_INTL),y)
override CONFIG_INTL_NATIVE:=y
endif
else
ifneq ($(filter y,$(CONFIG_TEMPORAL) $(CONFIG_INTL)),)
ifneq ($(CONFIG_ICU),y)
$(error enabled ICU frontends require CONFIG_ICU=y)
endif
endif
endif
override CONFIG_TEMPORAL_ICU:=n
ifeq ($(CONFIG_TEMPORAL),y)
override CONFIG_TEMPORAL_ICU:=$(CONFIG_ICU)
endif
# ECMA-402 legacy constructor chaining is normative optional.
CONFIG_INTL_LEGACY?=y

ifeq ($(filter y n,$(CONFIG_INTL)),)
$(error CONFIG_INTL must be exactly y or n)
endif
ifneq ($(words $(CONFIG_INTL)),1)
$(error CONFIG_INTL must be exactly y or n)
endif
ifeq ($(filter y n,$(CONFIG_TEMPORAL)),)
$(error CONFIG_TEMPORAL must be exactly y or n)
endif
ifneq ($(words $(CONFIG_TEMPORAL)),1)
$(error CONFIG_TEMPORAL must be exactly y or n)
endif
ifeq ($(filter y n,$(CONFIG_INTL_LEGACY)),)
$(error CONFIG_INTL_LEGACY must be exactly y or n)
endif
ifneq ($(words $(CONFIG_INTL_LEGACY)),1)
$(error CONFIG_INTL_LEGACY must be exactly y or n)
endif

# installation directory
PREFIX?=/usr/local

# use the gprof profiler
#CONFIG_PROFILE=y
# use address sanitizer
#CONFIG_ASAN=y
# use memory sanitizer
#CONFIG_MSAN=y
# use UB sanitizer
#CONFIG_UBSAN=y
# use thread sanitizer
#CONFIG_TSAN=y

# TEST262 bootstrap config: commit id and shallow "since" parameter
TEST262_COMMIT?=7ab7fafa0003f73fc85c1b95d88094d33f7eb8bd
TEST262_SINCE?=2025-09-01

OBJDIR=.obj

ifdef CONFIG_ASAN
OBJDIR:=$(OBJDIR)/asan
endif
ifdef CONFIG_MSAN
OBJDIR:=$(OBJDIR)/msan
endif
ifdef CONFIG_UBSAN
OBJDIR:=$(OBJDIR)/ubsan
endif

ifdef CONFIG_DARWIN
# use clang instead of gcc
CONFIG_CLANG=y
CONFIG_DEFAULT_AR=y
endif
ifdef CONFIG_FREEBSD
# use clang instead of gcc
CONFIG_CLANG=y
CONFIG_DEFAULT_AR=y
CONFIG_LTO=
endif

ifdef CONFIG_WIN32
  ifdef CONFIG_M32
    CROSS_PREFIX?=i686-w64-mingw32-
  else
    CROSS_PREFIX?=x86_64-w64-mingw32-
  endif
  EXE=.exe
else ifdef MSYSTEM
  CONFIG_WIN32=y
  CROSS_PREFIX?=
  EXE=.exe
else
  CROSS_PREFIX?=
  EXE=
endif

DEPFLAGS=-MMD -MF $@.d

ifdef CONFIG_CLANG
  HOST_CC=clang
  CC=$(CROSS_PREFIX)clang
  CFLAGS+=-g -Wall 
  CFLAGS += -Wextra
  CFLAGS += -Wno-sign-compare
  CFLAGS += -Wno-missing-field-initializers
  CFLAGS += -Wundef -Wuninitialized
  CFLAGS += -Wunused -Wno-unused-parameter
  CFLAGS += -Wwrite-strings
  CFLAGS += -Wchar-subscripts -funsigned-char
  CFLAGS += 
  ifdef CONFIG_DEFAULT_AR
    AR=$(CROSS_PREFIX)ar
  else
    ifdef CONFIG_LTO
      AR=$(CROSS_PREFIX)llvm-ar
    else
      AR=$(CROSS_PREFIX)ar
    endif
  endif
  LIB_FUZZING_ENGINE ?= "-fsanitize=fuzzer"
else ifdef CONFIG_COSMO
  CONFIG_LTO=
  HOST_CC=gcc
  CC=cosmocc
  # Both architecture dependency files must trigger the shared build recipe.
  DEPFLAGS=-MMD -MF $@.d -MT $@
  CFLAGS=-g -Wall #
  CFLAGS += -Wno-array-bounds -Wno-format-truncation -Wno-infinite-recursion
  AR=cosmoar
else
  HOST_CC=gcc
  CC=$(CROSS_PREFIX)gcc
  CFLAGS+=-g -Wall 
  CFLAGS += -Wno-array-bounds -Wno-format-truncation -Wno-infinite-recursion
  ifdef CONFIG_LTO
    AR=$(CROSS_PREFIX)gcc-ar
  else
    AR=$(CROSS_PREFIX)ar
  endif
endif

include tools/icu-build-config.mk

STRIP?=$(CROSS_PREFIX)strip
ifdef CONFIG_M32
CFLAGS+=-msse2 -mfpmath=sse # use SSE math for correct FP rounding
ifndef CONFIG_WIN32
CFLAGS+=-m32
LDFLAGS+=-m32
endif
endif
CFLAGS+=-fwrapv # ensure that signed overflows behave as expected
ifdef CONFIG_WERROR
CFLAGS+=-Werror
endif
DEFINES:=-D_GNU_SOURCE -DCONFIG_VERSION=\"$(shell cat VERSION)\"
# Private Linux capture transport for an isolated diagnostic engine.
ifeq ($(CONFIG_TZ_CAPTURE_SNAPSHOT),y)
ifdef CONFIG_WIN32
$(error CONFIG_TZ_CAPTURE_SNAPSHOT is a Linux diagnostic configuration)
endif
ifneq ($(CONFIG_ICU),n)
$(error CONFIG_TZ_CAPTURE_SNAPSHOT requires CONFIG_ICU=n)
endif
ifeq ($(filter y,$(CONFIG_TEMPORAL) $(CONFIG_INTL_NATIVE)),)
$(error CONFIG_TZ_CAPTURE_SNAPSHOT requires the native timezone provider)
endif
DEFINES+=-DQJS_TZ_CAPTURE_SNAPSHOT
endif
ifeq ($(CONFIG_TEMPORAL),y)
DEFINES+=-DCONFIG_TEMPORAL
endif
ifeq ($(CONFIG_ICU),y)
DEFINES+=-DCONFIG_ICU
endif
ifeq ($(CONFIG_INTL),y)
DEFINES+=-DCONFIG_INTL
ifeq ($(CONFIG_INTL_NATIVE),y)
DEFINES+=-DCONFIG_INTL_NATIVE
endif
ifeq ($(CONFIG_INTL_LEGACY),y)
DEFINES+=-DCONFIG_INTL_LEGACY
endif
endif
ifdef CONFIG_WIN32
DEFINES+=-D__USE_MINGW_ANSI_STDIO # for standard snprintf behavior
endif
ifndef CONFIG_WIN32
ifeq ($(shell $(CC) -o /dev/null compat/test-closefrom.c 2>/dev/null && echo 1),1)
DEFINES+=-DHAVE_CLOSEFROM
endif
endif

CFLAGS+=$(DEFINES)
CFLAGS+=-Iinclude -Isrc/cutils -Isrc/dtoa -Isrc/unicode -Isrc/regexp
CFLAGS_DEBUG=$(CFLAGS) -O0
CFLAGS_SMALL=$(CFLAGS) -Os
CFLAGS_OPT=$(CFLAGS) -O2
CFLAGS_NOLTO:=$(CFLAGS_OPT)
ifdef CONFIG_COSMO
LDFLAGS+=-s # better to strip by default
else
LDFLAGS+=-g
endif
LTO_FLAG ?= -flto
ifdef CONFIG_LTO
CFLAGS_SMALL+=$(LTO_FLAG)
CFLAGS_OPT+=$(LTO_FLAG)
LDFLAGS+=$(LTO_FLAG)
endif
ifdef CONFIG_PROFILE
CFLAGS+=-p
LDFLAGS+=-p
endif
ifdef CONFIG_ASAN
CFLAGS+=-fsanitize=address -fno-omit-frame-pointer
LDFLAGS+=-fsanitize=address -fno-omit-frame-pointer
endif
ifdef CONFIG_MSAN
CFLAGS+=-fsanitize=memory -fno-omit-frame-pointer
LDFLAGS+=-fsanitize=memory -fno-omit-frame-pointer
endif
ifdef CONFIG_UBSAN
CFLAGS+=-fsanitize=undefined -fno-omit-frame-pointer
LDFLAGS+=-fsanitize=undefined -fno-omit-frame-pointer
endif
ifdef CONFIG_TSAN
CFLAGS+=-fsanitize=thread -fno-omit-frame-pointer
LDFLAGS+=-fsanitize=thread -fno-omit-frame-pointer
endif
ifdef CONFIG_WIN32
LDEXPORT=
else
LDEXPORT=-rdynamic
endif

ifndef CONFIG_COSMO
ifndef CONFIG_DARWIN
ifndef CONFIG_WIN32
CONFIG_SHARED_LIBS=y # building shared libraries is supported
endif
endif
endif

PROGS=qjs$(EXE) qjsc$(EXE) run-test262$(EXE)

ifneq ($(CROSS_PREFIX),)
QJSC_CC=gcc
QJSC=./host-qjsc
PROGS+=$(QJSC)
else
QJSC_CC=$(CC)
QJSC=./qjsc$(EXE)
endif
PROGS+=libquickjs.a
ifdef CONFIG_LTO
PROGS+=libquickjs.lto.a
endif

# examples
ifeq ($(CROSS_PREFIX),)
ifndef CONFIG_ASAN
ifndef CONFIG_MSAN
ifndef CONFIG_UBSAN
PROGS+=examples/hello examples/test_fib
# no -m32 option in qjsc
ifndef CONFIG_M32
ifndef CONFIG_WIN32
PROGS+=examples/hello_module
endif
endif
ifdef CONFIG_SHARED_LIBS
PROGS+=examples/fib.so examples/point.so
endif
endif
endif
endif
endif

QUICKJS_SRCS= \
    src/quickjs/allocator.c \
    src/quickjs/atom.c \
    src/quickjs/bigint.c \
    src/quickjs/builtins/array-buffer.c \
    src/quickjs/builtins/array-from-async.c \
    src/quickjs/builtins/array.c \
    src/quickjs/builtins/async-from-sync-iterator.c \
    src/quickjs/builtins/async-resource-management.c \
    src/quickjs/builtins/async.c \
    src/quickjs/builtins/atomics.c \
    src/quickjs/builtins/bigint.c \
    src/quickjs/builtins/boolean.c \
    src/quickjs/builtins/collections.c \
    src/quickjs/builtins/data-view.c \
    src/quickjs/builtins/date.c \
    src/quickjs/builtins/error.c \
    src/quickjs/builtins/finalization-registry.c \
    src/quickjs/builtins/function.c \
    src/quickjs/builtins/global.c \
    src/quickjs/builtins/intrinsics.c \
    src/quickjs/builtins/iterator.c \
    src/quickjs/builtins/json-stringify.c \
    src/quickjs/builtins/json.c \
    src/quickjs/builtins/math.c \
    src/quickjs/builtins/number.c \
    src/quickjs/builtins/object.c \
    src/quickjs/builtins/promise.c \
    src/quickjs/builtins/proxy.c \
    src/quickjs/builtins/reflect.c \
    src/quickjs/builtins/regexp.c \
    src/quickjs/builtins/resource-management.c \
    src/quickjs/builtins/string.c \
    src/quickjs/builtins/symbol.c \
    src/quickjs/builtins/typed-array.c \
    src/quickjs/builtins/uint8array-encoding.c \
    src/quickjs/builtins/weakref.c \
    src/quickjs/bytecode-format.c \
    src/quickjs/c-function.c \
    src/quickjs/class.c \
    src/quickjs/compiler/backend.c \
    src/quickjs/compiler/bytecode-dump.c \
    src/quickjs/compiler/eval.c \
    src/quickjs/compiler/lexer.c \
    src/quickjs/compiler/parser.c \
    src/quickjs/compiler/stack-analysis.c \
    src/quickjs/error-support.c \
    src/quickjs/function-list.c \
    src/quickjs/function.c \
    src/quickjs/gc.c \
    src/quickjs/generator.c \
    src/quickjs/global-environment.c \
    src/quickjs/disposal.c \
    src/quickjs/default-time-zone.c \
    src/quickjs/iterator-protocol.c \
    src/quickjs/memory-usage.c \
    src/quickjs/module-evaluation.c \
    src/quickjs/module.c \
    src/quickjs/native-jobs.c \
    src/quickjs/number.c \
    src/quickjs/object.c \
    src/quickjs/parse-state.c \
    src/quickjs/runtime.c \
    src/quickjs/serialization/reader.c \
    src/quickjs/serialization/writer.c \
    src/quickjs/string-buffer.c \
    src/quickjs/string.c \
    src/quickjs/value/compare.c \
    src/quickjs/value/conversion.c \
    src/quickjs/value/print.c \
    src/quickjs/vm.c
# The pure C calendar library owns its authority and publication flags.
CALENDAR_SRCS= \
    src/calendar/calendar.c \
    src/calendar/solar.c \
    src/calendar/hebrew.c \
    src/calendar/islamic.c \
    src/calendar/astronomy.c \
    src/calendar/lunisolar.c \
    src/calendar/provider.c
CALENDAR_AUTHORITY_CFLAGS= \
    -DQJS_CAL_USE_PERSIAN_AUTHORITY_TABLE \
    -DQJS_CAL_ENABLE_LUNISOLAR_CANDIDATE \
    -DQJS_CAL_HAVE_PUBLISHED_LUNISOLAR_DATA \
    -DQJS_CAL_PERSIAN_AUTHORITY_VERIFIED \
    -DQJS_CAL_CHINESE_AUTHORITY_VERIFIED \
    -DQJS_CAL_DANGI_AUTHORITY_VERIFIED
CALENDAR_CONFIG_OBJECTS=$(foreach suffix,o host.o pic.o nolto.o debug.o fuzz.o check.o,$(patsubst %.c,$(OBJDIR)/%.$(suffix),$(CALENDAR_SRCS)))
$(CALENDAR_CONFIG_OBJECTS): CALENDAR_COMPILE_CFLAGS=$(CALENDAR_AUTHORITY_CFLAGS)
QUICKJS_SRCS+=src/quickjs/builtins/intl/core.c
QUICKJS_SRCS+=$(CALENDAR_SRCS)

TEMPORAL_LIBRARY_SRCS= \
    src/temporal/epoch.c \
    src/temporal/options.c \
    src/temporal/iso.c \
    src/temporal/civil.c \
    src/temporal/parse.c \
    src/temporal/format.c \
    src/temporal/duration.c \
    src/temporal/time.c \
    src/temporal/calendar.c \
    src/temporal/time-zone.c \
    src/temporal/relative.c

INTL_NATIVE_LIBRARY_SRCS= \
    src/intl/collator-native-data.c \
    src/intl/collator-native.c \
    src/intl/data/date-data-validation.c \
    src/intl/data/duration-data-validation.c \
    src/intl/data/locale-metadata.c \
    src/intl/data/native-data-reader.c \
    src/intl/data/number-data-validation.c \
    src/intl/data/number-extra-validation.c \
    src/intl/data/number-template-validation.c \
    src/intl/data/plural-data-validation.c \
    src/intl/data/relative-data-validation.c \
    src/intl/date-native-bank.c \
    src/intl/date-native-calendar-data.c \
    src/intl/date-native-calendar.c \
    src/intl/date-native-data.c \
    src/intl/date-native-zone.c \
    src/intl/date-native.c \
    src/intl/date-pattern.c \
    src/intl/decimal.c \
    src/intl/display-names-native.c \
    src/intl/duration-native-data.c \
    src/intl/duration-native.c \
    src/intl/list-native-data.c \
    src/intl/list-native.c \
    src/intl/locale-grammar.c \
    src/intl/native-locale-id.c \
    src/intl/native-locale-info.c \
    src/intl/number-compact.c \
    src/intl/number-decimal.c \
    src/intl/number-native-data.c \
    src/intl/number-native.c \
    src/intl/number-range.c \
    src/intl/number-unit.c \
    src/intl/plural-native-data.c \
    src/intl/plural-native.c \
    src/intl/plural.c \
    src/intl/provider-native-collator.c \
    src/intl/provider-native-date.c \
    src/intl/provider-native-display.c \
    src/intl/provider-native-duration.c \
    src/intl/provider-native-general.c \
    src/intl/provider-native-number.c \
    src/intl/provider-native-plural.c \
    src/intl/provider-native-relative.c \
    src/intl/provider-native-segmenter.c \
    src/intl/provider-native.c \
    src/intl/relative-native-data.c \
    src/intl/relative-native.c \
    src/intl/segmenter-native-data.c \
    src/intl/segmenter-native.c \
    src/intl/segmenter-unicode.c

# Complete frontend closure: no partial constructors or global namespace.
QUICKJS_SRCS+=src/quickjs/builtins/temporal/common.c
ifeq ($(CONFIG_TEMPORAL),y)
TEMPORAL_ENGINE_SRCS= \
    src/quickjs/builtins/temporal/calendar-fields.c \
    src/quickjs/builtins/temporal/duration.c \
    src/quickjs/builtins/temporal/instant.c \
    src/quickjs/builtins/temporal/now.c \
    src/quickjs/builtins/temporal/options.c \
    src/quickjs/builtins/temporal/plain-time.c \
    src/quickjs/builtins/temporal/plain.c \
    src/quickjs/builtins/temporal/zoned-arithmetic.c \
    src/quickjs/builtins/temporal/zoned-date-time.c
QUICKJS_SRCS+=$(TEMPORAL_ENGINE_SRCS) $(TEMPORAL_LIBRARY_SRCS)
endif

TIMEZONE_SRCS=src/timezone/timezone.c src/timezone/tzif.c src/timezone/embedded.c
ifeq ($(CONFIG_TZ_CAPTURE_SNAPSHOT),y)
TIMEZONE_SRCS+=tools/ci/native-prefix/selected-timezone-snapshot.c
endif
ifdef CONFIG_WIN32
TIMEZONE_SRCS+=src/timezone/windows-zone.c
endif
TIMEZONE_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(TIMEZONE_SRCS))


ifeq ($(CONFIG_INTL_NATIVE),y)
INTL_NATIVE_FRONTEND_SRCS= \
    src/quickjs/builtins/intl/native-collator.c \
    src/quickjs/builtins/intl/native-date-time-format.c \
    src/quickjs/builtins/intl/native-display-names.c \
    src/quickjs/builtins/intl/native-duration-format.c \
    src/quickjs/builtins/intl/native-general.c \
    src/quickjs/builtins/intl/native-list-format.c \
    src/quickjs/builtins/intl/native-locale-resolution.c \
    src/quickjs/builtins/intl/native-locale.c \
    src/quickjs/builtins/intl/native-number-common.c \
    src/quickjs/builtins/intl/native-number-format.c \
    src/quickjs/builtins/intl/native-plural-rules.c \
    src/quickjs/builtins/intl/native-relative-time-format.c \
    src/quickjs/builtins/intl/native-segmenter.c \
    src/quickjs/builtins/intl/native-values.c \
    src/quickjs/builtins/intl/options.c \
    src/quickjs/builtins/intl/intl-text.c \
    src/quickjs/builtins/intl/locale-syntax.c \
    src/quickjs/builtins/intl/bound-function.c
QUICKJS_SRCS:=$(strip $(QUICKJS_SRCS) $(filter-out $(QUICKJS_SRCS),$(TEMPORAL_LIBRARY_SRCS) $(INTL_NATIVE_LIBRARY_SRCS) $(INTL_NATIVE_FRONTEND_SRCS)))
endif

ifeq ($(CONFIG_ICU),y)
ICU_SHARED_SRCS=src/intl/locale-data.c src/intl/locale-grammar.c
QUICKJS_SRCS:=$(strip $(QUICKJS_SRCS) $(filter-out $(QUICKJS_SRCS),$(TEMPORAL_LIBRARY_SRCS) $(ICU_SHARED_SRCS)))
ifeq ($(CONFIG_INTL),y)
INTL_ICU_FRONTEND_SRCS= \
    src/quickjs/builtins/intl/bound-function.c \
    src/quickjs/builtins/intl/case-conversion.c \
    src/quickjs/builtins/intl/collator.c \
    src/quickjs/builtins/intl/date-time-format.c \
    src/quickjs/builtins/intl/display-names.c \
    src/quickjs/builtins/intl/duration-format.c \
    src/quickjs/builtins/intl/intl-text.c \
    src/quickjs/builtins/intl/intl-values.c \
    src/quickjs/builtins/intl/list-format.c \
    src/quickjs/builtins/intl/locale-resolution.c \
    src/quickjs/builtins/intl/locale-syntax.c \
    src/quickjs/builtins/intl/locale.c \
    src/quickjs/builtins/intl/number-common.c \
    src/quickjs/builtins/intl/number-format.c \
    src/quickjs/builtins/intl/options.c \
    src/quickjs/builtins/intl/plural-rules.c \
    src/quickjs/builtins/intl/relative-time-format.c \
    src/quickjs/builtins/intl/segmenter.c \
    src/quickjs/builtins/intl/values.c
INTL_ICU_LIBRARY_SRCS=src/intl/libintl.c src/intl/plural.c src/intl/plural-icu.c
QUICKJS_SRCS+=$(INTL_ICU_FRONTEND_SRCS) $(INTL_ICU_LIBRARY_SRCS)
endif
endif

QUICKJS_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(QUICKJS_SRCS))

all: $(OBJDIR) $(patsubst %.o,%.check.o,$(QUICKJS_OBJS)) $(OBJDIR)/tools/qjs.check.o $(PROGS)

WAIT_SRCS=src/libwait/wait-queue.c
WAIT_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(WAIT_SRCS))

REGEXP_SRCS= \
    src/regexp/compile.c \
    src/regexp/exec.c
REGEXP_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(REGEXP_SRCS))
QUICKJS_LIBC_SRCS= \
    src/quickjs-libc/event-loop.c \
    src/quickjs-libc/host.c \
    src/quickjs-libc/module-loader.c \
    src/quickjs-libc/os.c \
    src/quickjs-libc/std.c \
    src/quickjs-libc/worker.c
QUICKJS_LIBC_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(QUICKJS_LIBC_SRCS))
QJS_LIB_OBJS=$(QUICKJS_OBJS) $(if $(filter n,$(CONFIG_ICU)),$(if $(filter y,$(CONFIG_TEMPORAL) $(CONFIG_INTL_NATIVE)),$(TIMEZONE_OBJS))) $(WAIT_OBJS) $(OBJDIR)/src/dtoa/dtoa.o $(REGEXP_OBJS) $(OBJDIR)/src/unicode/libunicode.o $(OBJDIR)/src/cutils/cutils.o $(QUICKJS_LIBC_OBJS)

QJS_OBJS=$(OBJDIR)/tools/qjs.o $(OBJDIR)/repl.o $(QJS_LIB_OBJS)

HOST_LIBS=-lm -ldl -lpthread
LIBS=-lm -lpthread
ifndef CONFIG_WIN32
LIBS+=-ldl
endif
LIBS+=$(EXTRA_LIBS)
ifeq ($(CONFIG_ICU),y)
LIBS+=$(ICU_LIBS)
HOST_LIBS+=$(HOST_ICU_LIBS)
ICU_HEADER_SRCS=$(QUICKJS_SRCS) $(wildcard tests/*.c) src/unicode/libunicode.c src/quickjs-libc/host.c tools/qjs.c tools/qjsc.c tools/run-test262.c
ICU_TARGET_OBJECTS=$(foreach suffix,o pic.o nolto.o debug.o fuzz.o check.o,$(patsubst %.c,$(OBJDIR)/%.$(suffix),$(ICU_HEADER_SRCS)))
$(ICU_TARGET_OBJECTS): ICU_COMPILE_CFLAGS=$(ICU_CFLAGS)
$(patsubst %.c,$(OBJDIR)/%.host.o,$(ICU_HEADER_SRCS)): ICU_COMPILE_CFLAGS=$(HOST_ICU_CFLAGS)
endif


$(OBJDIR):
	mkdir -p $(OBJDIR) $(OBJDIR)/examples $(OBJDIR)/tests

qjs$(EXE): $(QJS_OBJS)
	$(CC) $(LDFLAGS) $(LDEXPORT) -o $@ $^ $(LIBS)

qjs-debug$(EXE): $(patsubst %.o, %.debug.o, $(QJS_OBJS))
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

qjsc$(EXE): $(OBJDIR)/tools/qjsc.o $(QJS_LIB_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

fuzz_eval: $(OBJDIR)/fuzz/fuzz_eval.o $(OBJDIR)/fuzz/fuzz_common.o libquickjs.fuzz.a
	$(CC) $(CFLAGS_OPT) $^ -o fuzz_eval $(LIB_FUZZING_ENGINE) $(LIBS)

fuzz_compile: $(OBJDIR)/fuzz/fuzz_compile.o $(OBJDIR)/fuzz/fuzz_common.o libquickjs.fuzz.a
	$(CC) $(CFLAGS_OPT) $^ -o fuzz_compile $(LIB_FUZZING_ENGINE) $(LIBS)

fuzz_regexp: $(OBJDIR)/fuzz/fuzz_regexp.o $(patsubst %.o,%.fuzz.o,$(REGEXP_OBJS)) $(OBJDIR)/src/cutils/cutils.fuzz.o $(OBJDIR)/src/unicode/libunicode.fuzz.o
	$(CC) $(CFLAGS_OPT) $^ -o fuzz_regexp $(LIB_FUZZING_ENGINE) $(LIBS)

libfuzzer: fuzz_eval fuzz_compile fuzz_regexp

ifneq ($(CROSS_PREFIX),)

$(QJSC): $(OBJDIR)/tools/qjsc.host.o \
    $(patsubst %.o, %.host.o, $(QJS_LIB_OBJS))
	$(HOST_CC) $(LDFLAGS) -o $@ $^ $(HOST_LIBS)

endif #CROSS_PREFIX

QJSC_DEFINES:=-DCONFIG_CC=\"$(QJSC_CC)\" -DCONFIG_PREFIX=\"$(PREFIX)\"
ifdef CONFIG_LTO
QJSC_DEFINES+=-DCONFIG_LTO -DCONFIG_LTO_FLAG=\"$(LTO_FLAG)\"
endif
ifdef CONFIG_M32
QJSC_DEFINES+=-DCONFIG_M32
endif
QJSC_HOST_DEFINES:=-DCONFIG_CC=\"$(HOST_CC)\" -DCONFIG_PREFIX=\"$(PREFIX)\"

# Executable output must link with the instrumented target archive.
QJSC_SANITIZER_DEFINES=$(foreach config,ASAN MSAN UBSAN TSAN,$(if $(CONFIG_$(config)),-DCONFIG_$(config)))
QJSC_DEFINES+=$(QJSC_SANITIZER_DEFINES)
QJSC_HOST_DEFINES+=$(QJSC_SANITIZER_DEFINES)

ifeq ($(CONFIG_ICU),y)
QJSC_DEFINES+=-I$(OBJDIR)
QJSC_HOST_DEFINES+=-I$(OBJDIR) -DQJSC_INTL_LINK_HEADER=\"qjsc-intl-host-link.h\"
$(OBJDIR)/qjsc-intl-link.h: tools/intl-link-config.py $(OBJDIR)/temporal-build-config
	$(PYTHON) $< header $@ -- $(ICU_LIBS)
$(OBJDIR)/qjsc-intl-host-link.h: tools/intl-link-config.py $(OBJDIR)/temporal-build-config
	$(PYTHON) $< header $@ -- $(HOST_ICU_LIBS)
$(OBJDIR)/tools/qjsc.o: $(OBJDIR)/qjsc-intl-link.h
$(OBJDIR)/tools/qjsc.host.o: $(OBJDIR)/qjsc-intl-host-link.h
$(OBJDIR)/quickjs.pc: tools/intl-link-config.py $(OBJDIR)/temporal-build-config
	$(PYTHON) $< pkgconfig $@ $(call intl_shell_quote,$(PREFIX)) $(call intl_shell_quote,$(shell cat VERSION)) -- $(LIBS)
install: $(OBJDIR)/quickjs.pc
else
ifeq ($(CONFIG_INTL_NATIVE),y)
$(OBJDIR)/quickjs.pc: tools/intl-link-config.py $(OBJDIR)/temporal-build-config
	$(PYTHON) $< pkgconfig $@ $(call intl_shell_quote,$(PREFIX)) $(call intl_shell_quote,$(shell cat VERSION)) -- $(LIBS)
install: $(OBJDIR)/quickjs.pc
endif
endif

$(OBJDIR)/tools/qjsc.o: CFLAGS+=$(QJSC_DEFINES)
$(OBJDIR)/tools/qjsc.host.o: CFLAGS+=$(QJSC_HOST_DEFINES)

ifdef CONFIG_LTO
LTOEXT=.lto
else
LTOEXT=
endif

libquickjs$(LTOEXT).a: $(QJS_LIB_OBJS)
	rm -f $@
	$(AR) rcs $@ $^

ifdef CONFIG_LTO
libquickjs.a: $(patsubst %.o, %.nolto.o, $(QJS_LIB_OBJS))
	rm -f $@
	$(AR) rcs $@ $^
endif # CONFIG_LTO

libquickjs.fuzz.a: $(patsubst %.o, %.fuzz.o, $(QJS_LIB_OBJS))
	rm -f $@
	$(AR) rcs $@ $^

repl.c: $(QJSC) tools/repl.js
	$(QJSC) -s -c -o $@ -m tools/repl.js

ifneq ($(wildcard unicode/UnicodeData.txt),)
$(OBJDIR)/src/unicode/libunicode.o $(OBJDIR)/src/unicode/libunicode.nolto.o: src/unicode/libunicode-table.h

src/unicode/libunicode-table.h: unicode_gen
	./unicode_gen unicode $@
endif

run-test262$(EXE): $(OBJDIR)/tools/run-test262.o $(QJS_LIB_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

run-test262-debug: $(patsubst %.o, %.debug.o, $(OBJDIR)/tools/run-test262.o $(QJS_LIB_OBJS))
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# object suffix order: nolto

ifdef CONFIG_CLANG
# Allow tail duplication of the VM's 256-way opcode dispatch. Probe the LLVM
# option because older Clang releases may not support it.
CLANG_VM_CFLAGS:=$(shell $(CC) -x c -c /dev/null -o /dev/null -mllvm -tail-dup-succ-size=256 >/dev/null 2>&1 && echo -mllvm -tail-dup-succ-size=256)
$(OBJDIR)/src/quickjs/vm.o $(OBJDIR)/src/quickjs/vm.pic.o $(OBJDIR)/src/quickjs/vm.fuzz.o: CFLAGS_OPT+=$(CLANG_VM_CFLAGS)
$(OBJDIR)/src/quickjs/vm.nolto.o: CFLAGS_NOLTO+=$(CLANG_VM_CFLAGS)
endif

$(OBJDIR)/%.o: %.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/fuzz/%.o: fuzz/%.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -c -I. -o $@ $<

$(OBJDIR)/%.host.o: %.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(HOST_CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/%.pic.o: %.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -fPIC -DJS_SHARED_LIBRARY -c -o $@ $<

$(OBJDIR)/%.nolto.o: %.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_NOLTO) $(ICU_COMPILE_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/%.debug.o: %.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_DEBUG) $(ICU_COMPILE_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/%.fuzz.o: %.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -fsanitize=fuzzer-no-link -c -o $@ $<

$(OBJDIR)/%.check.o: %.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(ICU_COMPILE_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -DCONFIG_CHECK_JSVALUE -c -o $@ $<

regexp_test$(EXE): tests/regexp_test.c src/regexp/compile.c src/regexp/exec.c src/unicode/libunicode.c src/cutils/cutils.c
	$(CC) $(LDFLAGS) $(CFLAGS) $(if $(filter y,$(CONFIG_ICU)),$(ICU_CFLAGS)) -DTEST -o $@ tests/regexp_test.c src/regexp/compile.c src/regexp/exec.c src/unicode/libunicode.c src/cutils/cutils.c $(LIBS)

unicode_gen: $(OBJDIR)/tools/unicode_gen.host.o $(OBJDIR)/src/cutils/cutils.host.o tools/unicode_gen_def.h
	$(HOST_CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJDIR)/tools/unicode_gen.host.o $(OBJDIR)/src/cutils/cutils.host.o

$(OBJDIR)/tools/unicode_gen.test.host.o: tools/unicode_gen.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(HOST_CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -DCONFIG_UNICODE_NATIVE -DUSE_TEST -c -o $@ $<

$(OBJDIR)/src/unicode/libunicode.test.host.o: src/unicode/libunicode.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(HOST_CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -DCONFIG_UNICODE_NATIVE -DUSE_TEST -c -o $@ $<

unicode_gen_test: $(OBJDIR)/tools/unicode_gen.test.host.o $(OBJDIR)/src/unicode/libunicode.test.host.o $(OBJDIR)/src/cutils/cutils.host.o tools/unicode_gen_def.h
	$(HOST_CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJDIR)/tools/unicode_gen.test.host.o $(OBJDIR)/src/unicode/libunicode.test.host.o $(OBJDIR)/src/cutils/cutils.host.o

clean:
	rm -f test262_intl-*_temporal-*.txt test262-intl_intl-*_temporal-*.txt test262-temporal_intl-*_temporal-*.txt
	rm -f repl.c out.c quickjs.h quickjs-libc.h
	rm -f *.a *.o *.d *~ unicode_gen unicode_gen_test regexp_test$(EXE) fuzz_eval fuzz_compile fuzz_regexp $(PROGS)
	rm -f hello.c test_fib.c
	rm -f examples/*.so tests/*.so $(C_TESTS) tests/run_temporal_zone_test$(EXE)
	rm -rf $(OBJDIR)/ *.dSYM/ qjs-debug$(EXE)
	rm -rf run-test262-debug$(EXE)
	rm -f run_octane run_sunspider_like

install: all
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	$(STRIP) qjs$(EXE) qjsc$(EXE)
	install -m755 qjs$(EXE) qjsc$(EXE) "$(DESTDIR)$(PREFIX)/bin"
	mkdir -p "$(DESTDIR)$(PREFIX)/lib/quickjs"
	install -m644 libquickjs.a "$(DESTDIR)$(PREFIX)/lib/quickjs"
ifdef CONFIG_LTO
	install -m644 libquickjs.lto.a "$(DESTDIR)$(PREFIX)/lib/quickjs"
endif
	mkdir -p "$(DESTDIR)$(PREFIX)/include/quickjs"
	install -m644 include/quickjs.h include/quickjs-libc.h "$(DESTDIR)$(PREFIX)/include/quickjs"
ifneq ($(filter y,$(CONFIG_ICU) $(CONFIG_INTL_NATIVE)),)
	mkdir -p "$(DESTDIR)$(PREFIX)/lib/pkgconfig"
	install -m644 $(OBJDIR)/quickjs.pc "$(DESTDIR)$(PREFIX)/lib/pkgconfig"
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
.PHONY: test-intl-native-date-exhaustive
test-intl-native-date-exhaustive: qjs$(EXE)
	$(WINE) ./qjs$(EXE) --std -e "globalThis.intlDateExhaustive = true; std.loadScript('tests/test_intl_native_date_all_calendars.js');"
endif

###############################################################################
# examples

# example of static JS compilation
HELLO_SRCS=examples/hello.js
HELLO_OPTS=-fno-string-normalize -fno-map -fno-promise -fno-typedarray \
           -fno-typedarray -fno-regexp -fno-json -fno-eval -fno-proxy \
           -fno-date -fno-module-loader

hello.c: $(QJSC) $(HELLO_SRCS)
	$(QJSC) -e $(HELLO_OPTS) -o $@ $(HELLO_SRCS)

examples/hello: $(OBJDIR)/hello.o $(QJS_LIB_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# example of static JS compilation with modules
HELLO_MODULE_SRCS=examples/hello_module.js
HELLO_OPTS+=$(if $(filter y,$(CONFIG_TEMPORAL)),-fno-temporal)

HELLO_MODULE_OPTS=-fno-string-normalize -fno-map -fno-typedarray \
           -fno-typedarray -fno-regexp -fno-json -fno-eval -fno-proxy \
           -fno-date -m
HELLO_MODULE_OPTS+=$(if $(filter y,$(CONFIG_TEMPORAL)),-fno-temporal)
examples/hello_module: $(QJSC) libquickjs$(LTOEXT).a $(HELLO_MODULE_SRCS)
	$(QJSC) $(HELLO_MODULE_OPTS) -o $@ $(HELLO_MODULE_SRCS)

# use of an external C module (static compilation)

test_fib.c: $(QJSC) examples/test_fib.js
	$(QJSC) -e -M examples/fib.so,fib -m -o $@ examples/test_fib.js

examples/test_fib: $(OBJDIR)/test_fib.o $(OBJDIR)/examples/fib.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

examples/fib.so: $(OBJDIR)/examples/fib.pic.o
	$(CC) $(LDFLAGS) -shared -o $@ $^

examples/point.so: $(OBJDIR)/examples/point.pic.o
	$(CC) $(LDFLAGS) -shared -o $@ $^

###############################################################################
# documentation

DOCS=doc/quickjs.pdf doc/quickjs.html

build_doc: $(DOCS)

clean_doc:
	rm -f $(DOCS)

doc/version.texi: VERSION
	@echo "@set VERSION `cat $<`" > $@

doc/%.pdf: doc/%.texi doc/version.texi
	texi2pdf --clean -o $@ -q $<

doc/%.html.pre: doc/%.texi doc/version.texi
	makeinfo --html --no-headers --no-split --number-sections -o $@ $<

doc/%.html: doc/%.html.pre
	sed -e 's|</style>|</style>\n<meta name="viewport" content="width=device-width, initial-scale=1.0">|' < $< > $@

###############################################################################
# tests

PYTHON?=python3
# APE executables use their shell launcher when invoked by Python.
QJSC_TEST_RUNNER?=$(if $(CONFIG_COSMO),sh,)

C_TESTS=tests/test_api$(EXE) tests/test_bytecode$(EXE) tests/test_cutils$(EXE) \
        tests/test_unicode$(EXE) tests/test_bytecode_trace$(EXE) \
        tests/test_typed_array$(EXE) tests/test_allocator$(EXE)
C_TESTS+=tests/test_qjsc_context_failures$(EXE)
C_TESTS+=tests/test_qjsc_json_preload$(EXE)
C_TESTS+=tests/test_qjsc_json_probe$(EXE) tests/test_qjsc_json_exec$(EXE) tests/test_qjsc_native_std_exec$(EXE)
QJSC_JSON_COLLISION_CASES:=attributes-first attributes-last size-first size-last
C_TESTS+=$(addprefix tests/test_qjsc_json_collision_,$(addsuffix $(EXE),$(QJSC_JSON_COLLISION_CASES)))

C_TESTS+=tests/test_iterator$(EXE)
C_TESTS+=tests/test_allocator_policy$(EXE)
C_TESTS+=tests/test_unicode_oom$(EXE)
C_TESTS+=tests/test_atomics_wait$(EXE)
C_TESTS+=tests/test_worker_context_failure$(EXE)

C_TESTS+=tests/test_fuzz_json$(EXE)

C_TESTS+=tests/test_fuzz_exception_ownership$(EXE)

C_TESTS+=tests/test_fuzz_allocations$(EXE)

C_TESTS+=tests/test_fuzz_regexp_timeout$(EXE)

C_TESTS+=tests/test_wait_async$(EXE)
C_TESTS+=tests/test_native_jobs$(EXE)
C_TESTS+=tests/test_wait_queue$(EXE)
C_TESTS+=tests/test_timezone$(EXE) tests/test_timezone_system_reader$(EXE)
C_TESTS+=tests/test_tzif_decoder$(EXE)
C_TESTS+=tests/test_windows_zones$(EXE)
ifdef CONFIG_WIN32
C_TESTS+=tests/test_windows_zones_system$(EXE)
endif
C_TESTS+=tests/test_temporal_epoch$(EXE)
C_TESTS+=tests/test_temporal_date_math$(EXE)
C_TESTS+=tests/test_temporal$(EXE)
C_TESTS+=tests/test_temporal_format$(EXE)
C_TESTS+=tests/test_temporal_duration_math$(EXE)
C_TESTS+=tests/test_temporal_api$(EXE)
ifeq ($(CONFIG_TEMPORAL),y)
C_TESTS+=tests/test_temporal_runtime_zone_provider$(EXE) tests/test_temporal_system_clock$(EXE)
endif
C_TESTS+=tests/test_default_time_zone_api$(EXE)
C_TESTS+=tests/test_intl_native_plural$(EXE) tests/test_intl_plural$(EXE)
C_TESTS+=tests/test_intl_decimal$(EXE) tests/test_intl_native_relative_decimal$(EXE)
C_TESTS+=tests/test_temporal_civil$(EXE)
C_TESTS+=tests/test_temporal_zones$(EXE)
C_TESTS+=tests/test_temporal_calendars$(EXE)
ifeq ($(CONFIG_ICU),n)
C_TESTS+=tests/test_temporal_native_calendars$(EXE)
endif
CALENDAR_TEST_SRCS=tests/test_calendar_portable.c tests/test_calendar_persian_authority.c tests/test_calendar_integrated_authority.c
C_TESTS+=$(patsubst %.c,%$(EXE),$(CALENDAR_TEST_SRCS))

# Link the tracing reader before the archive so it replaces the normal reader.
$(OBJDIR)/src/quickjs/serialization/reader.trace.o: src/quickjs/serialization/reader.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -DDUMP_READ_OBJECT -c -o $@ $<

tests/test_bytecode_trace$(EXE): $(OBJDIR)/tests/test_bytecode.o $(OBJDIR)/src/quickjs/serialization/reader.trace.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_iterator$(EXE): $(OBJDIR)/tests/test_iterator.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_allocator_policy$(EXE): $(OBJDIR)/tests/test_allocator_policy.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_allocator$(EXE): $(OBJDIR)/tests/test_allocator.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_atomics_wait$(EXE): $(OBJDIR)/tests/test_atomics_wait.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# The native unit includes the actual Worker caller with test call redirection.
$(OBJDIR)/tests/test_worker_context_failure.o: src/quickjs-libc/worker.c

tests/test_worker_context_failure$(EXE): $(OBJDIR)/tests/test_worker_context_failure.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Compile the actual qjsc-generated context/main into the native fault unit.
$(OBJDIR)/tests/qjsc-context-generated.c: $(QJSC) tests/fixture_qjsc_context.js
	mkdir -p $(@D)
	$(QJSC) -e -o $@ tests/fixture_qjsc_context.js

$(OBJDIR)/tests/test_qjsc_context_failures.o: $(OBJDIR)/tests/qjsc-context-generated.c
$(OBJDIR)/tests/test_qjsc_context_failures.o: CFLAGS+=-I$(OBJDIR)/tests

tests/test_qjsc_context_failures$(EXE): $(OBJDIR)/tests/test_qjsc_context_failures.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_wait_queue$(EXE): $(OBJDIR)/tests/test_wait_queue.o $(WAIT_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_native_jobs$(EXE): $(OBJDIR)/tests/test_native_jobs.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_wait_async$(EXE): $(OBJDIR)/tests/test_wait_async.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_qjsc_json_preload$(EXE): $(OBJDIR)/tests/test_qjsc_json_preload.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Use the actual compiler loader for deterministic attribute-probe errors.
$(OBJDIR)/tools/qjsc.json-probe.o: tools/qjsc.c $(OBJDIR)/tools/qjsc.o | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(QJSC_DEFINES) $(DEPFLAGS) -Dmain=qjsc_json_probe_tool_main -c -o $@ $<

tests/test_qjsc_json_probe$(EXE): $(OBJDIR)/tests/test_qjsc_json_probe.o $(OBJDIR)/tools/qjsc.json-probe.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/tests/qjsc-json-attributes.c: $(QJSC) tests/prepare_qjsc_json_attributes.py $(wildcard tests/qjsc-json-attributes/*)
	mkdir -p $(@D)
	$(PYTHON) tests/prepare_qjsc_json_attributes.py "$(QJSC)" "$@" "$(QJSC_TEST_RUNNER)"

$(OBJDIR)/tests/qjsc-json-attributes.o: $(OBJDIR)/tests/qjsc-json-attributes.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

tests/test_qjsc_json_exec$(EXE): $(OBJDIR)/tests/qjsc-json-attributes.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/tests/qjsc-native-std.c: $(QJSC) tests/qjsc-json-attributes/native-std.js
	mkdir -p $(@D)
	$(QJSC) -e -o $@ tests/qjsc-json-attributes/native-std.js

$(OBJDIR)/tests/qjsc-native-std.o: $(OBJDIR)/tests/qjsc-native-std.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

tests/test_qjsc_native_std_exec$(EXE): $(OBJDIR)/tests/qjsc-native-std.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

QJSC_JSON_COLLISION_SRCS:=$(addprefix $(OBJDIR)/tests/qjsc-json-collision-,$(addsuffix .c,$(QJSC_JSON_COLLISION_CASES)))
# Only fixture cases are generated; implicit .d remake probes are not cases.
$(QJSC_JSON_COLLISION_SRCS): $(OBJDIR)/tests/qjsc-json-collision-%.c: $(QJSC) tests/prepare_qjsc_json_collisions.py $(wildcard tests/qjsc-json-collisions/*)
	mkdir -p $(@D)
	$(PYTHON) tests/prepare_qjsc_json_collisions.py "$(QJSC)" "$@" "$*" "$(QJSC_TEST_RUNNER)"

$(OBJDIR)/tests/qjsc-json-collision-%.o: $(OBJDIR)/tests/qjsc-json-collision-%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

tests/test_qjsc_json_collision_%$(EXE): $(OBJDIR)/tests/qjsc-json-collision-%.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_api$(EXE): $(OBJDIR)/tests/test_api.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_typed_array$(EXE): $(OBJDIR)/tests/test_typed_array.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_bytecode$(EXE): $(OBJDIR)/tests/test_bytecode.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_cutils$(EXE): $(OBJDIR)/tests/test_cutils.o $(OBJDIR)/src/cutils/cutils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_unicode_oom$(EXE): $(OBJDIR)/tests/test_unicode_oom.o $(REGEXP_OBJS) $(OBJDIR)/src/unicode/libunicode.o $(OBJDIR)/src/cutils/cutils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_unicode$(EXE): $(OBJDIR)/tests/test_unicode.o $(OBJDIR)/src/unicode/libunicode.o $(OBJDIR)/src/cutils/cutils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

C_TESTS+=tests/test_intl_number_grammar$(EXE)

C_TESTS+=tests/test_intl_date_pattern$(EXE)

C_TESTS+=tests/test_intl_data_reader$(EXE)

C_TESTS+=tests/test_intl_data_bundle$(EXE)

C_TESTS+=tests/test_intl_native_locale_id$(EXE)

C_TESTS+=tests/test_intl_native_list_engine$(EXE)

C_TESTS+=tests/test_intl_native_list_data$(EXE)

C_TESTS+=tests/test_intl_native_number_engine$(EXE)

C_TESTS+=tests/test_intl_native_duration_engine$(EXE)

C_TESTS+=tests/test_intl_calendar_domain$(EXE)

C_TESTS+=tests/test_intl_native_timezone$(EXE)

C_TESTS+=tests/test_intl_receiver_api$(EXE)

C_TESTS+=tests/test_intl_default_locale_api$(EXE)

ifeq ($(CONFIG_INTL),y)
ifneq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_locale_lookup$(EXE)
endif
endif

ifeq ($(CONFIG_INTL),y)
ifneq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_embedder$(EXE)
endif
endif

ifeq ($(CONFIG_INTL),y)
ifneq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_text$(EXE)
endif
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_number_provider$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_plural_provider$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_duration_provider$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_frontend_oom$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_general_oom$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_collator_oom$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_segmenter_oom$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_number_oom$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_plural_oom$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_relative_oom$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_duration_oom$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_display_oom$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_timezone_runtime$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_date_bank_data$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_date_frontend_oom$(EXE)
endif

ifeq ($(CONFIG_INTL_NATIVE),y)
C_TESTS+=tests/test_intl_native_locale_info$(EXE)
endif

C_TESTS+=tests/test_intl_collator_segmenter$(EXE)

C_TESTS+=tests/test_intl_number_format_embed$(EXE)

C_TESTS+=tests/test_intl_duration_format_embed$(EXE)

C_TESTS+=tests/test_intl_services_api$(EXE)

ifeq ($(CONFIG_INTL)$(CONFIG_ICU),yy)
C_TESTS+=tests/test_intl_oom$(EXE)
endif

C_TESTS+=tests/test_unicode_provider$(EXE)

C_TESTS+=tests/test_unicode_initialization$(EXE)

ifeq ($(CONFIG_ICU),y)
C_TESTS+=tests/test_date_zone_order$(EXE)
endif

ifeq ($(CONFIG_ICU)$(CONFIG_INTL)$(CONFIG_TEMPORAL),ynn)
C_TESTS+=tests/test_date_icu_core_only$(EXE)
endif

ifeq ($(CONFIG_ICU),y)
C_TESTS+=tests/test_icu_version_contract$(EXE)
endif

# Compile test binaries before running the repository test groups.
.PHONY: test-binaries
test-binaries: $(C_TESTS) regexp_test$(EXE) qjs$(EXE) run-test262$(EXE)
ifdef CONFIG_SHARED_LIBS
test-binaries: tests/bjson.so examples/point.so
endif

ifeq ($(CONFIG_ICU)$(CONFIG_INTL)$(CONFIG_TEMPORAL),ynn)
test-c test-binaries: tests/run_temporal_zone_test$(EXE)
endif

.PHONY: test-c
test-c: $(C_TESTS)
ifeq ($(CONFIG_ICU),y)
	$(WINE) ./tests/test_icu_version_contract$(EXE)
endif
ifeq ($(CONFIG_ICU)$(CONFIG_INTL)$(CONFIG_TEMPORAL),ynn)
	$(WINE) ./tests/run_temporal_zone_test$(EXE) UTC ./tests/test_date_icu_core_only$(EXE)
endif
ifeq ($(CONFIG_ICU),y)
	$(WINE) ./tests/test_date_zone_order$(EXE)
endif
	$(WINE) ./tests/test_unicode_initialization$(EXE)
	$(WINE) ./tests/test_unicode_provider$(EXE)
ifeq ($(CONFIG_INTL)$(CONFIG_ICU),yy)
	$(WINE) ./tests/test_intl_oom$(EXE)
endif
	$(WINE) ./tests/test_intl_services_api$(EXE)
	$(WINE) ./tests/test_intl_duration_format_embed$(EXE)
	$(WINE) ./tests/test_intl_number_format_embed$(EXE)
	$(WINE) ./tests/test_intl_collator_segmenter$(EXE)
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_locale_info$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_date_frontend_oom$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_date_bank_data$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_timezone_runtime$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_display_oom$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_duration_oom$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_relative_oom$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_plural_oom$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_number_oom$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_segmenter_oom$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_collator_oom$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_general_oom$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_frontend_oom$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_duration_provider$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_plural_provider$(EXE)
endif
ifeq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_native_number_provider$(EXE)
endif
ifeq ($(CONFIG_INTL),y)
ifneq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_text$(EXE)
endif
endif
ifeq ($(CONFIG_INTL),y)
ifneq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_embedder$(EXE)
endif
endif
ifeq ($(CONFIG_INTL),y)
ifneq ($(CONFIG_INTL_NATIVE),y)
	$(WINE) ./tests/test_intl_locale_lookup$(EXE)
endif
endif
	$(WINE) ./tests/test_intl_default_locale_api$(EXE)
	$(WINE) ./tests/test_intl_receiver_api$(EXE)
	$(WINE) ./tests/test_intl_native_timezone$(EXE)
	$(WINE) ./tests/test_intl_calendar_domain$(EXE)
	$(WINE) ./tests/test_intl_native_duration_engine$(EXE)
	$(WINE) ./tests/test_intl_native_number_engine$(EXE)
	$(WINE) ./tests/test_intl_native_list_data$(EXE)
	$(WINE) ./tests/test_intl_native_list_engine$(EXE)
	$(WINE) ./tests/test_intl_native_locale_id$(EXE)
	$(WINE) ./tests/test_intl_data_bundle$(EXE)
	$(WINE) ./tests/test_intl_data_reader$(EXE)
	$(WINE) ./tests/test_intl_date_pattern$(EXE)
	$(WINE) ./tests/test_intl_number_grammar$(EXE)
	$(WINE) ./tests/test_fuzz_regexp_timeout$(EXE)
	$(WINE) ./tests/test_fuzz_allocations$(EXE)
	$(WINE) ./tests/test_fuzz_exception_ownership$(EXE)
	$(WINE) ./tests/test_fuzz_json$(EXE)
	$(WINE) ./tests/test_allocator$(EXE)
	$(WINE) ./tests/test_iterator$(EXE)
	$(WINE) ./tests/test_allocator_policy$(EXE)
	$(WINE) ./tests/test_qjsc_json_preload$(EXE)
	$(WINE) ./tests/test_qjsc_json_probe$(EXE)
	$(PYTHON) tests/run_qjsc_json_attributes.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_json_exec$(EXE)" qjsc-json-attribute-payload-ok
	$(PYTHON) tests/run_qjsc_json_attributes.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_native_std_exec$(EXE)" qjsc-native-std-ok
	$(PYTHON) tests/run_qjsc_json_collisions.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_json_collision_attributes-first$(EXE)" qjsc-json-collision-ok
	$(PYTHON) tests/run_qjsc_json_collisions.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_json_collision_attributes-last$(EXE)" qjsc-json-collision-ok
	$(PYTHON) tests/run_qjsc_json_collisions.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_json_collision_size-first$(EXE)" qjsc-json-collision-ok
	$(PYTHON) tests/run_qjsc_json_collisions.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_json_collision_size-last$(EXE)" qjsc-json-collision-ok
	$(WINE) ./tests/test_qjsc_context_failures$(EXE)
	$(WINE) ./tests/test_atomics_wait$(EXE)
	$(WINE) ./tests/test_worker_context_failure$(EXE)
	$(WINE) ./tests/test_wait_async$(EXE)
	$(WINE) ./tests/test_native_jobs$(EXE)
	$(WINE) ./tests/test_wait_queue$(EXE)
	$(WINE) ./tests/test_timezone$(EXE)
	$(WINE) ./tests/test_timezone_system_reader$(EXE)
	$(WINE) ./tests/test_tzif_decoder$(EXE)
	$(WINE) ./tests/test_windows_zones$(EXE)
ifdef CONFIG_WIN32
	$(WINE) ./tests/test_windows_zones_system$(EXE)
endif
	$(WINE) ./tests/test_temporal_epoch$(EXE)
	$(WINE) ./tests/test_temporal_date_math$(EXE)
	$(WINE) ./tests/test_temporal$(EXE)
	$(WINE) ./tests/test_temporal_format$(EXE)
	$(WINE) ./tests/test_temporal_duration_math$(EXE)
	$(WINE) ./tests/test_temporal_api$(EXE)
ifeq ($(CONFIG_TEMPORAL),y)
	$(WINE) ./tests/test_temporal_runtime_zone_provider$(EXE)
	$(WINE) ./tests/test_temporal_system_clock$(EXE)
endif
	$(WINE) ./tests/test_default_time_zone_api$(EXE)
	$(WINE) ./tests/test_intl_native_plural$(EXE)
	$(WINE) ./tests/test_intl_plural$(EXE)
	$(WINE) ./tests/test_intl_decimal$(EXE)
	$(WINE) ./tests/test_intl_native_relative_decimal$(EXE)
	$(WINE) ./tests/test_temporal_civil$(EXE)
	$(WINE) ./tests/test_temporal_zones$(EXE)
	$(WINE) ./tests/test_temporal_calendars$(EXE)
ifeq ($(CONFIG_ICU),n)
	$(WINE) ./tests/test_temporal_native_calendars$(EXE)
endif
	$(WINE) ./tests/test_calendar_portable$(EXE)
	$(WINE) ./tests/test_calendar_persian_authority$(EXE)
	$(WINE) ./tests/test_calendar_integrated_authority$(EXE)
	$(WINE) ./tests/test_api$(EXE)
	$(WINE) ./tests/test_typed_array$(EXE)
	$(WINE) ./tests/test_bytecode$(EXE)
	$(WINE) ./tests/test_cutils$(EXE)
	$(WINE) ./tests/test_unicode$(EXE)
	$(WINE) ./tests/test_unicode_oom$(EXE)
	$(WINE) ./tests/test_bytecode_trace$(EXE)

.PHONY: test-regexp
test-regexp: regexp_test$(EXE)
	sh tests/test_regexp.sh "$(WINE)" "./regexp_test$(EXE)"

.PHONY: test-run-test262
test-run-test262: run-test262$(EXE)
	sh tests/test_run_test262.sh "$(WINE)" "./run-test262$(EXE)"

.PHONY: test-test262-timings
test-test262-timings:
	$(PYTHON) tests/test_test262_timings.py

test: test-c test-regexp test-build-dependencies test-run-test262 test-test262-timings

.PHONY: test-qjsc-json-preloads
test-qjsc-json-preloads: tests/test_qjsc_json_preload$(EXE)
	$(WINE) ./tests/test_qjsc_json_preload$(EXE)

.PHONY: test-qjsc-json-attributes
test-qjsc-json-attributes: tests/test_qjsc_json_probe$(EXE) tests/test_qjsc_json_exec$(EXE) tests/test_qjsc_native_std_exec$(EXE) tests/test_qjsc_json_preload$(EXE) $(addprefix tests/test_qjsc_json_collision_,$(addsuffix $(EXE),$(QJSC_JSON_COLLISION_CASES)))
	$(WINE) ./tests/test_qjsc_json_preload$(EXE)
	$(WINE) ./tests/test_qjsc_json_probe$(EXE)
	$(PYTHON) tests/run_qjsc_json_attributes.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_json_exec$(EXE)" qjsc-json-attribute-payload-ok
	$(PYTHON) tests/run_qjsc_json_attributes.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_native_std_exec$(EXE)" qjsc-native-std-ok
	$(PYTHON) tests/run_qjsc_json_collisions.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_json_collision_attributes-first$(EXE)" qjsc-json-collision-ok
	$(PYTHON) tests/run_qjsc_json_collisions.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_json_collision_attributes-last$(EXE)" qjsc-json-collision-ok
	$(PYTHON) tests/run_qjsc_json_collisions.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_json_collision_size-first$(EXE)" qjsc-json-collision-ok
	$(PYTHON) tests/run_qjsc_json_collisions.py "$(QJSC_TEST_RUNNER) $(WINE)" "./tests/test_qjsc_json_collision_size-last$(EXE)" qjsc-json-collision-ok

.PHONY: test-qjs-cli
test-qjs-cli: qjs$(EXE)
	sh tests/test_qjs_cli.sh "$(WINE)" "./qjs$(EXE)"

test: test-qjs-cli

.PHONY: test-build-dependencies
test-build-dependencies:
	sh tests/test_build_dependencies.sh "$(MAKE)"

ifdef CONFIG_SHARED_LIBS
test: tests/bjson.so examples/point.so
endif

test: qjs$(EXE) run-test262$(EXE)
	$(WINE) ./qjs$(EXE) tests/test_closure.js
	$(WINE) ./qjs$(EXE) -m tests/test_realm.js
	$(WINE) ./qjs$(EXE) -m tests/test_iterator_realms.js
	$(WINE) ./qjs$(EXE) -m tests/test_disposal_realms.js
ifeq ($(CONFIG_ICU),y)
	unicode_args=$$($(WINE) ./tests/test_unicode_version$(EXE) --test-args) && \
	$(WINE) ./qjs$(EXE) tests/test_language.js "$$unicode_args"
else
	$(WINE) ./qjs$(EXE) tests/test_language.js
endif
	$(WINE) ./qjs$(EXE) -m tests/test_module.js
	$(WINE) ./qjs$(EXE) -m tests/test_module_names.js
ifeq ($(CONFIG_ICU),y)
	unicode_args=$$($(WINE) ./tests/test_unicode_version$(EXE) --test-args) && \
	$(WINE) ./qjs$(EXE) --std tests/test_builtin.js "$$unicode_args"
else
	$(WINE) ./qjs$(EXE) --std tests/test_builtin.js
endif
	$(WINE) ./qjs$(EXE) tests/test_unicode_property_grammar.js
	$(WINE) ./qjs$(EXE) tests/test_date_parse_boundaries.js
	$(WINE) ./qjs$(EXE) tests/test_date_setter_snapshot.js
	$(WINE) ./qjs$(EXE) tests/test_loop.js
	$(WINE) ./qjs$(EXE) tests/test_bigint.js
	$(WINE) ./qjs$(EXE) -m tests/test_async.js
	$(WINE) ./qjs$(EXE) -m tests/test_wait_async.js
	$(WINE) ./run-test262$(EXE) -N tests/test_wait_async_agents.js
	$(WINE) ./qjs$(EXE) tests/test_cyclic_import.js
	$(WINE) ./qjs$(EXE) tests/test_worker.js
ifdef CONFIG_WIN32
	$(WINE) ./qjs$(EXE) -m tests/test_wait_async_worker_overflow.js
endif
	$(WINE) ./qjs$(EXE) tests/test_gsab_worker.js
ifndef CONFIG_WIN32
	$(WINE) ./qjs$(EXE) tests/test_std.js
	$(WINE) ./qjs$(EXE) tests/test_rw_handler.js
endif
ifdef CONFIG_SHARED_LIBS
	$(WINE) ./qjs$(EXE) tests/test_bjson.js
	$(WINE) ./qjs$(EXE) examples/test_point.js
endif

stats: qjs$(EXE)
	$(WINE) ./qjs$(EXE) -qd

microbench: qjs$(EXE)
	$(WINE) ./qjs$(EXE) --std tests/microbench.js

ifeq ($(wildcard test262/features.txt),)
test2-bootstrap:
	git clone --single-branch --shallow-since=$(TEST262_SINCE) https://github.com/tc39/test262.git
	(cd test262 && git checkout -q $(TEST262_COMMIT) && \
	 patch -p1 < ../tests/test262.patch && \
	 if test "$(TEST262_UNICODE18_OVERLAY)" = y; then patch -p1 < ../tests/test262-unicode18.patch; fi && cd ..)
else
test2-bootstrap:
	(cd test262 && git fetch && git reset --hard $(TEST262_COMMIT) && \
	 patch -p1 < ../tests/test262.patch && \
	 if test "$(TEST262_UNICODE18_OVERLAY)" = y; then patch -p1 < ../tests/test262-unicode18.patch; fi && cd ..)
endif

# Use every available processor; callers can choose a smaller worker count.
TEST262_THREADS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
TEST262_TIMING_LOG ?=
TEST262_TIMING_OPTION = $(if $(TEST262_TIMING_LOG),--timing-log "$(TEST262_TIMING_LOG)")

ifeq ($(wildcard test262o/tests.txt),)
test2o test2o-update:
	@echo test262o tests not installed
else
# ES5 tests (obsolete)
test2o: run-test262
	time ./run-test262 -T $(TEST262_THREADS) $(TEST262_TIMING_OPTION) -t -m -c test262o.conf

test2o-update: run-test262
	./run-test262 -T $(TEST262_THREADS) $(TEST262_TIMING_OPTION) -t -u -c test262o.conf
endif

TEST262_CONFIG?=$(if $(filter y,$(CONFIG_INTL)),test262-intl.conf,test262.conf)
TEST262_FEATURE_CONFIG=$(basename $(TEST262_CONFIG))_intl-$(if $(filter y,$(CONFIG_INTL)),y,n)_temporal-$(if $(filter y,$(CONFIG_TEMPORAL)),y,n).txt
$(TEST262_FEATURE_CONFIG): $(TEST262_CONFIG) tools/ci/test262-config.py
	$(PYTHON) tools/ci/test262-config.py --intl "$(if $(filter y,$(CONFIG_INTL)),y,n)" --temporal "$(if $(filter y,$(CONFIG_TEMPORAL)),y,n)" --source "$<" --output "$@" --preserve-reportfile
.PHONY: test2-config
test2-config: $(TEST262_FEATURE_CONFIG)

ifeq ($(wildcard test262/features.txt),)
test2 test2-update test2-default test2-check:
	@echo test262 tests not installed
else
# Test262 tests
test2-default: run-test262 $(TEST262_FEATURE_CONFIG)
	time ./run-test262 -T $(TEST262_THREADS) $(TEST262_TIMING_OPTION) -t -m -c "$(TEST262_FEATURE_CONFIG)"

test2: run-test262 $(TEST262_FEATURE_CONFIG)
	time ./run-test262 -T $(TEST262_THREADS) $(TEST262_TIMING_OPTION) -t -m -c "$(TEST262_FEATURE_CONFIG)" -a

test2-update: run-test262 $(TEST262_FEATURE_CONFIG)
	./run-test262 -T $(TEST262_THREADS) $(TEST262_TIMING_OPTION) -t -u -c "$(TEST262_FEATURE_CONFIG)" -a

test2-check: run-test262 $(TEST262_FEATURE_CONFIG)
	time ./run-test262 -T $(TEST262_THREADS) $(TEST262_TIMING_OPTION) -t -m -c "$(TEST262_FEATURE_CONFIG)" -E -a
endif

testall: all test microbench test2o test2

testall-complete: testall

node-test:
	node tests/test_closure.js
	node tests/test_language.js
	node tests/test_builtin.js
	node tests/test_loop.js
	node tests/test_bigint.js

node-microbench:
	node tests/microbench.js -s microbench-node.txt
	node --jitless tests/microbench.js -s microbench-node-jitless.txt

bench-v8: qjs
	make -C tests/bench-v8
	./qjs -d tests/bench-v8/combined.js

node-bench-v8:
	make -C tests/bench-v8
	node --jitless tests/bench-v8/combined.js

tests/bjson.so: $(OBJDIR)/tests/bjson.pic.o
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(LIBS)

BENCHMARKDIR=../quickjs-benchmarks

run_sunspider_like: $(BENCHMARKDIR)/run_sunspider_like.c
	$(CC) $(CFLAGS) $(LDFLAGS) -DNO_INCLUDE_DIR -I. -o $@ $< libquickjs$(LTOEXT).a $(LIBS)

run_octane: $(BENCHMARKDIR)/run_octane.c
	$(CC) $(CFLAGS) $(LDFLAGS) -DNO_INCLUDE_DIR -I. -o $@ $< libquickjs$(LTOEXT).a $(LIBS)

benchmarks: run_sunspider_like run_octane
	./run_sunspider_like $(BENCHMARKDIR)/kraken-1.0/
	./run_sunspider_like $(BENCHMARKDIR)/kraken-1.1/
	./run_sunspider_like $(BENCHMARKDIR)/sunspider-1.0/
	./run_octane $(BENCHMARKDIR)/

-include $(shell find $(OBJDIR) -name '*.d' -print 2>/dev/null)

# Generated aliases preserve upstream qjsc source-tree header discovery.
quickjs.h: include/quickjs.h
	ln -sf include/quickjs.h $@

quickjs-libc.h: include/quickjs-libc.h
	ln -sf include/quickjs-libc.h $@

qjsc$(EXE) $(QJSC): | quickjs.h quickjs-libc.h

ifneq ($(wildcard fuzz/fuzz_common.c),)
$(OBJDIR)/fuzz/fuzz_common.o: fuzz/fuzz_common.c fuzz/fuzz_common.h | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -I. -c -o $@ $<

$(OBJDIR)/tests/test_fuzz_support.o: tests/test_fuzz_support.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -I. -c -o $@ $<

tests/test_fuzz_support$(EXE): $(OBJDIR)/tests/test_fuzz_support.o $(OBJDIR)/fuzz/fuzz_common.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: test-fuzz-support
test-fuzz-support: tests/test_fuzz_support$(EXE)
	$(WINE) ./tests/test_fuzz_support$(EXE)

test: test-fuzz-support
endif

clean-fuzz-test:
	rm -f tests/test_fuzz_support tests/test_fuzz_support.exe

.PHONY: clean-fuzz-test
clean: clean-fuzz-test

# Exercise qjsc's own compiler invocation for native targets.
PYTHON ?= python3
ifeq ($(CROSS_PREFIX),)
ifneq ($(CONFIG_WIN32),y)
.PHONY: test-qjsc-executable
test: test-qjsc-executable

test-qjsc-executable: $(QJSC) libquickjs$(LTOEXT).a
	$(PYTHON) tests/test_qjsc_executable.py "$(QJSC)" "" "$(filter -fsanitize=% -m32,$(CFLAGS))" "$(CONFIG_INTL)" "$(CONFIG_TEMPORAL)" "$(if $(CONFIG_LTO),$(LTO_FLAG))"
endif
endif

tests/test_fuzz_json$(EXE): $(OBJDIR)/tests/test_fuzz_json.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_fuzz_exception_ownership$(EXE): $(OBJDIR)/tests/test_fuzz_exception_ownership.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/tests/test_fuzz_allocations.o: tests/test_fuzz_allocations.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -I. -c -o $@ $<

tests/test_fuzz_allocations$(EXE): $(OBJDIR)/tests/test_fuzz_allocations.o $(OBJDIR)/fuzz/fuzz_common.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_fuzz_regexp_timeout$(EXE): $(OBJDIR)/tests/test_fuzz_regexp_timeout.o $(REGEXP_OBJS) $(OBJDIR)/src/unicode/libunicode.o $(OBJDIR)/src/cutils/cutils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Native Temporal arithmetic is tested before frontend activation.
TEMPORAL_ARITHMETIC_SRCS=src/temporal/epoch.c src/temporal/options.c
TEMPORAL_ARITHMETIC_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(TEMPORAL_ARITHMETIC_SRCS))

tests/test_temporal_epoch$(EXE): $(OBJDIR)/tests/test_temporal_epoch.o $(TEMPORAL_ARITHMETIC_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

TEMPORAL_CIVIL_SRCS=src/temporal/iso.c src/temporal/civil.c
TEMPORAL_CIVIL_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(TEMPORAL_CIVIL_SRCS))

tests/test_temporal_date_math$(EXE): $(OBJDIR)/tests/test_temporal_date_math.o $(TEMPORAL_ARITHMETIC_OBJS) $(TEMPORAL_CIVIL_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

TEMPORAL_PARSE_SRCS=src/temporal/parse.c
TEMPORAL_PARSE_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(TEMPORAL_PARSE_SRCS))

tests/test_temporal$(EXE): $(OBJDIR)/tests/test_temporal.o $(TEMPORAL_ARITHMETIC_OBJS) $(TEMPORAL_CIVIL_OBJS) $(TEMPORAL_PARSE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

TEMPORAL_FORMAT_SRCS=src/temporal/format.c
TEMPORAL_FORMAT_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(TEMPORAL_FORMAT_SRCS))

tests/test_temporal_format$(EXE): $(OBJDIR)/tests/test_temporal_format.o $(TEMPORAL_ARITHMETIC_OBJS) $(TEMPORAL_CIVIL_OBJS) $(TEMPORAL_FORMAT_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

TEMPORAL_DURATION_SRCS=src/temporal/duration.c src/temporal/time.c
TEMPORAL_DURATION_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(TEMPORAL_DURATION_SRCS))

tests/test_temporal_duration_math$(EXE): $(OBJDIR)/tests/test_temporal_duration_math.o $(TEMPORAL_ARITHMETIC_OBJS) $(TEMPORAL_CIVIL_OBJS) $(TEMPORAL_PARSE_OBJS) $(TEMPORAL_DURATION_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Native calendar tests consume the same linked library provider.
CALENDAR_TEST_OBJECTS=$(patsubst %.c,$(OBJDIR)/%.o,$(CALENDAR_TEST_SRCS))
$(CALENDAR_TEST_OBJECTS): CALENDAR_COMPILE_CFLAGS=$(CALENDAR_AUTHORITY_CFLAGS)
tests/test_calendar_portable$(EXE): $(OBJDIR)/tests/test_calendar_portable.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
tests/test_calendar_persian_authority$(EXE): $(OBJDIR)/tests/test_calendar_persian_authority.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
tests/test_calendar_integrated_authority$(EXE): $(OBJDIR)/tests/test_calendar_integrated_authority.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

TEMPORAL_CALENDAR_SRCS=src/temporal/calendar.c
TEMPORAL_CALENDAR_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(TEMPORAL_CALENDAR_SRCS))
tests/test_temporal_calendars$(EXE): $(OBJDIR)/tests/test_temporal_calendars.o $(TEMPORAL_ARITHMETIC_OBJS) $(TEMPORAL_CIVIL_OBJS) $(TEMPORAL_CALENDAR_OBJS) libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
tests/test_temporal_native_calendars$(EXE): $(OBJDIR)/tests/test_temporal_native_calendars.o $(TEMPORAL_ARITHMETIC_OBJS) $(TEMPORAL_CIVIL_OBJS) $(TEMPORAL_CALENDAR_OBJS) libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_tzif_decoder$(EXE): $(OBJDIR)/tests/test_tzif_decoder.o $(OBJDIR)/src/timezone/tzif.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Host-zic creates temporary per-zone files and packs one final native binary.
HOST_ZIC?=$(OBJDIR)/host-zic$(HOST_EXE)
HOST_ZIC_CFLAGS?=-O2
TZ_SOURCE_DIR=third_party/tz
TZ_GENERATED_DIR=$(OBJDIR)/src/timezone
TZ_PIN_INPUTS=$(addprefix $(TZ_SOURCE_DIR)/,africa antarctica asia australasia europe northamerica southamerica etcetera factory backward backzone zone.tab ziguard.awk zic.c private.h tzfile.h)

$(TZ_GENERATED_DIR)/tzdir.h: tools/timezone/headers.py
	$(PYTHON) $< $(TZ_GENERATED_DIR) --header tzdir.h
$(TZ_GENERATED_DIR)/version.h: tools/timezone/headers.py
	$(PYTHON) $< $(TZ_GENERATED_DIR) --header version.h

$(HOST_ZIC): tools/timezone/host-zic.c $(TZ_SOURCE_DIR)/zic.c $(TZ_SOURCE_DIR)/private.h $(TZ_SOURCE_DIR)/tzfile.h $(TZ_GENERATED_DIR)/tzdir.h $(TZ_GENERATED_DIR)/version.h
	$(HOST_CC) $(HOST_ZIC_CFLAGS) -I$(TZ_GENERATED_DIR) -o $@ $<

$(TZ_GENERATED_DIR)/timezone-data.bin: tools/timezone/generate.py tools/timezone/bundle.py tools/timezone/primary.py tools/timezone/pin.json tools/timezone/cldr/timezone.xml tools/timezone/cldr/timezone-pin.json $(TZ_PIN_INPUTS) $(HOST_ZIC)
	$(PYTHON) $< --tz $(TZ_SOURCE_DIR) --zic $(HOST_ZIC) --output $@ --manifest $(TZ_GENERATED_DIR)/timezone-data.json

$(TZ_GENERATED_DIR)/timezone-data.inc: tools/timezone/embed.py tools/timezone/bundle.py $(TZ_GENERATED_DIR)/timezone-data.bin
	$(PYTHON) $< --input $(TZ_GENERATED_DIR)/timezone-data.bin --output $@


.PHONY: test-timezone-generator
test-timezone-generator:
	$(PYTHON) tests/test_timezone_generator.py --tz-source $(TZ_SOURCE_DIR)

test: test-timezone-generator

$(OBJDIR)/tests/test_windows_zones.o: tests/test_windows_zones.c src/timezone/windows-zone.c src/timezone/windows-zone.h src/timezone/windows-zone-data.inc
tests/test_windows_zones$(EXE): $(OBJDIR)/tests/test_windows_zones.o
	$(CC) $(LDFLAGS) -o $@ $^
ifdef CONFIG_WIN32
tests/test_windows_zones_system$(EXE): $(OBJDIR)/tests/test_windows_zones_system.o $(OBJDIR)/src/timezone/windows-zone.o
	$(CC) $(LDFLAGS) -o $@ $^
endif
.PHONY: test-windows-zones-generator
test-windows-zones-generator:
	$(PYTHON) tests/test_windows_zones_generator.py --tz-source $(TZ_SOURCE_DIR) --tz-pin tools/timezone/pin.json

test: test-windows-zones-generator

TIMEZONE_ALL_OBJECTS=$(foreach suffix,o pic.o nolto.o debug.o fuzz.o check.o host.o,$(patsubst %.c,$(OBJDIR)/%.$(suffix),$(TIMEZONE_SRCS)))
$(TIMEZONE_ALL_OBJECTS): TIMEZONE_COMPILE_CFLAGS=-I$(TZ_GENERATED_DIR)
TIMEZONE_EMBEDDED_OBJECTS=$(foreach suffix,o pic.o nolto.o debug.o fuzz.o check.o host.o,$(OBJDIR)/src/timezone/embedded.$(suffix))
$(TIMEZONE_EMBEDDED_OBJECTS): $(TZ_GENERATED_DIR)/timezone-data.inc
ifdef CONFIG_WIN32
$(foreach suffix,o pic.o nolto.o debug.o fuzz.o check.o host.o,$(OBJDIR)/src/timezone/windows-zone.$(suffix)): src/timezone/windows-zone.h src/timezone/windows-zone-data.inc
endif

libqjstimezone.a: $(TIMEZONE_OBJS)
	rm -f $@
	$(AR) rcs $@ $^

tests/test_timezone$(EXE): $(OBJDIR)/tests/test_timezone.o libqjstimezone.a
	$(CC) $(LDFLAGS) -o $@ $^ -lm

$(OBJDIR)/tests/test_timezone_system_reader.o: src/timezone/timezone.c src/timezone/private.h src/timezone/timezone.h
tests/test_timezone_system_reader$(EXE): $(OBJDIR)/tests/test_timezone_system_reader.o $(OBJDIR)/src/timezone/tzif.o $(OBJDIR)/src/timezone/embedded.o $(filter %/windows-zone.o,$(TIMEZONE_OBJS))
	$(CC) $(LDFLAGS) -o $@ $^ -lm

TEMPORAL_TIMEZONE_SRCS=src/temporal/time-zone.c
TEMPORAL_TIMEZONE_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(TEMPORAL_TIMEZONE_SRCS))
tests/test_temporal_zones$(EXE): $(OBJDIR)/tests/test_temporal_zones.o $(TEMPORAL_ARITHMETIC_OBJS) $(TEMPORAL_CIVIL_OBJS) $(TEMPORAL_PARSE_OBJS) $(TEMPORAL_FORMAT_OBJS) $(TEMPORAL_TIMEZONE_OBJS) libqjstimezone.a $(if $(filter y,$(CONFIG_ICU)),libquickjs$(LTOEXT).a)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

TEMPORAL_RELATIVE_SRCS=src/temporal/relative.c
TEMPORAL_RELATIVE_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(TEMPORAL_RELATIVE_SRCS))
tests/test_temporal_civil$(EXE): $(OBJDIR)/tests/test_temporal_civil.o $(TEMPORAL_ARITHMETIC_OBJS) $(TEMPORAL_CIVIL_OBJS) $(TEMPORAL_PARSE_OBJS) $(TEMPORAL_FORMAT_OBJS) $(TEMPORAL_DURATION_OBJS) $(TEMPORAL_CALENDAR_OBJS) $(TEMPORAL_TIMEZONE_OBJS) $(TEMPORAL_RELATIVE_OBJS) libquickjs$(LTOEXT).a libqjstimezone.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_default_time_zone_api$(EXE): $(OBJDIR)/tests/test_default_time_zone_api.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_temporal_api$(EXE): $(OBJDIR)/tests/test_temporal_api.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

intl_shell_quote = '$(subst ','"'"',$(1))'
intl_build_config_args = \
    $(call intl_shell_quote,$(CONFIG_TEMPORAL)) \
    $(call intl_shell_quote,$(CONFIG_ICU)) \
    $(call intl_shell_quote,$(CONFIG_INTL)) \
    $(call intl_shell_quote,$(CONFIG_INTL_LEGACY)) \
    $(call intl_shell_quote,$(CONFIG_INTL_BACKEND)) \
    $(call intl_shell_quote,$(CONFIG_INTL_NATIVE)) \
    $(call intl_shell_quote,$(ICU_CFLAGS)) \
    $(call intl_shell_quote,$(ICU_LIBS)) \
    $(call intl_shell_quote,$(HOST_ICU_CFLAGS)) \
    $(call intl_shell_quote,$(HOST_ICU_LIBS)) \
    $(call intl_shell_quote,$(LIBS)) \
    $(call intl_shell_quote,$(PREFIX)) \
    $(call intl_shell_quote,$(shell cat VERSION)) \
    $(call intl_shell_quote,$(DEFINES))
# A changed configuration must invalidate consumers even when Make or
# the filesystem cannot distinguish the stamp and output timestamps.
ifneq ($(shell printf '%s\n' $(intl_build_config_args) | cmp -s - $(OBJDIR)/temporal-build-config || printf changed),)
.PHONY: $(OBJDIR)/temporal-build-config
endif
.PHONY: FORCE_TEMPORAL_BUILD_CONFIG
$(OBJDIR)/temporal-build-config: FORCE_TEMPORAL_BUILD_CONFIG
	@mkdir -p $(@D)
	@printf '%s\n' $(intl_build_config_args) > $@.tmp
	@if ! cmp -s $@.tmp $@; then mv $@.tmp $@; else rm $@.tmp; fi

.PHONY: test-temporal-config
test-temporal-config:
	$(PYTHON) tests/test_temporal_config.py
test: test-temporal-config

ifeq ($(CONFIG_TEMPORAL),y)
ifneq ($(CONFIG_ICU),y)
# Exact release-data transition boundaries belong to the native zone provider.
.PHONY: test-temporal-native-zone-boundaries
test-temporal-native-zone-boundaries: qjs$(EXE)
	$(WINE) ./qjs$(EXE) tests/test_temporal_native_zones.js
test: test-temporal-native-zone-boundaries

endif

.PHONY: test-temporal-js
test-temporal-js: qjs$(EXE) tests/run_temporal_zone_test$(EXE)
	$(WINE) ./qjs$(EXE) tests/test_temporal_instant.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_duration.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_plain_time.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_plain.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_zoned.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_zoned_native.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_now.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_plain_date_duration_balance.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_year_month_date_boundaries.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_calendar_identifier_boundaries.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_zoned_repeated_day.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_zero_zoned_duration_boundaries.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_offset_wall_date_boundaries.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_calendar_zones.js
ifeq ($(CONFIG_ICU),n)
	$(WINE) ./qjs$(EXE) tests/test_temporal_native_calendars.js
endif
	$(WINE) ./qjs$(EXE) -m tests/test_temporal_realm.js
	$(WINE) ./qjs$(EXE) tests/test_date_temporal_bridge.js
	$(WINE) ./tests/run_temporal_zone_test$(EXE) UTC ./qjs$(EXE) tests/test_temporal_date_provider.js
	$(WINE) ./tests/run_temporal_zone_test$(EXE) America/New_York ./qjs$(EXE) tests/test_date_temporal_time_zone.js
	$(WINE) ./tests/run_temporal_zone_test$(EXE) US/Eastern ./qjs$(EXE) tests/test_date_temporal_time_zone.js
test: test-temporal-js
endif

ifeq ($(CONFIG_TEMPORAL),y)
$(OBJDIR)/tests/time-zone-provider-backend.o: src/temporal/time-zone.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -Dqjs_temporal_system_zone=qjs_temporal_system_zone_test_backend -c -o $@ $<
$(OBJDIR)/tests/time-zone-clock-backend.o: src/temporal/time-zone.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -Dqjs_temporal_system_epoch=qjs_temporal_system_epoch_test_backend -c -o $@ $<
tests/test_temporal_runtime_zone_provider$(EXE): $(OBJDIR)/tests/test_temporal_runtime_zone_provider.o $(OBJDIR)/tests/time-zone-provider-backend.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
tests/test_temporal_system_clock$(EXE): $(OBJDIR)/tests/test_temporal_system_clock.o $(OBJDIR)/tests/time-zone-clock-backend.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
endif

# Exact-number preparation is independent of the installed Intl backend.
INTL_DECIMAL_OBJS=$(OBJDIR)/src/intl/decimal.o $(OBJDIR)/src/intl/number-decimal.o
tests/test_intl_native_relative_decimal$(EXE): $(OBJDIR)/tests/test_intl_native_relative_decimal.o $(INTL_DECIMAL_OBJS) $(OBJDIR)/src/dtoa/dtoa.o $(OBJDIR)/src/cutils/cutils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_decimal$(EXE): $(OBJDIR)/tests/test_intl_decimal.o $(OBJDIR)/src/intl/decimal.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_plural$(EXE): $(OBJDIR)/tests/test_intl_plural.o $(OBJDIR)/src/intl/plural.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Set IANA TZ inside a native process; MSYS2 strips it from child environments.
tests/run_temporal_zone_test$(EXE): $(OBJDIR)/tests/run_temporal_zone_test.o
	$(CC) $(LDFLAGS) -o $@ $^
ifeq ($(CONFIG_TEMPORAL),y)
test-binaries: tests/run_temporal_zone_test$(EXE)
endif

tests/test_intl_native_plural$(EXE): $(OBJDIR)/tests/test_intl_native_plural.o $(OBJDIR)/src/intl/plural-native.o $(OBJDIR)/src/intl/plural.o $(OBJDIR)/src/intl/decimal.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_number_grammar$(EXE): $(OBJDIR)/tests/test_intl_number_grammar.o $(OBJDIR)/src/intl/number-range.o $(OBJDIR)/src/intl/number-compact.o $(OBJDIR)/src/intl/number-unit.o $(OBJDIR)/src/intl/data/number-template-validation.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_date_pattern$(EXE): $(OBJDIR)/tests/test_intl_date_pattern.o $(OBJDIR)/src/intl/date-pattern.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Prepare complete validators before linking the generic wire reader.
INTL_DATA_VALIDATION_SRCS= \
    src/intl/collator-native-data.c \
    src/intl/data/date-data-validation.c \
    src/intl/data/duration-data-validation.c \
    src/intl/data/number-data-validation.c \
    src/intl/data/number-extra-validation.c \
    src/intl/data/plural-data-validation.c \
    src/intl/data/relative-data-validation.c
INTL_DATA_VALIDATION_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(INTL_DATA_VALIDATION_SRCS))
test test-binaries: $(INTL_DATA_VALIDATION_OBJS)


INTL_DATA_READER_OBJS= \
    $(OBJDIR)/src/intl/data/native-data-reader.o \
    $(INTL_DATA_VALIDATION_OBJS) \
    $(OBJDIR)/src/intl/plural.o \
    $(OBJDIR)/src/intl/date-pattern.o \
    $(OBJDIR)/src/intl/number-range.o \
    $(OBJDIR)/src/intl/number-unit.o \
    $(OBJDIR)/src/intl/data/number-template-validation.o

tests/test_intl_data_reader$(EXE): $(OBJDIR)/tests/test_intl_data_reader.o $(INTL_DATA_READER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: test-intl-generator
test-intl-generator:
	$(PYTHON) tests/test_intl_generator.py
test: test-intl-generator

tests/test_intl_data_bundle$(EXE): $(OBJDIR)/tests/test_intl_data_bundle.o $(OBJDIR)/src/intl/data/locale-metadata.o $(INTL_DATA_READER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_locale_id$(EXE): $(INTL_DATA_READER_OBJS) $(OBJDIR)/tests/test_intl_native_locale_id.o $(OBJDIR)/src/intl/native-locale-id.o $(OBJDIR)/src/intl/locale-grammar.o $(OBJDIR)/src/intl/data/locale-metadata.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# LocaleInfo preparation shares native ID/reader contracts without providers.
test test-binaries: $(OBJDIR)/src/intl/native-locale-info.o

tests/test_intl_native_list_engine$(EXE): $(OBJDIR)/tests/test_intl_native_list_engine.o $(OBJDIR)/src/intl/list-native.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_list_data$(EXE): $(INTL_DATA_READER_OBJS) $(OBJDIR)/tests/test_intl_native_list_data.o $(OBJDIR)/src/intl/list-native.o $(OBJDIR)/src/intl/list-native-data.o $(OBJDIR)/src/intl/data/locale-metadata.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_number_engine$(EXE): $(OBJDIR)/tests/test_intl_native_number_engine.o $(OBJDIR)/src/intl/number-native.o $(OBJDIR)/src/intl/decimal.o $(OBJDIR)/src/intl/plural-native.o $(OBJDIR)/src/intl/plural.o $(OBJDIR)/src/intl/number-range.o $(OBJDIR)/src/intl/number-compact.o $(OBJDIR)/src/intl/number-unit.o $(OBJDIR)/src/intl/data/number-template-validation.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_duration_engine$(EXE): $(OBJDIR)/tests/test_intl_native_duration_engine.o $(OBJDIR)/src/intl/number-native.o $(OBJDIR)/src/intl/decimal.o $(OBJDIR)/src/intl/plural-native.o $(OBJDIR)/src/intl/plural.o $(OBJDIR)/src/intl/number-range.o $(OBJDIR)/src/intl/number-compact.o $(OBJDIR)/src/intl/number-unit.o $(OBJDIR)/src/intl/data/number-template-validation.o $(OBJDIR)/src/intl/duration-native.o $(OBJDIR)/src/intl/list-native.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Complete native collator sources precede aggregate-provider activation.
test test-binaries: $(OBJDIR)/src/intl/collator-native.o

# Complete native segmenter sources precede aggregate-provider activation.
test test-binaries: $(OBJDIR)/src/intl/segmenter-native.o $(OBJDIR)/src/intl/segmenter-native-data.o $(OBJDIR)/src/intl/segmenter-unicode.o

# Complete native relative sources precede aggregate-provider activation.
test test-binaries: $(OBJDIR)/src/intl/relative-native.o $(OBJDIR)/src/intl/relative-native-data.o $(OBJDIR)/src/intl/plural-native-data.o

# Complete native display sources precede aggregate-provider activation.
test test-binaries: $(OBJDIR)/src/intl/display-names-native.o

# Complete native number-data sources precede aggregate-provider activation.
test test-binaries: $(OBJDIR)/src/intl/number-native-data.o $(OBJDIR)/src/intl/duration-native-data.o

tests/test_intl_calendar_domain$(EXE): $(OBJDIR)/tests/test_intl_calendar_domain.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Native date engine sources precede aggregate-provider activation.
test test-binaries: $(OBJDIR)/src/intl/date-native.o

# Native date data sources precede aggregate-provider activation.
test test-binaries: $(OBJDIR)/src/intl/date-native-bank.o $(OBJDIR)/src/intl/date-native-data.o $(OBJDIR)/src/intl/date-native-calendar.o $(OBJDIR)/src/intl/date-native-calendar-data.o $(OBJDIR)/src/intl/date-native-zone.o

# Native number-plural adapters precede aggregate-provider construction.
test test-binaries: $(OBJDIR)/src/intl/provider-native-number.o $(OBJDIR)/src/intl/provider-native-plural.o

# Native relative adapters precede aggregate-provider construction.
test test-binaries: $(OBJDIR)/src/intl/provider-native-relative.o

# Native duration adapters precede aggregate-provider construction.
test test-binaries: $(OBJDIR)/src/intl/provider-native-duration.o

# Native collator adapters precede aggregate-provider construction.
test test-binaries: $(OBJDIR)/src/intl/provider-native-collator.o

# Native segmenter adapters precede aggregate-provider construction.
test test-binaries: $(OBJDIR)/src/intl/provider-native-segmenter.o

# Native display adapters precede aggregate-provider construction.
test test-binaries: $(OBJDIR)/src/intl/provider-native-display.o

# Native date-general adapters precede aggregate-provider construction.
test test-binaries: $(OBJDIR)/src/intl/provider-native-date.o $(OBJDIR)/src/intl/provider-native-general.o

# Host atomic cache operations are independent of JavaScript atomics.
INTL_CACHE_PROVIDER_OBJECTS=$(foreach suffix,\
    o host.o pic.o nolto.o debug.o fuzz.o check.o,\
    $(OBJDIR)/src/intl/provider-native.$(suffix))
$(INTL_CACHE_PROVIDER_OBJECTS): CFLAGS+=-std=c11
$(INTL_CACHE_PROVIDER_OBJECTS): CFLAGS_OPT+=-std=c11
$(INTL_CACHE_PROVIDER_OBJECTS): CFLAGS_NOLTO+=-std=c11
$(INTL_CACHE_PROVIDER_OBJECTS): CFLAGS_DEBUG+=-std=c11


INTL_NATIVE_LIBRARY_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(INTL_NATIVE_LIBRARY_SRCS))
INTL_NATIVE_DATE_SHARED_OBJS= \
    $(OBJDIR)/src/temporal/epoch.o \
    $(OBJDIR)/src/temporal/iso.o \
    $(OBJDIR)/src/temporal/civil.o

NATIVE_INTL_TEST_UNICODE=$(OBJDIR)/src/unicode/libunicode.native-test.o
$(NATIVE_INTL_TEST_UNICODE): src/unicode/libunicode.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -DCONFIG_UNICODE_NATIVE -c -o $@ $<

tests/test_intl_native_timezone$(EXE): $(OBJDIR)/tests/test_intl_native_timezone.o \
    $(NATIVE_INTL_TEST_UNICODE) \
    $(INTL_NATIVE_LIBRARY_OBJS) $(INTL_NATIVE_DATE_SHARED_OBJS) \
    libquickjs$(LTOEXT).a libqjstimezone.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_receiver_api$(EXE): $(OBJDIR)/tests/test_intl_receiver_api.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_default_locale_api$(EXE): $(OBJDIR)/tests/test_intl_default_locale_api.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)














.PHONY: test-icu-build-config
test-icu-build-config:
	QJS_TEST_MAKE="$(MAKE)" $(PYTHON) tests/test_icu_build_config.py
test: test-icu-build-config















ifeq ($(CONFIG_INTL_NATIVE),y)
.PHONY: test-intl-native-js
test-intl-native-js: qjs$(EXE) run-test262$(EXE)
	$(WINE) ./qjs$(EXE) tests/test_intl_native_frontend.js
	$(WINE) ./qjs$(EXE) tests/test_intl_native_general.js
	$(WINE) ./qjs$(EXE) tests/test_intl_native_collator.js
	$(WINE) ./qjs$(EXE) tests/test_intl_native_segmenter.js
	$(WINE) ./qjs$(EXE) tests/test_intl_native_number.js
	$(WINE) ./qjs$(EXE) tests/test_intl_native_plural.js
	$(WINE) ./qjs$(EXE) tests/test_intl_native_relative.js
	$(WINE) ./qjs$(EXE) tests/test_intl_native_display.js
	$(WINE) ./qjs$(EXE) tests/test_intl_native_duration.js
	$(WINE) ./qjs$(EXE) tests/test_intl_native_date_frontend.js
	$(WINE) ./qjs$(EXE) tests/test_intl_native_date_all_calendars.js
	$(WINE) ./qjs$(EXE) tests/test_intl_native_date_timezone_names.js
test: test-intl-native-js
endif

.PHONY: test-intl-native-selector test-icu-link-metadata
test-intl-native-selector:
	QJS_TEST_MAKE="$(MAKE)" $(PYTHON) tests/test_intl_native_selector.py
test-icu-link-metadata:
	QJS_TEST_MAKE="$(MAKE)" $(PYTHON) tests/test_icu_link_metadata.py
test: test-intl-native-selector test-icu-link-metadata

tests/test_unicode_version$(EXE): $(OBJDIR)/tests/test_unicode_version.o $(OBJDIR)/src/unicode/libunicode.o $(OBJDIR)/src/cutils/cutils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

ifeq ($(CONFIG_ICU),y)
test: tests/test_unicode_version$(EXE)
endif

ifeq ($(CONFIG_ICU),y)
$(OBJDIR)/tests/time-zone-provider-backend.o $(OBJDIR)/tests/time-zone-clock-backend.o: ICU_COMPILE_CFLAGS=$(ICU_CFLAGS)
endif

# The standalone native-provider unit never instantiates the ICU engine.
# Its private data owner and native Unicode owner form a separate test image.
ifeq ($(CONFIG_ICU),y)
$(INTL_CACHE_PROVIDER_OBJECTS): CFLAGS+=-UCONFIG_ICU
$(INTL_CACHE_PROVIDER_OBJECTS): CFLAGS_OPT+=-UCONFIG_ICU
$(INTL_CACHE_PROVIDER_OBJECTS): CFLAGS_NOLTO+=-UCONFIG_ICU
$(INTL_CACHE_PROVIDER_OBJECTS): CFLAGS_DEBUG+=-UCONFIG_ICU
endif

tests/test_intl_locale_lookup$(EXE): $(OBJDIR)/tests/test_intl_locale_lookup.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_embedder$(EXE): $(OBJDIR)/tests/test_intl_embedder.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_text$(EXE): $(OBJDIR)/tests/test_intl_text.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

ifeq ($(CONFIG_INTL),y)
.PHONY: test-intl-bound-realms
test-intl-bound-realms: run-test262$(EXE)
	$(WINE) ./run-test262$(EXE) -N tests/test_intl_bound_function_realms.js
test: test-intl-bound-realms
ifneq ($(CONFIG_INTL_NATIVE),y)
.PHONY: test-intl-icu-locale
test-intl-icu-locale: qjs$(EXE)
	$(WINE) ./qjs$(EXE) tests/test_intl_locale.js
	$(WINE) ./qjs$(EXE) tests/test_intl_locale_resolution.js
	$(WINE) ./qjs$(EXE) tests/test_intl_text.js
	$(WINE) ./qjs$(EXE) tests/test_intl_locale_integration.js
test: test-intl-icu-locale
endif
endif

ifeq ($(CONFIG_INTL),y)
ifneq ($(CONFIG_INTL_NATIVE),y)
.PHONY: test-intl-icu-number
test-intl-icu-number: qjs$(EXE)
ifeq ($(CONFIG_INTL_LEGACY),y)
	$(WINE) ./qjs$(EXE) tests/test_intl_number_format.js
else
	$(WINE) ./qjs$(EXE) --std -e "globalThis.intlLegacyExpected = false; std.loadScript('tests/test_intl_number_format.js');"
endif
	$(WINE) ./qjs$(EXE) tests/test_intl_number_locale_methods.js
	$(WINE) ./qjs$(EXE) tests/test_intl_plural_rules.js
	$(WINE) ./qjs$(EXE) tests/test_intl_relative_time_format.js
test: test-intl-icu-number
endif
endif

ifeq ($(CONFIG_INTL),y)
ifneq ($(CONFIG_INTL_NATIVE),y)
.PHONY: test-intl-icu-text
test-intl-icu-text: qjs$(EXE)
	$(WINE) ./qjs$(EXE) tests/test_intl_collator.js
	$(WINE) ./qjs$(EXE) tests/test_intl_segmenter.js
	$(WINE) ./qjs$(EXE) tests/test_intl_list_format.js
	$(WINE) ./qjs$(EXE) tests/test_intl_display_names.js
test: test-intl-icu-text
endif
endif

ifeq ($(CONFIG_INTL),y)
ifneq ($(CONFIG_INTL_NATIVE),y)
.PHONY: test-intl-icu-duration
test-intl-icu-duration: qjs$(EXE)
	$(WINE) ./qjs$(EXE) tests/test_intl_duration_format.js
ifeq ($(CONFIG_TEMPORAL),y)
	$(WINE) ./qjs$(EXE) tests/test_intl_temporal_duration.js
endif
test: test-intl-icu-duration
endif
endif

ifeq ($(CONFIG_INTL),y)
ifneq ($(CONFIG_INTL_NATIVE),y)
.PHONY: test-intl-icu-date
test-intl-icu-date: qjs$(EXE)
	$(WINE) ./qjs$(EXE) tests/test_intl_date_time.js
	$(WINE) ./qjs$(EXE) tests/test_intl_date_time_hour_cycle.js
	$(WINE) ./qjs$(EXE) tests/test_intl_date_time_numbering_parts.js
ifeq ($(CONFIG_INTL_LEGACY),y)
	$(WINE) ./qjs$(EXE) tests/test_intl_date_time_legacy.js
endif
	$(WINE) ./qjs$(EXE) tests/test_intl_era_monthcode_calendars.js
ifeq ($(CONFIG_TEMPORAL),y)
	$(WINE) ./qjs$(EXE) tests/test_temporal_icu_calendar_target.js
endif
ifeq ($(CONFIG_TEMPORAL),y)
	$(WINE) ./qjs$(EXE) tests/test_temporal_icu_monthday_year.js
endif
ifeq ($(CONFIG_TEMPORAL),y)
	$(WINE) ./qjs$(EXE) tests/test_intl_temporal_style_preservation.js
endif
ifeq ($(CONFIG_TEMPORAL),y)
	$(WINE) ./qjs$(EXE) tests/test_temporal_intl.js
endif
ifeq ($(CONFIG_TEMPORAL),y)
	$(WINE) ./qjs$(EXE) tests/test_temporal_zoned_intl.js
endif
test: test-intl-icu-date
endif
endif

tests/test_intl_native_number_provider$(EXE): $(OBJDIR)/tests/test_intl_native_number_provider.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_plural_provider$(EXE): $(OBJDIR)/tests/test_intl_native_plural_provider.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_duration_provider$(EXE): $(OBJDIR)/tests/test_intl_native_duration_provider.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_frontend_oom$(EXE): $(OBJDIR)/tests/test_intl_native_frontend_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_general_oom$(EXE): $(OBJDIR)/tests/test_intl_native_general_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_collator_oom$(EXE): $(OBJDIR)/tests/test_intl_native_collator_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_segmenter_oom$(EXE): $(OBJDIR)/tests/test_intl_native_segmenter_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_number_oom$(EXE): $(OBJDIR)/tests/test_intl_native_number_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_plural_oom$(EXE): $(OBJDIR)/tests/test_intl_native_plural_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_relative_oom$(EXE): $(OBJDIR)/tests/test_intl_native_relative_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_duration_oom$(EXE): $(OBJDIR)/tests/test_intl_native_duration_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_display_oom$(EXE): $(OBJDIR)/tests/test_intl_native_display_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_timezone_runtime$(EXE): $(OBJDIR)/tests/test_intl_native_timezone_runtime.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_date_bank_data$(EXE): $(OBJDIR)/tests/test_intl_native_date_bank_data.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_native_date_frontend_oom$(EXE): $(OBJDIR)/tests/test_intl_native_date_frontend_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/tests/test_intl_native_date_bank_data.o: CFLAGS_OPT+=-std=c11


# Counted provider/data copies are confined to seven fresh test processes.
ifeq ($(CONFIG_INTL),y)
ifeq ($(CONFIG_INTL_NATIVE),y)
INTL_CACHE_TEST=tests/test_intl_native_compiled_cache$(EXE)
INTL_CACHE_TEST_OBJECT=$(OBJDIR)/tests/test_intl_native_compiled_cache.o
INTL_CACHE_TEST_PROVIDER=$(OBJDIR)/src/intl/provider-native.cache-test.o
INTL_CACHE_TEST_NUMBER=$(OBJDIR)/src/intl/number-native-data.cache-test.o
$(INTL_CACHE_TEST_OBJECT): CFLAGS_OPT+=-std=c11

# The counted provider replaces the archive's provider for this test.
$(INTL_CACHE_TEST_PROVIDER): src/intl/provider-native.c \
                            $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) \
	  $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) \
	  $(DEPFLAGS) -std=c11 \
	  -Dqjs_intl_data_open=qjs_intl_counted_data_open -c -o $@ $<

# Count only the generic adapter's whole-table scans. The real reader and
# provider certificate continue to use their ordinary validation functions.
$(INTL_CACHE_TEST_NUMBER): src/intl/number-native-data.c \
                          $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) \
	  $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) \
	  $(DEPFLAGS) \
	  -Dqjs_intl_number_extra_data_validate=qjs_intl_counted_number_validate \
	  -c -o $@ $<

$(INTL_CACHE_TEST): $(INTL_CACHE_TEST_OBJECT) \
                   $(INTL_CACHE_TEST_PROVIDER) $(INTL_CACHE_TEST_NUMBER) \
                   libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: test-intl-native-compiled-cache
test-intl-native-compiled-cache: $(INTL_CACHE_TEST)
	$(WINE) ./$(INTL_CACHE_TEST) retry
	$(WINE) ./$(INTL_CACHE_TEST) provider-failure
	$(WINE) ./$(INTL_CACHE_TEST) concurrent
	$(WINE) ./$(INTL_CACHE_TEST) mutable
	$(WINE) ./$(INTL_CACHE_TEST) number
	$(WINE) ./$(INTL_CACHE_TEST) duration
	$(WINE) ./$(INTL_CACHE_TEST) number-mutable

test-c: test-intl-native-compiled-cache
test-binaries: $(INTL_CACHE_TEST)
endif
endif

tests/test_intl_native_locale_info$(EXE): $(OBJDIR)/tests/test_intl_native_locale_info.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_collator_segmenter$(EXE): $(OBJDIR)/tests/test_intl_collator_segmenter.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_number_format_embed$(EXE): $(OBJDIR)/tests/test_intl_number_format_embed.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_duration_format_embed$(EXE): $(OBJDIR)/tests/test_intl_duration_format_embed.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_services_api$(EXE): $(OBJDIR)/tests/test_intl_services_api.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_oom$(EXE): $(OBJDIR)/tests/test_intl_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_unicode_provider$(EXE): $(OBJDIR)/tests/test_unicode_provider.o $(OBJDIR)/src/unicode/libunicode.o $(OBJDIR)/src/cutils/cutils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_unicode_initialization$(EXE): $(OBJDIR)/tests/test_unicode_initialization.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) $(ICU_INIT_WRAP_LDFLAGS)

# The initialization fault fixture uses GNU linker wrapping only. Apple's
# linker has no --wrap; retain the ordinary initialization checks there.
ifeq ($(CONFIG_ICU),y)
ifndef CONFIG_DARWIN
ICU_INIT_WRAP_LDFLAGS=-Wl$(comma)--wrap=$(ICU_INIT_WRAP_SYMBOL)
$(OBJDIR)/tests/test_unicode_initialization.o: CFLAGS+=-DQJS_TEST_ICU_INIT_WRAP
endif
endif
comma:=,
ICU_INIT_WRAP_SYMBOL?=u_init_$(shell $(CC) $(ICU_CFLAGS) -dM -E -include unicode/uvernum.h -x c /dev/null | sed -n 's/^\#define U_ICU_VERSION_MAJOR_NUM //p')
$(OBJDIR)/tests/test_unicode_provider.o: $(UNICODE_PROVIDER_INPUTS)
# Full scalar and binary-property ICU oracles are explicit optional modes.
.PHONY: test-unicode-provider-exhaustive
test-unicode-provider-exhaustive: tests/test_unicode_provider$(EXE)
	$(WINE) ./tests/test_unicode_provider$(EXE) --exhaustive
	$(WINE) ./tests/test_unicode_provider$(EXE) --properties


tests/test_date_zone_order$(EXE): $(OBJDIR)/tests/test_date_zone_order.o $(OBJDIR)/src/temporal/time-zone.date-test.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_date_icu_core_only$(EXE): $(OBJDIR)/tests/test_date_icu_core_only.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

ifeq ($(CONFIG_ICU),y)
$(OBJDIR)/src/temporal/time-zone.date-test.o: src/temporal/time-zone.c $(OBJDIR)/temporal-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_CFLAGS) $(CALENDAR_COMPILE_CFLAGS) $(TIMEZONE_COMPILE_CFLAGS) $(DEPFLAGS) -Dqjs_temporal_system_zone=qjs_temporal_system_zone_test_backend -c -o $@ $<
endif
ifneq ($(filter y,$(CONFIG_ICU) $(CONFIG_TEMPORAL)),)
.PHONY: test-date-offset-seconds
test-date-offset-seconds: qjs$(EXE) tests/run_temporal_zone_test$(EXE)
	$(WINE) ./qjs$(EXE) tests/test_date_second_offset.js $(if $(filter y,$(CONFIG_ICU)),--icu-date-provider)
	$(WINE) ./tests/run_temporal_zone_test$(EXE) America/New_York ./qjs$(EXE) tests/test_date_second_offset.js --expect-new-york-seconds $(if $(filter y,$(CONFIG_ICU)),--icu-date-provider)
test: test-date-offset-seconds
endif

tests/test_icu_version_contract$(EXE): $(OBJDIR)/tests/test_icu_version_contract.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: test-unicode-backend-contract
test-unicode-backend-contract: qjs$(EXE)
	$(WINE) ./qjs$(EXE) tests/test_unicode_backend_contract.js
test: test-unicode-backend-contract

# Kept out of all/test: only the owned witness launcher uses this preflight.
ifeq ($(CONFIG_TZ_CAPTURE_SNAPSHOT),y)
.PHONY: selected-timezone-probe
selected-timezone-probe: $(OBJDIR)/tools/ci/native-prefix/probe-selected-timezones$(EXE)
$(OBJDIR)/tools/ci/native-prefix/probe-selected-timezones$(EXE): $(OBJDIR)/tools/ci/native-prefix/probe-selected-timezones.o $(TIMEZONE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
endif

TEST262_UNICODE18_OVERLAY?=y

.PHONY: test-test262-config
test-test262-config:
	QJS_TEST_MAKE="$(MAKE)" $(PYTHON) tests/test_test262_config.py
test: test-test262-config
