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

# Temporal defaults on; CONFIG_TEMPORAL=n CONFIG_INTL=n needs no ICU.
# JavaScript Intl remains optional; ICU is the shared native backend.
CONFIG_INTL?=n
CONFIG_TEMPORAL?=n
ifeq ($(CONFIG_TEMPORAL),y)
$(error Complete Temporal activation is not part of this preparation commit)
endif
# Derived backend switch; callers select the two JavaScript features above.
override CONFIG_ICU:=n
ifeq ($(CONFIG_TEMPORAL),y)
override CONFIG_ICU:=y
endif
ifeq ($(CONFIG_INTL),y)
override CONFIG_ICU:=y
endif
# ECMA-402 legacy constructor chaining is normative optional.
CONFIG_INTL_LEGACY?=y
PKG_CONFIG?=pkg-config
HOST_PKG_CONFIG?=pkg-config
ICU_STATIC?=n
ICU_PKG_MODULES?=icu-i18n icu-uc
ifeq ($(CONFIG_ICU),y)
ifeq ($(ICU_STATIC),y)
ICU_PKG_LINK_MODE=--static
endif
ifeq ($(origin ICU_LIBS),undefined)
ifeq ($(shell $(PKG_CONFIG) --atleast-version=78.3 $(ICU_PKG_MODULES) >/dev/null 2>&1 && echo y),)
$(error CONFIG_TEMPORAL=y or CONFIG_INTL=y needs ICU4C >=78.3 development files or explicit ICU_CFLAGS and ICU_LIBS)
endif
ICU_LIBS:=$(shell $(PKG_CONFIG) $(ICU_PKG_LINK_MODE) --libs $(ICU_PKG_MODULES))
endif
ifeq ($(origin ICU_CFLAGS),undefined)
ICU_CFLAGS:=$(shell $(PKG_CONFIG) --cflags $(ICU_PKG_MODULES))
endif
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

ifeq ($(CONFIG_ICU),y)
ifneq ($(CROSS_PREFIX),)
ifeq ($(origin HOST_ICU_LIBS),undefined)
HOST_ICU_LIBS:=$(shell $(HOST_PKG_CONFIG) $(ICU_PKG_LINK_MODE) --libs $(ICU_PKG_MODULES) 2>/dev/null)
endif
ifeq ($(origin HOST_ICU_CFLAGS),undefined)
HOST_ICU_CFLAGS:=$(shell $(HOST_PKG_CONFIG) --cflags $(ICU_PKG_MODULES) 2>/dev/null)
endif
ifeq ($(strip $(HOST_ICU_LIBS)),)
$(error ICU-enabled cross builds need separate HOST_ICU_CFLAGS and HOST_ICU_LIBS)
endif
else
HOST_ICU_LIBS?=$(ICU_LIBS)
HOST_ICU_CFLAGS?=$(ICU_CFLAGS)
endif
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
ifeq ($(CONFIG_ICU),y)
DEFINES+=-DCONFIG_ICU
endif
ifeq ($(CONFIG_TEMPORAL),y)
DEFINES+=-DCONFIG_TEMPORAL
endif
ifeq ($(CONFIG_INTL),y)
DEFINES+=-DCONFIG_INTL
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
ifdef CONFIG_LTO
CFLAGS_SMALL+=-flto
CFLAGS_OPT+=-flto
LDFLAGS+=-flto
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
    src/quickjs/builtins/intl/core.c \
    src/quickjs/builtins/temporal/common.c \
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
TEMPORAL_SHARED_SRCS= \
    src/temporal/epoch.c \
    src/temporal/options.c \
    src/temporal/duration.c
ifeq ($(CONFIG_ICU),y)
QUICKJS_SRCS+=$(TEMPORAL_SHARED_SRCS)
endif
ifeq ($(CONFIG_TEMPORAL),y)
TEMPORAL_LIBRARY_SRCS= \
    src/temporal/calendar.c \
    src/temporal/civil.c \
    src/temporal/relative.c \
    src/temporal/time.c
TEMPORAL_ENGINE_SRCS= \
    src/quickjs/builtins/temporal/calendar-fields.c \
    src/quickjs/builtins/temporal/duration.c \
    src/quickjs/builtins/temporal/instant.c \
    src/quickjs/builtins/temporal/options.c \
    src/quickjs/builtins/temporal/plain-time.c \
    src/quickjs/builtins/temporal/plain.c \
    src/quickjs/builtins/temporal/zoned-arithmetic.c
QUICKJS_SRCS+=$(TEMPORAL_LIBRARY_SRCS) $(TEMPORAL_ENGINE_SRCS)
endif
ifeq ($(CONFIG_ICU),y)
ICU_BACKEND_SRCS= \
    src/intl/locale-data.c \
    src/temporal/format.c \
    src/temporal/iso.c \
    src/temporal/parse.c \
    src/temporal/time-zone.c
