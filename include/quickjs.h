/*
 * QuickJS Javascript Engine
 *
 * Copyright (c) 2017-2026 Fabrice Bellard
 * Copyright (c) 2026 Yan-Jie Wang
 * Copyright (c) 2017-2025 Charlie Gordon
 * Copyright (c) 2023-2026 Ben Noordhuis
 * Copyright (c) 2023-2026 Saúl Ibarra Corretgé
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
#ifndef QUICKJS_H
#define QUICKJS_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define js_likely(x)          __builtin_expect(!!(x), 1)
#define js_unlikely(x)        __builtin_expect(!!(x), 0)
#define js_force_inline       inline __attribute__((always_inline))
#define __js_printf_like(f, a)   __attribute__((format(printf, f, a)))
#else
#define js_likely(x)     (x)
#define js_unlikely(x)   (x)
#define js_force_inline  inline
#define __js_printf_like(a, b)
#endif

/* Public callable symbols, including helpers used by public inline functions.
   Consumers can link either the static archive or a shared-library import lib. */
#if defined(_WIN32) && defined(JS_BUILD_SHARED_LIBRARY)
#define JS_EXTERN __declspec(dllexport)
#elif defined(__GNUC__) && !defined(_WIN32)
#define JS_EXTERN __attribute__((visibility("default")))
#else
#define JS_EXTERN
#endif

#define JS_BOOL int

typedef struct JSRuntime JSRuntime;
typedef struct JSContext JSContext;
typedef void JSRuntimeFinalizer(JSRuntime *rt, void *opaque);
typedef struct JSClass JSClass;
typedef uint32_t JSClassID;
typedef uint32_t JSAtom;

#if INTPTR_MAX >= INT64_MAX
#define JS_PTR64
#define JS_PTR64_DEF(a) a
#else
#define JS_PTR64_DEF(a)
#endif

#ifndef JS_PTR64
#define JS_NAN_BOXING
#endif

#if defined(__SIZEOF_INT128__) && (INTPTR_MAX >= INT64_MAX)
#define JS_LIMB_BITS 64
#else
#define JS_LIMB_BITS 32
#endif

#define JS_SHORT_BIG_INT_BITS JS_LIMB_BITS
    
enum {
    /* all tags with a reference count are negative */
    JS_TAG_FIRST       = -9, /* first negative tag */
    JS_TAG_BIG_INT     = -9,
    JS_TAG_SYMBOL      = -8,
    JS_TAG_STRING      = -7,
    JS_TAG_STRING_ROPE = -6,
    JS_TAG_MODULE      = -3, /* used internally */
    JS_TAG_FUNCTION_BYTECODE = -2, /* used internally */
    JS_TAG_OBJECT      = -1,

    JS_TAG_INT         = 0,
    JS_TAG_BOOL        = 1,
    JS_TAG_NULL        = 2,
    JS_TAG_UNDEFINED   = 3,
    JS_TAG_UNINITIALIZED = 4,
    JS_TAG_CATCH_OFFSET = 5,
    JS_TAG_EXCEPTION   = 6,
    JS_TAG_SHORT_BIG_INT = 7,
    JS_TAG_FLOAT64     = 8,
    /* any larger tag is FLOAT64 if JS_NAN_BOXING */
};

/* must match the layout of 'JSMallocBlockHeader' */
typedef struct JSRefCountHeader {
    int ref_count;
} JSRefCountHeader;

#define JS_FLOAT64_NAN NAN

#ifdef CONFIG_CHECK_JSVALUE
/* JSValue consistency : it is not possible to run the code in this
   mode, but it is useful to detect simple reference counting
   errors. It would be interesting to modify a static C analyzer to
   handle specific annotations (clang has such annotations but only
   for objective C) */
typedef struct __JSValue *JSValue;
typedef const struct __JSValue *JSValueConst;

#define JS_VALUE_GET_TAG(v) (int)((uintptr_t)(v) & 0xf)
/* same as JS_VALUE_GET_TAG, but return JS_TAG_FLOAT64 with NaN boxing */
#define JS_VALUE_GET_NORM_TAG(v) JS_VALUE_GET_TAG(v)
#define JS_VALUE_GET_INT(v) (int)((intptr_t)(v) >> 4)
#define JS_VALUE_GET_BOOL(v) JS_VALUE_GET_INT(v)
#define JS_VALUE_GET_FLOAT64(v) (double)JS_VALUE_GET_INT(v)
#define JS_VALUE_GET_SHORT_BIG_INT(v) JS_VALUE_GET_INT(v)
#define JS_VALUE_GET_PTR(v) (void *)((intptr_t)(v) & ~0xf)

#define JS_MKVAL(tag, val) (JSValue)(intptr_t)(((val) << 4) | (tag))
#define JS_MKPTR(tag, p) (JSValue)((intptr_t)(p) | (tag))

#define JS_TAG_IS_FLOAT64(tag) ((unsigned)(tag) == JS_TAG_FLOAT64)

#define JS_NAN JS_MKVAL(JS_TAG_FLOAT64, 1)

static inline JSValue __JS_NewFloat64(JSContext *ctx, double d)
{
    return JS_MKVAL(JS_TAG_FLOAT64, (int)d);
}

static inline JS_BOOL JS_VALUE_IS_NAN(JSValue v)
{
    return 0;
}

static inline JSValue __JS_NewShortBigInt(int32_t d)
{
    return JS_MKVAL(JS_TAG_SHORT_BIG_INT, d);
}

#elif defined(JS_NAN_BOXING)

typedef uint64_t JSValue;

#define JSValueConst JSValue

#define JS_VALUE_GET_TAG(v) (int)((v) >> 32)
#define JS_VALUE_GET_INT(v) (int)(v)
#define JS_VALUE_GET_BOOL(v) (int)(v)
#define JS_VALUE_GET_SHORT_BIG_INT(v) (int)(v)
#define JS_VALUE_GET_PTR(v) (void *)(intptr_t)(v)

#define JS_MKVAL(tag, val) (((uint64_t)(tag) << 32) | (uint32_t)(val))
#define JS_MKPTR(tag, ptr) (((uint64_t)(tag) << 32) | (uintptr_t)(ptr))

#define JS_FLOAT64_TAG_ADDEND (0x7ff80000 - JS_TAG_FIRST + 1) /* quiet NaN encoding */

static inline double JS_VALUE_GET_FLOAT64(JSValue v)
{
    union {
        JSValue v;
        double d;
    } u;
    u.v = v;
    u.v += (uint64_t)JS_FLOAT64_TAG_ADDEND << 32;
    return u.d;
}

#define JS_NAN (0x7ff8000000000000 - ((uint64_t)JS_FLOAT64_TAG_ADDEND << 32))

static inline JSValue __JS_NewFloat64(JSContext *ctx, double d)
{
    union {
        double d;
        uint64_t u64;
    } u;
    JSValue v;
    u.d = d;
    /* normalize NaN */
    if (js_unlikely((u.u64 & 0x7fffffffffffffff) > 0x7ff0000000000000))
        v = JS_NAN;
    else
        v = u.u64 - ((uint64_t)JS_FLOAT64_TAG_ADDEND << 32);
    return v;
}

#define JS_TAG_IS_FLOAT64(tag) ((unsigned)((tag) - JS_TAG_FIRST) >= (JS_TAG_FLOAT64 - JS_TAG_FIRST))

/* same as JS_VALUE_GET_TAG, but return JS_TAG_FLOAT64 with NaN boxing */
static inline int JS_VALUE_GET_NORM_TAG(JSValue v)
{
    uint32_t tag;
    tag = JS_VALUE_GET_TAG(v);
    if (JS_TAG_IS_FLOAT64(tag))
        return JS_TAG_FLOAT64;
    else
        return tag;
}

static inline JS_BOOL JS_VALUE_IS_NAN(JSValue v)
{
    uint32_t tag;
    tag = JS_VALUE_GET_TAG(v);
    return tag == (JS_NAN >> 32);
}

static inline JSValue __JS_NewShortBigInt(int32_t d)
{
    return JS_MKVAL(JS_TAG_SHORT_BIG_INT, d);
}

#else /* !JS_NAN_BOXING */

typedef union JSValueUnion {
    uint64_t uint64;
    double float64;
    void *ptr;
#if JS_SHORT_BIG_INT_BITS == 32
    int32_t short_big_int;
#else
    int64_t short_big_int;
#endif
} JSValueUnion;

typedef struct JSValue {
    JSValueUnion u;
    int64_t tag;
} JSValue;

#define JSValueConst JSValue

#define JS_VALUE_GET_TAG(v) ((int32_t)(v).tag)
/* same as JS_VALUE_GET_TAG, but return JS_TAG_FLOAT64 with NaN boxing */
#define JS_VALUE_GET_NORM_TAG(v) JS_VALUE_GET_TAG(v)
#define JS_VALUE_GET_INT(v) ((int)(v).u.uint64)
#define JS_VALUE_GET_BOOL(v) ((int)(v).u.uint64)
#define JS_VALUE_GET_FLOAT64(v) ((v).u.float64)
#define JS_VALUE_GET_SHORT_BIG_INT(v) ((v).u.short_big_int)
#define JS_VALUE_GET_PTR(v) ((v).u.ptr)

/* avoid uninitialized data by using a 64 bit field even if only 32
   bits are needed because some compilers generate slower code */
#define JS_MKVAL(tag, val) (JSValue){ (JSValueUnion){ .uint64 = (uint32_t)(val) }, tag }
#define JS_MKPTR(tag, p) (JSValue){ (JSValueUnion){ .ptr = p }, tag }

#define JS_TAG_IS_FLOAT64(tag) ((unsigned)(tag) == JS_TAG_FLOAT64)

#define JS_NAN (JSValue){ .u.float64 = JS_FLOAT64_NAN, JS_TAG_FLOAT64 }

static inline JSValue __JS_NewFloat64(JSContext *ctx, double d)
{
    JSValue v;
    v.tag = JS_TAG_FLOAT64;
    v.u.float64 = d;
    return v;
}

