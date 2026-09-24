/*
 * QuickJS Number Types
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
#ifndef QUICKJS_INTERNAL_NUMBER_H
#define QUICKJS_INTERNAL_NUMBER_H

#include "base.h"

typedef enum JSStrictEqModeEnum {
    JS_EQ_STRICT,
    JS_EQ_SAME_VALUE,
    JS_EQ_SAME_VALUE_ZERO,
} JSStrictEqModeEnum;

#define HINT_STRING  0
#define HINT_NUMBER  1
#define HINT_NONE    2
#define HINT_FORCE_ORDINARY (1 << 4) // don't try Symbol.toPrimitive
#define MAX_SAFE_INTEGER (((int64_t)1 << 53) - 1)

int JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val,
                    int min, int max, int min_offset);
int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val);
int JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val);
int JS_ToInt64Clamp(JSContext *ctx, int64_t *pres, JSValueConst val,
                    int64_t min, int64_t max, int64_t neg_offset);

double js_pow(double a, double b);

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

typedef union JSFloat64Union {
    double d;
    uint64_t u64;
    uint32_t u32[2];
} JSFloat64Union;

JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);

__exception int __JS_ToFloat64Free(JSContext *ctx, double *pres,
                                          JSValue val);

static inline int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val)
{
    uint32_t tag;

    tag = JS_VALUE_GET_TAG(val);
    if (tag <= JS_TAG_NULL) {
        *pres = JS_VALUE_GET_INT(val);
        return 0;
    } else if (JS_TAG_IS_FLOAT64(tag)) {
        *pres = JS_VALUE_GET_FLOAT64(val);
        return 0;
    } else {
        return __JS_ToFloat64Free(ctx, pres, val);
    }
}

int JS_ToBoolFree(JSContext *ctx, JSValue val);

JSBigInt *js_bigint_set_short(JSBigIntBuf *buf, JSValueConst val);

BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2);

JSValue JS_ToNumber(JSContext *ctx, JSValueConst val);

__maybe_unused JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val);

int JS_ToInt64Free(JSContext *ctx, int64_t *pres, JSValue val);

__exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen,
                                            JSValue val, BOOL is_array_ctor);

JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val);

JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val);

BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2,
                          JSStrictEqModeEnum eq_mode);

BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2);

JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val);

__exception int JS_ToLengthFree(JSContext *ctx, int64_t *plen,
                                       JSValue val);

int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);

/* return 0 or 1 depending on the sign */
static inline int js_bigint_sign(const JSBigInt *a)
{
    return a->tab[a->len - 1] >> (JS_LIMB_BITS - 1);
}

int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val);

JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);

JSValue JS_ToStringFree(JSContext *ctx, JSValue val);

static inline int to_digit(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    else
        return 36;
}

JSValue JS_CompactBigInt(JSContext *ctx, JSBigInt *p);

int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);

JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val);

JSValue JS_ToBigInt(JSContext *ctx, JSValueConst val);

JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val);

BOOL is_safe_integer(double d);

JSValue js_atof(JSContext *ctx, const char *str, const char **pp,
                       int radix, int flags);

JSBigInt *js_bigint_from_float64(JSContext *ctx, int *pres, double a1);

JSBigInt *js_bigint_new(JSContext *ctx, int len);

JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *a);

double js_bigint_to_float64(JSContext *ctx, const JSBigInt *a);

JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix);

JSValue js_dtoa2(JSContext *ctx,
                        double d, int radix, int n_digits, int flags);

int skip_spaces(const char *pc);

#define ATOD_INT_ONLY        (1 << 0)
/* accept Oo and Ob prefixes in addition to 0x prefix if radix = 0 */
#define ATOD_ACCEPT_BIN_OCT  (1 << 2)
/* accept O prefix as octal if radix == 0 and properly formed (Annex B) */
#define ATOD_ACCEPT_LEGACY_OCTAL  (1 << 4)
/* accept _ between digits as a digit separator */
#define ATOD_ACCEPT_UNDERSCORES  (1 << 5)
/* allow a suffix to override the type */
#define ATOD_ACCEPT_SUFFIX    (1 << 6)
/* default type */
#define ATOD_TYPE_MASK        (3 << 7)
#define ATOD_TYPE_FLOAT64     (0 << 7)
#define ATOD_TYPE_BIG_INT     (1 << 7)
/* accept -0x1 */
#define ATOD_ACCEPT_PREFIX_AFTER_SIGN (1 << 10)


static inline int JS_ToUint32Free(JSContext *ctx, uint32_t *pres, JSValue val)
{
    return JS_ToInt32Free(ctx, (int32_t *)pres, val);
}

#endif /* QUICKJS_INTERNAL_NUMBER_H */
