/*
 * QuickJS value conversion
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
#include "internal/function.h"
#include "internal/atom.h"
#include "internal/value-conversion.h"
#include "dtoa.h"

JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint)
{
    int i;
    BOOL force_ordinary;

    JSAtom method_name;
    JSValue method, ret;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return val;
    force_ordinary = hint & HINT_FORCE_ORDINARY;
    hint &= ~HINT_FORCE_ORDINARY;
    if (!force_ordinary) {
        method = JS_GetProperty(ctx, val, JS_ATOM_Symbol_toPrimitive);
        if (JS_IsException(method))
            goto exception;
        /* ECMA says *If exoticToPrim is not undefined* but tests in
           test262 use null as a non callable converter */
        if (!JS_IsUndefined(method) && !JS_IsNull(method)) {
            JSAtom atom;
            JSValue arg;
            switch(hint) {
            case HINT_STRING:
                atom = JS_ATOM_string;
                break;
            case HINT_NUMBER:
                atom = JS_ATOM_number;
                break;
            default:
            case HINT_NONE:
                atom = JS_ATOM_default;
                break;
            }
            arg = JS_AtomToString(ctx, atom);
            ret = JS_CallFree(ctx, method, val, 1, (JSValueConst *)&arg);
            JS_FreeValue(ctx, arg);
            if (JS_IsException(ret))
                goto exception;
            JS_FreeValue(ctx, val);
            if (JS_VALUE_GET_TAG(ret) != JS_TAG_OBJECT)
                return ret;
            JS_FreeValue(ctx, ret);
            return JS_ThrowTypeError(ctx, "toPrimitive");
        }
    }
    if (hint != HINT_STRING)
        hint = HINT_NUMBER;
    for(i = 0; i < 2; i++) {
        if ((i ^ hint) == 0) {
            method_name = JS_ATOM_toString;
        } else {
            method_name = JS_ATOM_valueOf;
        }
        method = JS_GetProperty(ctx, val, method_name);
        if (JS_IsException(method))
            goto exception;
        if (JS_IsFunction(ctx, method)) {
            ret = JS_CallFree(ctx, method, val, 0, NULL);
            if (JS_IsException(ret))
                goto exception;
            if (JS_VALUE_GET_TAG(ret) != JS_TAG_OBJECT) {
                JS_FreeValue(ctx, val);
                return ret;
            }
            JS_FreeValue(ctx, ret);
        } else {
            JS_FreeValue(ctx, method);
        }
    }
    JS_ThrowTypeError(ctx, "toPrimitive");
exception:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint)
{
    return JS_ToPrimitiveFree(ctx, JS_DupValue(ctx, val), hint);
}

int JS_ToBoolFree(JSContext *ctx, JSValue val)
{
    uint32_t tag = JS_VALUE_GET_TAG(val);
    switch(tag) {
    case JS_TAG_INT:
        return JS_VALUE_GET_INT(val) != 0;
    case JS_TAG_BOOL:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        return JS_VALUE_GET_INT(val);
    case JS_TAG_EXCEPTION:
        return -1;
    case JS_TAG_STRING:
        {
            BOOL ret = JS_VALUE_GET_STRING(val)->len != 0;
            JS_FreeValue(ctx, val);
            return ret;
        }
    case JS_TAG_STRING_ROPE:
        {
            BOOL ret = JS_VALUE_GET_STRING_ROPE(val)->len != 0;
            JS_FreeValue(ctx, val);
            return ret;
        }
    case JS_TAG_SHORT_BIG_INT:
        return JS_VALUE_GET_SHORT_BIG_INT(val) != 0;
    case JS_TAG_BIG_INT:
        {
            JSBigInt *p = JS_VALUE_GET_PTR(val);
            BOOL ret;
            int i;

            /* fail safe: we assume it is not necessarily
               normalized. Beginning from the MSB ensures that the
               test is fast. */
            ret = FALSE;
            for(i = p->len - 1; i >= 0; i--) {
                if (p->tab[i] != 0) {
                    ret = TRUE;
                    break;
                }
            }
            JS_FreeValue(ctx, val);
            return ret;
        }
    case JS_TAG_OBJECT:
        {
            JSObject *p = JS_VALUE_GET_OBJ(val);
            BOOL ret;
            ret = !p->is_HTMLDDA;
            JS_FreeValue(ctx, val);
            return ret;
        }
        break;
    default:
        if (JS_TAG_IS_FLOAT64(tag)) {
            double d = JS_VALUE_GET_FLOAT64(val);
            return !isnan(d) && d != 0;
        } else {
            JS_FreeValue(ctx, val);
            return TRUE;
        }
    }
}

