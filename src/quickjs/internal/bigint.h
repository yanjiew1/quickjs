/*
 * QuickJS BigInt Internal Interface
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
#ifndef QJS_BIGINT_H
#define QJS_BIGINT_H

#include "base.h"

typedef enum OPCodeEnum OPCodeEnum;

/* bigint */

#if JS_LIMB_BITS == 32

typedef int32_t js_slimb_t;
typedef uint32_t js_limb_t;
typedef int64_t js_sdlimb_t;
typedef uint64_t js_dlimb_t;

#define JS_LIMB_DIGITS 9

#else

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;
typedef int64_t js_slimb_t;
typedef uint64_t js_limb_t;
typedef int128_t js_sdlimb_t;
typedef uint128_t js_dlimb_t;

#define JS_LIMB_DIGITS 19

#endif

typedef struct JSBigInt {
    uint32_t len; /* number of limbs, >= 1 */
    js_limb_t tab[]; /* two's complement representation, always
                        normalized so that 'len' is the minimum
                        possible length >= 1 */
} JSBigInt;

/* this bigint structure can hold a 64 bit integer */
typedef struct {
    js_limb_t big_int_buf[sizeof(JSBigInt) / sizeof(js_limb_t)]; /* for JSBigInt */
    /* must come just after */
    js_limb_t tab[(64 + JS_LIMB_BITS - 1) / JS_LIMB_BITS];
} JSBigIntBuf;
    

QJS_INTERNAL JSBigInt *js_bigint_new(JSContext *ctx, int len);
QJS_INTERNAL JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *a);
QJS_INTERNAL JSBigInt *js_bigint_from_float64(JSContext *ctx, int *pres, double a1);
QJS_INTERNAL JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix);
QJS_INTERNAL JSValue JS_CompactBigInt(JSContext *ctx, JSBigInt *p);
QJS_INTERNAL JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ToBigInt(JSContext *ctx, JSValueConst val);
QJS_INTERNAL double js_bigint_to_float64(JSContext *ctx, const JSBigInt *a);

QJS_INTERNAL JSBigInt *js_bigint_set_short(JSBigIntBuf *buf, JSValueConst val);

static inline int js_bigint_sign(const JSBigInt *a)
{
    return a->tab[a->len - 1] >> (JS_LIMB_BITS - 1);
}

QJS_INTERNAL JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val);

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

QJS_INTERNAL JSBigInt *js_bigint_from_string(JSContext *ctx, const char *str, int radix);

QJS_INTERNAL JSBigInt *js_bigint_set_si(JSBigIntBuf *buf, js_slimb_t a);
QJS_INTERNAL JSBigInt *js_bigint_new_si64(JSContext *ctx, int64_t a);
QJS_INTERNAL JSBigInt *js_bigint_new_ui64(JSContext *ctx, uint64_t a);
QJS_INTERNAL JSBigInt *js_bigint_new_di(JSContext *ctx, js_sdlimb_t a);
QJS_INTERNAL JSBigInt *js_bigint_add(JSContext *ctx, const JSBigInt *a,
                               const JSBigInt *b, int b_neg);
QJS_INTERNAL JSBigInt *js_bigint_neg(JSContext *ctx, const JSBigInt *a);
QJS_INTERNAL JSBigInt *js_bigint_not(JSContext *ctx, const JSBigInt *a);
QJS_INTERNAL JSBigInt *js_bigint_mul(JSContext *ctx, const JSBigInt *a,
                               const JSBigInt *b);
QJS_INTERNAL JSBigInt *js_bigint_divrem(JSContext *ctx, const JSBigInt *a,
                                  const JSBigInt *b, BOOL is_rem);
QJS_INTERNAL JSBigInt *js_bigint_logic(JSContext *ctx, const JSBigInt *a,
                                 const JSBigInt *b, OPCodeEnum op);
QJS_INTERNAL js_slimb_t js_bigint_get_si_sat(const JSBigInt *a);
QJS_INTERNAL JSBigInt *js_bigint_shl(JSContext *ctx, const JSBigInt *a,
                               unsigned int shift1);
QJS_INTERNAL JSBigInt *js_bigint_shr(JSContext *ctx, const JSBigInt *a,
                               unsigned int shift1);
QJS_INTERNAL JSBigInt *js_bigint_pow(JSContext *ctx, const JSBigInt *a, JSBigInt *b);
QJS_INTERNAL int js_bigint_float64_cmp(JSContext *ctx, const JSBigInt *a,
                                 double b);
QJS_INTERNAL int js_bigint_cmp(JSContext *ctx, const JSBigInt *a,
                         const JSBigInt *b);

QJS_INTERNAL int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val);

#endif /* QJS_BIGINT_H */
