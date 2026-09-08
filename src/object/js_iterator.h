/*
 * QuickJS Javascript Engine - Iterators & Enumeration Protocols
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

#ifndef QUICKJS_JS_ITERATOR_H
#define QUICKJS_JS_ITERATOR_H

#include "quickjs.h"
#include "quickjs-internal.h"

/* For-in iteration */
__exception int js_for_in_start(JSContext *ctx, JSValue *sp);
__exception int js_for_in_next(JSContext *ctx, JSValue *sp);

/* For-of iteration */
__exception int js_for_of_start(JSContext *ctx, JSValue *sp, BOOL is_async);
__exception int js_for_of_next(JSContext *ctx, JSValue *sp, int offset);
__exception int js_for_await_of_next(JSContext *ctx, JSValue *sp);
__exception int js_iterator_get_value_done(JSContext *ctx, JSValue *sp);

/* Enumeration helpers */
__exception int js_append_enumerate(JSContext *ctx, JSValue *sp);

#endif /* QUICKJS_JS_ITERATOR_H */
