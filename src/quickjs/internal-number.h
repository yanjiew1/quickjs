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

QJS_INTERNAL int qjs_to_digit(int c);
QJS_INTERNAL JSValue qjs_to_primitive_free(JSContext *ctx, JSValue value,
                                           int hint);
QJS_INTERNAL JSValue qjs_to_primitive(JSContext *ctx, JSValueConst value,
                                      int hint);
QJS_INTERNAL JSValue qjs_atof(JSContext *ctx, const char *str,
                              const char **end, int radix, int flags);
QJS_INTERNAL JSBigInt *qjs_bigint_new(JSContext *ctx, int len);
QJS_INTERNAL JSBigInt *qjs_bigint_set_short(JSBigIntBuf *buf,
                                            JSValueConst value);
static inline int qjs_bigint_sign(const JSBigInt *value)
{
    return (value->tab[value->len - 1] >> (JS_LIMB_BITS - 1)) != 0;
}
QJS_INTERNAL JSValue qjs_compact_bigint(JSContext *ctx, JSBigInt *value);
QJS_INTERNAL JSBigInt *qjs_bigint_normalize(JSContext *ctx, JSBigInt *value);
QJS_INTERNAL double qjs_bigint_to_float64(JSContext *ctx,
                                          const JSBigInt *value);
QJS_INTERNAL JSBigInt *qjs_bigint_from_float64(JSContext *ctx, int *status,
                                               double value);
QJS_INTERNAL JSValue qjs_bigint_to_string(JSContext *ctx,
                                          JSValueConst value, int radix);
QJS_INTERNAL JSValue qjs_to_numeric(JSContext *ctx, JSValueConst value);
QJS_INTERNAL JSValue qjs_to_number_free(JSContext *ctx, JSValue value);
QJS_INTERNAL JSValue qjs_to_number(JSContext *ctx, JSValueConst value);
QJS_INTERNAL int qjs_to_bool_free(JSContext *ctx, JSValue value);
QJS_INTERNAL int qjs_to_int32_free(JSContext *ctx, int32_t *result,
                                   JSValue value);
QJS_INTERNAL int qjs_to_float64_free_slow(JSContext *ctx, double *result,
                                          JSValue value);
static inline int qjs_to_float64_free(JSContext *ctx, double *result,
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
        return qjs_to_float64_free_slow(ctx, result, value);
    }
}
QJS_INTERNAL JSValue qjs_to_integer_free(JSContext *ctx, JSValue value);
QJS_INTERNAL BOOL qjs_is_safe_integer(double value);
QJS_INTERNAL int qjs_number_is_integer(JSContext *ctx, JSValueConst value);
QJS_INTERNAL BOOL qjs_number_is_negative_or_minus_zero(
    JSContext *ctx, JSValueConst value);
QJS_INTERNAL JSValue qjs_dtoa2(JSContext *ctx, double value, int radix,
                               int precision, int flags);
QJS_INTERNAL JSValue qjs_to_string_internal(JSContext *ctx,
                                            JSValueConst value,
                                            BOOL is_property_key);
QJS_INTERNAL JSValue qjs_string_to_bigint_error(JSContext *ctx,
                                                JSValue value);
QJS_INTERNAL JSValue qjs_to_bigint(JSContext *ctx, JSValueConst value);
QJS_INTERNAL JSValue qjs_to_bigint_free(JSContext *ctx, JSValue value);
QJS_INTERNAL int qjs_to_bigint64_free(JSContext *ctx, int64_t *result,
                                      JSValue value);
QJS_INTERNAL int qjs_to_int32_sat(JSContext *ctx, int *result,
                                  JSValueConst value);
QJS_INTERNAL int qjs_to_int32_clamp(JSContext *ctx, int *result,
                                    JSValueConst value, int min, int max,
                                    int min_offset);
QJS_INTERNAL int qjs_to_int64_sat(JSContext *ctx, int64_t *result,
                                  JSValueConst value);
QJS_INTERNAL int qjs_to_int64_clamp(JSContext *ctx, int64_t *result,
                                    JSValueConst value, int64_t min,
                                    int64_t max, int64_t min_offset);
QJS_INTERNAL int qjs_to_int64_free(JSContext *ctx, int64_t *result,
                                   JSValue value);
static inline int qjs_to_uint32_free(JSContext *ctx, uint32_t *result,
                                     JSValue value)
{
    return qjs_to_int32_free(ctx, (int32_t *)result, value);
}
QJS_INTERNAL int qjs_to_uint8_clamp_free(JSContext *ctx, int32_t *result,
                                         JSValue value);
QJS_INTERNAL int qjs_to_length_free(JSContext *ctx, int64_t *length,
                                    JSValue value);
QJS_INTERNAL int qjs_to_array_length_free(JSContext *ctx, uint32_t *length,
                                          JSValue value,
                                          BOOL is_array_constructor);

#endif /* QUICKJS_INTERNAL_NUMBER_H */
