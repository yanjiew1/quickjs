#ifndef QUICKJS_CONVERSION_H
#define QUICKJS_CONVERSION_H

#include "quickjs/def.h"
#include "quickjs/value.h"

static inline BOOL is_safe_integer(double d)
{
    return isfinite(d) && floor(d) == d &&
        fabs(d) <= (double)MAX_SAFE_INTEGER;
}

int JS_ToBoolFree(JSContext *ctx, JSValue val);
int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val);
int JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val, int min, int max, int val_def);
int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val);
double js_pow(double a, double b);
static force_inline int JS_ToUint32Free(JSContext *ctx, uint32_t *pres, JSValue val)
{
    return JS_ToInt32Free(ctx, (int32_t *)pres, val);
}
int JS_ToInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
int JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val);
int JS_ToInt64Clamp(JSContext *ctx, int64_t *pres, JSValueConst val, int64_t min, int64_t max, int64_t val_def);
int JS_ToInt64Ext(JSContext *ctx, int64_t *pres, JSValueConst val);
int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val);
JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val);
int JS_ToLengthFree(JSContext *ctx, int64_t *plen, JSValue val);
int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen, JSValue val, BOOL is_set_length);
int JS_ToIndex(JSContext *ctx, uint64_t *plen, JSValueConst val);
int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);
void JS_SetIsHTMLDDA(JSContext *ctx, JSValueConst obj);
JSValue JS_ToPropertyKey(JSContext *ctx, JSValueConst val);
JSValue JS_ToNumber(JSContext *ctx, JSValueConst val);
JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);
JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val);
JSValue JS_ToNumericFree(JSContext *ctx, JSValue val);
JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);
JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint);
JSValue JS_ToStringFree(JSContext *ctx, JSValue val);
JSValue JS_ToStringInternal(JSContext *ctx, JSValueConst val, BOOL is_throw);
JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val);
JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val);
int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);
BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val);
JSValue js_dtoa2(JSContext *ctx, double d, int radix, int n_digits, int flags);
int js_get_radix(JSContext *ctx, JSValueConst val);

BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2,
                   JSStrictEqModeEnum eq_mode);
static force_inline BOOL js_strict_eq(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq2(ctx, op1, op2, JS_EQ_STRICT);
}
static force_inline BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq2(ctx, op1, op2, JS_EQ_SAME_VALUE);
}
static force_inline BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq2(ctx, op1, op2, JS_EQ_SAME_VALUE_ZERO);
}

#endif /* QUICKJS_CONVERSION_H */
