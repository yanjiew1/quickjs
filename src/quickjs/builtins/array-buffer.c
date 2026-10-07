/*
 * QuickJS ArrayBuffer and SharedArrayBuffer builtins
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
#include "intrinsics.h"
#include "../internal/base.h"
#include "../value/compare.h"
#include "../internal/runtime.h"
#include "../internal/number.h"
#include "../internal/string.h"
#include "../internal/object.h"
#include "../internal/error.h"
#include "../internal/function-list.h"
#include "typed-array.h"
#include "array-buffer.h"

#ifdef CONFIG_ATOMICS
static BOOL js_array_buffer_atomic_is_aligned(const uint8_t *data)
{
    size_t alignment = max_int(_Alignof(_Atomic(uint8_t)),
                               _Alignof(_Atomic(uint16_t)));
    alignment = max_int(alignment, _Alignof(_Atomic(uint32_t)));
    alignment = max_int(alignment, _Alignof(_Atomic(uint64_t)));
    return (uintptr_t)data % alignment == 0;
}
#endif

static size_t js_shared_array_buffer_length_offset(uint8_t *data, size_t maximum)
{
    size_t alignment = _Alignof(JSSharedArrayBufferLength);
    size_t offset = maximum ? maximum : 1;
    size_t remainder = (uintptr_t)(data + offset) % alignment;
    if (remainder)
        offset += alignment - remainder;
    return offset;
}

static JSValue js_array_buffer_constructor4(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, uint64_t *max_len,
                                            JSClassID class_id,
                                            uint8_t *buf,
                                            JSFreeArrayBufferDataFunc *free_func,
                                            void *opaque, BOOL alloc_flag, BOOL shared_clone)
{
    JSRuntime *rt = ctx->rt;
    JSSharedArrayBufferFunctions shared_functions;
    JSValue obj;
    JSArrayBuffer *abuf = NULL;
    size_t alloc_len;
    BOOL growable_shared = class_id == JS_CLASS_SHARED_ARRAY_BUFFER && max_len;
    BOOL uses_shared_callbacks = FALSE;

    if (!alloc_flag && buf && max_len &&
        free_func != js_array_buffer_free && !shared_clone) {
        // not observable from JS land, only through C API misuse;
        // JS code cannot create externally managed buffers directly
        return JS_ThrowInternalError(ctx,
                                     "resizable ArrayBuffers not supported "
                                     "for externally managed buffers");
    }
#ifdef CONFIG_ATOMICS
    /* Imported storage stays with its caller if construction fails. */
    if (!alloc_flag && buf && class_id == JS_CLASS_SHARED_ARRAY_BUFFER &&
        !js_array_buffer_atomic_is_aligned(buf)) {
        return JS_ThrowTypeError(ctx, "misaligned SharedArrayBuffer backing storage");
    }
