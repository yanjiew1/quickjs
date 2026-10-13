/*
 * QuickJS number types
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
#ifndef QUICKJS_NUMBER_H
#define QUICKJS_NUMBER_H

#include "base.h"

typedef union JSFloat64Union {
    double d;
    uint64_t u64;
    uint32_t u32[2];
} JSFloat64Union;

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

#define JS_TO_INT64_SAT_INF 1 /* result was +/-Infinity */
#define JS_TO_INT64_SAT_NAN 2 /* result was NaN */

int JS_ToInt64SatF(JSContext *ctx, int64_t *pres, JSValueConst val);

#define MAX_SAFE_INTEGER (((int64_t)1 << 53) - 1)

__exception int __JS_ToLengthFree(JSContext *ctx, int64_t *plen,
                                  JSValue val);

static inline __exception int JS_ToLengthFree(JSContext *ctx, int64_t *plen, JSValue val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_INT) {
        int32_t len = JS_VALUE_GET_INT(val);
        *plen = len < 0 ? 0 : len;
        return 0;
    }
    return __JS_ToLengthFree(ctx, plen, val);
}

/* accept Oo and Ob prefixes in addition to 0x prefix if radix = 0 */
#define ATOD_ACCEPT_BIN_OCT  (1 << 2)
/* accept O prefix as octal if radix == 0 and properly formed (Annex B) */
#define ATOD_ACCEPT_LEGACY_OCTAL  (1 << 4)
/* accept _ between digits as a digit separator */
#define ATOD_ACCEPT_UNDERSCORES  (1 << 5)
/* allow a suffix to override the type */
#define ATOD_ACCEPT_SUFFIX    (1 << 6)
/* accept -0x1 */
#define ATOD_ACCEPT_PREFIX_AFTER_SIGN (1 << 10)

static inline int is_digit(int c) {
    return c >= '0' && c <= '9';
}

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

JSValue js_atof(JSContext *ctx, const char *str, const char **pp,
                int radix, int flags);

JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);

/* return (<0, 0) in case of exception */
static inline int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val)
{
    uint32_t tag;
    int32_t ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        ret = JS_VALUE_GET_INT(val);
        break;
    case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            double d;
            int e;
            d = JS_VALUE_GET_FLOAT64(val);
            u.d = d;
            /* we avoid doing fmod(x, 2^32) */
            e = (u.u64 >> 52) & 0x7ff;
            if (likely(e <= (1023 + 30))) {
                /* fast case */
                ret = (int32_t)d;
            } else if (e <= (1023 + 30 + 53)) {
                uint64_t v;
                /* remainder modulo 2^32 */
                v = (u.u64 & (((uint64_t)1 << 52) - 1)) | ((uint64_t)1 << 52);
                v = v << ((e - 1023) - 52 + 32);
                ret = v >> 32;
                /* take the sign into account */
                if (u.u64 >> 63)
                    ret = -ret;
            } else {
                ret = 0; /* also handles NaN and +inf */
            }
        }
        break;
    default:
        val = JS_ToNumberFree(ctx, val);
        if (JS_IsException(val)) {
            *pres = 0;
            return -1;
        }
        goto redo;
    }
    *pres = ret;
    return 0;
}

int JS_ToInt64Free(JSContext *ctx, int64_t *pres, JSValue val);

int JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val,
                    int min, int max, int max_default);
int JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val);
int JS_ToInt64Clamp(JSContext *ctx, int64_t *pres, JSValueConst val,
                    int64_t min, int64_t max, int64_t max_default);

int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);

__maybe_unused JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val);

int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val);

__exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen,
                                     JSValue val, BOOL is_array_ctor);

JSValue JS_ToNumber(JSContext *ctx, JSValueConst val);

JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val);
BOOL is_safe_integer(double d);
int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);
JSValue js_dtoa2(JSContext *ctx,
                 double d, int radix, int n_digits, int flags);
int skip_spaces(const char *pc);
int js_get_radix(JSContext *ctx, JSValueConst val);

#define ATOD_INT_ONLY        (1 << 0)

double js_pow(double a, double b);

/* default type */
#define ATOD_TYPE_MASK        (3 << 7)
#define ATOD_TYPE_FLOAT64     (0 << 7)
#define ATOD_TYPE_BIG_INT     (1 << 7)

static inline int JS_ToUint32Free(JSContext *ctx, uint32_t *pres, JSValue val)
{
    return JS_ToInt32Free(ctx, (int32_t *)pres, val);
}

BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val);
JSValue JS_ToNumericFree(JSContext *ctx, JSValue val);

#endif
