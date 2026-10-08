/*
 * QuickJS Atomics builtin
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2026 Yan-Jie Wang
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
#include "../internal/base.h"
#include "../internal/atom.h"
#include "../internal/runtime.h"
#include "../internal/native-jobs.h"
#include "../internal/number.h"
#include "../internal/bigint.h"
#include "../internal/object.h"
#include "typed-array.h"
#include "array-buffer.h"
#include "atomics.h"

/* Atomics */
#ifdef CONFIG_ATOMICS

/* The native backend accesses one TypedArray element per atomic object. */
#define CHECK_ATOMIC_WIDTH(type)                                      \
    _Static_assert(sizeof(_Atomic(type)) == sizeof(type),             \
                   "unsupported atomic integer representation")
CHECK_ATOMIC_WIDTH(uint8_t);
CHECK_ATOMIC_WIDTH(uint16_t);
CHECK_ATOMIC_WIDTH(uint32_t);
CHECK_ATOMIC_WIDTH(uint64_t);
#undef CHECK_ATOMIC_WIDTH

typedef enum AtomicsOpEnum {
    ATOMICS_OP_ADD,
    ATOMICS_OP_AND,
    ATOMICS_OP_OR,
    ATOMICS_OP_SUB,
    ATOMICS_OP_XOR,
    ATOMICS_OP_EXCHANGE,
    ATOMICS_OP_COMPARE_EXCHANGE,
    ATOMICS_OP_LOAD,
} AtomicsOpEnum;

static uint64_t js_atomics_load_unaligned(const uint8_t *ptr, int size_log2)
{
    switch(size_log2) {
    case 0: return *ptr;
    case 1: return get_u16(ptr);
    case 2: return get_u32(ptr);
    case 3: return get_u64(ptr);
    default: abort();
    }
}

static void js_atomics_store_unaligned(uint8_t *ptr, int size_log2, uint64_t value)
{
    switch(size_log2) {
    case 0: *ptr = value; break;
    case 1: put_u16(ptr, value); break;
    case 2: put_u32(ptr, value); break;
    case 3: put_u64(ptr, value); break;
    default: abort();
    }
}

/* Only nonshared external buffers use this path. Their embedding owner must
   not access the same storage concurrently without its own synchronization. */
static uint64_t js_atomics_op_unaligned(uint8_t *ptr, int size_log2, int op,
                                       uint64_t value, uint64_t replacement)
{
    uint64_t old = js_atomics_load_unaligned(ptr, size_log2);
    uint64_t mask = UINT64_MAX >> (64 - (8 << size_log2));
    uint64_t result;

    value &= mask;
    switch(op) {
    case ATOMICS_OP_ADD: result = old + value; break;
    case ATOMICS_OP_AND: result = old & value; break;
    case ATOMICS_OP_OR: result = old | value; break;
    case ATOMICS_OP_SUB: result = old - value; break;
    case ATOMICS_OP_XOR: result = old ^ value; break;
    case ATOMICS_OP_EXCHANGE: result = value; break;
    case ATOMICS_OP_COMPARE_EXCHANGE:
        if (old != value)
            return old;
        result = replacement;
        break;
    case ATOMICS_OP_LOAD:
        return old;
    default:
        abort();
    }
    js_atomics_store_unaligned(ptr, size_log2, result);
    return old;
}

/* Backing alignment is classified once at construction. Reuse the existing
   dispatch rather than checking host address alignment on every operation. */
#define ATOMICS_UNALIGNED_OP (1 << 5)
#define ATOMICS_UNALIGNED_STORE (1 << 2)