#endif
    obj = js_create_from_ctor(ctx, new_target, class_id);
    if (JS_IsException(obj))
        return obj;
    /* XXX: we are currently limited to 2 GB */
    if (len > INT32_MAX) {
        JS_ThrowRangeError(ctx, "invalid array buffer length");
        goto fail;
    }
    if (max_len && *max_len > INT32_MAX) {
        JS_ThrowRangeError(ctx, "invalid max array buffer length");
        goto fail;
    }
    alloc_len = class_id == JS_CLASS_SHARED_ARRAY_BUFFER && max_len ?
        *max_len : len;
    alloc_len = max_int(alloc_len, 1);
    if (growable_shared) {
        size_t overhead = _Alignof(JSSharedArrayBufferLength) - 1 +
            sizeof(JSSharedArrayBufferLength);
        if (alloc_len > SIZE_MAX - overhead) {
            JS_ThrowRangeError(ctx, "shared array buffer capacity overflow");
            goto fail;
        }
        alloc_len = js_shared_array_buffer_allocation_size(*max_len);
    }
    abuf = js_malloc(ctx, sizeof(*abuf));
    if (!abuf)
        goto fail;
    abuf->byte_length = len;
    abuf->max_byte_length = max_len ? *max_len : -1;
    if (!buf) {
        alloc_flag = TRUE;
        free_func = js_array_buffer_free;
    }
    if (class_id == JS_CLASS_SHARED_ARRAY_BUFFER)
        shared_functions = rt->sab_funcs;
    if (alloc_flag) {
        if (class_id == JS_CLASS_SHARED_ARRAY_BUFFER &&
            shared_functions.sab_alloc) {
            if (!shared_functions.sab_free) {
                JS_ThrowTypeError(ctx,
                                  "SharedArrayBuffer allocator has no free callback");
                goto fail;
            }
            abuf->data = shared_functions.sab_alloc(shared_functions.sab_opaque,
                                                    alloc_len);
            if (!abuf->data)
                goto fail;
            uses_shared_callbacks = TRUE;
#ifdef CONFIG_ATOMICS
            if (!js_array_buffer_atomic_is_aligned(abuf->data)) {
                shared_functions.sab_free(shared_functions.sab_opaque, abuf->data);
                abuf->data = NULL;
                JS_ThrowTypeError(ctx, "misaligned SharedArrayBuffer backing storage");
                goto fail;
            }
#endif
            memset(abuf->data, 0, alloc_len);
        } else {
            /* the allocation must be done after the object creation */
            abuf->data = js_mallocz(ctx, alloc_len);
            if (!abuf->data)
                goto fail;
#ifdef CONFIG_ATOMICS
            if (class_id == JS_CLASS_SHARED_ARRAY_BUFFER &&
                !js_array_buffer_atomic_is_aligned(abuf->data)) {
                js_free(ctx, abuf->data);
                abuf->data = NULL;
                JS_ThrowTypeError(ctx, "misaligned SharedArrayBuffer backing storage");
                goto fail;
            }
#endif
        }
    } else {
        if (class_id == JS_CLASS_SHARED_ARRAY_BUFFER &&
            shared_functions.sab_dup) {
            if (!shared_functions.sab_free) {
                JS_ThrowTypeError(ctx,
                                  "SharedArrayBuffer duplicate has no free callback");
                goto fail;
            }
            shared_functions.sab_dup(shared_functions.sab_opaque, buf);
        }
        abuf->data = buf;
        uses_shared_callbacks =
            class_id == JS_CLASS_SHARED_ARRAY_BUFFER && shared_functions.sab_free;
    }
    if (growable_shared) {
        size_t length_offset = js_shared_array_buffer_length_offset(abuf->data,
                                                                    *max_len);
        abuf->shared_length = (JSSharedArrayBufferLength *)(abuf->data + length_offset);
        if (alloc_flag) {
#ifdef CONFIG_ATOMICS
            atomic_init(abuf->shared_length, len);
#else
            *abuf->shared_length = len;
#endif
        }
    }
    init_list_head(&abuf->array_list);
    abuf->detached = FALSE;
    abuf->shared = (class_id == JS_CLASS_SHARED_ARRAY_BUFFER);
    abuf->uses_shared_callbacks = uses_shared_callbacks;
    abuf->atomic_unaligned = FALSE;
#ifdef CONFIG_ATOMICS
    if (!abuf->shared)
        abuf->atomic_unaligned = !js_array_buffer_atomic_is_aligned(abuf->data);
#endif
    if (class_id == JS_CLASS_SHARED_ARRAY_BUFFER && uses_shared_callbacks) {
        abuf->opaque = shared_functions.sab_opaque;
        abuf->shared_free_func = shared_functions.sab_free;
    } else {
        abuf->opaque = opaque;
        abuf->free_func = free_func;
    }
    if (alloc_flag && buf)
        memcpy(abuf->data, buf, len);
    JS_SetOpaque(obj, abuf);
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    js_free(ctx, abuf);
    return JS_EXCEPTION;
}