static inline JS_BOOL JS_VALUE_IS_NAN(JSValue v)
{
    union {
        double d;
        uint64_t u64;
    } u;
    if (v.tag != JS_TAG_FLOAT64)
        return 0;
    u.d = v.u.float64;
    return (u.u64 & 0x7fffffffffffffff) > 0x7ff0000000000000;
}

static inline JSValue __JS_NewShortBigInt(int64_t d)
{
    JSValue v;
    v.tag = JS_TAG_SHORT_BIG_INT;
    v.u.short_big_int = d;
    return v;
}

#endif /* !JS_NAN_BOXING */

#define JS_VALUE_IS_BOTH_INT(v1, v2) ((JS_VALUE_GET_TAG(v1) | JS_VALUE_GET_TAG(v2)) == 0)
#define JS_VALUE_IS_BOTH_FLOAT(v1, v2) (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(v1)) && JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(v2)))

#define JS_VALUE_HAS_REF_COUNT(v) ((unsigned)JS_VALUE_GET_TAG(v) >= (unsigned)JS_TAG_FIRST)

/* special values */
#define JS_NULL      JS_MKVAL(JS_TAG_NULL, 0)
#define JS_UNDEFINED JS_MKVAL(JS_TAG_UNDEFINED, 0)
#define JS_FALSE     JS_MKVAL(JS_TAG_BOOL, 0)
#define JS_TRUE      JS_MKVAL(JS_TAG_BOOL, 1)
#define JS_EXCEPTION JS_MKVAL(JS_TAG_EXCEPTION, 0)
#define JS_UNINITIALIZED JS_MKVAL(JS_TAG_UNINITIALIZED, 0)

/* flags for object properties */
#define JS_PROP_CONFIGURABLE  (1 << 0)
#define JS_PROP_WRITABLE      (1 << 1)
#define JS_PROP_ENUMERABLE    (1 << 2)
#define JS_PROP_C_W_E         (JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE | JS_PROP_ENUMERABLE)
#define JS_PROP_LENGTH        (1 << 3) /* used internally in Arrays */
#define JS_PROP_TMASK         (3 << 4) /* mask for NORMAL, GETSET, VARREF, AUTOINIT */
#define JS_PROP_NORMAL         (0 << 4)
#define JS_PROP_GETSET         (1 << 4)
#define JS_PROP_VARREF         (2 << 4) /* used internally */
#define JS_PROP_AUTOINIT       (3 << 4) /* used internally */

/* flags for JS_DefineProperty */
#define JS_PROP_HAS_SHIFT        8
#define JS_PROP_HAS_CONFIGURABLE (1 << 8)
#define JS_PROP_HAS_WRITABLE     (1 << 9)
#define JS_PROP_HAS_ENUMERABLE   (1 << 10)
#define JS_PROP_HAS_GET          (1 << 11)
#define JS_PROP_HAS_SET          (1 << 12)
#define JS_PROP_HAS_VALUE        (1 << 13)

/* throw an exception if false would be returned
   (JS_DefineProperty/JS_SetProperty) */
#define JS_PROP_THROW            (1 << 14)
/* throw an exception if false would be returned in strict mode
   (JS_SetProperty) */
#define JS_PROP_THROW_STRICT     (1 << 15)

#define JS_PROP_NO_EXOTIC        (1 << 16) /* internal use */

#ifndef JS_DEFAULT_STACK_SIZE
#define JS_DEFAULT_STACK_SIZE (1024 * 1024)
#endif

/* JS_Eval() flags */
#define JS_EVAL_TYPE_GLOBAL   (0 << 0) /* global code (default) */
#define JS_EVAL_TYPE_MODULE   (1 << 0) /* module code */
#define JS_EVAL_TYPE_DIRECT   (2 << 0) /* direct call (internal use) */
#define JS_EVAL_TYPE_INDIRECT (3 << 0) /* indirect call (internal use) */
#define JS_EVAL_TYPE_MASK     (3 << 0)

#define JS_EVAL_FLAG_STRICT   (1 << 3) /* force 'strict' mode */
/* compile but do not run. The result is an object with a
   JS_TAG_FUNCTION_BYTECODE or JS_TAG_MODULE tag. It can be executed
   with JS_EvalFunction(). */
#define JS_EVAL_FLAG_COMPILE_ONLY (1 << 5)
/* don't include the stack frames before this eval in the Error() backtraces */
#define JS_EVAL_FLAG_BACKTRACE_BARRIER (1 << 6)
/* allow top-level await in normal script. JS_Eval() returns a
   promise. Only allowed with JS_EVAL_TYPE_GLOBAL */
#define JS_EVAL_FLAG_ASYNC (1 << 7)

