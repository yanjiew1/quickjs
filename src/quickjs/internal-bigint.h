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
#ifndef QUICKJS_INTERNAL_BIGINT_H
#define QUICKJS_INTERNAL_BIGINT_H

#include "internal-types.h"

/* Cross-TU declarations owned by this subsystem. */
QJS_INTERNAL JSValue JS_StringToBigInt(JSContext *ctx, JSValue val);

QJS_INTERNAL JSBigInt *js_bigint_add(JSContext *ctx, const JSBigInt *a,
                               const JSBigInt *b, int b_neg);

QJS_INTERNAL int js_bigint_cmp(JSContext *ctx, const JSBigInt *a,
                         const JSBigInt *b);

QJS_INTERNAL JSBigInt *js_bigint_divrem(JSContext *ctx, const JSBigInt *a,
                                  const JSBigInt *b, BOOL is_rem);

QJS_INTERNAL JSBigInt *js_bigint_from_string(JSContext *ctx,
                                       const char *str, int radix);

QJS_INTERNAL js_slimb_t js_bigint_get_si_sat(const JSBigInt *a);

QJS_INTERNAL JSBigInt *js_bigint_logic(JSContext *ctx, const JSBigInt *a,
                                 const JSBigInt *b, OPCodeEnum op);

QJS_INTERNAL JSBigInt *js_bigint_mul(JSContext *ctx, const JSBigInt *a,
                               const JSBigInt *b);

QJS_INTERNAL JSBigInt *js_bigint_neg(JSContext *ctx, const JSBigInt *a);

QJS_INTERNAL JSBigInt *js_bigint_new_di(JSContext *ctx, js_sdlimb_t a);

QJS_INTERNAL JSBigInt *js_bigint_not(JSContext *ctx, const JSBigInt *a);

QJS_INTERNAL JSBigInt *js_bigint_pow(JSContext *ctx, const JSBigInt *a, JSBigInt *b);

QJS_INTERNAL JSBigInt *js_bigint_set_si(JSBigIntBuf *buf, js_slimb_t a);

QJS_INTERNAL JSBigInt *js_bigint_shl(JSContext *ctx, const JSBigInt *a,
                               unsigned int shift1);

QJS_INTERNAL JSBigInt *js_bigint_shr(JSContext *ctx, const JSBigInt *a,
                               unsigned int shift1);

QJS_INTERNAL JSValue js_bigint_to_string(JSContext *ctx, JSValueConst val);

QJS_INTERNAL int js_compare_bigint(JSContext *ctx, OPCodeEnum op,
                             JSValue op1, JSValue op2);

#endif
