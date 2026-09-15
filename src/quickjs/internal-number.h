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
#ifndef QUICKJS_INTERNAL_NUMBER_H
#define QUICKJS_INTERNAL_NUMBER_H

#include "internal-string.h"

#define ATOD_ACCEPT_BIN_OCT       (1 << 2)
#define ATOD_ACCEPT_LEGACY_OCTAL  (1 << 4)
#define ATOD_ACCEPT_UNDERSCORES   (1 << 5)
#define ATOD_ACCEPT_SUFFIX        (1 << 6)

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
QJS_INTERNAL JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue value,
                                           int hint);
QJS_INTERNAL JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst value,
                                      int hint);
QJS_INTERNAL JSValue js_atof(JSContext *ctx, const char *str,
                              const char **end, int radix, int flags);
QJS_INTERNAL JSBigInt *js_bigint_new(JSContext *ctx, int len);
#ifndef QUICKJS_NUMBER_OWNER
static inline JSBigInt *js_bigint_set_short(JSBigIntBuf *buf,
                                             JSValueConst value)
{
    JSBigInt *result = (JSBigInt *)buf->big_int_buf;

    result->len = 1;
    result->tab[0] = JS_VALUE_GET_SHORT_BIG_INT(value);
    return result;
}
#endif
static inline int js_bigint_sign(const JSBigInt *value)
{
    return (value->tab[value->len - 1] >> (JS_LIMB_BITS - 1)) != 0;
}
QJS_INTERNAL JSValue JS_CompactBigInt(JSContext *ctx, JSBigInt *value);
QJS_INTERNAL JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *value);
QJS_INTERNAL double js_bigint_to_float64(JSContext *ctx,
                                          const JSBigInt *value);
QJS_INTERNAL JSBigInt *js_bigint_from_float64(JSContext *ctx, int *status,
                                               double value);
QJS_INTERNAL JSValue js_bigint_to_string1(JSContext *ctx,
                                          JSValueConst value, int radix);
QJS_INTERNAL JSValue JS_ToNumeric(JSContext *ctx, JSValueConst value);
QJS_INTERNAL JSValue JS_ToNumberFree(JSContext *ctx, JSValue value);
QJS_INTERNAL JSValue JS_ToNumber(JSContext *ctx, JSValueConst value);
QJS_INTERNAL int JS_ToBoolFree(JSContext *ctx, JSValue value);
#ifndef QUICKJS_NUMBER_OWNER
static inline int JS_ToInt32Free(JSContext *ctx, int32_t *result,
                                    JSValue value)
{
    uint32_t tag;
    int32_t ret;

 redo:
    tag = JS_VALUE_GET_NORM_TAG(value);
    switch(tag) {
    case JS_TAG_INT:
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        ret = JS_VALUE_GET_INT(value);
        break;
    case JS_TAG_FLOAT64:
        {
            JSFloat64Union u;
            double d;
            int e;

            d = JS_VALUE_GET_FLOAT64(value);
            u.d = d;
            e = (u.u64 >> 52) & 0x7ff;
            if (likely(e <= (1023 + 30))) {
                ret = (int32_t)d;
            } else if (e <= (1023 + 30 + 53)) {
                uint64_t v;

                v = (u.u64 & (((uint64_t)1 << 52) - 1)) |
                    ((uint64_t)1 << 52);
                v = v << ((e - 1023) - 52 + 32);
                ret = v >> 32;
                if (u.u64 >> 63)
                    ret = -ret;
            } else {
                ret = 0;
            }
        }
        break;
    default:
        value = JS_ToNumberFree(ctx, value);
        if (JS_IsException(value)) {
            *result = 0;
            return -1;
        }
        goto redo;
    }
    *result = ret;
    return 0;
}
#endif
QJS_INTERNAL int __JS_ToFloat64Free(JSContext *ctx, double *result,
                                    JSValue value);
#ifndef QUICKJS_NUMBER_OWNER
static inline int JS_ToFloat64Free(JSContext *ctx, double *result,
                                      JSValue value)
{
    uint32_t tag = JS_VALUE_GET_TAG(value);

    if (tag <= JS_TAG_NULL) {
        *result = JS_VALUE_GET_INT(value);
        return 0;
    } else if (JS_TAG_IS_FLOAT64(tag)) {
        *result = JS_VALUE_GET_FLOAT64(value);
        return 0;
    } else {
        return __JS_ToFloat64Free(ctx, result, value);
    }
}
#endif
QJS_INTERNAL JSValue JS_ToIntegerFree(JSContext *ctx, JSValue value);
QJS_INTERNAL BOOL is_safe_integer(double value);
QJS_INTERNAL int JS_NumberIsInteger(JSContext *ctx, JSValueConst value);
QJS_INTERNAL BOOL JS_NumberIsNegativeOrMinusZero(
    JSContext *ctx, JSValueConst value);
QJS_INTERNAL JSValue js_dtoa2(JSContext *ctx, double value, int radix,
                               int precision, int flags);
QJS_INTERNAL JSValue JS_ToStringInternal(JSContext *ctx,
                                            JSValueConst value,
                                            BOOL is_property_key);
QJS_INTERNAL JSValue JS_StringToBigIntErr(JSContext *ctx,
                                                JSValue value);
QJS_INTERNAL JSValue JS_ToBigInt(JSContext *ctx, JSValueConst value);
QJS_INTERNAL JSValue JS_ToBigIntFree(JSContext *ctx, JSValue value);
QJS_INTERNAL int JS_ToBigInt64Free(JSContext *ctx, int64_t *result,
                                      JSValue value);
int JS_ToInt32Sat(JSContext *ctx, int *result,
                                  JSValueConst value);
int JS_ToInt32Clamp(JSContext *ctx, int *result,
                                    JSValueConst value, int min, int max,
                                    int min_offset);
int JS_ToInt64Sat(JSContext *ctx, int64_t *result,
                                  JSValueConst value);
int JS_ToInt64Clamp(JSContext *ctx, int64_t *result,
                                    JSValueConst value, int64_t min,
                                    int64_t max, int64_t min_offset);
QJS_INTERNAL int JS_ToInt64Free(JSContext *ctx, int64_t *result,
                                   JSValue value);
#ifndef QUICKJS_NUMBER_OWNER
static inline int JS_ToUint32Free(JSContext *ctx, uint32_t *result,
                                     JSValue value)
{
    return JS_ToInt32Free(ctx, (int32_t *)result, value);
}
#endif
QJS_INTERNAL int JS_ToUint8ClampFree(JSContext *ctx, int32_t *result,
                                         JSValue value);
QJS_INTERNAL int JS_ToLengthFree(JSContext *ctx, int64_t *length,
                                    JSValue value);
QJS_INTERNAL int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *length,
                                          JSValue value,
                                          BOOL is_array_constructor);

#endif /* QUICKJS_INTERNAL_NUMBER_H */
