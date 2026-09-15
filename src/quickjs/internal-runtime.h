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
#ifndef QUICKJS_INTERNAL_RUNTIME_H
#define QUICKJS_INTERNAL_RUNTIME_H

#include "internal-opcode.h"

static inline JSMallocBlockHeader *js_rc(void *ptr)
{
    return container_of(ptr, JSMallocBlockHeader, user_data);
}

static force_inline void JS_FreeValue_inline(JSContext *ctx, JSValue value);

#define JS_FreeValue(ctx, value) JS_FreeValue_inline((ctx), (value))

/* Keep the engine-internal zero-ref path direct in normal non-LTO builds. */
static force_inline void JS_FreeValue_inline(JSContext *ctx, JSValue value)
{
    if (JS_VALUE_HAS_REF_COUNT(value)) {
        JSRefCountHeader *header = __js_rc(JS_VALUE_GET_PTR(value));

        if (--header->ref_count <= 0)
            __JS_FreeValueRT(ctx->rt, value);
    }
}

static inline void js_dbuf_init(JSContext *ctx, DynBuf *s)
{
    dbuf_init2(s, ctx->rt, (DynBufReallocFunc *)js_realloc_rt);
}

#ifndef QUICKJS_NUMBER_OWNER
static inline int is_digit(int c)
{
    return c >= '0' && c <= '9';
}
#else
static inline int is_digit(int c);
#endif

static inline BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size)
{
#if !defined(CONFIG_STACK_CHECK)
    return FALSE;
#else
    uintptr_t sp = (uintptr_t)__builtin_frame_address(0) - alloca_size;
    return unlikely(sp < rt->stack_limit);
#endif
}

QJS_INTERNAL __exception int __js_poll_interrupts(JSContext *ctx);

#ifndef QUICKJS_OBJECT_OWNER
static inline __exception int js_poll_interrupts(JSContext *ctx)
{
    if (unlikely(--ctx->interrupt_counter <= 0))
        return __js_poll_interrupts(ctx);
    return 0;
}
#endif

static inline BOOL is_be(void)
{
    union {
        uint16_t value;
        uint8_t byte;
    } endian = { 0x100 };
    return endian.byte;
}

static inline void set_value(JSContext *ctx, JSValue *slot,
                                 JSValue value)
{
    JSValue old_value = *slot;
    *slot = value;
    JS_FreeValue(ctx, old_value);
}

QJS_INTERNAL int js_resize_array(JSContext *ctx, void **parray, int elem_size,
                                 int *psize, int req_size);
QJS_INTERNAL void js_dbuf_bytecode_init(JSContext *ctx, DynBuf *buf);
QJS_INTERNAL void dbuf_put_leb128(DynBuf *s, uint32_t v);
QJS_INTERNAL void dbuf_put_sleb128(DynBuf *s, int32_t v);
QJS_INTERNAL int get_leb128(uint32_t *pval, const uint8_t *buf,
                            const uint8_t *buf_end);
QJS_INTERNAL int get_sleb128(int32_t *pval, const uint8_t *buf,
                             const uint8_t *buf_end);
QJS_INTERNAL JSValue JS_ThrowStackOverflow(JSContext *ctx);
QJS_INTERNAL void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                                JSGCObjectTypeEnum type);
QJS_INTERNAL void remove_gc_object(JSGCObjectHeader *h);
static inline void qjs_add_gc_object_fast(JSRuntime *rt,
                                          JSGCObjectHeader *header,
                                          JSGCObjectTypeEnum type)
{
    js_rc(header)->mark = 0;
    js_rc(header)->gc_obj_type = type;
    list_add_tail(&header->link, &rt->gc_obj_list);
}

static inline void qjs_remove_gc_object_fast(JSGCObjectHeader *header)
{
    list_del(&header->link);
}
QJS_INTERNAL void JS_MarkContext(JSRuntime *rt, JSContext *ctx,
                                   JS_MarkFunc *mark_func);
QJS_INTERNAL int find_line_num(JSContext *ctx, JSFunctionBytecode *bytecode,
                                   uint32_t pc_value, int *pcol_num);
QJS_INTERNAL void build_backtrace(JSContext *ctx, JSValueConst error_obj,
                                      const char *filename, int line_num,
                                      int col_num, int flags);
QJS_INTERNAL JSValue JS_ThrowError2(JSContext *ctx, JSErrorEnum error_num,
                                      const char *fmt, va_list ap,
                                      BOOL add_backtrace);
QJS_INTERNAL BOOL is_backtrace_needed(JSContext *ctx,
                                          JSValueConst obj);
QJS_INTERNAL JSValue JS_ThrowReferenceErrorNotDefined(
    JSContext *ctx, JSAtom atom);
QJS_INTERNAL JSValue JS_ThrowReferenceErrorUninitialized(
    JSContext *ctx, JSAtom atom);
QJS_INTERNAL JSValue JS_ThrowReferenceErrorUninitialized2(
    JSContext *ctx, JSFunctionBytecode *bytecode, int index, BOOL is_arg);
QJS_INTERNAL JSValue JS_ThrowSyntaxErrorVarRedeclaration(
    JSContext *ctx, JSAtom atom);
QJS_INTERNAL int JS_ThrowTypeErrorReadOnly(
    JSContext *ctx, int flags, JSAtom atom);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAConstructor(
    JSContext *ctx, JSValueConst value);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);

#endif /* QUICKJS_INTERNAL_RUNTIME_H */
