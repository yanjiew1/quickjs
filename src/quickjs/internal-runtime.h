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

QJS_INTERNAL JSMallocBlockHeader *js_rc(void *ptr);

QJS_INTERNAL void js_dbuf_init(JSContext *ctx, DynBuf *s);

QJS_INTERNAL int is_digit(int c);

QJS_INTERNAL BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size);

QJS_INTERNAL no_inline __exception int __js_poll_interrupts(JSContext *ctx);

QJS_INTERNAL __exception int js_poll_interrupts(JSContext *ctx);

QJS_INTERNAL BOOL is_be(void);

QJS_INTERNAL void set_value(JSContext *ctx, JSValue *pval, JSValue new_val);

QJS_INTERNAL int js_resize_array(JSContext *ctx, void **parray, int elem_size,
                                  int *psize, int req_size);
QJS_INTERNAL void js_dbuf_bytecode_init(JSContext *ctx, DynBuf *s);
QJS_INTERNAL void dbuf_put_leb128(DynBuf *s, uint32_t v);
QJS_INTERNAL void dbuf_put_sleb128(DynBuf *s, int32_t v1);
QJS_INTERNAL int get_leb128(uint32_t *pval, const uint8_t *buf,
                      const uint8_t *buf_end);
QJS_INTERNAL int get_sleb128(int32_t *pval, const uint8_t *buf,
                       const uint8_t *buf_end);
QJS_INTERNAL JSValue JS_ThrowStackOverflow(JSContext *ctx);
QJS_INTERNAL void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type);
QJS_INTERNAL void remove_gc_object(JSGCObjectHeader *h);

QJS_INTERNAL void JS_MarkContext(JSRuntime *rt, JSContext *ctx,
                           JS_MarkFunc *mark_func);
QJS_INTERNAL int find_line_num(JSContext *ctx, JSFunctionBytecode *b,
                         uint32_t pc_value, int *pcol_num);
QJS_INTERNAL void build_backtrace(JSContext *ctx, JSValueConst error_obj,
                            const char *filename, int line_num, int col_num,
                            int backtrace_flags);
QJS_INTERNAL JSValue JS_ThrowError2(JSContext *ctx, JSErrorEnum error_num,
                              const char *fmt, va_list ap, BOOL add_backtrace);
QJS_INTERNAL BOOL is_backtrace_needed(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL JSValue JS_ThrowReferenceErrorNotDefined(JSContext *ctx, JSAtom name);
QJS_INTERNAL JSValue JS_ThrowReferenceErrorUninitialized(JSContext *ctx, JSAtom name);
QJS_INTERNAL JSValue JS_ThrowReferenceErrorUninitialized2(JSContext *ctx,
                                                    JSFunctionBytecode *b,
                                                    int idx, BOOL is_ref);
QJS_INTERNAL JSValue JS_ThrowSyntaxErrorVarRedeclaration(JSContext *ctx, JSAtom prop);
QJS_INTERNAL int JS_ThrowTypeErrorReadOnly(JSContext *ctx, int flags, JSAtom atom);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx,
                                                JSValueConst func_obj);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);

#endif /* QUICKJS_INTERNAL_RUNTIME_H */
