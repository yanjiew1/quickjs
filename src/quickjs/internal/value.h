/*
 * QuickJS internal value interfaces
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
#ifndef QUICKJS_PRIVATE_VALUE_H
#define QUICKJS_PRIVATE_VALUE_H

/* Internal implementation details; not part of the public QuickJS API. */
JSValue JS_ToObject(JSContext *ctx, JSValueConst val);
int JS_SetObjectData(JSContext *ctx, JSValueConst obj, JSValue val);

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

static inline void set_value(JSContext *ctx, JSValue *pval, JSValue new_val)
{
    JSValue old_val;
    old_val = *pval;
    *pval = new_val;
    JS_FreeValue(ctx, old_val);
}

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_atof(JSContext *ctx, const char *str, const char **pp,
                       int radix, int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_ToBoolFree(JSContext *ctx, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
__exception int JS_ToLengthFree(JSContext *ctx, int64_t *plen,
                                       JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ToStringFree(JSContext *ctx, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2);

#define HINT_STRING  0
#define HINT_NUMBER  1
#define HINT_NONE    2
#define HINT_FORCE_ORDINARY (1 << 4) // don't try Symbol.toPrimitive

typedef enum JSStrictEqModeEnum {
    JS_EQ_STRICT,
    JS_EQ_SAME_VALUE,
    JS_EQ_SAME_VALUE_ZERO,
} JSStrictEqModeEnum;

#define MAX_SAFE_INTEGER (((int64_t)1 << 53) - 1)

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);

/* Internal implementation detail; not part of the public QuickJS API. */
__exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen,
                                            JSValue val, BOOL is_array_ctor);

/* Internal implementation detail; not part of the public QuickJS API. */
__exception int __JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val);

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

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val,
                    int min, int max, int min_offset);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_ToInt64Clamp(JSContext *ctx, int64_t *pres, JSValueConst val,
                    int64_t min, int64_t max, int64_t neg_offset);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_ToInt64Free(JSContext *ctx, int64_t *pres, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val);

/* Internal implementation detail; not part of the public QuickJS API. */
__maybe_unused JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ToNumber(JSContext *ctx, JSValueConst val);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val);

static inline int JS_ToUint32Free(JSContext *ctx, uint32_t *pres, JSValue val)
{
    return JS_ToInt32Free(ctx, (int32_t *)pres, val);
}

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
BOOL is_safe_integer(double d);

static inline BOOL is_strict_mode(JSContext *ctx)
{
    JSStackFrame *sf = ctx->rt->current_stack_frame;
    return (sf && (sf->js_mode & JS_MODE_STRICT));
}

/* Internal implementation detail; not part of the public QuickJS API. */
BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2);

/* Internal implementation detail; not part of the public QuickJS API. */
BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2,
                          JSStrictEqModeEnum eq_mode);

/* Internal implementation detail; not part of the public QuickJS API. */
double js_pow(double a, double b);

#endif /* QUICKJS_PRIVATE_VALUE_H */
