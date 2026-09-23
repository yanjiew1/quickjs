/*
 * QuickJS Number Internal Interface
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
#ifndef QJS_NUMBER_H
#define QJS_NUMBER_H

#include "base.h"

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

typedef union JSFloat64Union {
    double d;
    uint64_t u64;
    uint32_t u32[2];
} JSFloat64Union;

QJS_INTERNAL double js_pow(double a, double b);
QJS_INTERNAL BOOL is_safe_integer(double d);
QJS_INTERNAL int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val);
QJS_INTERNAL int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val);
QJS_INTERNAL int JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val,
                                 int min, int max, int min_offset);
QJS_INTERNAL int JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val);
QJS_INTERNAL int skip_spaces(const char *pc);
QJS_INTERNAL JSValue js_atof(JSContext *ctx, const char *str,
                              const char **pp, int radix, int flags);
QJS_INTERNAL __exception int JS_ToLengthFree(JSContext *ctx, int64_t *plen,
                                              JSValue val);
QJS_INTERNAL JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue js_dtoa2(JSContext *ctx, double d, int radix,
                              int n_digits, int flags);

QJS_INTERNAL __exception int __JS_ToFloat64Free(JSContext *ctx, double *pres,
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

#endif /* QJS_NUMBER_H */
