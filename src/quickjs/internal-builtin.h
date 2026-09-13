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
#ifndef QUICKJS_INTERNAL_BUILTIN_H
#define QUICKJS_INTERNAL_BUILTIN_H

#include "internal-frontend.h"

QJS_INTERNAL int qjs_add_intrinsic_basic_objects(JSContext *ctx);
QJS_INTERNAL int qjs_add_intrinsic_math(JSContext *ctx);
QJS_INTERNAL int qjs_add_intrinsics(JSContext *ctx);

QJS_INTERNAL JSValue qjs_math_get_iterator(JSContext *ctx,
                                           JSValueConst obj, BOOL is_async);
QJS_INTERNAL JSValue qjs_math_iterator_next(JSContext *ctx,
                                            JSValueConst iterator,
                                            JSValueConst method,
                                            int argc, JSValueConst *argv,
                                            BOOL *pdone);
QJS_INTERNAL int qjs_math_iterator_close(JSContext *ctx,
                                         JSValueConst iterator,
                                         BOOL is_exception_pending);
QJS_INTERNAL double qjs_math_pow(double left, double right);

#endif /* QUICKJS_INTERNAL_BUILTIN_H */