JSValue js_array_buffer_constructor3(JSContext *ctx, JSValueConst new_target,
                                     uint64_t len, uint64_t *max_len,
                                     JSClassID class_id, uint8_t *buf,
                                     JSFreeArrayBufferDataFunc *free_func,
                                     void *opaque, BOOL alloc_flag)
{
    return js_array_buffer_constructor4(ctx, new_target, len, max_len, class_id,
                                        buf, free_func, opaque, alloc_flag, FALSE);
}

JSValue js_clone_shared_array_buffer(JSContext *ctx, uint32_t len,
                                     uint64_t *max_len, uint8_t *data)
{
    if (max_len) {
        JSSharedArrayBufferLength *shared_length;
        uint32_t current_length;
        if (*max_len > INT32_MAX || len > *max_len || !data)
            return JS_ThrowTypeError(ctx, "invalid growable shared array buffer");
        shared_length = (JSSharedArrayBufferLength *)(data +
            js_shared_array_buffer_length_offset(data, *max_len));
#ifdef CONFIG_ATOMICS
        current_length = atomic_load_explicit(shared_length, memory_order_seq_cst);
#else
        current_length = *shared_length;
#endif
        if (current_length < len || current_length > *max_len)
            return JS_ThrowTypeError(ctx, "invalid shared array buffer length");
    }
    return js_array_buffer_constructor4(ctx, JS_UNDEFINED, len, max_len,
                                        JS_CLASS_SHARED_ARRAY_BUFFER, data,
                                        NULL, NULL, FALSE, TRUE);
}

void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr)
{
    js_free_rt(rt, ptr);
}

static JSValue js_array_buffer_constructor2(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, uint64_t *max_len,
                                            JSClassID class_id)
{
    return js_array_buffer_constructor3(ctx, new_target, len, max_len, class_id,
                                        NULL, js_array_buffer_free, NULL,
                                        TRUE);
}

JSValue js_array_buffer_constructor1(JSContext *ctx,
                                     JSValueConst new_target,
                                     uint64_t len, uint64_t *max_len)
{
    return js_array_buffer_constructor2(ctx, new_target, len, max_len,
                                        JS_CLASS_ARRAY_BUFFER);
}

JSValue JS_NewArrayBuffer(JSContext *ctx, uint8_t *buf, size_t len,
                          JSFreeArrayBufferDataFunc *free_func, void *opaque,
                          BOOL is_shared)
{
    JSClassID class_id =
        is_shared ? JS_CLASS_SHARED_ARRAY_BUFFER : JS_CLASS_ARRAY_BUFFER;
    return js_array_buffer_constructor3(ctx, JS_UNDEFINED, len, NULL, class_id,
                                        buf, free_func, opaque, FALSE);
}

/* create a new ArrayBuffer of length 'len' and copy 'buf' to it */
JSValue JS_NewArrayBufferCopy(JSContext *ctx, const uint8_t *buf, size_t len)
{
    return js_array_buffer_constructor3(ctx, JS_UNDEFINED, len, NULL,
                                        JS_CLASS_ARRAY_BUFFER,
                                        (uint8_t *)buf,
                                        js_array_buffer_free, NULL,
                                        TRUE);
}

static JSValue js_array_buffer_constructor0(JSContext *ctx, JSValueConst new_target,
                                            int argc, JSValueConst *argv,
                                            JSClassID class_id)
 {
    uint64_t len, max_len, *pmax_len = NULL;
    JSValue obj, val;
    int ret;

     if (JS_ToIndex(ctx, &len, argv[0]))
         return JS_EXCEPTION;
    if (argc < 2)
        goto next;
    if (!JS_IsObject(argv[1]))
        goto next;
    obj = JS_ToObject(ctx, argv[1]);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    val = JS_GetProperty(ctx, obj, JS_ATOM_maxByteLength);
    JS_FreeValue(ctx, obj);
    if (JS_IsException(val))
        return JS_EXCEPTION;
    if (JS_IsUndefined(val))
        goto next;
    ret = JS_ToIndex(ctx, &max_len, val);
    JS_FreeValue(ctx, val);
    if (ret)
        return JS_EXCEPTION;
    if (len > max_len)
        return JS_ThrowRangeError(ctx, "invalid array buffer max length");
    pmax_len = &max_len;
next:
    return js_array_buffer_constructor2(ctx, new_target, len, pmax_len,
                                        class_id);
}

