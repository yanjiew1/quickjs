/*
 * QuickJS Javascript Engine: BigInt Definitions
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

#ifndef QUICKJS_VALUE_JS_BIGINT_H
#define QUICKJS_VALUE_JS_BIGINT_H

#include "quickjs-internal.h"
#include "compiler/js_opcode.h"

/* it is currently assumed that JS_SHORT_BIG_INT_BITS = JS_LIMB_BITS */
#if JS_SHORT_BIG_INT_BITS == 32
#define JS_SHORT_BIG_INT_MIN INT32_MIN
#define JS_SHORT_BIG_INT_MAX INT32_MAX
#elif JS_SHORT_BIG_INT_BITS == 64
#define JS_SHORT_BIG_INT_MIN INT64_MIN
#define JS_SHORT_BIG_INT_MAX INT64_MAX
#else
#error unsupported
#endif

#define JS_BIGINT_MAX_SIZE ((1024 * 1024) / JS_LIMB_BITS) /* in limbs */

JSBigInt *js_bigint_new(JSContext *ctx, int len);
JSBigInt *js_bigint_new_si64(JSContext *ctx, int64_t a);
JSBigInt *js_bigint_new_ui64(JSContext *ctx, uint64_t a);
JSBigInt *js_bigint_new_di(JSContext *ctx, js_sdlimb_t a);
JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *a);
js_slimb_t js_bigint_get_si_sat(const JSBigInt *a);

JSBigInt *js_bigint_add(JSContext *ctx, const JSBigInt *a, const JSBigInt *b, int b_neg);
JSBigInt *js_bigint_neg(JSContext *ctx, const JSBigInt *a);
JSBigInt *js_bigint_mul(JSContext *ctx, const JSBigInt *a, const JSBigInt *b);
JSBigInt *js_bigint_divrem(JSContext *ctx, const JSBigInt *a, const JSBigInt *b, BOOL is_rem);
JSBigInt *js_bigint_logic(JSContext *ctx, const JSBigInt *a, const JSBigInt *b, OPCodeEnum op);
JSBigInt *js_bigint_not(JSContext *ctx, const JSBigInt *a);
JSBigInt *js_bigint_shl(JSContext *ctx, const JSBigInt *a, unsigned int shift);
JSBigInt *js_bigint_shr(JSContext *ctx, const JSBigInt *a, unsigned int shift);
JSBigInt *js_bigint_pow(JSContext *ctx, const JSBigInt *a, JSBigInt *b);
JSBigInt *js_bigint_from_float64(JSContext *ctx, int *pres, double a1);
int js_bigint_float64_cmp(JSContext *ctx, const JSBigInt *a, double b);
int js_bigint_cmp(JSContext *ctx, const JSBigInt *a, const JSBigInt *b);
JSBigInt *js_bigint_from_string(JSContext *ctx, const char *str, int radix);
JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix);

JSValue JS_NewBigInt64(JSContext *ctx, int64_t v);
JSValue JS_NewBigUint64(JSContext *ctx, uint64_t v);
JSValue JS_StringToBigInt(JSContext *ctx, JSValue val);
JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val);
JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val);
JSValue JS_ToBigInt(JSContext *ctx, JSValueConst val);
int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
int JS_ToBigInt64(JSContext *ctx, int64_t *pres, JSValueConst val);

#endif /* QUICKJS_VALUE_JS_BIGINT_H */
