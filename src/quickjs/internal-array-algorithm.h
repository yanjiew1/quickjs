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
#ifndef QUICKJS_INTERNAL_ARRAY_ALGORITHM_H
#define QUICKJS_INTERNAL_ARRAY_ALGORITHM_H

#include "internal-iterator.h"

#define QJS_MAX_SAFE_INTEGER (((int64_t)1 << 53) - 1)

enum {
    QJS_ARRAY_EVERY,
    QJS_ARRAY_SOME,
    QJS_ARRAY_FOR_EACH,
    QJS_ARRAY_MAP,
    QJS_ARRAY_FILTER,
    QJS_ARRAY_TYPED = 8,
};

enum {
    QJS_ARRAY_REDUCE,
    QJS_ARRAY_REDUCE_RIGHT,
};

typedef enum QJSArrayFindMode {
    QJS_ARRAY_FIND,
    QJS_ARRAY_FIND_INDEX,
    QJS_ARRAY_FIND_LAST,
    QJS_ARRAY_FIND_LAST_INDEX,
} QJSArrayFindMode;

#define QJS_ITERATOR_NEXT   0
#define QJS_ITERATOR_RETURN 1

QJS_INTERNAL JSValue qjs_array_every(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue qjs_array_reduce(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue qjs_create_array_iterator(JSContext *ctx,
                                               JSValueConst this_val,
                                               int argc, JSValueConst *argv,
                                               int magic);
QJS_INTERNAL JSValue qjs_array_get_this(JSContext *ctx,
                                        JSValueConst this_val);

#endif /* QUICKJS_INTERNAL_ARRAY_ALGORITHM_H */