JSValue js_array_buffer_constructor(JSContext *ctx,
                                    JSValueConst new_target,
                                    int argc, JSValueConst *argv)
{
    return js_array_buffer_constructor0(ctx, new_target, argc, argv,
                                        JS_CLASS_ARRAY_BUFFER);
}

JSValue js_shared_array_buffer_constructor(JSContext *ctx,
                                           JSValueConst new_target,
                                           int argc, JSValueConst *argv)
{
    return js_array_buffer_constructor0(ctx, new_target, argc, argv,
                                        JS_CLASS_SHARED_ARRAY_BUFFER);
}

/* also used for SharedArrayBuffer */
void js_array_buffer_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSArrayBuffer *abuf = p->u.array_buffer;
    struct list_head *el, *el1;

    if (abuf) {
        /* The ArrayBuffer finalizer may be called before the typed
           array finalizers using it, so abuf->array_list is not
           necessarily empty. */
        list_for_each_safe(el, el1, &abuf->array_list) {
            JSTypedArray *ta;
            JSObject *p1;

            ta = list_entry(el, JSTypedArray, link);
            ta->link.prev = NULL;
            ta->link.next = NULL;
            p1 = ta->obj;
            /* Note: the typed array length and offset fields are not modified */
            if (p1->class_id != JS_CLASS_DATAVIEW) {
                p1->u.array.count = 0;
                p1->u.array.u.ptr = NULL;
            }
        }
        if (abuf->uses_shared_callbacks) {
            abuf->shared_free_func(abuf->opaque, abuf->data);
        } else {
            if (abuf->free_func)
                abuf->free_func(rt, abuf->opaque, abuf->data);
        }
        js_free_rt(rt, abuf);
    }
}

static JSValue js_array_buffer_isView(JSContext *ctx,
                                      JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    JSObject *p;
    BOOL res;
    res = FALSE;
    if (JS_VALUE_GET_TAG(argv[0]) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(argv[0]);
        if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
            p->class_id <= JS_CLASS_DATAVIEW) {
            res = TRUE;
        }
    }
    return JS_NewBool(ctx, res);
}

const JSCFunctionListEntry js_array_buffer_funcs[] = {
    JS_CFUNC_DEF("isView", 1, js_array_buffer_isView ),
    JS_CGETSET_DEF("[Symbol.species]", js_get_this, NULL ),
};

JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "ArrayBuffer is detached");
}

JSValue JS_ThrowTypeErrorArrayBufferOOB(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "ArrayBuffer is detached or resized");
}

// #sec-get-arraybuffer.prototype.detached
static JSValue js_array_buffer_get_detached(JSContext *ctx,
                                                 JSValueConst this_val)
{
    JSArrayBuffer *abuf = JS_GetOpaque2(ctx, this_val, JS_CLASS_ARRAY_BUFFER);
    if (!abuf)
        return JS_EXCEPTION;
    if (abuf->shared)
        return JS_ThrowTypeError(ctx, "detached called on SharedArrayBuffer");
    return JS_NewBool(ctx, abuf->detached);
}

static JSValue js_array_buffer_get_byteLength(JSContext *ctx,
                                              JSValueConst this_val,
                                              int class_id)
{
    JSArrayBuffer *abuf = JS_GetOpaque2(ctx, this_val, class_id);
    if (!abuf)
        return JS_EXCEPTION;
    /* return 0 if detached */
    return JS_NewUint32(ctx, js_array_buffer_byte_length(abuf));
}

