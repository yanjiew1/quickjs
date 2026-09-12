#ifndef QUICKJS_BIGINT_H
#define QUICKJS_BIGINT_H

#include "quickjs/def.h"
#include "quickjs/value.h"

JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val);
JSValue JS_ToBigInt(JSContext *ctx, JSValueConst val);
JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val);
JSBigInt *js_bigint_from_float64(JSContext *ctx, int *pres, double a1);
JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *a);
int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
static force_inline int js_bigint_sign(const JSBigInt *a)
{
    return a->tab[a->len - 1] >> (JS_LIMB_BITS - 1);
}
double js_bigint_to_float64(JSContext *ctx, const JSBigInt *a);
JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix);

#endif /* QUICKJS_BIGINT_H */