QUICKJS_SRCS+=$(ICU_BACKEND_SRCS)
endif
ifeq ($(CONFIG_INTL),y)
# Source components join libquickjs.a; no additional archive is produced.
INTL_SRCS= \
    src/intl/libintl.c \
    src/intl/plural.c \
    src/intl/plural-icu.c \
    src/quickjs/builtins/intl/options.c \
    src/quickjs/builtins/intl/values.c \
    src/quickjs/builtins/intl/locale-syntax.c \
    src/quickjs/builtins/intl/locale-resolution.c \
    src/quickjs/builtins/intl/intl-values.c \
    src/quickjs/builtins/intl/locale.c \
    src/quickjs/builtins/intl/bound-function.c \
    src/quickjs/builtins/intl/collator.c \
    src/quickjs/builtins/intl/segmenter.c \
    src/quickjs/builtins/intl/date-time-format.c \
    src/quickjs/builtins/intl/list-format.c \
    src/quickjs/builtins/intl/display-names.c \
    src/quickjs/builtins/intl/number-common.c \
    src/quickjs/builtins/intl/number-format.c \
    src/quickjs/builtins/intl/plural-rules.c \
    src/quickjs/builtins/intl/relative-time-format.c \
    src/quickjs/builtins/intl/duration-format.c \
    src/quickjs/builtins/intl/case-conversion.c
QUICKJS_SRCS+=$(INTL_SRCS)
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
QJS_LIB_OBJS=$(QUICKJS_OBJS) $(WAIT_OBJS) $(OBJDIR)/src/dtoa/dtoa.o $(REGEXP_OBJS) $(OBJDIR)/src/unicode/libunicode.o $(OBJDIR)/src/cutils/cutils.o $(QUICKJS_LIBC_OBJS)

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
# Host and target include paths stay separate for cross compilation.
ICU_HEADER_SRCS=$(QUICKJS_SRCS) src/quickjs-libc/host.c tools/qjs.c tests/test_intl_embedder.c tests/test_intl_locale_lookup.c tests/test_intl_plural.c
ICU_TARGET_OBJECTS=$(foreach suffix,o pic.o nolto.o debug.o fuzz.o check.o,$(patsubst %.c,$(OBJDIR)/%.$(suffix),$(ICU_HEADER_SRCS)))
$(ICU_TARGET_OBJECTS): ICU_COMPILE_CFLAGS=$(ICU_CFLAGS)
$(patsubst %.c,$(OBJDIR)/%.host.o,$(ICU_HEADER_SRCS)): ICU_COMPILE_CFLAGS=$(HOST_ICU_CFLAGS)
endif

# Archives/executables share output names in both profiles. A common stamp
# rebuilds their object inputs when either feature or ICU flags change.
intl_shell_quote = '$(subst ','"'"',$(1))'
intl_build_config_args = $(call intl_shell_quote,$(CONFIG_TEMPORAL)) $(call intl_shell_quote,$(CONFIG_ICU)) $(call intl_shell_quote,$(CONFIG_INTL)) $(call intl_shell_quote,$(CONFIG_INTL_LEGACY)) $(call intl_shell_quote,$(ICU_CFLAGS)) $(call intl_shell_quote,$(ICU_LIBS)) $(call intl_shell_quote,$(HOST_ICU_CFLAGS)) $(call intl_shell_quote,$(HOST_ICU_LIBS)) $(call intl_shell_quote,$(LIBS))
# A changed configuration must invalidate consumers even when Make or
# the filesystem cannot distinguish the stamp and output timestamps.
ifneq ($(shell printf '%s\n' $(intl_build_config_args) | cmp -s - .obj/intl-build-config || printf changed),)
.PHONY: .obj/intl-build-config
endif
.PHONY: force-intl-build-config
.obj/intl-build-config: force-intl-build-config
	@mkdir -p $(@D)
	@printf '%s\n' $(intl_build_config_args) > $@.tmp
	@if ! cmp -s $@.tmp $@; then mv $@.tmp $@; else rm $@.tmp; fi
INTL_CONFIG_OBJECTS=$(foreach suffix,o host.o pic.o nolto.o debug.o fuzz.o check.o,$(patsubst %.o,%.$(suffix),$(QJS_LIB_OBJS)))
$(INTL_CONFIG_OBJECTS) $(OBJDIR)/tools/qjs.o $(OBJDIR)/tools/qjsc.o $(OBJDIR)/tools/qjsc.host.o $(OBJDIR)/tools/run-test262.o: .obj/intl-build-config


$(OBJDIR):
	mkdir -p $(OBJDIR) $(OBJDIR)/examples $(OBJDIR)/tests