static JSValue js_array_buffer_get_maxByteLength(JSContext *ctx,
                                                 JSValueConst this_val,
                                                 int class_id)
{
    JSArrayBuffer *abuf = JS_GetOpaque2(ctx, this_val, class_id);
    if (!abuf)
        return JS_EXCEPTION;
    if (array_buffer_is_resizable(abuf))
        return JS_NewUint32(ctx, abuf->max_byte_length);
    return JS_NewUint32(ctx, js_array_buffer_byte_length(abuf));
}

static JSValue js_array_buffer_get_resizable(JSContext *ctx,
                                             JSValueConst this_val,
                                             int class_id)
{
    JSArrayBuffer *abuf = JS_GetOpaque2(ctx, this_val, class_id);
    if (!abuf)
        return JS_EXCEPTION;
    return JS_NewBool(ctx, array_buffer_is_resizable(abuf));
}

static void js_array_buffer_update_typed_arrays(JSArrayBuffer *abuf)
{
    uint32_t size_log2;
    struct list_head *el;
    JSTypedArray *ta;
    JSObject *p;
    uint8_t *data;
    int64_t len;

    len = js_array_buffer_byte_length(abuf);
    data = abuf->data;
    // update lengths of all typed arrays backed by this array buffer
    list_for_each(el, &abuf->array_list) {
        ta = list_entry(el, JSTypedArray, link);
        p = ta->obj;
        if (p->class_id == JS_CLASS_DATAVIEW) {
            if (ta->track_rab) {
                if (ta->offset < len)
                    ta->length = len - ta->offset;
                else
                    ta->length = 0;
            }
        } else {
            p->u.array.count = 0;
            p->u.array.u.ptr = NULL;
            if (abuf->detached)
                continue;
            size_log2 = typed_array_size_log2(p->class_id);
            /* Attached, in-bounds views keep a pointer even when empty. */
            if (ta->track_rab) {
                if (len >= ta->offset) {
                    p->u.array.count = (len - ta->offset) >> size_log2;
                    p->u.array.u.ptr = &data[ta->offset];
                }
            } else {
                if (len >= (int64_t)ta->offset + ta->length) {
                    p->u.array.count = ta->length >> size_log2;
                    p->u.array.u.ptr = &data[ta->offset];
                }
            }
        }
    }
    
}

void JS_DetachArrayBuffer(JSContext *ctx, JSValueConst obj)
{
    JSArrayBuffer *abuf = JS_GetOpaque(obj, JS_CLASS_ARRAY_BUFFER);

    if (!abuf || abuf->detached)
        return;
    if (abuf->free_func)
        abuf->free_func(ctx->rt, abuf->opaque, abuf->data);
    abuf->data = NULL;
    abuf->atomic_unaligned = FALSE;
    abuf->byte_length = 0;
    abuf->detached = TRUE;
    js_array_buffer_update_typed_arrays(abuf);
}

/* get an ArrayBuffer or SharedArrayBuffer */
JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        goto fail;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id != JS_CLASS_ARRAY_BUFFER &&
        p->class_id != JS_CLASS_SHARED_ARRAY_BUFFER) {
    fail:
        JS_ThrowTypeErrorInvalidClass(ctx, JS_CLASS_ARRAY_BUFFER);
        return NULL;
    }
    return p->u.array_buffer;
}

/* return NULL if exception. WARNING: any JS call can detach the
   buffer and render the returned pointer invalid */
uint8_t *JS_GetArrayBuffer(JSContext *ctx, size_t *psize, JSValueConst obj)
{
    JSArrayBuffer *abuf = js_get_array_buffer(ctx, obj);
    if (!abuf)
        goto fail;
    if (abuf->detached) {
        JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
        goto fail;
    }
    *psize = js_array_buffer_byte_length(abuf);
    return abuf->data;
 fail:
    *psize = 0;
    return NULL;
}

BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf)
{
    return abuf->max_byte_length >= 0;
}

// ES #sec-arraybuffer.prototype.transfer
static JSValue js_array_buffer_transfer(JSContext *ctx,
                                        JSValueConst this_val,
                                        int argc, JSValueConst *argv,
                                        int transfer_to_fixed_length)
{
    JSArrayBuffer *abuf;
    uint64_t new_len, *pmax_len, max_len;
    JSValue res;

    abuf = JS_GetOpaque2(ctx, this_val, JS_CLASS_ARRAY_BUFFER);
    if (!abuf)
        return JS_EXCEPTION;
    if (abuf->shared)
        return JS_ThrowTypeError(ctx, "cannot transfer a SharedArrayBuffer");
    if (argc < 1 || JS_IsUndefined(argv[0]))
        new_len = js_array_buffer_byte_length(abuf);
    else if (JS_ToIndex(ctx, &new_len, argv[0]))
        return JS_EXCEPTION;
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    pmax_len = NULL;
    if (!transfer_to_fixed_length) {
        if (array_buffer_is_resizable(abuf)) { // carry over maxByteLength
            max_len = abuf->max_byte_length;
            if (new_len > max_len)
                return JS_ThrowRangeError(ctx, "invalid array buffer length");
            // TODO(bnoordhuis) support externally managed RABs
            if (abuf->free_func == js_array_buffer_free)
                pmax_len = &max_len;
        }
    }

    /* create an empty AB */
    if (new_len == 0) {
        res = js_array_buffer_constructor2(ctx, JS_UNDEFINED, 0, pmax_len, JS_CLASS_ARRAY_BUFFER);
        if (JS_IsException(res))
            return res;
        JS_DetachArrayBuffer(ctx, this_val);
    } else {
        uint64_t old_len;
        
        old_len = js_array_buffer_byte_length(abuf);

        /* if length mismatch, realloc. Otherwise, use the same backing buffer. */
        if (new_len != old_len) {
            /* XXX: we are currently limited to 2 GB */
            if (new_len > INT32_MAX)
                return JS_ThrowRangeError(ctx, "invalid array buffer length");

            if (abuf->free_func != js_array_buffer_free) {
                JSArrayBuffer *new_abuf;
                /* cannot use js_realloc() because the buffer was
                   allocated with a custom allocator */
                res = js_array_buffer_constructor2(ctx, JS_UNDEFINED, new_len, pmax_len, JS_CLASS_ARRAY_BUFFER);
                if (JS_IsException(res))
                    return res;
                new_abuf = JS_GetOpaque2(ctx, res, JS_CLASS_ARRAY_BUFFER);
                memcpy(new_abuf->data, abuf->data, min_int(old_len, new_len));
                if (abuf->free_func)
                    abuf->free_func(ctx->rt, abuf->opaque, abuf->data);
            } else {
                JSArrayBuffer *new_abuf;
                uint8_t *new_bs;
                /* reallocate the buffer after the new array buffer is
                   created in case the new array buffer creation
                   fails. */
                res = js_array_buffer_constructor2(ctx, JS_UNDEFINED, 0, pmax_len, JS_CLASS_ARRAY_BUFFER);
                if (JS_IsException(res))
                    return res;
                new_bs = js_realloc(ctx, abuf->data, new_len);
                if (!new_bs) {
                    JS_FreeValue(ctx, res);
                    return JS_EXCEPTION;
                }
                if (new_len > old_len)
                    memset(new_bs + old_len, 0, new_len - old_len);
                new_abuf = JS_GetOpaque2(ctx, res, JS_CLASS_ARRAY_BUFFER);
                js_free(ctx, new_abuf->data);
                new_abuf->data = new_bs;
                new_abuf->byte_length = new_len;
#ifdef CONFIG_ATOMICS
                new_abuf->atomic_unaligned = !js_array_buffer_atomic_is_aligned(new_bs);
#endif
            }
        } else {
            /* can keep the custom free function */
            res = js_array_buffer_constructor3(ctx, JS_UNDEFINED, new_len, pmax_len,
                                               JS_CLASS_ARRAY_BUFFER,
                                               abuf->data, abuf->free_func,
                                               abuf->opaque, FALSE);
            if (JS_IsException(res))
                return res;
        }
        /* neuter the backing buffer */
        abuf->data = NULL;
        abuf->atomic_unaligned = FALSE;
        abuf->byte_length = 0;
        abuf->detached = TRUE;
        js_array_buffer_update_typed_arrays(abuf);
    }
    return res;
}