int JS_ToBool(JSContext *ctx, JSValueConst val)
{
    return JS_ToBoolFree(ctx, JS_DupValue(ctx, val));
}

static JSValue JS_ToStringInternal(JSContext *ctx, JSValueConst val, BOOL is_ToPropertyKey)
{
    uint32_t tag;
    char buf[32];

    tag = JS_VALUE_GET_NORM_TAG(val);
    switch(tag) {
    case JS_TAG_STRING:
        return JS_DupValue(ctx, val);
    case JS_TAG_STRING_ROPE:
        return js_linearize_string_rope(ctx, JS_DupValue(ctx, val));
    case JS_TAG_INT:
        {
            size_t len;
            len = i32toa(buf, JS_VALUE_GET_INT(val));
            return js_new_string8_len(ctx, buf, len);
        }
        break;
    case JS_TAG_BOOL:
        return JS_AtomToString(ctx, JS_VALUE_GET_BOOL(val) ?
                          JS_ATOM_true : JS_ATOM_false);
    case JS_TAG_NULL:
        return JS_AtomToString(ctx, JS_ATOM_null);
    case JS_TAG_UNDEFINED:
        return JS_AtomToString(ctx, JS_ATOM_undefined);
    case JS_TAG_EXCEPTION:
        return JS_EXCEPTION;
    case JS_TAG_OBJECT:
        {
            JSValue val1, ret;
            val1 = JS_ToPrimitive(ctx, val, HINT_STRING);
            if (JS_IsException(val1))
                return val1;
            ret = JS_ToStringInternal(ctx, val1, is_ToPropertyKey);
            JS_FreeValue(ctx, val1);
            return ret;
        }
        break;
    case JS_TAG_FUNCTION_BYTECODE:
        return js_new_string8(ctx, "[function bytecode]");
    case JS_TAG_SYMBOL:
        if (is_ToPropertyKey) {
            return JS_DupValue(ctx, val);
        } else {
            return JS_ThrowTypeError(ctx, "cannot convert symbol to string");
        }
    case JS_TAG_FLOAT64:
        return js_dtoa2(ctx, JS_VALUE_GET_FLOAT64(val), 10, 0,
                        JS_DTOA_FORMAT_FREE);
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        return js_bigint_to_string(ctx, val);
    default:
        return js_new_string8(ctx, "[unsupported type]");
    }
}

JSValue JS_ToString(JSContext *ctx, JSValueConst val)
{
    return JS_ToStringInternal(ctx, val, FALSE);
}

JSValue JS_ToStringFree(JSContext *ctx, JSValue val)
{
    JSValue ret;
    ret = JS_ToString(ctx, val);
    JS_FreeValue(ctx, val);
    return ret;
}

JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val)
{
    if (JS_IsUndefined(val) || JS_IsNull(val))
        return JS_ToStringFree(ctx, val);
    return JS_InvokeFree(ctx, val, JS_ATOM_toLocaleString, 0, NULL);
}

JSValue JS_ToPropertyKey(JSContext *ctx, JSValueConst val)
{
    return JS_ToStringInternal(ctx, val, TRUE);
}

JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val)
{
    uint32_t tag = JS_VALUE_GET_TAG(val);
    if (tag == JS_TAG_NULL || tag == JS_TAG_UNDEFINED)
        return JS_ThrowTypeError(ctx, "null or undefined are forbidden");
    return JS_ToString(ctx, val);
}
