/*
 * QuickJS Array Internal Interface
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
#ifndef QJS_ARRAY_H
#define QJS_ARRAY_H

#include "object.h"

#define special_every    0
#define special_some     1
#define special_forEach  2
#define special_map      3
#define special_filter   4
#define special_TA       8

enum {
    ArrayFind,
    ArrayFindIndex,
    ArrayFindLast,
    ArrayFindLastIndex,
};

#define special_reduce       0
#define special_reduceRight  1

QJS_INTERNAL int expand_fast_array(JSContext *ctx, JSObject *p, uint32_t new_len);
QJS_INTERNAL JSValue js_create_array(JSContext *ctx, int len,
                                      JSValueConst *tab);

QJS_INTERNAL JSValue js_array_every(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int special);
QJS_INTERNAL JSValue js_array_reduce(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int special);

/* return true if an element can be added to a fast array without further tests */
static force_inline BOOL can_extend_fast_array(JSObject *p)
{
    JSObject *proto;
    if (!p->extensible)
        return FALSE;
    proto = p->shape->proto;
    if (!proto)
        return TRUE;
    return proto->is_std_array_prototype;
}

QJS_INTERNAL JSValue js_create_array_free(JSContext *ctx, int len, JSValue *tab);

#endif /* QJS_ARRAY_H */
