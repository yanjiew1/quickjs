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

static inline JSMallocBlockHeader *qjs_get_ref_header(void *ptr)
{
    return container_of(ptr, JSMallocBlockHeader, user_data);
}

static inline void qjs_dbuf_init(JSContext *ctx, DynBuf *s)
{
    dbuf_init2(s, ctx->rt, (DynBufReallocFunc *)js_realloc_rt);
}

static inline int qjs_is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static inline BOOL qjs_check_stack_overflow(JSRuntime *rt, size_t alloca_size)
{
#if !defined(CONFIG_STACK_CHECK)
    return FALSE;
#else
    uintptr_t sp = (uintptr_t)__builtin_frame_address(0) - alloca_size;
    return unlikely(sp < rt->stack_limit);
#endif
}

QJS_INTERNAL __exception int qjs_poll_interrupts_slow(JSContext *ctx);

static inline __exception int qjs_poll_interrupts(JSContext *ctx)
{
    if (unlikely(--ctx->interrupt_counter <= 0))
        return qjs_poll_interrupts_slow(ctx);
    return 0;
}

static inline BOOL qjs_is_be(void)
{
    union {
        uint16_t value;
        uint8_t byte;
    } endian = { 0x100 };
    return endian.byte;
}

static inline void qjs_set_value(JSContext *ctx, JSValue *slot,
                                 JSValue value)
{
    JSValue old_value = *slot;
    *slot = value;
    JS_FreeValue(ctx, old_value);
}

QJS_INTERNAL int qjs_resize_array(JSContext *ctx, void **parray, int elem_size,
                                  int *psize, int req_size);
QJS_INTERNAL void qjs_dbuf_bytecode_init(JSContext *ctx, DynBuf *buf);
QJS_INTERNAL void qjs_dbuf_put_leb128(DynBuf *s, uint32_t v);
QJS_INTERNAL void qjs_dbuf_put_sleb128(DynBuf *s, int32_t v);
QJS_INTERNAL int qjs_get_leb128(uint32_t *pval, const uint8_t *buf,
                            const uint8_t *buf_end);
QJS_INTERNAL int qjs_get_sleb128(int32_t *pval, const uint8_t *buf,
                             const uint8_t *buf_end);
QJS_INTERNAL JSValue qjs_throw_stack_overflow(JSContext *ctx);
QJS_INTERNAL void qjs_add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                                JSGCObjectTypeEnum type);
QJS_INTERNAL void qjs_remove_gc_object(JSGCObjectHeader *h);
QJS_INTERNAL void qjs_mark_context(JSRuntime *rt, JSContext *ctx,
                                   JS_MarkFunc *mark_func);
QJS_INTERNAL int qjs_find_line_num(JSContext *ctx, JSFunctionBytecode *bytecode,
                                   uint32_t pc_value, int *pcol_num);
QJS_INTERNAL void qjs_build_backtrace(JSContext *ctx, JSValueConst error_obj,
                                      const char *filename, int line_num,
                                      int col_num, int flags);
QJS_INTERNAL JSValue qjs_throw_error2(JSContext *ctx, JSErrorEnum error_num,
                                      const char *fmt, va_list ap,
                                      BOOL add_backtrace);
QJS_INTERNAL BOOL qjs_is_backtrace_needed(JSContext *ctx,
                                          JSValueConst obj);
QJS_INTERNAL JSValue qjs_throw_reference_error_not_defined(
    JSContext *ctx, JSAtom atom);
QJS_INTERNAL JSValue qjs_throw_reference_error_uninitialized(
    JSContext *ctx, JSAtom atom);
QJS_INTERNAL JSValue qjs_throw_reference_error_uninitialized2(
    JSContext *ctx, JSFunctionBytecode *bytecode, int index, BOOL is_arg);
QJS_INTERNAL JSValue qjs_throw_syntax_error_var_redeclaration(
    JSContext *ctx, JSAtom atom);
QJS_INTERNAL int qjs_throw_type_error_read_only(
    JSContext *ctx, int flags, JSAtom atom);
QJS_INTERNAL JSValue qjs_throw_type_error_not_constructor(
    JSContext *ctx, JSValueConst value);
QJS_INTERNAL JSValue qjs_throw_type_error_not_object(JSContext *ctx);

#endif /* QUICKJS_INTERNAL_RUNTIME_H */