typedef JSValue JSCFunction(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
typedef JSValue JSCFunctionMagic(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
typedef JSValue JSCFunctionData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);
typedef JSValue JSCClosure(JSContext *ctx, JSValueConst this_val, int argc,
                          JSValueConst *argv, int magic, void *opaque);
typedef void JSCClosureFinalizerFunc(void *opaque);

typedef struct JSMallocState {
    size_t malloc_count;
    size_t malloc_size;
    size_t malloc_limit;
    void *opaque; /* user opaque */
} JSMallocState;

typedef struct JSMallocFunctions {
    void *(*js_malloc)(JSMallocState *s, size_t size);
    void (*js_free)(JSMallocState *s, void *ptr);
    void *(*js_realloc)(JSMallocState *s, void *ptr, size_t size);
    size_t (*js_malloc_usable_size)(const void *ptr);
} JSMallocFunctions;

typedef struct JSGCObjectHeader JSGCObjectHeader;

/* Library-owned build version text; not an ABI or feature-capability query. */
JS_EXTERN const char *JS_GetVersion(void);
JS_EXTERN JSRuntime *JS_NewRuntime(void);
/* info lifetime must exceed that of rt */
JS_EXTERN void JS_SetRuntimeInfo(JSRuntime *rt, const char *info);
JS_EXTERN void JS_SetMemoryLimit(JSRuntime *rt, size_t limit);
JS_EXTERN void JS_SetGCThreshold(JSRuntime *rt, size_t gc_threshold);
/* Current threshold, including adjustments made by automatic GC. */
JS_EXTERN size_t JS_GetGCThreshold(JSRuntime *rt);
/* use 0 to disable maximum stack size check */
JS_EXTERN void JS_SetMaxStackSize(JSRuntime *rt, size_t stack_size);
/* should be called when changing thread to update the stack top value
   used to check stack overflow. */
JS_EXTERN void JS_UpdateStackTop(JSRuntime *rt);
JS_EXTERN JSRuntime *JS_NewRuntime2(const JSMallocFunctions *mf, void *opaque);
JS_EXTERN void JS_FreeRuntime(JSRuntime *rt);
JS_EXTERN void *JS_GetRuntimeOpaque(JSRuntime *rt);
JS_EXTERN void JS_SetRuntimeOpaque(JSRuntime *rt, void *opaque);
/* Register C-only cleanup, called once in reverse registration order near the
   end of JS_FreeRuntime. Return -1 without registering on failure or teardown.
   Callbacks may free native storage, but cannot use JS state or destroy the
   allocator; it remains in use until JS_FreeRuntime returns. */
JS_EXTERN int JS_AddRuntimeFinalizer(JSRuntime *rt, JSRuntimeFinalizer *finalizer,
                                   void *opaque);
typedef void JS_MarkFunc(JSRuntime *rt, JSGCObjectHeader *gp);
JS_EXTERN void JS_MarkValue(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JS_EXTERN void JS_RunGC(JSRuntime *rt);
/* Release WeakRef targets after a job/microtask checkpoint. Does nothing
   while JavaScript is executing; JS_RunGC itself preserves these roots. */
JS_EXTERN void JS_ClearKeptObjects(JSRuntime *rt);
JS_EXTERN JS_BOOL JS_IsLiveObject(JSRuntime *rt, JSValueConst obj);

JS_EXTERN JSContext *JS_NewContext(JSRuntime *rt);
JS_EXTERN void JS_FreeContext(JSContext *s);
JS_EXTERN JSContext *JS_DupContext(JSContext *ctx);
JS_EXTERN void *JS_GetContextOpaque(JSContext *ctx);
JS_EXTERN void JS_SetContextOpaque(JSContext *ctx, void *opaque);
JS_EXTERN JSRuntime *JS_GetRuntime(JSContext *ctx);
JS_EXTERN void JS_SetClassProto(JSContext *ctx, JSClassID class_id, JSValue obj);
JS_EXTERN JSValue JS_GetClassProto(JSContext *ctx, JSClassID class_id);

/* Set this runtime's shared Date/Temporal/Intl default zone. The identifier is
   copied; failure preserves the old setting and reports an exception.
   A NULL or unsupported identifier throws RangeError. Disabled builds throw
   TypeError when neither Temporal nor Intl is enabled. Raw contexts require
   base objects for exception construction. */
JS_EXTERN int JS_SetDefaultTimeZone(JSContext *ctx, const char *time_zone);
/* Return an owned identifier string. Before an explicit setting, snapshot
   the discovered system default on first use, subject to the build's
   timezone capabilities and UTC or current-host-offset fallback policy.
   Disabled builds throw TypeError. */
JS_EXTERN JSValue JS_GetDefaultTimeZone(JSContext *ctx);

/* the following functions are used to select the intrinsic object to
   save memory */
JS_EXTERN JSContext *JS_NewContextRaw(JSRuntime *rt);
JS_EXTERN int JS_AddIntrinsicBaseObjects(JSContext *ctx);
JS_EXTERN int JS_AddIntrinsicDate(JSContext *ctx);
/* Publish this realm's compiled Intl intrinsics. Raw contexts require base
   objects. Idempotent; returns -1 with an exception on failure, including
   TypeError when Intl is disabled. */
JS_EXTERN int JS_AddIntrinsicIntl(JSContext *ctx);
/* Select this runtime's canonical, supported default locale by prefix lookup;
   extensions are removed. Unsupported or malformed names throw RangeError.
   Failure preserves the old setting. NULL resets lazy host discovery.
   Raw contexts require base objects; Intl-disabled builds throw TypeError. */
JS_EXTERN int JS_SetIntlDefaultLocale(JSContext *ctx, const char *locale);
/* Return an owned string. Host discovery is snapshotted on first use and falls
   back to English when missing, invalid or unsupported by the data profile. */
JS_EXTERN JSValue JS_GetIntlDefaultLocale(JSContext *ctx);
/* Initialize native Temporal intrinsics and publish the namespace. Raw
   contexts require base objects. Idempotent; returns -1 with an exception
   on failure, including TypeError in a disabled build. */
JS_EXTERN int JS_AddIntrinsicTemporal(JSContext *ctx);
JS_EXTERN int JS_AddIntrinsicEval(JSContext *ctx);
JS_EXTERN int JS_AddIntrinsicStringNormalize(JSContext *ctx);
JS_EXTERN void JS_AddIntrinsicRegExpCompiler(JSContext *ctx);
JS_EXTERN int JS_AddIntrinsicRegExp(JSContext *ctx);
JS_EXTERN int JS_AddIntrinsicJSON(JSContext *ctx);
JS_EXTERN int JS_AddIntrinsicProxy(JSContext *ctx);
JS_EXTERN int JS_AddIntrinsicMapSet(JSContext *ctx);
JS_EXTERN int JS_AddIntrinsicTypedArrays(JSContext *ctx);
JS_EXTERN int JS_AddIntrinsicPromise(JSContext *ctx);
JS_EXTERN int JS_AddIntrinsicWeakRef(JSContext *ctx);

JS_EXTERN JSValue js_string_codePointRange(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv);

JS_EXTERN void *js_malloc_rt(JSRuntime *rt, size_t size);
JS_EXTERN void js_free_rt(JSRuntime *rt, void *ptr);
JS_EXTERN void *js_realloc_rt(JSRuntime *rt, void *ptr, size_t size);
JS_EXTERN size_t js_malloc_usable_size_rt(JSRuntime *rt, const void *ptr);
JS_EXTERN void *js_mallocz_rt(JSRuntime *rt, size_t size);

JS_EXTERN void *(js_malloc)(JSContext *ctx, size_t size);
JS_EXTERN void (js_free)(JSContext *ctx, void *ptr);
JS_EXTERN void *(js_realloc)(JSContext *ctx, void *ptr, size_t size);
JS_EXTERN size_t (js_malloc_usable_size)(JSContext *ctx, const void *ptr);
JS_EXTERN void *(js_realloc2)(JSContext *ctx, void *ptr, size_t size, size_t *pslack);
JS_EXTERN void *(js_mallocz)(JSContext *ctx, size_t size);
JS_EXTERN char *js_strdup(JSContext *ctx, const char *str);
JS_EXTERN char *js_strndup(JSContext *ctx, const char *s, size_t n);

typedef struct JSMemoryUsage {
    int64_t malloc_size, malloc_limit, memory_used_size;
    int64_t malloc_count;
    int64_t memory_used_count;
    int64_t atom_count, atom_size;
    int64_t str_count, str_size;
    int64_t obj_count, obj_size;
    int64_t prop_count, prop_size;
    int64_t shape_count, shape_size;
    int64_t js_func_count, js_func_size, js_func_code_size;
    int64_t js_func_pc2line_count, js_func_pc2line_size;
    int64_t c_func_count, array_count;
    int64_t fast_array_count, fast_array_elements;
    int64_t binary_object_count, binary_object_size;
} JSMemoryUsage;

JS_EXTERN void JS_ComputeMemoryUsage(JSRuntime *rt, JSMemoryUsage *s);
JS_EXTERN void JS_DumpMemoryUsage(FILE *fp, const JSMemoryUsage *s, JSRuntime *rt);

/* atom support */
#define JS_ATOM_NULL 0

JS_EXTERN JSAtom JS_NewAtomLen(JSContext *ctx, const char *str, size_t len);
JS_EXTERN JSAtom JS_NewAtom(JSContext *ctx, const char *str);
JS_EXTERN JSAtom JS_NewAtomUInt32(JSContext *ctx, uint32_t n);
/* Exact signed decimal key; returns an owned atom or JS_ATOM_NULL
   with an exception. Release the result with JS_FreeAtom. */
JS_EXTERN JSAtom JS_NewAtomInt64(JSContext *ctx, int64_t n);
JS_EXTERN JSAtom JS_DupAtom(JSContext *ctx, JSAtom v);
/* Retains an atom from rt without allocation. */
JS_EXTERN JSAtom JS_DupAtomRT(JSRuntime *rt, JSAtom v);
JS_EXTERN void JS_FreeAtom(JSContext *ctx, JSAtom v);
JS_EXTERN void JS_FreeAtomRT(JSRuntime *rt, JSAtom v);
JS_EXTERN JSValue JS_AtomToValue(JSContext *ctx, JSAtom atom);
JS_EXTERN JSValue JS_AtomToString(JSContext *ctx, JSAtom atom);
JS_EXTERN const char *JS_AtomToCStringLen(JSContext *ctx, size_t *plen, JSAtom atom);
static inline const char *JS_AtomToCString(JSContext *ctx, JSAtom atom)
{
    return JS_AtomToCStringLen(ctx, NULL, atom);
}
JS_EXTERN JSAtom JS_ValueToAtom(JSContext *ctx, JSValueConst val);

/* object class support */

typedef struct JSPropertyEnum {
    JS_BOOL is_enumerable;
    JSAtom atom;
} JSPropertyEnum;

typedef struct JSPropertyDescriptor {
    int flags;
    JSValue value;
    JSValue getter;
    JSValue setter;
} JSPropertyDescriptor;

typedef struct JSClassExoticMethods {
    /* Return -1 if exception (can only happen in case of Proxy object),
       FALSE if the property does not exists, TRUE if it exists. If 1 is
       returned, the property descriptor 'desc' is filled if != NULL. */
    int (*get_own_property)(JSContext *ctx, JSPropertyDescriptor *desc,
                             JSValueConst obj, JSAtom prop);
    /* '*ptab' should hold the '*plen' property keys. Return 0 if OK,
       -1 if exception. The 'is_enumerable' field is ignored.
    */
    int (*get_own_property_names)(JSContext *ctx, JSPropertyEnum **ptab,
                                  uint32_t *plen,
                                  JSValueConst obj);
    /* return < 0 if exception, or TRUE/FALSE */
    int (*delete_property)(JSContext *ctx, JSValueConst obj, JSAtom prop);
    /* return < 0 if exception or TRUE/FALSE */
    int (*define_own_property)(JSContext *ctx, JSValueConst this_obj,
                               JSAtom prop, JSValueConst val,
                               JSValueConst getter, JSValueConst setter,
                               int flags);
    /* The following methods can be emulated with the previous ones,
       so they are usually not needed */
    /* return < 0 if exception or TRUE/FALSE */
    int (*has_property)(JSContext *ctx, JSValueConst obj, JSAtom atom);
    JSValue (*get_property)(JSContext *ctx, JSValueConst obj, JSAtom atom,
                            JSValueConst receiver);
    /* return < 0 if exception or TRUE/FALSE */
    int (*set_property)(JSContext *ctx, JSValueConst obj, JSAtom atom,
                        JSValueConst value, JSValueConst receiver, int flags);

    /* To get a consistent object behavior when get_prototype != NULL,
       get_property, set_property and set_prototype must be != NULL
       and the object must be created with a JS_NULL prototype. */
    JSValue (*get_prototype)(JSContext *ctx, JSValueConst obj);
    /* return < 0 if exception or TRUE/FALSE */
    int (*set_prototype)(JSContext *ctx, JSValueConst obj, JSValueConst proto_val);
    /* return < 0 if exception or TRUE/FALSE */
    int (*is_extensible)(JSContext *ctx, JSValueConst obj);
    /* return < 0 if exception or TRUE/FALSE */
    int (*prevent_extensions)(JSContext *ctx, JSValueConst obj);
} JSClassExoticMethods;

typedef void JSClassFinalizer(JSRuntime *rt, JSValue val);
typedef void JSClassGCMark(JSRuntime *rt, JSValueConst val,
                           JS_MarkFunc *mark_func);
#define JS_CALL_FLAG_CONSTRUCTOR (1 << 0)
typedef JSValue JSClassCall(JSContext *ctx, JSValueConst func_obj,
                            JSValueConst this_val, int argc, JSValueConst *argv,
                            int flags);

typedef struct JSClassDef {
    const char *class_name;
    JSClassFinalizer *finalizer;
    JSClassGCMark *gc_mark;
    /* if call != NULL, the object is a function. If (flags &
       JS_CALL_FLAG_CONSTRUCTOR) != 0, the function is called as a
       constructor. In this case, 'this_val' is new.target. A
       constructor call only happens if the object constructor bit is
       set (see JS_SetConstructorBit()). */
    JSClassCall *call;
    /* XXX: suppress this indirection ? It is here only to save memory
       because only a few classes need these methods */
    JSClassExoticMethods *exotic;
} JSClassDef;

#define JS_INVALID_CLASS_ID 0
JS_EXTERN JSClassID JS_NewClassID(JSClassID *pclass_id);
/* Returns the class ID if `v` is an object, otherwise returns JS_INVALID_CLASS_ID. */
JS_EXTERN JSClassID JS_GetClassID(JSValue v);
JS_EXTERN int JS_NewClass(JSRuntime *rt, JSClassID class_id, const JSClassDef *class_def);
JS_EXTERN int JS_IsRegisteredClass(JSRuntime *rt, JSClassID class_id);
/* Owned atom, released with JS_FreeAtomRT; JS_ATOM_NULL for unregistered IDs. */
JS_EXTERN JSAtom JS_GetClassName(JSRuntime *rt, JSClassID class_id);

/* value handling */

static js_force_inline JSValue JS_NewBool(JSContext *ctx, JS_BOOL val)
{
    return JS_MKVAL(JS_TAG_BOOL, (val != 0));
}

static js_force_inline JSValue JS_NewInt32(JSContext *ctx, int32_t val)
{
    return JS_MKVAL(JS_TAG_INT, val);
}

static js_force_inline JSValue JS_NewInt64(JSContext *ctx, int64_t val)
{
    JSValue v;
    if (val == (int32_t)val) {
        v = JS_NewInt32(ctx, val);
    } else {
        v = __JS_NewFloat64(ctx, val);
    }
    return v;
}

static js_force_inline JSValue JS_NewUint32(JSContext *ctx, uint32_t val)
{
    JSValue v;
    if (val <= 0x7fffffff) {
        v = JS_NewInt32(ctx, val);
    } else {
        v = __JS_NewFloat64(ctx, val);
    }
    return v;
}

JS_EXTERN JSValue JS_NewBigInt64(JSContext *ctx, int64_t v);
JS_EXTERN JSValue JS_NewBigUint64(JSContext *ctx, uint64_t v);

static js_force_inline JSValue JS_NewFloat64(JSContext *ctx, double d)
{
    int32_t val;
    union {
        double d;
        uint64_t u;
    } u, t;
    if (d >= INT32_MIN && d <= INT32_MAX) {
        u.d = d;
        val = (int32_t)d;
        t.d = val;
        /* -0 cannot be represented as integer, so we compare the bit
           representation */
        if (u.u == t.u)
            return JS_MKVAL(JS_TAG_INT, val);
    }
    return __JS_NewFloat64(ctx, d);
}

static inline JS_BOOL JS_IsNumber(JSValueConst v)
{
    int tag = JS_VALUE_GET_TAG(v);
    return tag == JS_TAG_INT || JS_TAG_IS_FLOAT64(tag);
}

static inline JS_BOOL JS_IsBigInt(JSContext *ctx, JSValueConst v)
{
    int tag = JS_VALUE_GET_TAG(v);
    return tag == JS_TAG_BIG_INT || tag == JS_TAG_SHORT_BIG_INT;
}

static inline JS_BOOL JS_IsBool(JSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_BOOL;
}

static inline JS_BOOL JS_IsNull(JSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_NULL;
}

static inline JS_BOOL JS_IsUndefined(JSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_UNDEFINED;
}

static inline JS_BOOL JS_IsException(JSValueConst v)
{
    return js_unlikely(JS_VALUE_GET_TAG(v) == JS_TAG_EXCEPTION);
}

static inline JS_BOOL JS_IsUninitialized(JSValueConst v)
{
    return js_unlikely(JS_VALUE_GET_TAG(v) == JS_TAG_UNINITIALIZED);
}

static inline JS_BOOL JS_IsString(JSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_STRING ||
        JS_VALUE_GET_TAG(v) == JS_TAG_STRING_ROPE;
}

static inline JS_BOOL JS_IsSymbol(JSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_SYMBOL;
}

static inline JS_BOOL JS_IsObject(JSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_OBJECT;
}

/* Nonobservable builtin brands: no property access or Proxy unwrapping.
   Subclasses and detached/out-of-bounds views retain their brands.
   IsProxy includes revoked proxies. IsAsyncFunction excludes bound functions,
   async generators and proxies. IsRegExp does not inspect Symbol.match.
   Unlike these predicates, JS_IsArray retains its proxy-aware behavior. */
JS_EXTERN JS_BOOL JS_IsMap(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsSet(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsWeakMap(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsWeakSet(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsWeakRef(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsRegExp(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsDate(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsProxy(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsArrayBuffer(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsSharedArrayBuffer(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsDataView(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsPromise(JSValueConst value);
JS_EXTERN JS_BOOL JS_IsAsyncFunction(JSValueConst value);

JS_EXTERN JSValue JS_Throw(JSContext *ctx, JSValue obj);
JS_EXTERN void JS_SetUncatchableException(JSContext *ctx, JS_BOOL flag);
JS_EXTERN JSValue JS_GetException(JSContext *ctx);
JS_EXTERN JS_BOOL JS_HasException(JSContext *ctx);
JS_EXTERN JS_BOOL JS_IsError(JSContext *ctx, JSValueConst val);
JS_EXTERN JSValue JS_NewError(JSContext *ctx);
/* Formatted Error values with the context's intrinsic prototype and stack.
   Success preserves an existing pending exception. Failure returns JS_EXCEPTION
   and replaces it with the construction error. InternalError stays catchable. */
JS_EXTERN JSValue __js_printf_like(2, 3) JS_NewPlainError(JSContext *ctx, const char *fmt, ...);
JS_EXTERN JSValue __js_printf_like(2, 3) JS_NewTypeError(JSContext *ctx, const char *fmt, ...);
JS_EXTERN JSValue __js_printf_like(2, 3) JS_NewRangeError(JSContext *ctx, const char *fmt, ...);
JS_EXTERN JSValue __js_printf_like(2, 3) JS_NewReferenceError(JSContext *ctx, const char *fmt, ...);
JS_EXTERN JSValue __js_printf_like(2, 3) JS_NewSyntaxError(JSContext *ctx, const char *fmt, ...);
JS_EXTERN JSValue __js_printf_like(2, 3) JS_NewInternalError(JSContext *ctx, const char *fmt, ...);
JS_EXTERN JSValue __js_printf_like(2, 3) JS_ThrowSyntaxError(JSContext *ctx, const char *fmt, ...);
JS_EXTERN JSValue __js_printf_like(2, 3) JS_ThrowTypeError(JSContext *ctx, const char *fmt, ...);
JS_EXTERN JSValue __js_printf_like(2, 3) JS_ThrowReferenceError(JSContext *ctx, const char *fmt, ...);
JS_EXTERN JSValue __js_printf_like(2, 3) JS_ThrowRangeError(JSContext *ctx, const char *fmt, ...);
JS_EXTERN JSValue __js_printf_like(2, 3) JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...);
JS_EXTERN JSValue JS_ThrowOutOfMemory(JSContext *ctx);

JS_EXTERN void __JS_FreeValue(JSContext *ctx, JSValue v);

static inline JSRefCountHeader *__js_rc(void *ptr)
{
    return (JSRefCountHeader *)((uint32_t *)ptr - 1);
}

static inline void JS_FreeValue(JSContext *ctx, JSValue v)
{
    if (JS_VALUE_HAS_REF_COUNT(v)) {
        JSRefCountHeader *p = __js_rc(JS_VALUE_GET_PTR(v));
        if (--p->ref_count <= 0) {
            __JS_FreeValue(ctx, v);
        }
    }
}
JS_EXTERN void __JS_FreeValueRT(JSRuntime *rt, JSValue v);
static inline void JS_FreeValueRT(JSRuntime *rt, JSValue v)
{
    if (JS_VALUE_HAS_REF_COUNT(v)) {
        JSRefCountHeader *p = __js_rc(JS_VALUE_GET_PTR(v));
        if (--p->ref_count <= 0) {
            __JS_FreeValueRT(rt, v);
        }
    }
}

/* Shared implementation for callers that do not need a context or runtime. */
static inline JSValue __JS_DupValue(JSValueConst v)
{
    if (JS_VALUE_HAS_REF_COUNT(v)) {
        JSRefCountHeader *p = __js_rc(JS_VALUE_GET_PTR(v));
        p->ref_count++;
    }
    return (JSValue)v;
}

static inline JSValue JS_DupValue(JSContext *ctx, JSValueConst v)
{
    return __JS_DupValue(v);
}

static inline JSValue JS_DupValueRT(JSRuntime *rt, JSValueConst v)
{
    return __JS_DupValue(v);
}

JS_EXTERN JS_BOOL JS_StrictEq(JSContext *ctx, JSValueConst op1, JSValueConst op2);
JS_EXTERN JS_BOOL JS_SameValue(JSContext *ctx, JSValueConst op1, JSValueConst op2);
JS_EXTERN JS_BOOL JS_SameValueZero(JSContext *ctx, JSValueConst op1, JSValueConst op2);

JS_EXTERN JSValue JS_ToObject(JSContext *ctx, JSValueConst val);
#define JS_HINT_STRING  0
#define JS_HINT_NUMBER  1
#define JS_HINT_NONE    2
JS_EXTERN JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);
JS_EXTERN int JS_ToBool(JSContext *ctx, JSValueConst val); /* return -1 for JS_EXCEPTION */
JS_EXTERN int JS_ToInt32(JSContext *ctx, int32_t *pres, JSValueConst val);
static inline int JS_ToUint32(JSContext *ctx, uint32_t *pres, JSValueConst val)
{
    return JS_ToInt32(ctx, (int32_t*)pres, val);
}
JS_EXTERN int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val);
JS_EXTERN int JS_ToInt64(JSContext *ctx, int64_t *pres, JSValueConst val);
JS_EXTERN int JS_ToIndex(JSContext *ctx, uint64_t *plen, JSValueConst val);
/* Standard ToNumber: borrows val, returns an owned number or JS_EXCEPTION. */
JS_EXTERN JSValue JS_ToNumber(JSContext *ctx, JSValueConst val);
JS_EXTERN int JS_ToFloat64(JSContext *ctx, double *pres, JSValueConst val);
/* return an exception if 'val' is a Number */
JS_EXTERN int JS_ToBigInt64(JSContext *ctx, int64_t *pres, JSValueConst val);
/* same as JS_ToInt64() but allow BigInt */
JS_EXTERN int JS_ToInt64Ext(JSContext *ctx, int64_t *pres, JSValueConst val);
JS_EXTERN JSValue JS_NewBigInt128(JSContext *ctx, uint64_t low, uint64_t high);
JS_EXTERN int JS_ToBigInt128(JSContext *ctx, uint64_t *plow, uint64_t *phigh, JSValueConst val);
JS_EXTERN int JS_ToBigInt128Sat(JSContext *ctx, uint64_t *plow, uint64_t *phigh, JSValueConst val);

JS_EXTERN JSValue JS_NewStringLen(JSContext *ctx, const char *str1, size_t len1);
static inline JSValue JS_NewString(JSContext *ctx, const char *str)
{
    return JS_NewStringLen(ctx, str, strlen(str));
}
/* Copies len native-endian UTF-16 code units, preserving embedded NULs
   and unmatched surrogates. buf may be NULL only when len is zero.
   Returns an owned string or JS_EXCEPTION; oversized len throws RangeError. */
JS_EXTERN JSValue JS_NewStringUTF16(JSContext *ctx, const uint16_t *buf, size_t len);
JS_EXTERN JSValue JS_NewAtomString(JSContext *ctx, const char *str);
JS_EXTERN JSValue JS_ToString(JSContext *ctx, JSValueConst val);
JS_EXTERN JSValue JS_ToPropertyKey(JSContext *ctx, JSValueConst val);
JS_EXTERN const char *JS_ToCStringLen2(JSContext *ctx, size_t *plen, JSValueConst val1, JS_BOOL cesu8);
static inline const char *JS_ToCStringLen(JSContext *ctx, size_t *plen, JSValueConst val1)
{
    return JS_ToCStringLen2(ctx, plen, val1, 0);
}
static inline const char *JS_ToCString(JSContext *ctx, JSValueConst val1)
{
    return JS_ToCStringLen2(ctx, NULL, val1, 0);
}
/* Reject embedded NUL; free a successful result with JS_FreeCString. */
JS_EXTERN const char *JS_ToCStringNZ(JSContext *ctx, JSValueConst val);
JS_EXTERN void JS_FreeCString(JSContext *ctx, const char *ptr);
/* Return a code point count, or -1 with *pbuf = NULL; free with js_free. */
JS_EXTERN int JS_ToUTF32String(JSContext *ctx, uint32_t **pbuf, JSValueConst val);
/* Releases a JS_ToCString result after its originating context is freed.
   rt must remain alive and must be the runtime that owns ptr. */
JS_EXTERN void JS_FreeCStringRT(JSRuntime *rt, const char *ptr);
/* ToString conversion, including observable user coercion. The returned
   owned UTF-16 buffer is native-endian, not NUL-terminated, and preserves
   all code units. *plen counts uint16_t units, not Unicode code points.
   On failure returns NULL, sets *plen to zero and leaves an exception.
   Release with a UTF16 free function, even after freeing the input value. */
JS_EXTERN const uint16_t *JS_ToCStringLenUTF16(JSContext *ctx, size_t *plen,
                                              JSValueConst val);
static inline const uint16_t *JS_ToCStringUTF16(JSContext *ctx,
                                               JSValueConst val)
{
    return JS_ToCStringLenUTF16(ctx, NULL, val);
}
/* NULL is allowed. ctx/rt must belong to the buffer's originating runtime.
   The runtime form permits releasing it after context destruction. */
JS_EXTERN void JS_FreeCStringUTF16(JSContext *ctx, const uint16_t *ptr);
JS_EXTERN void JS_FreeCStringRT_UTF16(JSRuntime *rt, const uint16_t *ptr);

typedef enum JSWellKnownSymbolEnum {
    JS_WELL_KNOWN_SYMBOL_ASYNC_ITERATOR = 0,
    JS_WELL_KNOWN_SYMBOL_HAS_INSTANCE = 1,
    JS_WELL_KNOWN_SYMBOL_IS_CONCAT_SPREADABLE = 2,
    JS_WELL_KNOWN_SYMBOL_ITERATOR = 3,
    JS_WELL_KNOWN_SYMBOL_MATCH = 4,
    JS_WELL_KNOWN_SYMBOL_MATCH_ALL = 5,
    JS_WELL_KNOWN_SYMBOL_REPLACE = 6,
    JS_WELL_KNOWN_SYMBOL_SEARCH = 7,
    JS_WELL_KNOWN_SYMBOL_SPECIES = 8,
    JS_WELL_KNOWN_SYMBOL_SPLIT = 9,
    JS_WELL_KNOWN_SYMBOL_TO_PRIMITIVE = 10,
    JS_WELL_KNOWN_SYMBOL_TO_STRING_TAG = 11,
    JS_WELL_KNOWN_SYMBOL_UNSCOPABLES = 12,
    JS_WELL_KNOWN_SYMBOL_DISPOSE = 13,
    JS_WELL_KNOWN_SYMBOL_ASYNC_DISPOSE = 14,
} JSWellKnownSymbolEnum;

/* Returns an owned well-known symbol without accessing global Symbol
   or invoking JavaScript. Works in raw contexts. An invalid selector
   throws RangeError and returns JS_EXCEPTION. */
JS_EXTERN JSValue JS_GetWellKnownSymbol(JSContext *ctx, JSWellKnownSymbolEnum symbol);

JS_EXTERN JSValue JS_NewSymbol(JSContext *ctx, const char *description, JS_BOOL is_global);

JS_EXTERN JSValue JS_NewObjectProtoClass(JSContext *ctx, JSValueConst proto, JSClassID class_id);
JS_EXTERN JSValue JS_NewObjectClass(JSContext *ctx, int class_id);
JS_EXTERN JSValue JS_NewObjectProto(JSContext *ctx, JSValueConst proto);
JS_EXTERN JSValue JS_NewObject(JSContext *ctx);
/* class_id must be registered in this runtime. */
JS_EXTERN JSValue JS_CreateFromConstructor(JSContext *ctx, JSValueConst ctor,
                                           int class_id);

JS_EXTERN JS_BOOL JS_IsFunction(JSContext* ctx, JSValueConst val);
JS_EXTERN JS_BOOL JS_IsConstructor(JSContext* ctx, JSValueConst val);
JS_EXTERN JS_BOOL JS_SetConstructorBit(JSContext *ctx, JSValueConst func_obj, JS_BOOL val);

JS_EXTERN JSValue JS_NewArray(JSContext *ctx);
JS_EXTERN int JS_IsArray(JSContext *ctx, JSValueConst val);

JS_EXTERN JSValue JS_NewDate(JSContext *ctx, double epoch_ms);
/* Exact signed two's-complement epoch nanoseconds, low word first as in
   JS_NewBigInt128. Creates an owned Instant without global property access.
   Raw contexts require base objects. RangeError for an invalid epoch;
   disabled builds throw TypeError. Private intrinsics initialize on demand. */
JS_EXTERN JSValue JS_NewTemporalInstant(JSContext *ctx, uint64_t low, uint64_t high);
/* Borrow a branded Instant from this runtime; no coercion or allocation on
   success. Output pointers must be distinct and non-NULL. Failure preserves
   both outputs and throws TypeError, also in disabled builds. */
JS_EXTERN int JS_GetTemporalInstantEpochNanoseconds(JSContext *ctx,
                                                   JSValueConst value,
                                                   uint64_t *plow,
                                                   uint64_t *phigh);
JS_EXTERN int JS_ThisDateValue(JSContext *ctx, double *valp, JSValueConst this_val);

JS_EXTERN JSValue JS_GetPropertyInternal(JSContext *ctx, JSValueConst obj,
                                         JSAtom prop, JSValueConst receiver,
                                         JS_BOOL throw_ref_error);
static js_force_inline JSValue JS_GetProperty(JSContext *ctx, JSValueConst this_obj,
                                              JSAtom prop)
{
    return JS_GetPropertyInternal(ctx, this_obj, prop, this_obj, 0);
}
JS_EXTERN JSValue JS_GetPropertyStr(JSContext *ctx, JSValueConst this_obj,
                                    const char *prop);
JS_EXTERN JSValue JS_GetPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                       uint32_t idx);
/* Int64 property helpers use the exact signed decimal property key,
   including values outside the ECMAScript Number safe integer range.
   Get returns an owned value or JS_EXCEPTION and may invoke user code. */
JS_EXTERN JSValue JS_GetPropertyInt64(JSContext *ctx, JSValueConst this_obj,
                                      int64_t idx);

JS_EXTERN int JS_SetPropertyInternal(JSContext *ctx, JSValueConst obj,
                                     JSAtom prop, JSValue val, JSValueConst this_obj,
                                     int flags);
static inline int JS_SetProperty(JSContext *ctx, JSValueConst this_obj,
                                 JSAtom prop, JSValue val)
{
    return JS_SetPropertyInternal(ctx, this_obj, prop, val, this_obj, JS_PROP_THROW);
}
JS_EXTERN int JS_SetPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                   uint32_t idx, JSValue val);
JS_EXTERN int JS_SetPropertyInt64(JSContext *ctx, JSValueConst this_obj,
                                  int64_t idx, JSValue val);
JS_EXTERN int JS_SetPropertyStr(JSContext *ctx, JSValueConst this_obj,
                                const char *prop, JSValue val);
JS_EXTERN int JS_HasProperty(JSContext *ctx, JSValueConst this_obj, JSAtom prop);
JS_EXTERN int JS_IsExtensible(JSContext *ctx, JSValueConst obj);
JS_EXTERN int JS_PreventExtensions(JSContext *ctx, JSValueConst obj);
JS_EXTERN int JS_DeleteProperty(JSContext *ctx, JSValueConst obj, JSAtom prop, int flags);
/* Same flags and return convention as JS_DeleteProperty. */
JS_EXTERN int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj,
                                     int64_t idx, int flags);
JS_EXTERN int JS_SetPrototype(JSContext *ctx, JSValueConst obj, JSValueConst proto_val);
JS_EXTERN JSValue JS_GetPrototype(JSContext *ctx, JSValueConst val);

#define JS_GPN_STRING_MASK  (1 << 0)
#define JS_GPN_SYMBOL_MASK  (1 << 1)
#define JS_GPN_PRIVATE_MASK (1 << 2)
/* only include the enumerable properties */
#define JS_GPN_ENUM_ONLY    (1 << 4)
/* set theJSPropertyEnum.is_enumerable field */
#define JS_GPN_SET_ENUM     (1 << 5)

JS_EXTERN int JS_GetOwnPropertyNames(JSContext *ctx, JSPropertyEnum **ptab,
                                     uint32_t *plen, JSValueConst obj, int flags);
JS_EXTERN void JS_FreePropertyEnum(JSContext *ctx, JSPropertyEnum *tab,
                                   uint32_t len);
JS_EXTERN int JS_GetOwnProperty(JSContext *ctx, JSPropertyDescriptor *desc,
                                JSValueConst obj, JSAtom prop);

JS_EXTERN JSValue JS_Call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj,
                          int argc, JSValueConst *argv);
JS_EXTERN JSValue JS_Invoke(JSContext *ctx, JSValueConst this_val, JSAtom atom,
                            int argc, JSValueConst *argv);
JS_EXTERN JSValue JS_CallConstructor(JSContext *ctx, JSValueConst func_obj,
                                     int argc, JSValueConst *argv);
JS_EXTERN JSValue JS_CallConstructor2(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst new_target,
                                      int argc, JSValueConst *argv);
/* JS_DetectModule(), JS_Eval(), and JS_EvalThis() require
   input[input_len] = '\0'. */
JS_EXTERN JS_BOOL JS_DetectModule(const char *input, size_t input_len);
JS_EXTERN JSValue JS_Eval(JSContext *ctx, const char *input, size_t input_len,
                          const char *filename, int eval_flags);
/* same as JS_Eval() but with an explicit 'this_obj' parameter */
JS_EXTERN JSValue JS_EvalThis(JSContext *ctx, JSValueConst this_obj,
                              const char *input, size_t input_len,
                              const char *filename, int eval_flags);
JS_EXTERN JSValue JS_GetGlobalObject(JSContext *ctx);
JS_EXTERN int JS_OrdinaryIsInstanceOf(JSContext *ctx, JSValueConst val,
                                      JSValueConst obj);
JS_EXTERN int JS_IsInstanceOf(JSContext *ctx, JSValueConst val, JSValueConst obj);
JS_EXTERN int JS_DefineProperty(JSContext *ctx, JSValueConst this_obj,
                                JSAtom prop, JSValueConst val,
                                JSValueConst getter, JSValueConst setter, int flags);
JS_EXTERN int JS_DefinePropertyValue(JSContext *ctx, JSValueConst this_obj,
                                     JSAtom prop, JSValue val, int flags);
JS_EXTERN int JS_DefinePropertyValueUint32(JSContext *ctx, JSValueConst this_obj,
                                           uint32_t idx, JSValue val, int flags);
/* Consumes val on success and failure; uses the exact Int64 key.
   Same flags and return convention as JS_DefinePropertyValue. */
JS_EXTERN int JS_DefinePropertyValueInt64(JSContext *ctx, JSValueConst this_obj,
                                        int64_t idx, JSValue val, int flags);
JS_EXTERN int JS_DefinePropertyValueStr(JSContext *ctx, JSValueConst this_obj,
                                        const char *prop, JSValue val, int flags);
JS_EXTERN int JS_DefinePropertyGetSet(JSContext *ctx, JSValueConst this_obj,
                                      JSAtom prop, JSValue getter, JSValue setter,
                                      int flags);
JS_EXTERN void JS_SetOpaque(JSValue obj, void *opaque);
JS_EXTERN void *JS_GetOpaque(JSValueConst obj, JSClassID class_id);
JS_EXTERN void *JS_GetOpaque2(JSContext *ctx, JSValueConst obj, JSClassID class_id);
JS_EXTERN void *JS_GetAnyOpaque(JSValueConst obj, JSClassID *class_id);

/* 'buf' must be zero terminated i.e. buf[buf_len] = '\0'. */
JS_EXTERN JSValue JS_ParseJSON(JSContext *ctx, const char *buf, size_t buf_len,
                               const char *filename);
#define JS_PARSE_JSON_EXT (1 << 0) /* allow extended JSON */
JS_EXTERN JSValue JS_ParseJSON2(JSContext *ctx, const char *buf, size_t buf_len,
                                const char *filename, int flags);
JS_EXTERN JSValue JS_JSONStringify(JSContext *ctx, JSValueConst obj,
                                   JSValueConst replacer, JSValueConst space0);

typedef void JSFreeArrayBufferDataFunc(JSRuntime *rt, void *opaque, void *ptr);
/* Nonshared external backing storage may be unaligned. With Atomics enabled,
   shared storage must satisfy the alignment of all engine-supported atomic
   integer types. A misaligned shared pointer throws TypeError without taking
   ownership or calling free_func. Nonshared storage requires host-side
   synchronization if the embedding accesses it from another thread. */
JS_EXTERN JSValue JS_NewArrayBuffer(JSContext *ctx, uint8_t *buf, size_t len,
                                    JSFreeArrayBufferDataFunc *free_func, void *opaque,
                                    JS_BOOL is_shared);
JS_EXTERN JSValue JS_NewArrayBufferCopy(JSContext *ctx, const uint8_t *buf, size_t len);
JS_EXTERN void JS_DetachArrayBuffer(JSContext *ctx, JSValueConst obj);
JS_EXTERN uint8_t *JS_GetArrayBuffer(JSContext *ctx, size_t *psize, JSValueConst obj);

typedef enum JSTypedArrayEnum {
    JS_TYPED_ARRAY_UINT8C = 0,
    JS_TYPED_ARRAY_INT8,
    JS_TYPED_ARRAY_UINT8,
    JS_TYPED_ARRAY_INT16,
    JS_TYPED_ARRAY_UINT16,
    JS_TYPED_ARRAY_INT32,
    JS_TYPED_ARRAY_UINT32,
    JS_TYPED_ARRAY_BIG_INT64,
    JS_TYPED_ARRAY_BIG_UINT64,
    JS_TYPED_ARRAY_FLOAT16,
    JS_TYPED_ARRAY_FLOAT32,
    JS_TYPED_ARRAY_FLOAT64,
} JSTypedArrayEnum;

JS_EXTERN JSValue JS_NewTypedArray(JSContext *ctx, int argc, JSValueConst *argv,
                                   JSTypedArrayEnum array_type);
/* Returns a JSTypedArrayEnum value, or -1 for other values. Does not
   invoke user code or throw; detached and out-of-bounds views retain
   their type. Proxies and DataView objects are not TypedArrays. */
JS_EXTERN int JS_GetTypedArrayType(JSValueConst obj);
/* Copy len input bytes into a new Uint8Array. buf may be NULL when
   len is zero. Returns an owned value or JS_EXCEPTION. For standard
   prototypes in raw contexts, initialize TypedArrays first. */
JS_EXTERN JSValue JS_NewUint8ArrayCopy(JSContext *ctx, const uint8_t *buf, size_t len);
JS_EXTERN JSValue JS_GetTypedArrayBuffer(JSContext *ctx, JSValueConst obj,
                                         size_t *pbyte_offset,
                                         size_t *pbyte_length,
                                         size_t *pbytes_per_element);
/* sab_alloc must return storage aligned for the engine's atomic integer types
   when Atomics is enabled. A misaligned allocation is released through the
   original sab_free callback and construction throws TypeError. */
typedef struct {
    void *(*sab_alloc)(void *opaque, size_t size);
    void (*sab_free)(void *opaque, void *ptr);
    void (*sab_dup)(void *opaque, void *ptr);
    void *sab_opaque;
} JSSharedArrayBufferFunctions;
JS_EXTERN void JS_SetSharedArrayBufferFunctions(JSRuntime *rt,
                                                const JSSharedArrayBufferFunctions *sf);

typedef enum JSPromiseStateEnum {
    JS_PROMISE_PENDING,
    JS_PROMISE_FULFILLED,
    JS_PROMISE_REJECTED,
} JSPromiseStateEnum;

JS_EXTERN JSValue JS_NewPromiseCapability(JSContext *ctx, JSValue *resolving_funcs);
/* All inputs are borrowed; returned values are owned. PromiseThen requires
   a genuine Promise and creates an intrinsic Promise in ctx without reading
   the input's then, constructor or species. Noncallable handlers are ignored. */
JS_EXTERN JSValue JS_PromiseThen(JSContext *ctx, JSValueConst promise,
                                 JSValueConst on_fulfilled,
                                 JSValueConst on_rejected);
/* Idempotent, including pending Promises. Non-Promises are ignored. */
JS_EXTERN void JS_PromiseMarkAsHandled(JSContext *ctx, JSValueConst promise);
/* Intrinsic Promise.resolve/reject semantics, including identity reuse and
   thenable assimilation. Resolution need not settle synchronously. */
JS_EXTERN JSValue JS_NewSettledPromise(JSContext *ctx, JS_BOOL is_reject,
                                       JSValueConst value);
JS_EXTERN JSPromiseStateEnum JS_PromiseState(JSContext *ctx, JSValue promise);
JS_EXTERN JSValue JS_PromiseResult(JSContext *ctx, JSValue promise);

/* is_handled = TRUE means that the rejection is handled */
typedef void JSHostPromiseRejectionTracker(JSContext *ctx, JSValueConst promise,
                                           JSValueConst reason,
                                           JS_BOOL is_handled, void *opaque);
JS_EXTERN void JS_SetHostPromiseRejectionTracker(JSRuntime *rt, JSHostPromiseRejectionTracker *cb, void *opaque);

/* return != 0 if the JS code needs to be interrupted */
typedef int JSInterruptHandler(JSRuntime *rt, void *opaque);
JS_EXTERN void JS_SetInterruptHandler(JSRuntime *rt, JSInterruptHandler *cb, void *opaque);
/* Return the current callback and, if requested, its borrowed opaque value. */
JS_EXTERN JSInterruptHandler *JS_GetInterruptHandler(JSRuntime *rt, void **popaque);
/* if can_block is TRUE, Atomics.wait() can be used */
JS_EXTERN void JS_SetCanBlock(JSRuntime *rt, JS_BOOL can_block);
/* select which debug info is stripped from the compiled code */
#define JS_STRIP_SOURCE (1 << 0) /* strip source code */
#define JS_STRIP_DEBUG  (1 << 1) /* strip all debug info including source code */
JS_EXTERN void JS_SetStripInfo(JSRuntime *rt, int flags);
JS_EXTERN int JS_GetStripInfo(JSRuntime *rt);

/* set the [IsHTMLDDA] internal slot */
JS_EXTERN void JS_SetIsHTMLDDA(JSContext *ctx, JSValueConst obj);

typedef struct JSModuleDef JSModuleDef;

/* return the module specifier (allocated with js_malloc()) or NULL if
   exception */
typedef char *JSModuleNormalizeFunc(JSContext *ctx,
                                    const char *module_base_name,
                                    const char *module_name, void *opaque);
/* Called before loaded-module lookup. Return a js_malloc()-allocated name,
   or NULL with a pending exception. Base/specifier strings and attributes are
   borrowed for the callback. Attributes are undefined or an object whose own
   enumerable properties with string keys contain string-valued import
   attributes. No prototype is guaranteed. Read attributes as own properties;
   do not free or mutate them. Retain with JS_DupValue() if needed after return.
   A normalized name need not be the complete request identity; attributes may
   also distinguish it. */
typedef char *JSModuleNormalizeFunc2(JSContext *ctx,
                                     const char *module_base_name,
                                     const char *module_name,
                                     JSValueConst attributes,
                                     void *opaque);
typedef JSModuleDef *JSModuleLoaderFunc(JSContext *ctx,
                                        const char *module_name, void *opaque);
typedef JSModuleDef *JSModuleLoaderFunc2(JSContext *ctx,
                                         const char *module_name, void *opaque,
                                         JSValueConst attributes);
/* return -1 if exception, 0 if OK */
typedef int JSModuleCheckSupportedImportAttributes(JSContext *ctx, void *opaque,
                                                   JSValueConst attributes);
                                                   
/* module_normalize = NULL is allowed and invokes the default module
   filename normalizer */
JS_EXTERN void JS_SetModuleLoaderFunc(JSRuntime *rt,
                                      JSModuleNormalizeFunc *module_normalize,
                                      JSModuleLoaderFunc *module_loader, void *opaque);
/* same as JS_SetModuleLoaderFunc but with attributes. if
   module_check_attrs = NULL, no attribute checking is done. */
JS_EXTERN void JS_SetModuleLoaderFunc2(JSRuntime *rt,
                                       JSModuleNormalizeFunc *module_normalize,
                                       JSModuleLoaderFunc2 *module_loader,
                                       JSModuleCheckSupportedImportAttributes *module_check_attrs,
                                       void *opaque);
/* Set an attributes-aware normalizer after either module loader setter.
   Uses the loader opaque; leaves loader, checker and loaded modules intact.
   NULL selects the default filename normalizer. Either loader setter resets
   this callback. Configure callbacks on the runtime's owner thread. */
JS_EXTERN void JS_SetModuleNormalizeFunc2(JSRuntime *rt,
                                          JSModuleNormalizeFunc2 *module_normalize);
/* return the import.meta object of a module */
JS_EXTERN JSValue JS_GetImportMeta(JSContext *ctx, JSModuleDef *m);
JS_EXTERN JSAtom JS_GetModuleName(JSContext *ctx, JSModuleDef *m);
JS_EXTERN JSValue JS_GetModuleNamespace(JSContext *ctx, JSModuleDef *m);

/* JS Job support */

typedef JSValue JSJobFunc(JSContext *ctx, int argc, JSValueConst *argv);
JS_EXTERN int JS_EnqueueJob(JSContext *ctx, JSJobFunc *job_func, int argc, JSValueConst *argv);

JS_EXTERN JS_BOOL JS_IsJobPending(JSRuntime *rt);
JS_EXTERN int JS_ExecutePendingJob(JSRuntime *rt, JSContext **pctx);

/* Native asynchronous waits. All four functions run on the runtime owner
   thread. Pending native waits retain their realm and shared backing storage.
   The borrowed wake handle is a POSIX fd or Windows HANDLE; -1 means inactive.
   Hosts may wait for readiness, but must not close, reset or read this handle.
   Use JS_PollNativeJobs(rt, 0) and JS_ExecutePendingJob to make owner progress.
   Timeout is a ceiling in milliseconds, capped at INT_MAX; -1 means no finite
   deadline. Poll accepts any negative timeout as an unlimited wait, returns
   1 for queued work, 0 for no work/interruption, and -1 for an OS failure with
   errno (POSIX) or GetLastError (Windows). Poll never executes JavaScript. */
JS_EXTERN JS_BOOL JS_IsNativeJobPending(JSRuntime *rt);
JS_EXTERN intptr_t JS_GetNativeJobWakeHandle(JSRuntime *rt);
JS_EXTERN int JS_GetNativeJobTimeout(JSRuntime *rt);
JS_EXTERN int JS_PollNativeJobs(JSRuntime *rt, int timeout_ms);

/* Object Writer/Reader (currently only used to handle precompiled code) */
#define JS_WRITE_OBJ_BYTECODE  (1 << 0) /* allow function/module */
/* Accepted for compatibility; bytecode is always little-endian. */
#define JS_WRITE_OBJ_BSWAP     (1 << 1)
#define JS_WRITE_OBJ_SAB       (1 << 2) /* allow SharedArrayBuffer */
#define JS_WRITE_OBJ_REFERENCE (1 << 3) /* allow object references to
                                           encode arbitrary object
                                           graph */
JS_EXTERN uint8_t *JS_WriteObject(JSContext *ctx, size_t *psize, JSValueConst obj,
                                  int flags);
JS_EXTERN uint8_t *JS_WriteObject2(JSContext *ctx, size_t *psize, JSValueConst obj,
                                   int flags, uint8_t ***psab_tab, size_t *psab_tab_len);

#define JS_READ_OBJ_BYTECODE  (1 << 0) /* allow function/module */
#define JS_READ_OBJ_ROM_DATA  (1 << 1) /* avoid duplicating 'buf' data */
#define JS_READ_OBJ_SAB       (1 << 2) /* allow SharedArrayBuffer */
#define JS_READ_OBJ_REFERENCE (1 << 3) /* allow object references */
JS_EXTERN JSValue JS_ReadObject(JSContext *ctx, const uint8_t *buf, size_t buf_len,
                                int flags);
/* instantiate and evaluate a bytecode function. Only used when
   reading a script or module with JS_ReadObject() */
JS_EXTERN JSValue JS_EvalFunction(JSContext *ctx, JSValue fun_obj);
/* load the dependencies of the module 'obj'. Useful when JS_ReadObject()
   returns a module. */
JS_EXTERN int JS_ResolveModule(JSContext *ctx, JSValueConst obj);
/* instantiate a module without evaluating its body; useful for embedders
   that need to distinguish resolution from evaluation errors. */
JS_EXTERN int JS_LinkModule(JSContext *ctx, JSValueConst obj);

/* only exported for os.Worker() */
JS_EXTERN JSAtom JS_GetScriptOrModuleName(JSContext *ctx, int n_stack_levels);
/* only exported for os.Worker() */
JS_EXTERN JSValue JS_LoadModule(JSContext *ctx, const char *basename,
                                const char *filename);

/* C function definition */
typedef enum JSCFunctionEnum {  /* XXX: should rename for namespace isolation */
    JS_CFUNC_generic,
    JS_CFUNC_generic_magic,
    JS_CFUNC_constructor,
    JS_CFUNC_constructor_magic,
    JS_CFUNC_constructor_or_func,
    JS_CFUNC_constructor_or_func_magic,
    JS_CFUNC_f_f,
    JS_CFUNC_f_f_f,
    JS_CFUNC_getter,
    JS_CFUNC_setter,
    JS_CFUNC_getter_magic,
    JS_CFUNC_setter_magic,
    JS_CFUNC_iterator_next,
    JS_CFUNC_constructor_or_func_flags,
} JSCFunctionEnum;

typedef union JSCFunctionType {
    JSCFunction *generic;
    JSValue (*generic_magic)(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
    JSCFunction *constructor;
    JSValue (*constructor_magic)(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv, int magic);
    JSCFunction *constructor_or_func;
    JSValue (*constructor_or_func_flags)(JSContext *ctx, JSValueConst func_obj,
                                        JSValueConst this_val, int argc,
                                        JSValueConst *argv, int flags);
    double (*f_f)(double);
    double (*f_f_f)(double, double);
    JSValue (*getter)(JSContext *ctx, JSValueConst this_val);
    JSValue (*setter)(JSContext *ctx, JSValueConst this_val, JSValueConst val);
    JSValue (*getter_magic)(JSContext *ctx, JSValueConst this_val, int magic);
    JSValue (*setter_magic)(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);
    JSValue (*iterator_next)(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int *pdone, int magic);
} JSCFunctionType;

JS_EXTERN JSValue JS_NewCFunction2(JSContext *ctx, JSCFunction *func,
                                   const char *name,
                                   int length, JSCFunctionEnum cproto, int magic);
JS_EXTERN JSValue JS_NewCFunctionData(JSContext *ctx, JSCFunctionData *func,
                                      int length, int magic, int data_len,
                                      JSValueConst *data);
/* Same ownership and callback rules, with a configurable function name.
   A NULL name selects the empty string. length must be in [0, 255]. */
JS_EXTERN JSValue JS_NewCFunctionData2(JSContext *ctx, JSCFunctionData *func,
                                       const char *name, int length, int magic,
                                       int data_len, JSValueConst *data);
/* Native-only opaque state: no owned JSValue/JSContext references in opaque.
   Finalizer cleanup responsibility transfers only on success; without one,
   keep opaque valid for the closure lifetime. A finalizer runs once and must
   not execute JavaScript. Calls use the retained creator context.
   length is in [0, 65535]; at least that many argv entries are readable,
   while argc remains the actual argument count. NULL name selects "". */
JS_EXTERN JSValue JS_NewCClosure(JSContext *ctx, JSCClosure *func,
                                const char *name,
                                JSCClosureFinalizerFunc *opaque_finalize,
                                int length, int magic, void *opaque);

static inline JSValue JS_NewCFunction(JSContext *ctx, JSCFunction *func, const char *name,
                                      int length)
{
    return JS_NewCFunction2(ctx, func, name, length, JS_CFUNC_generic, 0);
}

static inline JSValue JS_NewCFunctionMagic(JSContext *ctx, JSCFunctionMagic *func,
                                           const char *name,
                                           int length, JSCFunctionEnum cproto, int magic)
{
    /* Used to squelch a -Wcast-function-type warning. */
    JSCFunctionType ft = { .generic_magic = func };
    return JS_NewCFunction2(ctx, ft.generic, name, length, cproto, magic);
}
JS_EXTERN int JS_SetConstructor(JSContext *ctx, JSValueConst func_obj,
                                JSValueConst proto);

/* C property definition */

typedef struct JSCFunctionListEntry {
    const char *name;
    uint8_t prop_flags;
    uint8_t def_type;
    int16_t magic;
    union {
        struct {
            uint8_t length; /* XXX: should move outside union */
            uint8_t cproto; /* XXX: should move outside union */
            JSCFunctionType cfunc;
        } func;
        struct {
            JSCFunctionType get;
            JSCFunctionType set;
        } getset;
        struct {
            const char *name;
            int base;
        } alias;
        struct {
            const struct JSCFunctionListEntry *tab;
            int len;
        } prop_list;
        const char *str;
        int32_t i32;
        int64_t i64;
        double f64;
    } u;
} JSCFunctionListEntry;

#define JS_DEF_CFUNC          0
#define JS_DEF_CGETSET        1
#define JS_DEF_CGETSET_MAGIC  2
#define JS_DEF_PROP_STRING    3
#define JS_DEF_PROP_INT32     4
#define JS_DEF_PROP_INT64     5
#define JS_DEF_PROP_DOUBLE    6
#define JS_DEF_PROP_UNDEFINED 7
#define JS_DEF_OBJECT         8
#define JS_DEF_ALIAS          9
#define JS_DEF_PROP_ATOM     10
#define JS_DEF_PROP_BOOL     11

/* Note: c++ does not like nested designators */
#define JS_CFUNC_DEF(name, length, func1) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_CFUNC, 0, .u = { .func = { length, JS_CFUNC_generic, { .generic = func1 } } } }
#define JS_CFUNC_MAGIC_DEF(name, length, func1, magic) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_CFUNC, magic, .u = { .func = { length, JS_CFUNC_generic_magic, { .generic_magic = func1 } } } }
#define JS_CFUNC_SPECIAL_DEF(name, length, cproto, func1) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_CFUNC, 0, .u = { .func = { length, JS_CFUNC_ ## cproto, { .cproto = func1 } } } }
#define JS_ITERATOR_NEXT_DEF(name, length, func1, magic) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_CFUNC, magic, .u = { .func = { length, JS_CFUNC_iterator_next, { .iterator_next = func1 } } } }
#define JS_CGETSET_DEF(name, fgetter, fsetter) { name, JS_PROP_CONFIGURABLE, JS_DEF_CGETSET, 0, .u = { .getset = { .get = { .getter = fgetter }, .set = { .setter = fsetter } } } }
#define JS_CGETSET_MAGIC_DEF(name, fgetter, fsetter, magic) { name, JS_PROP_CONFIGURABLE, JS_DEF_CGETSET_MAGIC, magic, .u = { .getset = { .get = { .getter_magic = fgetter }, .set = { .setter_magic = fsetter } } } }
#define JS_PROP_STRING_DEF(name, cstr, prop_flags) { name, prop_flags, JS_DEF_PROP_STRING, 0, .u = { .str = cstr } }
#define JS_PROP_INT32_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_INT32, 0, .u = { .i32 = val } }
#define JS_PROP_INT64_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_INT64, 0, .u = { .i64 = val } }
#define JS_PROP_DOUBLE_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_DOUBLE, 0, .u = { .f64 = val } }
#define JS_PROP_UNDEFINED_DEF(name, prop_flags) { name, prop_flags, JS_DEF_PROP_UNDEFINED, 0, .u = { .i32 = 0 } }
#define JS_PROP_ATOM_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_ATOM, 0, .u = { .i32 = val } }
#define JS_PROP_BOOL_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_BOOL, 0, .u = { .i32 = val } }
#define JS_OBJECT_DEF(name, tab, len, prop_flags) { name, prop_flags, JS_DEF_OBJECT, 0, .u = { .prop_list = { tab, len } } }
#define JS_ALIAS_DEF(name, from) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_ALIAS, 0, .u = { .alias = { from, -1 } } }
#define JS_ALIAS_BASE_DEF(name, from, base) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_ALIAS, 0, .u = { .alias = { from, base } } }

/* Lazy function, string and object entries retain their table pointers;
   keep those entries and their referenced data alive until object disposal.
   Returns 0 on success, -1 with an exception on rejected definitions. */
JS_EXTERN int JS_SetPropertyFunctionList(JSContext *ctx, JSValueConst obj,
                                         const JSCFunctionListEntry *tab,
                                         int len);

#define JS_NEW_CTOR_NO_GLOBAL   (1 << 0) /* do not create a global binding */
#define JS_NEW_CTOR_PROTO_CLASS (1 << 1) /* use class_id for the prototype */
#define JS_NEW_CTOR_PROTO_EXIST (1 << 2) /* use the existing class prototype */
#define JS_NEW_CTOR_READONLY    (1 << 3) /* read-only constructor property */

/* class_id must be registered, or -1 without class-specific prototype flags.
   Inputs are borrowed and the returned constructor is owned. Field tables
   follow JS_SetPropertyFunctionList's lifetime rules. A successful creation
   replaces the class prototype and publishes the global after link setup.
   Properties already defined on a reused prototype are not rolled back. */
JS_EXTERN JSValue JS_NewCConstructor(JSContext *ctx, int class_id, const char *name,
                                     JSCFunction *func, int length, JSCFunctionEnum cproto,
                                     int magic, JSValueConst parent_ctor,
                                     const JSCFunctionListEntry *ctor_fields, int n_ctor_fields,
                                     const JSCFunctionListEntry *proto_fields, int n_proto_fields,
                                     int flags);

/* Iterator protocol. Inputs are borrowed; returned values are owned.
   JS_IteratorNext returns undefined when done. With an exception pending,
   JS_IteratorClose preserves it and returns -1. */
JS_EXTERN JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, JS_BOOL is_async);
JS_EXTERN JSValue JS_IteratorNext(JSContext *ctx, JSValueConst iter,
                                 JSValueConst method, JS_BOOL *pdone);
JS_EXTERN int JS_IteratorClose(JSContext *ctx, JSValueConst iter,
                               JS_BOOL is_exception_pending);

/* C module definition */

typedef int JSModuleInitFunc(JSContext *ctx, JSModuleDef *m);

JS_EXTERN JSModuleDef *JS_NewCModule(JSContext *ctx, const char *name_str,
                                     JSModuleInitFunc *func);
/* An attribute object is borrowed and retained. Supply normalized own
   enumerable string key/value pairs and do not mutate after creation.
   An explicit object supersedes the active loader request. Undefined uses
   the same active-request inheritance as JS_NewCModule; outside a loader
   request it creates a module with no attributes. */
JS_EXTERN JSModuleDef *JS_NewCModule2(JSContext *ctx, const char *name_str,
                                      JSModuleInitFunc *func,
                                      JSValueConst attributes);
/* can only be called before the module is instantiated */
JS_EXTERN int JS_AddModuleExport(JSContext *ctx, JSModuleDef *m, const char *name_str);
JS_EXTERN int JS_AddModuleExportList(JSContext *ctx, JSModuleDef *m,
                                     const JSCFunctionListEntry *tab, int len);
/* can only be called after the module is instantiated */
JS_EXTERN int JS_SetModuleExport(JSContext *ctx, JSModuleDef *m, const char *export_name,
                                 JSValue val);
JS_EXTERN int JS_SetModuleExportList(JSContext *ctx, JSModuleDef *m,
                                     const JSCFunctionListEntry *tab, int len);
/* associate a JSValue to a C module */
JS_EXTERN int JS_SetModulePrivateValue(JSContext *ctx, JSModuleDef *m, JSValue val);
JS_EXTERN JSValue JS_GetModulePrivateValue(JSContext *ctx, JSModuleDef *m);
                        
/* debug value output */

typedef struct {
    JS_BOOL show_hidden : 8; /* only show enumerable properties */
    JS_BOOL raw_dump : 8; /* avoid doing autoinit and avoid any malloc() call (for internal use) */
    uint32_t max_depth; /* recurse up to this depth, 0 = no limit */
    uint32_t max_string_length; /* print no more than this length for
                                   strings, 0 = no limit */
    uint32_t max_item_count; /*  print no more than this count for
                                 arrays or objects, 0 = no limit */
} JSPrintValueOptions;

typedef void JSPrintValueWrite(void *opaque, const char *buf, size_t len);

JS_EXTERN void JS_PrintValueSetDefaultOptions(JSPrintValueOptions *options);
JS_EXTERN void JS_PrintValueRT(JSRuntime *rt, JSPrintValueWrite *write_func, void *write_opaque,
                               JSValueConst val, const JSPrintValueOptions *options);
JS_EXTERN void JS_PrintValue(JSContext *ctx, JSPrintValueWrite *write_func, void *write_opaque,
                             JSValueConst val, const JSPrintValueOptions *options);

#undef js_unlikely
#undef js_force_inline

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* QUICKJS_H */
