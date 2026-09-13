/*
 * QuickJS Javascript Engine
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef QUICKJS_INTERNAL_CONFIG_H
#define QUICKJS_INTERNAL_CONFIG_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <fenv.h>
#include <math.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__) || defined(__GLIBC__)
#include <malloc.h>
#elif defined(__FreeBSD__)
#include <malloc_np.h>
#endif

#include "cutils.h"
#include "list.h"
#include "quickjs.h"
#include "libregexp.h"
#include "libunicode.h"
#include "dtoa.h"

#if defined(__GNUC__) || defined(__clang__)
#define QJS_INTERNAL __attribute__((visibility("hidden")))
#else
#define QJS_INTERNAL
#endif

#define OPTIMIZE         1
#define SHORT_OPCODES    1
#if defined(__EMSCRIPTEN__)
#define DIRECT_DISPATCH  0
#else
#define DIRECT_DISPATCH  1
#endif

#if defined(__APPLE__)
#define MALLOC_OVERHEAD  0
#else
#define MALLOC_OVERHEAD  8
#endif

#if !defined(_WIN32)
/* define it if printf uses the RNDN rounding mode instead of RNDNA */
#define CONFIG_PRINTF_RNDN
#endif

/* define to include Atomics.* operations which depend on the OS
   threads */
#if !defined(__EMSCRIPTEN__)
#define CONFIG_ATOMICS
#endif

#if !defined(__EMSCRIPTEN__)
/* enable stack limitation */
#define CONFIG_STACK_CHECK
#endif


/* dump object free */
//#define DUMP_FREE
//#define DUMP_CLOSURE
/* dump the bytecode of the compiled functions: combination of bits
   1: dump pass 3 final byte code
   2: dump pass 2 code
   4: dump pass 1 code
   8: dump stdlib functions
  16: dump bytecode in hex
  32: dump line number table
  64: dump compute_stack_size
 */
//#define DUMP_BYTECODE  (1)
/* dump the occurence of the automatic GC */
//#define DUMP_GC
/* dump objects freed by the garbage collector */
//#define DUMP_GC_FREE
/* dump objects leaking when freeing the runtime */
//#define DUMP_LEAKS  1
/* dump memory usage before running the garbage collector */
//#define DUMP_MEM
//#define DUMP_OBJECTS    /* dump objects in JS_FreeContext */
//#define DUMP_ATOMS      /* dump atoms in JS_FreeContext */
//#define DUMP_SHAPES     /* dump shapes in JS_FreeContext */
//#define DUMP_MODULE_RESOLVE
//#define DUMP_MODULE_EXEC
//#define DUMP_PROMISE
//#define DUMP_READ_OBJECT
//#define DUMP_ROPE_REBALANCE
/* add asm labels to each opcode so that it is easier to see the generated code */
//#define OPCODE_ASM_LABEL

/* test the GC by forcing it before each object allocation */
//#define FORCE_GC_AT_MALLOC

#ifdef CONFIG_ATOMICS
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#endif

