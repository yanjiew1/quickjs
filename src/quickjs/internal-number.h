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

QJS_INTERNAL int to_digit(int c);
QJS_INTERNAL JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint);
QJS_INTERNAL JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);
QJS_INTERNAL JSValue js_atof(JSContext *ctx, const char *str, const char **pp,
                       int radix, int flags);
QJS_INTERNAL JSBigInt *js_bigint_new(JSContext *ctx, int len);
QJS_INTERNAL JSBigInt *js_bigint_set_short(JSBigIntBuf *buf, JSValueConst val);
QJS_INTERNAL int js_bigint_sign(const JSBigInt *a);
QJS_INTERNAL JSValue JS_CompactBigInt(JSContext *ctx, JSBigInt *p);
QJS_INTERNAL JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *a);
QJS_INTERNAL double js_bigint_to_float64(JSContext *ctx, const JSBigInt *a);
QJS_INTERNAL JSBigInt *js_bigint_from_float64(JSContext *ctx, int *pres, double a1);
QJS_INTERNAL JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix);
QJS_INTERNAL JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ToNumber(JSContext *ctx, JSValueConst val);
QJS_INTERNAL int JS_ToBoolFree(JSContext *ctx, JSValue val);
QJS_INTERNAL int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val);
QJS_INTERNAL __exception int __JS_ToFloat64Free(JSContext *ctx, double *pres,
                                          JSValue val);
QJS_INTERNAL int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val);
QJS_INTERNAL __maybe_unused JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val);
QJS_INTERNAL BOOL is_safe_integer(double d);
QJS_INTERNAL int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);
QJS_INTERNAL BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue js_dtoa2(JSContext *ctx,
                        double d, int radix, int n_digits, int flags);
QJS_INTERNAL JSValue JS_ToStringInternal(JSContext *ctx, JSValueConst val, BOOL is_ToPropertyKey);
QJS_INTERNAL JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ToBigInt(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val);
QJS_INTERNAL int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val);
int JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val,
                    int min, int max, int min_offset);
int JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val);
int JS_ToInt64Clamp(JSContext *ctx, int64_t *pres, JSValueConst val,
                    int64_t min, int64_t max, int64_t neg_offset);
QJS_INTERNAL int JS_ToInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
QJS_INTERNAL int JS_ToUint32Free(JSContext *ctx, uint32_t *pres, JSValue val);
QJS_INTERNAL int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);
QJS_INTERNAL __exception int JS_ToLengthFree(JSContext *ctx, int64_t *plen,
                                       JSValue val);
QJS_INTERNAL __exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen,
                                            JSValue val, BOOL is_array_ctor);

#if JS_SHORT_BIG_INT_BITS == 32
#define JS_SHORT_BIG_INT_MIN INT32_MIN
#define JS_SHORT_BIG_INT_MAX INT32_MAX
#elif JS_SHORT_BIG_INT_BITS == 64
#define JS_SHORT_BIG_INT_MIN INT64_MIN
#define JS_SHORT_BIG_INT_MAX INT64_MAX
#else
#error unsupported
#endif


/* Cross-TU declarations owned by this subsystem. */
QJS_INTERNAL BOOL js_string_eq(JSContext *ctx,
                         const JSString *p1, const JSString *p2);

#endif /* QUICKJS_INTERNAL_NUMBER_H */