static JSValue js_array_buffer_resize(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int class_id)
{
    JSArrayBuffer *abuf;
    uint8_t *data;
    uint64_t len;

    abuf = JS_GetOpaque2(ctx, this_val, class_id);
    if (!abuf)
        return JS_EXCEPTION;
    if (!array_buffer_is_resizable(abuf))
        return JS_ThrowTypeError(ctx, "array buffer is not resizable");
    if (JS_ToIndex(ctx, &len, argv[0]))
        return JS_EXCEPTION;
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    // TODO(bnoordhuis) support externally managed RABs
    if (!abuf->shared && abuf->free_func != js_array_buffer_free)
        return JS_ThrowTypeError(ctx, "external array buffer is not resizable");
    if (len > abuf->max_byte_length) {
    bad_length:
        return JS_ThrowRangeError(ctx, "invalid array buffer length");
    }
    // SABs can only grow and we don't need to realloc because
    // js_array_buffer_constructor3 commits all memory upfront;
    // regular RABs are resizable both ways and realloc
    if (abuf->shared) {
        uint32_t current_length = js_array_buffer_byte_length(abuf);
        for (;;) {
            if (len < current_length)
                goto bad_length;
            if (len == current_length)
                break;
#ifdef CONFIG_ATOMICS
            if (atomic_compare_exchange_strong_explicit(abuf->shared_length,
                    &current_length, len, memory_order_seq_cst,
                    memory_order_seq_cst))
                break;
#else
            *abuf->shared_length = len;
            break;
#endif
        }
    } else {
        data = js_realloc(ctx, abuf->data, max_int(len, 1));
        if (!data)
            return JS_EXCEPTION;
        if (len > abuf->byte_length)
            memset(&data[abuf->byte_length], 0, len - abuf->byte_length);
        abuf->byte_length = len;
        abuf->data = data;
#ifdef CONFIG_ATOMICS
        abuf->atomic_unaligned = !js_array_buffer_atomic_is_aligned(data);
#endif
    }
    js_array_buffer_update_typed_arrays(abuf);
    return JS_UNDEFINED;
}