qjs$(EXE): $(QJS_OBJS)
	$(CC) $(LDFLAGS) $(LDEXPORT) -o $@ $^ $(LIBS)

qjs-debug$(EXE): $(patsubst %.o, %.debug.o, $(QJS_OBJS))
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

qjsc$(EXE): $(OBJDIR)/tools/qjsc.o $(QJS_LIB_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

fuzz_eval: $(OBJDIR)/fuzz/fuzz_eval.o $(OBJDIR)/fuzz/fuzz_common.o libquickjs.fuzz.a
	$(CC) $(CFLAGS_OPT) $^ -o fuzz_eval $(LIB_FUZZING_ENGINE)

fuzz_compile: $(OBJDIR)/fuzz/fuzz_compile.o $(OBJDIR)/fuzz/fuzz_common.o libquickjs.fuzz.a
	$(CC) $(CFLAGS_OPT) $^ -o fuzz_compile $(LIB_FUZZING_ENGINE)

fuzz_regexp: $(OBJDIR)/fuzz/fuzz_regexp.o $(patsubst %.o,%.fuzz.o,$(REGEXP_OBJS)) $(OBJDIR)/src/cutils/cutils.fuzz.o $(OBJDIR)/src/unicode/libunicode.fuzz.o
	$(CC) $(CFLAGS_OPT) $^ -o fuzz_regexp $(LIB_FUZZING_ENGINE)

libfuzzer: fuzz_eval fuzz_compile fuzz_regexp

ifneq ($(CROSS_PREFIX),)

$(QJSC): $(OBJDIR)/tools/qjsc.host.o \
    $(patsubst %.o, %.host.o, $(QJS_LIB_OBJS))
	$(HOST_CC) $(LDFLAGS) -o $@ $^ $(HOST_LIBS)

endif #CROSS_PREFIX

QJSC_DEFINES:=-DCONFIG_CC=\"$(QJSC_CC)\" -DCONFIG_PREFIX=\"$(PREFIX)\"
ifdef CONFIG_LTO
QJSC_DEFINES+=-DCONFIG_LTO
endif
QJSC_HOST_DEFINES:=-DCONFIG_CC=\"$(HOST_CC)\" -DCONFIG_PREFIX=\"$(PREFIX)\"
ifeq ($(CONFIG_ICU),y)
QJSC_DEFINES+=-I$(OBJDIR)
QJSC_HOST_DEFINES+=-I$(OBJDIR) -DQJSC_INTL_LINK_HEADER=\"qjsc-intl-host-link.h\"
$(OBJDIR)/qjsc-intl-link.h: tools/intl-link-config.py .obj/intl-build-config
	python3 $< header $@ -- $(ICU_LIBS)
$(OBJDIR)/qjsc-intl-host-link.h: tools/intl-link-config.py .obj/intl-build-config
	python3 $< header $@ -- $(HOST_ICU_LIBS)
$(OBJDIR)/tools/qjsc.o: $(OBJDIR)/qjsc-intl-link.h
$(OBJDIR)/tools/qjsc.host.o: $(OBJDIR)/qjsc-intl-host-link.h
$(OBJDIR)/quickjs.pc: tools/intl-link-config.py .obj/intl-build-config
	python3 $< pkgconfig $@ $(call intl_shell_quote,$(PREFIX)) $(call intl_shell_quote,$(shell cat VERSION)) -- $(LIBS)
install: $(OBJDIR)/quickjs.pc
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

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/fuzz/%.o: fuzz/%.c | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -c -I. -o $@ $<

$(OBJDIR)/%.host.o: %.c | $(OBJDIR)
	mkdir -p $(@D)
	$(HOST_CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/%.pic.o: %.c | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -fPIC -DJS_SHARED_LIBRARY -c -o $@ $<

$(OBJDIR)/%.nolto.o: %.c | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_NOLTO) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/%.debug.o: %.c | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_DEBUG) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/%.fuzz.o: %.c | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -fsanitize=fuzzer-no-link -c -o $@ $<

$(OBJDIR)/%.check.o: %.c | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -DCONFIG_CHECK_JSVALUE -c -o $@ $<

regexp_test$(EXE): tests/regexp_test.c src/regexp/compile.c src/regexp/exec.c src/unicode/libunicode.c src/cutils/cutils.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DTEST -o $@ tests/regexp_test.c src/regexp/compile.c src/regexp/exec.c src/unicode/libunicode.c src/cutils/cutils.c $(LIBS)

unicode_gen: $(OBJDIR)/tools/unicode_gen.host.o $(OBJDIR)/src/cutils/cutils.host.o tools/unicode_gen_def.h
	$(HOST_CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJDIR)/tools/unicode_gen.host.o $(OBJDIR)/src/cutils/cutils.host.o