static JSObject *js_atomics_get_buf(JSContext *ctx, 
                                    JSValueConst obj, JSValueConst idx_val,
                                    uint64_t *pidx, int is_waitable)
{
    JSObject *p;
    JSTypedArray *ta;
    JSArrayBuffer *abuf;
    uint64_t idx;
    BOOL err;
    int old_len;

    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        goto fail;
    p = JS_VALUE_GET_OBJ(obj);
    if (is_waitable)
        err = (p->class_id != JS_CLASS_INT32_ARRAY &&
               p->class_id != JS_CLASS_BIG_INT64_ARRAY);
    else
        err = !(p->class_id >= JS_CLASS_INT8_ARRAY &&
                p->class_id <= JS_CLASS_BIG_UINT64_ARRAY);
    if (err) {
    fail:
        JS_ThrowTypeError(ctx, "integer TypedArray expected");
        return NULL;
    }
    ta = p->u.typed_array;
    abuf = ta->buffer->u.array_buffer;
    if (!abuf->shared) {
        if (typed_array_is_oob(p)) {
            JS_ThrowTypeErrorArrayBufferOOB(ctx);
            return NULL;
        }
        if (is_waitable == 2) {
            JS_ThrowTypeError(ctx, "not a SharedArrayBuffer TypedArray");
            return NULL;
        }
    }
    old_len = js_typed_array_update_length(p);
    
    if (JS_ToIndex(ctx, &idx, idx_val)) {
        return NULL;
    }

    if (idx >= old_len)
        goto oob;

    if (is_waitable != 1) {
        /* RevalidateAtomicAccess() */
        if (typed_array_is_oob(p)) {
            JS_ThrowTypeErrorArrayBufferOOB(ctx);
            return NULL;
        }
        if (idx >= p->u.array.count) {
        oob:
            JS_ThrowRangeError(ctx, "out-of-bound access");
            return NULL;
        }
    }

    *pidx = idx;
    return p;
}

