#ifndef QUICKJS_BIGINT_H
#define QUICKJS_BIGINT_H

#include "quickjs/def.h"
#include "quickjs/value.h"
#include "quickjs/opcode.h"

#define JS_BIGINT_MAX_SIZE ((1024 * 1024) / JS_LIMB_BITS) /* in limbs */

#if JS_SHORT_BIG_INT_BITS == 32
#define JS_SHORT_BIG_INT_MIN INT32_MIN
#define JS_SHORT_BIG_INT_MAX INT32_MAX
#elif JS_SHORT_BIG_INT_BITS == 64
#define JS_SHORT_BIG_INT_MIN INT64_MIN
#define JS_SHORT_BIG_INT_MAX INT64_MAX
#else
#error unsupported
#endif

static force_inline int js_bigint_sign(const JSBigInt *a)
{
    return a->tab[a->len - 1] >> (JS_LIMB_BITS - 1);
}

JSBigInt *js_bigint_set_si(JSBigIntBuf *buf, js_slimb_t a);
JSBigInt *js_bigint_set_si64(JSBigIntBuf *buf, int64_t a);
js_slimb_t js_bigint_get_si_sat(const JSBigInt *p);

JSBigInt *js_bigint_new(JSContext *ctx, int len);
JSBigInt *js_bigint_new_si64(JSContext *ctx, int64_t a);
JSBigInt *js_bigint_new_ui64(JSContext *ctx, uint64_t a);
JSBigInt *js_bigint_new_di(JSContext *ctx, js_sdlimb_t a);
JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *a);

JSBigInt *js_bigint_add(JSContext *ctx, const JSBigInt *a, const JSBigInt *b, int is_sub);
JSBigInt *js_bigint_neg(JSContext *ctx, const JSBigInt *a);
JSBigInt *js_bigint_not(JSContext *ctx, const JSBigInt *a);
JSBigInt *js_bigint_mul(JSContext *ctx, const JSBigInt *a, const JSBigInt *b);
JSBigInt *js_bigint_divrem(JSContext *ctx, const JSBigInt *a, const JSBigInt *b, BOOL is_rem);
JSBigInt *js_bigint_pow(JSContext *ctx, const JSBigInt *a, JSBigInt *b);
JSBigInt *js_bigint_logic(JSContext *ctx, const JSBigInt *a, const JSBigInt *b, OPCodeEnum op);
JSBigInt *js_bigint_shl(JSContext *ctx, const JSBigInt *a, unsigned int shift1);
JSBigInt *js_bigint_shr(JSContext *ctx, const JSBigInt *a, unsigned int shift1);
int js_bigint_cmp(JSContext *ctx, const JSBigInt *a, const JSBigInt *b);
JSBigInt *js_bigint_from_float64(JSContext *ctx, int *pres, double a1);
double js_bigint_to_float64(JSContext *ctx, const JSBigInt *a);
int js_bigint_float64_cmp(JSContext *ctx, const JSBigInt *a, double d);
JSBigInt *js_bigint_from_string(JSContext *ctx, const char *buf, int radix);
JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix);
static inline JSValue js_bigint_to_string(JSContext *ctx, JSValueConst val)
{
    return js_bigint_to_string1(ctx, val, 10);
}
JSValue JS_CompactBigInt(JSContext *ctx, JSBigInt *p);

JSValue JS_NewBigInt64(JSContext *ctx, int64_t v);
JSValue JS_NewBigUint64(JSContext *ctx, uint64_t v);
JSValue JS_StringToBigInt(JSContext *ctx, JSValue val);
JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val);
JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val);
JSValue JS_ToBigInt(JSContext *ctx, JSValueConst val);
int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
int JS_ToBigInt64(JSContext *ctx, int64_t *pres, JSValueConst val);

#endif /* QUICKJS_BIGINT_H */
