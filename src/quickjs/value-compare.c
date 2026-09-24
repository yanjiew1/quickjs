/*
 * QuickJS value comparison
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
#include "internal/base.h"
#include "internal/runtime.h"
#include "internal/number.h"
#include "internal/bigint.h"
#include "internal/string.h"
#include "internal/object.h"
#include "internal/value-compare.h"

BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2,
                          JSStrictEqModeEnum eq_mode)
{
    BOOL res;
    int tag1, tag2;
    double d1, d2;

    tag1 = JS_VALUE_GET_NORM_TAG(op1);
    tag2 = JS_VALUE_GET_NORM_TAG(op2);
    switch(tag1) {
    case JS_TAG_BOOL:
        if (tag1 != tag2) {
            res = FALSE;
        } else {
            res = JS_VALUE_GET_INT(op1) == JS_VALUE_GET_INT(op2);
        }
        break;
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        res = (tag1 == tag2);
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        {
            if (!tag_is_string(tag2)) {
                res = FALSE;
            } else if (tag1 == JS_TAG_STRING && tag2 == JS_TAG_STRING) {
                res = js_string_eq(ctx, JS_VALUE_GET_STRING(op1),
                                   JS_VALUE_GET_STRING(op2));
            } else {
                res = (js_string_rope_compare(ctx, op1, op2, TRUE) == 0);
            }
        }
        break;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p1, *p2;
            if (tag1 != tag2) {
                res = FALSE;
            } else {
                p1 = JS_VALUE_GET_PTR(op1);
                p2 = JS_VALUE_GET_PTR(op2);
                res = (p1 == p2);
            }
        }
        break;
    case JS_TAG_OBJECT:
        if (tag1 != tag2)
            res = FALSE;
        else
            res = JS_VALUE_GET_OBJ(op1) == JS_VALUE_GET_OBJ(op2);
        break;
    case JS_TAG_INT:
        d1 = JS_VALUE_GET_INT(op1);
        if (tag2 == JS_TAG_INT) {
            d2 = JS_VALUE_GET_INT(op2);
            goto number_test;
        } else if (tag2 == JS_TAG_FLOAT64) {
            d2 = JS_VALUE_GET_FLOAT64(op2);
            goto number_test;
        } else {
            res = FALSE;
        }
        break;
    case JS_TAG_FLOAT64:
        d1 = JS_VALUE_GET_FLOAT64(op1);
        if (tag2 == JS_TAG_FLOAT64) {
            d2 = JS_VALUE_GET_FLOAT64(op2);
        } else if (tag2 == JS_TAG_INT) {
            d2 = JS_VALUE_GET_INT(op2);
        } else {
            res = FALSE;
            break;
        }
    number_test:
        if (unlikely(eq_mode >= JS_EQ_SAME_VALUE)) {
            JSFloat64Union u1, u2;
            /* NaN is not always normalized, so this test is necessary */
            if (isnan(d1) || isnan(d2)) {
                res = isnan(d1) == isnan(d2);
            } else if (eq_mode == JS_EQ_SAME_VALUE_ZERO) {
                res = (d1 == d2); /* +0 == -0 */
            } else {
                u1.d = d1;
                u2.d = d2;
                res = (u1.u64 == u2.u64); /* +0 != -0 */
            }
        } else {
            res = (d1 == d2); /* if NaN return false and +0 == -0 */
        }
        break;
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        {
            JSBigIntBuf buf1, buf2;
            JSBigInt *p1, *p2;

            if (tag2 != JS_TAG_SHORT_BIG_INT &&
                tag2 != JS_TAG_BIG_INT) {
                res = FALSE;
                break;
            }

            if (JS_VALUE_GET_TAG(op1) == JS_TAG_SHORT_BIG_INT)
                p1 = js_bigint_set_short(&buf1, op1);
            else
                p1 = JS_VALUE_GET_PTR(op1);
            if (JS_VALUE_GET_TAG(op2) == JS_TAG_SHORT_BIG_INT)
                p2 = js_bigint_set_short(&buf2, op2);
            else
                p2 = JS_VALUE_GET_PTR(op2);
            res = (js_bigint_cmp(ctx, p1, p2) == 0);
        }
        break;
    default:
        res = FALSE;
        break;
    }
    return res;
}

static BOOL js_strict_eq(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq2(ctx, op1, op2, JS_EQ_STRICT);
}

BOOL JS_StrictEq(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq(ctx, op1, op2);
}

BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq2(ctx, op1, op2, JS_EQ_SAME_VALUE);
}

BOOL JS_SameValue(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_same_value(ctx, op1, op2);
}

BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_strict_eq2(ctx, op1, op2, JS_EQ_SAME_VALUE_ZERO);
}

BOOL JS_SameValueZero(JSContext *ctx, JSValueConst op1, JSValueConst op2)
{
    return js_same_value_zero(ctx, op1, op2);
}