static JSValue js_atomics_op(JSContext *ctx,
                             JSValueConst this_obj,
                             int argc, JSValueConst *argv, int op)
{
    int size_log2;
    uint64_t v, a, rep_val, idx;
    void *ptr;
    JSValue ret;
    JSObject *p;
    
    p = js_atomics_get_buf(ctx, argv[0], argv[1], &idx, 0);
    if (!p)
        return JS_EXCEPTION;
    size_log2 = typed_array_size_log2(p->class_id);
    rep_val = 0;
    if (op == ATOMICS_OP_LOAD) {
        v = 0;
    } else {
        if (size_log2 == 3) {
            int64_t v64;
            if (JS_ToBigInt64(ctx, &v64, argv[2]))
                return JS_EXCEPTION;
            v = v64;
            if (op == ATOMICS_OP_COMPARE_EXCHANGE) {
                if (JS_ToBigInt64(ctx, &v64, argv[3]))
                    return JS_EXCEPTION;
                rep_val = v64;
            }
        } else {
                uint32_t v32;
                if (JS_ToUint32(ctx, &v32, argv[2]))
                    return JS_EXCEPTION;
                v = v32;
                if (op == ATOMICS_OP_COMPARE_EXCHANGE) {
                    if (JS_ToUint32(ctx, &v32, argv[3]))
                        return JS_EXCEPTION;
                    rep_val = v32;
                }
        }
        if (typed_array_is_oob(p))
            return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
        if (idx >= p->u.array.count)
            return JS_ThrowRangeError(ctx, "out-of-bound access");
    }
    ptr = p->u.array.u.uint8_ptr + ((uintptr_t)idx << size_log2);
    
    switch(op | (size_log2 << 3) |
           (p->u.typed_array->buffer->u.array_buffer->atomic_unaligned << 5)) {

#define OP(op_name, func_name)                          \
    case ATOMICS_OP_ ## op_name | (0 << 3):             \
       a = func_name((_Atomic(uint8_t) *)ptr, v);       \
       break;                                           \
    case ATOMICS_OP_ ## op_name | (1 << 3):             \
        a = func_name((_Atomic(uint16_t) *)ptr, v);     \
        break;                                          \
    case ATOMICS_OP_ ## op_name | (2 << 3):             \
        a = func_name((_Atomic(uint32_t) *)ptr, v);     \
        break;                                          \
    case ATOMICS_OP_ ## op_name | (3 << 3):             \
        a = func_name((_Atomic(uint64_t) *)ptr, v);     \
        break;

        OP(ADD, atomic_fetch_add)
        OP(AND, atomic_fetch_and)
        OP(OR, atomic_fetch_or)
        OP(SUB, atomic_fetch_sub)
        OP(XOR, atomic_fetch_xor)
        OP(EXCHANGE, atomic_exchange)
#undef OP

    case ATOMICS_OP_LOAD | (0 << 3):
        a = atomic_load((_Atomic(uint8_t) *)ptr);
        break;
    case ATOMICS_OP_LOAD | (1 << 3):
        a = atomic_load((_Atomic(uint16_t) *)ptr);
        break;
    case ATOMICS_OP_LOAD | (2 << 3):
        a = atomic_load((_Atomic(uint32_t) *)ptr);
        break;
    case ATOMICS_OP_LOAD | (3 << 3):
        a = atomic_load((_Atomic(uint64_t) *)ptr);
        break;

    case ATOMICS_OP_COMPARE_EXCHANGE | (0 << 3):
        {
            uint8_t v1 = v;
            atomic_compare_exchange_strong((_Atomic(uint8_t) *)ptr, &v1, rep_val);
            a = v1;
        }
        break;
    case ATOMICS_OP_COMPARE_EXCHANGE | (1 << 3):
        {
            uint16_t v1 = v;
            atomic_compare_exchange_strong((_Atomic(uint16_t) *)ptr, &v1, rep_val);
            a = v1;
        }
        break;
    case ATOMICS_OP_COMPARE_EXCHANGE | (2 << 3):
        {
            uint32_t v1 = v;
            atomic_compare_exchange_strong((_Atomic(uint32_t) *)ptr, &v1, rep_val);
            a = v1;
        }
        break;
    case ATOMICS_OP_COMPARE_EXCHANGE | (3 << 3):
        {
            uint64_t v1 = v;
            atomic_compare_exchange_strong((_Atomic(uint64_t) *)ptr, &v1, rep_val);
            a = v1;
        }
        break;
#define UNALIGNED_OP(op_name)                                         \
    case ATOMICS_UNALIGNED_OP | ATOMICS_OP_ ## op_name | (0 << 3):      \
    case ATOMICS_UNALIGNED_OP | ATOMICS_OP_ ## op_name | (1 << 3):      \
    case ATOMICS_UNALIGNED_OP | ATOMICS_OP_ ## op_name | (2 << 3):      \
    case ATOMICS_UNALIGNED_OP | ATOMICS_OP_ ## op_name | (3 << 3):
        UNALIGNED_OP(ADD)
        UNALIGNED_OP(AND)
        UNALIGNED_OP(OR)
        UNALIGNED_OP(SUB)
        UNALIGNED_OP(XOR)
        UNALIGNED_OP(EXCHANGE)
        UNALIGNED_OP(COMPARE_EXCHANGE)
        UNALIGNED_OP(LOAD)
            a = js_atomics_op_unaligned(ptr, size_log2, op, v, rep_val);
            break;
#undef UNALIGNED_OP
    default:
        abort();
    }

    switch(p->class_id) {
    case JS_CLASS_INT8_ARRAY:
        a = (int8_t)a;
        goto done;
    case JS_CLASS_UINT8_ARRAY:
        a = (uint8_t)a;
        goto done;
    case JS_CLASS_INT16_ARRAY:
        a = (int16_t)a;
        goto done;
    case JS_CLASS_UINT16_ARRAY:
        a = (uint16_t)a;
        goto done;
    case JS_CLASS_INT32_ARRAY:
    done:
        ret = JS_NewInt32(ctx, a);
        break;
    case JS_CLASS_UINT32_ARRAY:
        ret = JS_NewUint32(ctx, a);
        break;
    case JS_CLASS_BIG_INT64_ARRAY:
        ret = JS_NewBigInt64(ctx, a);
        break;
    case JS_CLASS_BIG_UINT64_ARRAY:
        ret = JS_NewBigUint64(ctx, a);
        break;
    default:
        abort();
    }
    return ret;
}