static JSValue js_array_buffer_slice(JSContext *ctx,
                                     JSValueConst this_val,
                                     int argc, JSValueConst *argv, int class_id)
{
    JSArrayBuffer *abuf, *new_abuf;
    int64_t len, start, end, new_len;
    JSValue ctor, new_obj;

    abuf = JS_GetOpaque2(ctx, this_val, class_id);
    if (!abuf)
        return JS_EXCEPTION;
    if (abuf->detached)
        return JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
    len = js_array_buffer_byte_length(abuf);

    if (JS_ToInt64Clamp(ctx, &start, argv[0], 0, len, len))
        return JS_EXCEPTION;

    end = len;
    if (!JS_IsUndefined(argv[1])) {
        if (JS_ToInt64Clamp(ctx, &end, argv[1], 0, len, len))
            return JS_EXCEPTION;
    }
    new_len = max_int64(end - start, 0);
    ctor = JS_SpeciesConstructor(ctx, this_val, JS_UNDEFINED);
    if (JS_IsException(ctor))
        return ctor;
    if (JS_IsUndefined(ctor)) {
        new_obj = js_array_buffer_constructor2(ctx, JS_UNDEFINED, new_len,
                                               NULL, class_id);
    } else {
        JSValue args[1];
        args[0] = JS_NewInt64(ctx, new_len);
        new_obj = JS_CallConstructor(ctx, ctor, 1, (JSValueConst *)args);
        JS_FreeValue(ctx, ctor);
        JS_FreeValue(ctx, args[0]);
    }
    if (JS_IsException(new_obj))
        return new_obj;
    new_abuf = JS_GetOpaque2(ctx, new_obj, class_id);
    if (!new_abuf)
        goto fail;
    if (abuf->shared ? new_abuf->data == abuf->data :
        js_same_value(ctx, new_obj, this_val)) {
        JS_ThrowTypeError(ctx, "cannot use identical ArrayBuffer");
        goto fail;
    }
    if (new_abuf->detached) {
        JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
        goto fail;
    }
    if (js_array_buffer_byte_length(new_abuf) < new_len) {
        JS_ThrowTypeError(ctx, "new ArrayBuffer is too small");
        goto fail;
    }
    /* must test again because of side effects */
    if (abuf->detached) {
        JS_ThrowTypeErrorDetachedArrayBuffer(ctx);
        goto fail;
    }
    new_len = min_int64(new_len, max_int64((int64_t)js_array_buffer_byte_length(abuf) - start, 0));
    if (new_len > 0)
        memcpy(new_abuf->data, abuf->data + start, new_len);
    return new_obj;
 fail:
    JS_FreeValue(ctx, new_obj);
    return JS_EXCEPTION;
}

const JSCFunctionListEntry js_array_buffer_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("byteLength", js_array_buffer_get_byteLength, NULL, JS_CLASS_ARRAY_BUFFER ),
    JS_CGETSET_MAGIC_DEF("maxByteLength", js_array_buffer_get_maxByteLength, NULL, JS_CLASS_ARRAY_BUFFER ),
    JS_CGETSET_MAGIC_DEF("resizable", js_array_buffer_get_resizable, NULL, JS_CLASS_ARRAY_BUFFER ),
    JS_CGETSET_DEF("detached", js_array_buffer_get_detached, NULL ),
    JS_CFUNC_MAGIC_DEF("resize", 1, js_array_buffer_resize, JS_CLASS_ARRAY_BUFFER ),
    JS_CFUNC_MAGIC_DEF("slice", 2, js_array_buffer_slice, JS_CLASS_ARRAY_BUFFER ),
    JS_CFUNC_MAGIC_DEF("transfer", 0, js_array_buffer_transfer, 0 ),
    JS_CFUNC_MAGIC_DEF("transferToFixedLength", 0, js_array_buffer_transfer, 1 ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "ArrayBuffer", JS_PROP_CONFIGURABLE ),
};

/* SharedArrayBuffer */

const JSCFunctionListEntry js_shared_array_buffer_funcs[] = {
    JS_CGETSET_DEF("[Symbol.species]", js_get_this, NULL ),
};

const JSCFunctionListEntry js_shared_array_buffer_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("byteLength", js_array_buffer_get_byteLength, NULL, JS_CLASS_SHARED_ARRAY_BUFFER ),
    JS_CGETSET_MAGIC_DEF("maxByteLength", js_array_buffer_get_maxByteLength, NULL, JS_CLASS_SHARED_ARRAY_BUFFER ),
    JS_CGETSET_MAGIC_DEF("growable", js_array_buffer_get_resizable, NULL, JS_CLASS_SHARED_ARRAY_BUFFER ),
    JS_CFUNC_MAGIC_DEF("grow", 1, js_array_buffer_resize, JS_CLASS_SHARED_ARRAY_BUFFER ),
    JS_CFUNC_MAGIC_DEF("slice", 2, js_array_buffer_slice, JS_CLASS_SHARED_ARRAY_BUFFER ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "SharedArrayBuffer", JS_PROP_CONFIGURABLE ),
};