$(OBJDIR)/tools/unicode_gen.test.host.o: tools/unicode_gen.c | $(OBJDIR)
	mkdir -p $(@D)
	$(HOST_CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -DUSE_TEST -c -o $@ $<

$(OBJDIR)/src/unicode/libunicode.test.host.o: src/unicode/libunicode.c | $(OBJDIR)
	mkdir -p $(@D)
	$(HOST_CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -DUSE_TEST -c -o $@ $<

unicode_gen_test: $(OBJDIR)/tools/unicode_gen.test.host.o $(OBJDIR)/src/unicode/libunicode.test.host.o $(OBJDIR)/src/cutils/cutils.host.o tools/unicode_gen_def.h
	$(HOST_CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJDIR)/tools/unicode_gen.test.host.o $(OBJDIR)/src/unicode/libunicode.test.host.o $(OBJDIR)/src/cutils/cutils.host.o

clean:
	rm -f repl.c out.c quickjs.h quickjs-libc.h
	rm -f *.a *.o *.d *~ unicode_gen unicode_gen_test regexp_test$(EXE) fuzz_eval fuzz_compile fuzz_regexp $(PROGS)
	rm -f hello.c test_fib.c
	rm -f examples/*.so tests/*.so $(C_TESTS)
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
ifeq ($(CONFIG_ICU),y)
	mkdir -p "$(DESTDIR)$(PREFIX)/lib/pkgconfig"
	install -m644 $(OBJDIR)/quickjs.pc "$(DESTDIR)$(PREFIX)/lib/pkgconfig/quickjs.pc"
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
HELLO_MODULE_OPTS=-fno-string-normalize -fno-map -fno-typedarray \
           -fno-typedarray -fno-regexp -fno-json -fno-eval -fno-proxy \
           -fno-date -m
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


C_TESTS=tests/test_api$(EXE) tests/test_bytecode$(EXE) tests/test_cutils$(EXE) \
        tests/test_unicode$(EXE) tests/test_bytecode_trace$(EXE) \
        tests/test_typed_array$(EXE) tests/test_allocator$(EXE)
C_TESTS+=tests/test_intl_receiver_api$(EXE)
C_TESTS+=tests/test_temporal_api$(EXE)
C_TESTS+=tests/test_qjsc_context_failures$(EXE)
ifeq ($(CONFIG_TEMPORAL),y)
C_TESTS+=tests/test_temporal$(EXE)
C_TESTS+=tests/test_temporal_civil$(EXE)
C_TESTS+=tests/test_temporal_duration_math$(EXE)
C_TESTS+=tests/test_temporal_calendars$(EXE)
C_TESTS+=tests/test_temporal_zones$(EXE)
endif
ifeq ($(CONFIG_ICU),y)
endif
C_TESTS+=tests/test_intl_duration_format_embed$(EXE)
C_TESTS+=tests/test_intl_services_api$(EXE)
C_TESTS+=tests/test_intl_plural$(EXE)
C_TESTS+=tests/test_intl_number_format_embed$(EXE)
C_TESTS+=tests/test_intl_collator_segmenter$(EXE)
C_TESTS+=tests/test_native_data_realms$(EXE)
ifeq ($(CONFIG_INTL),y)
C_TESTS+=tests/test_intl_locale_lookup$(EXE)
endif

C_TESTS+=tests/test_fuzz_json$(EXE)

C_TESTS+=tests/test_fuzz_exception_ownership$(EXE)

C_TESTS+=tests/test_fuzz_allocations$(EXE)

C_TESTS+=tests/test_fuzz_regexp_timeout$(EXE)

C_TESTS+=tests/test_atomics_wait$(EXE)
C_TESTS+=tests/test_wait_async$(EXE)
C_TESTS+=tests/test_native_jobs$(EXE)
C_TESTS+=tests/test_wait_queue$(EXE)
C_TESTS+=tests/test_intl_embedder$(EXE)
C_TESTS+=tests/test_intl_oom$(EXE)
$(patsubst tests/%$(EXE),$(OBJDIR)/tests/%.o,$(C_TESTS)): .obj/intl-build-config

tests/test_intl_embedder$(EXE): $(OBJDIR)/tests/test_intl_embedder.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_oom$(EXE): $(OBJDIR)/tests/test_intl_oom.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Link the tracing reader before the archive so it replaces the normal reader.
$(OBJDIR)/src/quickjs/serialization/reader.trace.o: src/quickjs/serialization/reader.c | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -DDUMP_READ_OBJECT -c -o $@ $<

tests/test_bytecode_trace$(EXE): $(OBJDIR)/tests/test_bytecode.o $(OBJDIR)/src/quickjs/serialization/reader.trace.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_allocator$(EXE): $(OBJDIR)/tests/test_allocator.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_receiver_api$(EXE): $(OBJDIR)/tests/test_intl_receiver_api.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_locale_lookup$(EXE): $(OBJDIR)/tests/test_intl_locale_lookup.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_atomics_wait$(EXE): $(OBJDIR)/tests/test_atomics_wait.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_wait_queue$(EXE): $(OBJDIR)/tests/test_wait_queue.o $(WAIT_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_native_jobs$(EXE): $(OBJDIR)/tests/test_native_jobs.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_wait_async$(EXE): $(OBJDIR)/tests/test_wait_async.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_api$(EXE): $(OBJDIR)/tests/test_api.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_typed_array$(EXE): $(OBJDIR)/tests/test_typed_array.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_bytecode$(EXE): $(OBJDIR)/tests/test_bytecode.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_cutils$(EXE): $(OBJDIR)/tests/test_cutils.o $(OBJDIR)/src/cutils/cutils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_unicode$(EXE): $(OBJDIR)/tests/test_unicode.o $(OBJDIR)/src/unicode/libunicode.o $(OBJDIR)/src/cutils/cutils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_native_data_realms$(EXE): $(OBJDIR)/tests/test_native_data_realms.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_collator_segmenter$(EXE): $(OBJDIR)/tests/test_intl_collator_segmenter.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_number_format_embed$(EXE): $(OBJDIR)/tests/test_intl_number_format_embed.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_services_api$(EXE): $(OBJDIR)/tests/test_intl_services_api.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_intl_plural$(EXE): $(OBJDIR)/tests/test_intl_plural.o $(OBJDIR)/src/intl/plural.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

ifeq ($(CONFIG_ICU),y)
$(OBJDIR)/src/temporal/time-zone.date-test.o: src/temporal/time-zone.c .obj/intl-build-config | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_CFLAGS) $(DEPFLAGS) -Dqjs_temporal_system_zone=qjs_temporal_system_zone_test_backend -c -o $@ $<

endif

tests/test_intl_duration_format_embed$(EXE): $(OBJDIR)/tests/test_intl_duration_format_embed.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Compile the actual qjsc-generated context/main into the native fault unit.
$(OBJDIR)/tests/qjsc-context-generated.c: $(QJSC) tests/fixture_qjsc_context.js .obj/intl-build-config
	mkdir -p $(@D)
	$(QJSC) -e -o $@ tests/fixture_qjsc_context.js

$(OBJDIR)/tests/test_qjsc_context_failures.o: $(OBJDIR)/tests/qjsc-context-generated.c .obj/intl-build-config
$(OBJDIR)/tests/test_qjsc_context_failures.o: CFLAGS+=-I$(OBJDIR)/tests

tests/test_qjsc_context_failures$(EXE): $(OBJDIR)/tests/test_qjsc_context_failures.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_temporal_api$(EXE): $(OBJDIR)/tests/test_temporal_api.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)






.PHONY: test-c
test-c: $(C_TESTS)
	$(WINE) ./tests/test_fuzz_regexp_timeout$(EXE)
	$(WINE) ./tests/test_fuzz_allocations$(EXE)
	$(WINE) ./tests/test_fuzz_exception_ownership$(EXE)
	$(WINE) ./tests/test_fuzz_json$(EXE)
	$(WINE) ./tests/test_allocator$(EXE)
	$(WINE) ./tests/test_atomics_wait$(EXE)
	$(WINE) ./tests/test_wait_async$(EXE)
	$(WINE) ./tests/test_native_jobs$(EXE)
	$(WINE) ./tests/test_wait_queue$(EXE)
	$(WINE) ./tests/test_intl_embedder$(EXE)
	$(WINE) ./tests/test_intl_oom$(EXE)
	$(WINE) ./tests/test_api$(EXE)
	$(WINE) ./tests/test_typed_array$(EXE)
	$(WINE) ./tests/test_bytecode$(EXE)
	$(WINE) ./tests/test_cutils$(EXE)
	$(WINE) ./tests/test_unicode$(EXE)
	$(WINE) ./tests/test_intl_receiver_api$(EXE)
	$(WINE) ./tests/test_temporal_api$(EXE)
	$(WINE) ./tests/test_qjsc_context_failures$(EXE)
ifeq ($(CONFIG_TEMPORAL),y)
	$(WINE) ./tests/test_temporal$(EXE)
	$(WINE) ./tests/test_temporal_civil$(EXE)
	$(WINE) ./tests/test_temporal_duration_math$(EXE)
	$(WINE) ./tests/test_temporal_calendars$(EXE)
	$(WINE) ./tests/test_temporal_zones$(EXE)
endif
ifeq ($(CONFIG_ICU),y)
endif
	$(WINE) ./tests/test_intl_duration_format_embed$(EXE)
	$(WINE) ./tests/test_intl_services_api$(EXE)
	$(WINE) ./tests/test_intl_plural$(EXE)
	$(WINE) ./tests/test_intl_number_format_embed$(EXE)
	$(WINE) ./tests/test_intl_collator_segmenter$(EXE)
	$(WINE) ./tests/test_native_data_realms$(EXE)
	$(WINE) ./tests/test_bytecode_trace$(EXE)
ifeq ($(CONFIG_INTL),y)
	$(WINE) ./tests/test_intl_locale_lookup$(EXE)
endif

.PHONY: test-regexp
test-regexp: regexp_test$(EXE)
	sh tests/test_regexp.sh "$(WINE)" "./regexp_test$(EXE)"

.PHONY: test-run-test262
test-run-test262: run-test262$(EXE)
	sh tests/test_run_test262.sh "$(WINE)" "./run-test262$(EXE)"

test: test-c test-regexp test-build-dependencies test-run-test262

# Metadata-only Windows/POSIX selection checks use a rejecting fake compiler.
.PHONY: test-icu-link-metadata
test-icu-link-metadata:
	QJS_TEST_MAKE="$(MAKE)" python3 tests/test_icu_link_metadata.py

test: test-icu-link-metadata

.PHONY: test-build-dependencies
test-build-dependencies:
	sh tests/test_build_dependencies.sh "$(MAKE)"

ifdef CONFIG_SHARED_LIBS
test: tests/bjson.so examples/point.so
endif

test: qjs$(EXE) run-test262$(EXE)
	$(WINE) ./qjs$(EXE) tests/test_closure.js
	$(WINE) ./qjs$(EXE) tests/test_language.js
	$(WINE) ./qjs$(EXE) -m tests/test_module.js
	$(WINE) ./qjs$(EXE) -m tests/test_module_names.js
	$(WINE) ./qjs$(EXE) --std tests/test_builtin.js
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

ifeq ($(CONFIG_INTL),y)
	$(WINE) ./qjs$(EXE) tests/test_intl_locale.js
	$(WINE) ./qjs$(EXE) tests/test_intl_era_monthcode_calendars.js
	$(WINE) ./qjs$(EXE) tests/test_intl_locale_resolution.js
	$(WINE) ./run-test262$(EXE) -N tests/test_intl_bound_function_realms.js
	$(WINE) ./qjs$(EXE) tests/test_intl_locale_integration.js
	$(WINE) ./qjs$(EXE) tests/test_intl_duration_format.js
ifeq ($(CONFIG_TEMPORAL),y)
endif
	$(WINE) ./qjs$(EXE) tests/test_intl_relative_time_format.js
	$(WINE) ./qjs$(EXE) tests/test_intl_plural_rules.js
	$(WINE) ./qjs$(EXE) tests/test_intl_number_locale_methods.js
ifeq ($(CONFIG_INTL_LEGACY),y)
	$(WINE) ./qjs$(EXE) tests/test_intl_number_format.js
else
	$(WINE) ./qjs$(EXE) --std -e "globalThis.intlLegacyExpected = false; std.loadScript('tests/test_intl_number_format.js');"
endif
	$(WINE) ./qjs$(EXE) tests/test_intl_display_names.js
	$(WINE) ./qjs$(EXE) tests/test_intl_list_format.js
	$(WINE) ./qjs$(EXE) tests/test_intl_date_time.js
ifeq ($(CONFIG_INTL_LEGACY),y)
	$(WINE) ./qjs$(EXE) tests/test_intl_date_time_legacy.js
endif
	$(WINE) ./qjs$(EXE) tests/test_intl_segmenter.js
	$(WINE) ./qjs$(EXE) tests/test_intl_collator.js
endif

ifeq ($(CONFIG_TEMPORAL),y)
	$(WINE) ./qjs$(EXE) tests/test_temporal_instant.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_duration.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_plain_time.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_plain.js
	$(WINE) ./qjs$(EXE) tests/test_temporal_calendar_zones.js
ifeq ($(CONFIG_INTL),y)
endif
endif

stats: qjs$(EXE)
	$(WINE) ./qjs$(EXE) -qd

microbench: qjs$(EXE)
	$(WINE) ./qjs$(EXE) --std tests/microbench.js

ifeq ($(CONFIG_INTL),y)
.PHONY: microbench-intl
microbench-intl: qjs$(EXE)
	@set -e; for name in intl_number_format intl_number_bigint intl_number_decimal intl_number_parts intl_number_range intl_number_constructor intl_duration_digital intl_duration_textual; do \
	done
endif

ifeq ($(wildcard test262/features.txt),)
test2-bootstrap:
	git clone --single-branch --shallow-since=$(TEST262_SINCE) https://github.com/tc39/test262.git
	(cd test262 && git checkout -q $(TEST262_COMMIT) && \
	 patch -p1 < ../tests/test262.patch && \
	 patch -p1 < ../tests/test262-unicode18.patch && cd ..)
else
test2-bootstrap:
	(cd test262 && git fetch && git reset --hard $(TEST262_COMMIT) && \
	 patch -p1 < ../tests/test262.patch && \
	 patch -p1 < ../tests/test262-unicode18.patch && cd ..)
endif

ifeq ($(wildcard test262o/tests.txt),)
test2o test2o-update:
	@echo test262o tests not installed
else
# ES5 tests (obsolete)
test2o: run-test262
	time ./run-test262 -t -m -c test262o.conf

test2o-update: run-test262
	./run-test262 -t -u -c test262o.conf
endif

ifeq ($(wildcard test262/features.txt),)
test2 test2-update test2-default test2-check:
	@echo test262 tests not installed
else
# Test262 tests
test2-default: run-test262
	time ./run-test262 -t -m -c test262.conf

test2: run-test262
	time ./run-test262 -t -m -c test262.conf -a

test2-update: run-test262
	./run-test262 -t -u -c test262.conf -a

test2-check: run-test262
	time ./run-test262 -t -m -c test262.conf -E -a
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

$(OBJDIR)/tests/test_fuzz_support.o: tests/test_fuzz_support.c | $(OBJDIR)
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

tests/test_fuzz_json$(EXE): $(OBJDIR)/tests/test_fuzz_json.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_fuzz_exception_ownership$(EXE): $(OBJDIR)/tests/test_fuzz_exception_ownership.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/tests/test_fuzz_allocations.o: tests/test_fuzz_allocations.c | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_COMPILE_CFLAGS) $(DEPFLAGS) -I. -c -o $@ $<

tests/test_fuzz_allocations$(EXE): $(OBJDIR)/tests/test_fuzz_allocations.o $(OBJDIR)/fuzz/fuzz_common.o libquickjs$(LTOEXT).a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tests/test_fuzz_regexp_timeout$(EXE): $(OBJDIR)/tests/test_fuzz_regexp_timeout.o $(REGEXP_OBJS) $(OBJDIR)/src/unicode/libunicode.o $(OBJDIR)/src/cutils/cutils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# TZ is selected once at process creation; tests do not mutate global defaults.
ifeq ($(CONFIG_TEMPORAL),y)
ifneq ($(CONFIG_WIN32),y)
.PHONY: test-date-temporal-time-zone test-qjsc-temporal
test: test-date-temporal-time-zone test-qjsc-temporal

test-date-temporal-time-zone: qjs$(EXE)

test-qjsc-temporal: qjsc$(EXE)
endif
endif

C_TESTS+=tests/test_temporal$(EXE)
tests/test_temporal$(EXE): $(OBJDIR)/tests/test_temporal.o $(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
.PHONY: test-test_temporal-prepared
test-c: test-test_temporal-prepared
test-test_temporal-prepared: tests/test_temporal$(EXE)
	$(WINE) ./tests/test_temporal$(EXE)
$(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o: ICU_COMPILE_CFLAGS=$(ICU_CFLAGS)
$(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o: .obj/intl-build-config

C_TESTS+=tests/test_temporal_calendars$(EXE)
tests/test_temporal_calendars$(EXE): $(OBJDIR)/tests/test_temporal_calendars.o $(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/duration.o $(OBJDIR)/src/temporal/calendar.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
.PHONY: test-test_temporal_calendars-prepared
test-c: test-test_temporal_calendars-prepared
test-test_temporal_calendars-prepared: tests/test_temporal_calendars$(EXE)
	$(WINE) ./tests/test_temporal_calendars$(EXE)
$(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/duration.o $(OBJDIR)/src/temporal/calendar.o: ICU_COMPILE_CFLAGS=$(ICU_CFLAGS)
$(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/duration.o $(OBJDIR)/src/temporal/calendar.o: .obj/intl-build-config

C_TESTS+=tests/test_temporal_civil$(EXE)
tests/test_temporal_civil$(EXE): $(OBJDIR)/tests/test_temporal_civil.o $(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/duration.o $(OBJDIR)/src/temporal/time.o $(OBJDIR)/src/temporal/relative.o $(OBJDIR)/src/temporal/calendar.o $(OBJDIR)/src/temporal/time-zone.o $(if $(filter y,$(CONFIG_ICU)),$(OBJDIR)/src/intl/locale-data.o)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
.PHONY: test-test_temporal_civil-prepared
test-c: test-test_temporal_civil-prepared
test-test_temporal_civil-prepared: tests/test_temporal_civil$(EXE)
	$(WINE) ./tests/test_temporal_civil$(EXE)
$(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/duration.o $(OBJDIR)/src/temporal/time.o $(OBJDIR)/src/temporal/relative.o $(OBJDIR)/src/temporal/calendar.o $(OBJDIR)/src/temporal/time-zone.o $(OBJDIR)/src/intl/locale-data.o: ICU_COMPILE_CFLAGS=$(ICU_CFLAGS)
$(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/duration.o $(OBJDIR)/src/temporal/time.o $(OBJDIR)/src/temporal/relative.o $(OBJDIR)/src/temporal/calendar.o $(OBJDIR)/src/temporal/time-zone.o $(OBJDIR)/src/intl/locale-data.o: .obj/intl-build-config

C_TESTS+=tests/test_temporal_duration_math$(EXE)
tests/test_temporal_duration_math$(EXE): $(OBJDIR)/tests/test_temporal_duration_math.o $(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/duration.o $(OBJDIR)/src/temporal/time.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
.PHONY: test-test_temporal_duration_math-prepared
test-c: test-test_temporal_duration_math-prepared
test-test_temporal_duration_math-prepared: tests/test_temporal_duration_math$(EXE)
	$(WINE) ./tests/test_temporal_duration_math$(EXE)
$(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/duration.o $(OBJDIR)/src/temporal/time.o: ICU_COMPILE_CFLAGS=$(ICU_CFLAGS)
$(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/duration.o $(OBJDIR)/src/temporal/time.o: .obj/intl-build-config

C_TESTS+=tests/test_temporal_zones$(EXE)
tests/test_temporal_zones$(EXE): $(OBJDIR)/tests/test_temporal_zones.o $(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/time-zone.o $(if $(filter y,$(CONFIG_ICU)),$(OBJDIR)/src/intl/locale-data.o)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
.PHONY: test-test_temporal_zones-prepared
test-c: test-test_temporal_zones-prepared
test-test_temporal_zones-prepared: tests/test_temporal_zones$(EXE)
	$(WINE) ./tests/test_temporal_zones$(EXE)
$(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/time-zone.o $(OBJDIR)/src/intl/locale-data.o: ICU_COMPILE_CFLAGS=$(ICU_CFLAGS)
$(OBJDIR)/src/temporal/epoch.o $(OBJDIR)/src/temporal/iso.o $(OBJDIR)/src/temporal/options.o $(OBJDIR)/src/temporal/parse.o $(OBJDIR)/src/temporal/format.o $(OBJDIR)/src/temporal/civil.o $(OBJDIR)/src/temporal/time-zone.o $(OBJDIR)/src/intl/locale-data.o: .obj/intl-build-config

TEMPORAL_PREPARATION_OBJECTS=$(OBJDIR)/src/quickjs/builtins/temporal/calendar-fields.prepare.o $(OBJDIR)/src/quickjs/builtins/temporal/calendar-fields.prepare.check.o $(OBJDIR)/src/quickjs/builtins/temporal/common.prepare.o $(OBJDIR)/src/quickjs/builtins/temporal/common.prepare.check.o $(OBJDIR)/src/quickjs/builtins/temporal/duration.prepare.o $(OBJDIR)/src/quickjs/builtins/temporal/duration.prepare.check.o $(OBJDIR)/src/quickjs/builtins/temporal/instant.prepare.o $(OBJDIR)/src/quickjs/builtins/temporal/instant.prepare.check.o $(OBJDIR)/src/quickjs/builtins/temporal/options.prepare.o $(OBJDIR)/src/quickjs/builtins/temporal/options.prepare.check.o $(OBJDIR)/src/quickjs/builtins/temporal/plain-time.prepare.o $(OBJDIR)/src/quickjs/builtins/temporal/plain-time.prepare.check.o $(OBJDIR)/src/quickjs/builtins/temporal/plain.prepare.o $(OBJDIR)/src/quickjs/builtins/temporal/plain.prepare.check.o $(OBJDIR)/src/quickjs/builtins/temporal/zoned-arithmetic.prepare.o $(OBJDIR)/src/quickjs/builtins/temporal/zoned-arithmetic.prepare.check.o
$(OBJDIR)/%.prepare.o: %.c | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS_OPT) $(ICU_CFLAGS) -DCONFIG_TEMPORAL $(DEPFLAGS) -c -o $@ $<
$(OBJDIR)/%.prepare.check.o: %.c | $(OBJDIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(ICU_CFLAGS) -DCONFIG_TEMPORAL -DCONFIG_CHECK_JSVALUE $(DEPFLAGS) -c -o $@ $<
.PHONY: check-temporal-preparation
check-temporal-preparation: $(TEMPORAL_PREPARATION_OBJECTS)
