/*
 * QuickJS internal bigint interfaces
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
#ifndef QUICKJS_PRIVATE_BIGINT_H
#define QUICKJS_PRIVATE_BIGINT_H

/* Internal implementation details; not part of the public QuickJS API. */
JSBigInt *js_bigint_new(JSContext *ctx, int len);
JSBigInt *js_bigint_set_short(JSBigIntBuf *buf, JSValueConst val);
JSValue JS_CompactBigInt(JSContext *ctx, JSBigInt *p);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ToBigInt(JSContext *ctx, JSValueConst val);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
JSBigInt *js_bigint_from_float64(JSContext *ctx, int *pres, double a1);

/* Internal implementation detail; not part of the public QuickJS API. */
JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *a);

static inline int js_bigint_sign(const JSBigInt *a)
{
    return a->tab[a->len - 1] >> (JS_LIMB_BITS - 1);
}

/* Internal implementation detail; not part of the public QuickJS API. */
double js_bigint_to_float64(JSContext *ctx, const JSBigInt *a);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix);

#endif /* QUICKJS_PRIVATE_BIGINT_H */
