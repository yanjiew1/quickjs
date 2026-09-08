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

#ifndef QUICKJS_JS_ERROR_H
#define QUICKJS_JS_ERROR_H

#include <stdio.h>
#include <stdarg.h>
#include "quickjs.h"
#include "quickjs-internal.h"

JSValue JS_ThrowError2(JSContext *ctx, JSErrorEnum error_num,
                       const char *fmt, va_list ap, BOOL add_backtrace);
JSValue JS_ThrowError(JSContext *ctx, JSErrorEnum error_num,
                      const char *fmt, va_list ap);
BOOL is_backtrace_needed(JSContext *ctx, JSValueConst obj);

JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);
JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx, JSValueConst func_obj);
JSValue JS_ThrowTypeErrorNotASymbol(JSContext *ctx);
int JS_ThrowTypeErrorReadOnly(JSContext *ctx, int flags, JSAtom atom);
JSValue JS_ThrowReferenceErrorNotDefined(JSContext *ctx, JSAtom name);
JSValue JS_ThrowReferenceErrorUninitialized(JSContext *ctx, JSAtom name);
JSValue JS_ThrowReferenceErrorUninitialized2(JSContext *ctx,
                                             JSFunctionBytecode *b,
                                             int idx, BOOL is_ref);
JSValue JS_ThrowTypeErrorInvalidClass(JSContext *ctx, int class_id);

#endif /* QUICKJS_JS_ERROR_H */
