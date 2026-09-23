/*
 * QuickJS Error Internal Interface
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
#ifndef QJS_ERROR_H
#define QJS_ERROR_H

#include <stdarg.h>
#include "runtime.h"

QJS_INTERNAL JSValue JS_ThrowError(JSContext *ctx, JSErrorEnum error_num,
                                    const char *fmt, va_list ap);
QJS_INTERNAL JSValue JS_ThrowTypeErrorInvalidClass(JSContext *ctx, int class_id);
QJS_INTERNAL void JS_ThrowInterrupted(JSContext *ctx);
QJS_INTERNAL JSValue JS_ThrowStackOverflow(JSContext *ctx);

QJS_INTERNAL JSValue js_aggregate_error_constructor(JSContext *ctx, JSValueConst errors);

#define JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL (1 << 0)
QJS_INTERNAL void build_backtrace(JSContext *ctx, JSValueConst error_obj, const char *filename, int line_num, int col_num, int backtrace_flags);

QJS_INTERNAL JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowSyntaxErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...);
#define JS_ThrowSyntaxErrorAtom(ctx, fmt, atom) __JS_ThrowSyntaxErrorAtom(ctx, atom, fmt, "")

#endif /* QJS_ERROR_H */