static JSValue js_atomics_store(JSContext *ctx,
                                JSValueConst this_obj,
                                int argc, JSValueConst *argv)
{
    int size_log2;
    void *ptr;
    JSValue ret;
    JSObject *p;
    uint64_t idx;
    int64_t v;

    p = js_atomics_get_buf(ctx, argv[0], argv[1], &idx, 0);
    if (!p)
        return JS_EXCEPTION;
    size_log2 = typed_array_size_log2(p->class_id);
    if (size_log2 == 3) {
        ret = JS_ToBigIntFree(ctx, JS_DupValue(ctx, argv[2]));
        if (JS_IsException(ret))
            return ret;
        if (JS_ToBigInt64(ctx, &v, ret)) {
            JS_FreeValue(ctx, ret);
            return JS_EXCEPTION;
        }
    } else {
        uint32_t v32;
        /* XXX: spec, would be simpler to return the written value */
        ret = JS_ToIntegerFree(ctx, JS_DupValue(ctx, argv[2]));
        if (JS_IsException(ret))
            return ret;
        if (JS_ToUint32(ctx, &v32, ret)) {
            JS_FreeValue(ctx, ret);
            return JS_EXCEPTION;
        }
        v = v32;
    }
    if (typed_array_is_oob(p)) {
        JS_FreeValue(ctx, ret);
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    }
    if (idx >= p->u.array.count) {
        JS_FreeValue(ctx, ret);
        return JS_ThrowRangeError(ctx, "out-of-bound access");
    }

    ptr = p->u.array.u.uint8_ptr + ((uintptr_t)idx << size_log2);
    
    switch(size_log2 |
           (p->u.typed_array->buffer->u.array_buffer->atomic_unaligned << 2)) {
    case 0:
        atomic_store((_Atomic(uint8_t) *)ptr, v);
        break;
    case 1:
        atomic_store((_Atomic(uint16_t) *)ptr, v);
        break;
    case 2:
        atomic_store((_Atomic(uint32_t) *)ptr, v);
        break;
    case 3:
        atomic_store((_Atomic(uint64_t) *)ptr, v);
        break;
    case ATOMICS_UNALIGNED_STORE | 0:
    case ATOMICS_UNALIGNED_STORE | 1:
    case ATOMICS_UNALIGNED_STORE | 2:
    case ATOMICS_UNALIGNED_STORE | 3:
        js_atomics_store_unaligned(ptr, size_log2, v);
        break;
    default:
        abort();
    }
    return ret;
}

static JSValue js_atomics_isLockFree(JSContext *ctx,
                                     JSValueConst this_obj,
                                     int argc, JSValueConst *argv)
{
    int v, ret;
    if (JS_ToInt32Sat(ctx, &v, argv[0]))
        return JS_EXCEPTION;
    ret = (v == 1 || v == 2 || v == 4 || v == 8);
    return JS_NewBool(ctx, ret);
}

typedef struct JSAtomicsWaiter {
    JSNativeWaiter native;
    pthread_cond_t cond;
} JSAtomicsWaiter;

typedef struct JSAtomicsAsyncWaiter {
    JSNativeAsyncRecord record;
    JSValue resolve;
    JSValue buffer;
} JSAtomicsAsyncWaiter;

static JSValue js_atomics_resolve_async(JSNativeAsyncRecord *record,
                                       JSNativeWaitResult result)
{
    JSAtomicsAsyncWaiter *waiter = (JSAtomicsAsyncWaiter *)record;
    JSContext *ctx = record->ctx;
    JSValue value, ret;
    value = JS_AtomToString(ctx, result == JS_NATIVE_WAIT_OK ? JS_ATOM_ok :
                           JS_ATOM_timed_out);
    ret = JS_Call(ctx, waiter->resolve, JS_UNDEFINED, 1,
                  (JSValueConst *)&value);
    JS_FreeValue(ctx, value);
    return ret;
}

static void js_atomics_release_async(JSNativeAsyncRecord *record)
{
    JSAtomicsAsyncWaiter *waiter = (JSAtomicsAsyncWaiter *)record;
    JSContext *ctx = record->ctx;
    JS_FreeValue(ctx, waiter->resolve);
    JS_FreeValue(ctx, waiter->buffer);
    js_free(ctx, waiter);
    JS_FreeContext(ctx);
}

#if defined(__aarch64__)
static inline void cpu_pause(void)
{
    asm volatile("yield" ::: "memory");
}
#elif defined(__x86_64) || defined(__i386__)
static inline void cpu_pause(void)
{
    asm volatile("pause" ::: "memory");
}
#else
static inline void cpu_pause(void)
{
}
#endif

// no-op: Atomics.pause() is not allowed to block or yield to another
// thread, only to hint the CPU that it should back off for a bit;
// the amount of work we do here is a good enough substitute
static JSValue js_atomics_pause(JSContext *ctx, JSValueConst this_obj,
                                int argc, JSValueConst *argv)
{
    double d;

    if (argc > 0) {
        switch (JS_VALUE_GET_NORM_TAG(argv[0])) {
        case JS_TAG_FLOAT64: // accepted if and only if fraction == 0.0
            d = JS_VALUE_GET_FLOAT64(argv[0]);
            if (isfinite(d))
                if (0 == modf(d, &d))
                    break;
            // fallthru
        default:
            return JS_ThrowTypeError(ctx, "not an integral number");
        case JS_TAG_UNDEFINED:
        case JS_TAG_INT:
            break;
        }
    }
    cpu_pause();
    return JS_UNDEFINED;
}

