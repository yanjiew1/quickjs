/*
 * QuickJS exception and backtrace interface
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
#ifndef QUICKJS_ERROR_H
#define QUICKJS_ERROR_H

#include "function.h"

JSValue JS_ThrowError(JSContext *ctx, JSErrorEnum error_num,
                             const char *fmt, va_list ap);
JSValue JS_ThrowReferenceErrorNotDefined(JSContext *ctx, JSAtom name);
JSValue JS_ThrowReferenceErrorUninitialized(JSContext *ctx, JSAtom name);
JSValue JS_ThrowReferenceErrorUninitialized2(JSContext *ctx,
                                                    JSFunctionBytecode *b,
                                                    int idx, BOOL is_ref);
const char *get_prop_string(JSContext *ctx, JSValueConst obj, JSAtom prop);
BOOL is_backtrace_needed(JSContext *ctx, JSValueConst obj);
JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);
JSValue JS_ThrowStackOverflow(JSContext *ctx);
JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx,
                                                JSValueConst func_obj);
JSValue JS_ThrowError2(JSContext *ctx, JSErrorEnum error_num,
                              const char *fmt, va_list ap, BOOL add_backtrace);
void build_backtrace(JSContext *ctx, JSValueConst error_obj,
                            const char *filename, int line_num, int col_num,
                            int backtrace_flags);
int __attribute__((format(printf, 3, 4))) JS_ThrowTypeErrorOrFalse(JSContext *ctx, int flags, const char *fmt, ...);
#define JS_ThrowSyntaxErrorAtom(ctx, fmt, atom) __JS_ThrowSyntaxErrorAtom(ctx, atom, fmt, "")
JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowSyntaxErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...);
JSValue __attribute__((format(printf, 2, 3))) JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...);
JSValue JS_ThrowOutOfMemory(JSContext *ctx);

#endif