enum {
    /* classid tag        */    /* union usage   | properties */
    JS_CLASS_OBJECT = 1,        /* must be first */
    JS_CLASS_ARRAY,             /* u.array       | length */
    JS_CLASS_ERROR,
    JS_CLASS_NUMBER,            /* u.object_data */
    JS_CLASS_STRING,            /* u.object_data */
    JS_CLASS_BOOLEAN,           /* u.object_data */
    JS_CLASS_SYMBOL,            /* u.object_data */
    JS_CLASS_ARGUMENTS,         /* u.array       | length */
    JS_CLASS_MAPPED_ARGUMENTS,  /* u.array       | length */
    JS_CLASS_DATE,              /* u.object_data */
    JS_CLASS_MODULE_NS,
    JS_CLASS_C_FUNCTION,        /* u.cfunc */
    JS_CLASS_BYTECODE_FUNCTION, /* u.func */
    JS_CLASS_BOUND_FUNCTION,    /* u.bound_function */
    JS_CLASS_C_FUNCTION_DATA,   /* u.c_function_data_record */
    JS_CLASS_GENERATOR_FUNCTION, /* u.func */
    JS_CLASS_FOR_IN_ITERATOR,   /* u.for_in_iterator */
    JS_CLASS_REGEXP,            /* u.regexp */
    JS_CLASS_ARRAY_BUFFER,      /* u.array_buffer */
    JS_CLASS_SHARED_ARRAY_BUFFER, /* u.array_buffer */
    JS_CLASS_UINT8C_ARRAY,      /* u.array (typed_array) */
    JS_CLASS_INT8_ARRAY,        /* u.array (typed_array) */
    JS_CLASS_UINT8_ARRAY,       /* u.array (typed_array) */
    JS_CLASS_INT16_ARRAY,       /* u.array (typed_array) */
    JS_CLASS_UINT16_ARRAY,      /* u.array (typed_array) */
    JS_CLASS_INT32_ARRAY,       /* u.array (typed_array) */
    JS_CLASS_UINT32_ARRAY,      /* u.array (typed_array) */
    JS_CLASS_BIG_INT64_ARRAY,   /* u.array (typed_array) */
    JS_CLASS_BIG_UINT64_ARRAY,  /* u.array (typed_array) */
    JS_CLASS_FLOAT16_ARRAY,     /* u.array (typed_array) */
    JS_CLASS_FLOAT32_ARRAY,     /* u.array (typed_array) */
    JS_CLASS_FLOAT64_ARRAY,     /* u.array (typed_array) */
    JS_CLASS_DATAVIEW,          /* u.typed_array */
    JS_CLASS_BIG_INT,           /* u.object_data */
    JS_CLASS_MAP,               /* u.map_state */
    JS_CLASS_SET,               /* u.map_state */
    JS_CLASS_WEAKMAP,           /* u.map_state */
    JS_CLASS_WEAKSET,           /* u.map_state */
    JS_CLASS_ITERATOR,          /* u.map_iterator_data */
    JS_CLASS_ITERATOR_CONCAT,   /* u.iterator_concat_data */
    JS_CLASS_ITERATOR_HELPER,   /* u.iterator_helper_data */
    JS_CLASS_ITERATOR_WRAP,     /* u.iterator_wrap_data */
    JS_CLASS_MAP_ITERATOR,      /* u.map_iterator_data */
    JS_CLASS_SET_ITERATOR,      /* u.map_iterator_data */
    JS_CLASS_ARRAY_ITERATOR,    /* u.array_iterator_data */
    JS_CLASS_STRING_ITERATOR,   /* u.array_iterator_data */
    JS_CLASS_REGEXP_STRING_ITERATOR,   /* u.regexp_string_iterator_data */
    JS_CLASS_GENERATOR,         /* u.generator_data */
    JS_CLASS_GLOBAL_OBJECT,     /* u.global_object */
    JS_CLASS_RAWJSON,
    JS_CLASS_PROXY,             /* u.proxy_data */
    JS_CLASS_PROMISE,           /* u.promise_data */
    JS_CLASS_PROMISE_RESOLVE_FUNCTION,  /* u.promise_function_data */
    JS_CLASS_PROMISE_REJECT_FUNCTION,   /* u.promise_function_data */
    JS_CLASS_ASYNC_FUNCTION,            /* u.func */
    JS_CLASS_ASYNC_FUNCTION_RESOLVE,    /* u.async_function_data */
    JS_CLASS_ASYNC_FUNCTION_REJECT,     /* u.async_function_data */
    JS_CLASS_ASYNC_FROM_SYNC_ITERATOR,  /* u.async_from_sync_iterator_data */
    JS_CLASS_ASYNC_GENERATOR_FUNCTION,  /* u.func */
    JS_CLASS_ASYNC_GENERATOR,   /* u.async_generator_data */
    JS_CLASS_WEAK_REF,
    JS_CLASS_FINALIZATION_REGISTRY,

    JS_CLASS_INIT_COUNT, /* last entry for predefined classes */
};

/* number of typed array types */
#define JS_TYPED_ARRAY_COUNT  (JS_CLASS_FLOAT64_ARRAY - JS_CLASS_UINT8C_ARRAY + 1)
extern QJS_INTERNAL const uint8_t
qjs_typed_array_size_log2[JS_TYPED_ARRAY_COUNT];
#define typed_array_size_log2(classid)  \
    (qjs_typed_array_size_log2[(classid) - JS_CLASS_UINT8C_ARRAY])

typedef enum JSErrorEnum {
    JS_EVAL_ERROR,
    JS_RANGE_ERROR,
    JS_REFERENCE_ERROR,
    JS_SYNTAX_ERROR,
    JS_TYPE_ERROR,
    JS_URI_ERROR,
    JS_INTERNAL_ERROR,
    JS_AGGREGATE_ERROR,

    JS_NATIVE_ERROR_COUNT, /* number of different NativeError objects */
} JSErrorEnum;

#endif /* QUICKJS_INTERNAL_CONFIG_H */

/* the variable and scope indexes must fit on 16 bits. The (-1) and
   ARG_SCOPE_END values are reserved. */
#define JS_MAX_LOCAL_VARS 65534
#define JS_STACK_SIZE_MAX 65534
#define JS_STRING_LEN_MAX ((1 << 30) - 1)

/* strings <= this length are not concatenated using ropes. if too
   small, the rope memory overhead becomes high. */
#define JS_STRING_ROPE_SHORT_LEN  512
/* specific threshold for initial rope use */
#define JS_STRING_ROPE_SHORT2_LEN 8192
/* rope depth at which we rebalance */
#define JS_STRING_ROPE_MAX_DEPTH 60

#define __exception __attribute__((warn_unused_result))