static JSValue js_atomics_wait(JSContext *ctx,
                               JSValueConst this_obj,
                               int argc, JSValueConst *argv)
{
    JSObject *p;
    int64_t v;
    int32_t v32;
    uint64_t idx;
    void *ptr;
    int64_t timeout;
    struct timespec ts;
    JSAtomicsWaiter waiter_s, *waiter;
    JSNativeWaitQueue *queue;
    JSNativeWaitResult result;
    int ret, size_log2, res;
    double d;

    p = js_atomics_get_buf(ctx, argv[0], argv[1], &idx, 2);
    if (!p)
        return JS_EXCEPTION;
    size_log2 = typed_array_size_log2(p->class_id);
    ptr = p->u.array.u.uint8_ptr + ((uintptr_t)idx << size_log2);
    
    /* 'argv[0]' is a SharedArrayBuffer so it cannot be detached nor reduced */
    if (size_log2 == 3) {
        if (JS_ToBigInt64(ctx, &v, argv[2]))
            return JS_EXCEPTION;
    } else {
        if (JS_ToInt32(ctx, &v32, argv[2]))
            return JS_EXCEPTION;
        v = v32;
    }
    if (JS_ToFloat64(ctx, &d, argv[3]))
        return JS_EXCEPTION;
    /* must use INT64_MAX + 1 because INT64_MAX cannot be exactly represented as a double */
    if (isnan(d) || d >= 0x1p63)
        timeout = INT64_MAX;
    else if (d < 0)
        timeout = 0;
    else
        timeout = (int64_t)ceil(d);
    if (!ctx->rt->can_block)
        return JS_ThrowTypeError(ctx, "cannot block in this thread");

    /* XXX: inefficient if large number of waiters, should hash on
       'ptr' value */
    /* XXX: use Linux futexes when available ? */
    queue = js_native_jobs_wait_queue();
    if (!queue)
        return JS_ThrowInternalError(ctx, "cannot initialize native wait queue");
    js_native_wait_queue_lock(queue);
    if (size_log2 == 3) {
        res = atomic_load((_Atomic(int64_t) *)ptr) != v;
    } else {
        res = atomic_load((_Atomic(int32_t) *)ptr) != v;
    }
    if (res) {
        js_native_wait_queue_unlock(queue);
        return JS_AtomToString(ctx, JS_ATOM_not_equal);
    }

    waiter = &waiter_s;
    ret = pthread_cond_init(&waiter->cond, NULL);
    if (ret) {
        js_native_wait_queue_unlock(queue);
        return JS_ThrowInternalError(ctx, "cannot initialize native wait");
    }
    js_native_waiter_init(&waiter->native, NULL);
    js_native_wait_add_locked(queue, &waiter->native, ptr, NULL,
                              &waiter->cond, JS_NATIVE_WAIT_FOREVER);

    if (timeout != INT64_MAX) {
        /* XXX: use clock monotonic */
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout / 1000;
        ts.tv_nsec += (timeout % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_nsec -= 1000000000;
            ts.tv_sec++;
        }
    }
    ret = 0;
    do {
        if (timeout == INT64_MAX)
            ret = pthread_cond_wait(&waiter->cond, &queue->mutex);
        else
            ret = pthread_cond_timedwait(&waiter->cond, &queue->mutex, &ts);
        result = js_native_wait_finish_sync_locked(queue, &waiter->native,
                                                   ret != 0);
    } while (result == JS_NATIVE_WAIT_NO_EVENT);
    if (result == JS_NATIVE_WAIT_OK)
        ret = 0;
    js_native_wait_queue_unlock(queue);
    pthread_cond_destroy(&waiter->cond);
    if (ret == ETIMEDOUT) {
        return JS_AtomToString(ctx, JS_ATOM_timed_out);
    } else {
        return JS_AtomToString(ctx, JS_ATOM_ok);
    }
}

static JSValue js_atomics_waitAsync(JSContext *ctx, JSValueConst this_obj,
                                    int argc, JSValueConst *argv)
{
    JSObject *p;
    JSAtomicsAsyncWaiter *waiter = NULL;
    JSNativeJobOwner *owner = NULL;
    JSNativeWaitQueue *queue;
    JSValue promise, resolving[2], result;
    JSAtom immediate;
    int64_t expected;
    int32_t expected32;
    uint64_t idx;
    int size_log2, unequal;
    void *ptr;
    double timeout;
    long double now = 0;

    p = js_atomics_get_buf(ctx, argv[0], argv[1], &idx, 2);
    if (!p)
        return JS_EXCEPTION;
    size_log2 = typed_array_size_log2(p->class_id);
    ptr = p->u.array.u.uint8_ptr + ((uintptr_t)idx << size_log2);
    if (size_log2 == 3) {
        if (JS_ToBigInt64(ctx, &expected, argv[2]))
            return JS_EXCEPTION;
    } else {
        if (JS_ToInt32(ctx, &expected32, argv[2]))
            return JS_EXCEPTION;
        expected = expected32;
    }
    if (JS_ToFloat64(ctx, &timeout, argv[3]))
        return JS_EXCEPTION;
    if (isnan(timeout))
        timeout = INFINITY;
    else if (timeout < 0)
        timeout = 0;

    /* DoWait creates the intrinsic capability and ordinary result in the
       active function realm even when the witness or zero timeout is immediate.
       Prepare every result property before publishing a waiter, so an OOM
       cannot leave a wait behind an exceptional return. */
    promise = JS_NewPromiseCapability(ctx, resolving);
    if (JS_IsException(promise))
        return promise;
    JS_FreeValue(ctx, resolving[1]);
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        goto exception;
    if (JS_DefinePropertyValue(ctx, result, JS_ATOM_async, JS_TRUE,
                               JS_PROP_C_W_E) < 0 ||
        JS_DefinePropertyValue(ctx, result, JS_ATOM_value,
                               JS_DupValue(ctx, promise), JS_PROP_C_W_E) < 0)
        goto exception;
    queue = js_native_jobs_wait_queue();
    if (!queue) {
        JS_ThrowInternalError(ctx, "cannot initialize native wait queue");
        goto exception;
    }
    if (timeout > 0) {
        now = js_native_jobs_now();
        if (isnan(now)) {
            JS_ThrowInternalError(ctx, "native wait clock failed");
            goto exception;
        }
        owner = js_native_jobs_get_owner(ctx);
        if (!owner)
            goto exception;
        waiter = js_mallocz(ctx, sizeof(*waiter));
        if (!waiter)
            goto exception;
        waiter->record.ctx = JS_DupContext(ctx);
        waiter->record.resolve = js_atomics_resolve_async;
        waiter->record.release = js_atomics_release_async;
        waiter->resolve = JS_DupValue(ctx, resolving[0]);
        waiter->buffer = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT,
                                         p->u.typed_array->buffer));
        js_native_waiter_init(&waiter->record.native, &waiter->record);
    }
    js_native_wait_queue_lock(queue);
    unequal = size_log2 == 3 ?
        atomic_load((_Atomic(int64_t) *)ptr) != expected :
        atomic_load((_Atomic(int32_t) *)ptr) != expected;
    if (unequal || timeout == 0) {
        immediate = unequal ? JS_ATOM_not_equal : JS_ATOM_timed_out;
        js_native_wait_queue_unlock(queue);
        if (waiter)
            waiter->record.release(&waiter->record);
        waiter = NULL;
        /* Both properties are existing writable own data properties. */
        if (JS_SetProperty(ctx, result, JS_ATOM_async, JS_FALSE) < 0 ||
            JS_SetProperty(ctx, result, JS_ATOM_value,
                           JS_AtomToString(ctx, immediate)) < 0)
            goto exception;
    } else {
        /* The host timeout starts when this waiter is published, after all
           allocations and capability construction. Round real delays upward. */
        now = js_native_jobs_now();
        if (isnan(now)) {
            js_native_wait_queue_unlock(queue);
            JS_ThrowInternalError(ctx, "native wait clock failed");
            goto exception;
        }
        js_native_wait_add_relative_locked(queue, &waiter->record.native,
                                           ptr, &owner->native, now, timeout);
        js_native_wait_queue_unlock(queue);
        waiter = NULL; /* owner record now retains realm, capability, backing */
    }
    JS_FreeValue(ctx, resolving[0]);
    JS_FreeValue(ctx, promise);
    return result;
exception:
    if (waiter)
        waiter->record.release(&waiter->record);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, resolving[0]);
    JS_FreeValue(ctx, promise);
    return JS_EXCEPTION;
}

static JSValue js_atomics_notify(JSContext *ctx,
                                 JSValueConst this_obj,
                                 int argc, JSValueConst *argv)
{
    int32_t count, n = 0;
    uint64_t idx;
    int size_log2, error = 0;
    void *ptr;
    JSNativeWaiter *inline_waiter;
    JSNativeWaitQueue *queue;
    JSNativeWaitOwner *calling_owner;
    JSArrayBuffer *abuf;
    JSObject *p;

    p = js_atomics_get_buf(ctx, argv[0], argv[1], &idx, 1);
    if (!p)
        return JS_EXCEPTION;
    size_log2 = typed_array_size_log2(p->class_id);
    if (JS_IsUndefined(argv[2])) {
        count = INT32_MAX;
    } else if (JS_ToInt32Clamp(ctx, &count, argv[2], 0, INT32_MAX, 0)) {
        return JS_EXCEPTION;
    }
    abuf = p->u.typed_array->buffer->u.array_buffer;
    if (abuf->shared && count > 0) {
        ptr = p->u.array.u.uint8_ptr + ((uintptr_t)idx << size_log2);
        queue = js_native_jobs_wait_queue();
        if (!queue)
            return JS_ThrowInternalError(ctx, "cannot initialize native wait queue");
        calling_owner = ctx->rt->native_jobs ?
                        &ctx->rt->native_jobs->native : NULL;
        js_native_wait_queue_lock(queue);
        while (n < count &&
               js_native_wait_notify_one_locked(queue, ptr, calling_owner,
                                                &inline_waiter)) {
            n++;
            if (inline_waiter &&
                js_native_jobs_complete(ctx->rt, inline_waiter,
                                        JS_NATIVE_WAIT_OK, NULL) < 0) {
                error = 1;
                break;
            }
        }
        /* Each same-agent resolve publishes its reaction jobs before selecting
           the next waiter or permitting a foreign notifier to append work. */
        js_native_wait_queue_unlock(queue);
        if (ctx->rt->native_jobs)
            js_native_jobs_flush_retired(ctx->rt);
    }
    return error ? JS_EXCEPTION : JS_NewInt32(ctx, n);
}

static const JSCFunctionListEntry js_atomics_funcs[] = {
    JS_CFUNC_MAGIC_DEF("add", 3, js_atomics_op, ATOMICS_OP_ADD ),
    JS_CFUNC_MAGIC_DEF("and", 3, js_atomics_op, ATOMICS_OP_AND ),
    JS_CFUNC_MAGIC_DEF("or", 3, js_atomics_op, ATOMICS_OP_OR ),
    JS_CFUNC_MAGIC_DEF("sub", 3, js_atomics_op, ATOMICS_OP_SUB ),
    JS_CFUNC_MAGIC_DEF("xor", 3, js_atomics_op, ATOMICS_OP_XOR ),
    JS_CFUNC_MAGIC_DEF("exchange", 3, js_atomics_op, ATOMICS_OP_EXCHANGE ),
    JS_CFUNC_MAGIC_DEF("compareExchange", 4, js_atomics_op, ATOMICS_OP_COMPARE_EXCHANGE ),
    JS_CFUNC_MAGIC_DEF("load", 2, js_atomics_op, ATOMICS_OP_LOAD ),
    JS_CFUNC_DEF("store", 3, js_atomics_store ),
    JS_CFUNC_DEF("isLockFree", 1, js_atomics_isLockFree ),
    JS_CFUNC_DEF("pause", 0, js_atomics_pause ),
    JS_CFUNC_DEF("wait", 4, js_atomics_wait ),
    JS_CFUNC_DEF("waitAsync", 4, js_atomics_waitAsync ),
    JS_CFUNC_DEF("notify", 3, js_atomics_notify ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Atomics", JS_PROP_CONFIGURABLE ),
};

static const JSCFunctionListEntry js_atomics_obj[] = {
    JS_OBJECT_DEF("Atomics", js_atomics_funcs, countof(js_atomics_funcs), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE ),
};

int JS_AddIntrinsicAtomics(JSContext *ctx)
{
    /* add Atomics as autoinit object */
    return JS_SetPropertyFunctionList(ctx, ctx->global_obj, js_atomics_obj, countof(js_atomics_obj));
}

#endif /* CONFIG_ATOMICS */
