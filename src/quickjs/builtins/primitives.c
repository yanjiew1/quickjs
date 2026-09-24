/*
 * QuickJS primitive and Math builtins
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
#include "../internal/base.h"
#include "../internal/runtime.h"
#include "../internal/number.h"
#include "../internal/string.h"
#include "../internal/function.h"
#include "../internal/object.h"
#include "../internal/module.h"
#include "../internal/atom.h"
#include "../internal/bytecode.h"
#include "../internal/frontend.h"
#include "libunicode.h"
#include "dtoa.h"
#include "primitives.h"
#include "array.h"
#include "regexp.h"

/* Number */

JSValue js_number_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValue val, obj;
    if (argc == 0) {
        val = JS_NewInt32(ctx, 0);
    } else {
        val = JS_ToNumeric(ctx, argv[0]);
        if (JS_IsException(val))
            return val;
        switch(JS_VALUE_GET_TAG(val)) {
        case JS_TAG_SHORT_BIG_INT:
            val = JS_NewInt64(ctx, JS_VALUE_GET_SHORT_BIG_INT(val));
            if (JS_IsException(val))
                return val;
            break;
        case JS_TAG_BIG_INT:
            {
                JSBigInt *p = JS_VALUE_GET_PTR(val);
                double d;
                d = js_bigint_to_float64(ctx, p);
                JS_FreeValue(ctx, val);
                val = JS_NewFloat64(ctx, d);
            }
            break;
        default:
            break;
        }
    }
    if (!JS_IsUndefined(new_target)) {
        obj = js_create_from_ctor(ctx, new_target, JS_CLASS_NUMBER);
        if (!JS_IsException(obj))
            JS_SetObjectData(ctx, obj, val);
        return obj;
    } else {
        return val;
    }
}

#if 0
static JSValue js_number___toInteger(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    return JS_ToIntegerFree(ctx, JS_DupValue(ctx, argv[0]));
}

static JSValue js_number___toLength(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    int64_t v;
    if (JS_ToLengthFree(ctx, &v, JS_DupValue(ctx, argv[0])))
        return JS_EXCEPTION;
    return JS_NewInt64(ctx, v);
}
#endif

static JSValue js_number_isNaN(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    if (!JS_IsNumber(argv[0]))
        return JS_FALSE;
    return js_global_isNaN(ctx, this_val, argc, argv);
}

static JSValue js_number_isFinite(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    if (!JS_IsNumber(argv[0]))
        return JS_FALSE;
    return js_global_isFinite(ctx, this_val, argc, argv);
}

static JSValue js_number_isInteger(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    int ret;
    ret = JS_NumberIsInteger(ctx, argv[0]);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

static JSValue js_number_isSafeInteger(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    double d;
    if (!JS_IsNumber(argv[0]))
        return JS_FALSE;
    if (unlikely(JS_ToFloat64(ctx, &d, argv[0])))
        return JS_EXCEPTION;
    return JS_NewBool(ctx, is_safe_integer(d));
}

const JSCFunctionListEntry js_number_funcs[] = {
    /* global ParseInt and parseFloat should be defined already or delayed */
    JS_ALIAS_BASE_DEF("parseInt", "parseInt", 0 ),
    JS_ALIAS_BASE_DEF("parseFloat", "parseFloat", 0 ),
    JS_CFUNC_DEF("isNaN", 1, js_number_isNaN ),
    JS_CFUNC_DEF("isFinite", 1, js_number_isFinite ),
    JS_CFUNC_DEF("isInteger", 1, js_number_isInteger ),
    JS_CFUNC_DEF("isSafeInteger", 1, js_number_isSafeInteger ),
    JS_PROP_DOUBLE_DEF("MAX_VALUE", 1.7976931348623157e+308, 0 ),
    JS_PROP_DOUBLE_DEF("MIN_VALUE", 5e-324, 0 ),
    JS_PROP_DOUBLE_DEF("NaN", NAN, 0 ),
    JS_PROP_DOUBLE_DEF("NEGATIVE_INFINITY", -INFINITY, 0 ),
    JS_PROP_DOUBLE_DEF("POSITIVE_INFINITY", INFINITY, 0 ),
    JS_PROP_DOUBLE_DEF("EPSILON", 2.220446049250313e-16, 0 ), /* ES6 */
    JS_PROP_DOUBLE_DEF("MAX_SAFE_INTEGER", 9007199254740991.0, 0 ), /* ES6 */
    JS_PROP_DOUBLE_DEF("MIN_SAFE_INTEGER", -9007199254740991.0, 0 ), /* ES6 */
    //JS_CFUNC_DEF("__toInteger", 1, js_number___toInteger ),
    //JS_CFUNC_DEF("__toLength", 1, js_number___toLength ),
};

static JSValue js_thisNumberValue(JSContext *ctx, JSValueConst this_val)
{
    if (JS_IsNumber(this_val))
        return JS_DupValue(ctx, this_val);

    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(this_val);
        if (p->class_id == JS_CLASS_NUMBER) {
            if (JS_IsNumber(p->u.object_data))
                return JS_DupValue(ctx, p->u.object_data);
        }
    }
    return JS_ThrowTypeError(ctx, "not a number");
}

static JSValue js_number_valueOf(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    return js_thisNumberValue(ctx, this_val);
}

int js_get_radix(JSContext *ctx, JSValueConst val)
{
    int radix;
    if (JS_ToInt32Sat(ctx, &radix, val))
        return -1;
    if (radix < 2 || radix > 36) {
        JS_ThrowRangeError(ctx, "radix must be between 2 and 36");
        return -1;
    }
    return radix;
}

static JSValue js_number_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic)
{
    JSValue val;
    int base, flags;
    double d;

    val = js_thisNumberValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    if (magic || JS_IsUndefined(argv[0])) {
        base = 10;
    } else {
        base = js_get_radix(ctx, argv[0]);
        if (base < 0)
            goto fail;
    }
    if (JS_VALUE_GET_TAG(val) == JS_TAG_INT) {
        char buf1[70];
        int len;
        len = i64toa_radix(buf1, JS_VALUE_GET_INT(val), base);
        return js_new_string8_len(ctx, buf1, len);
    }
    if (JS_ToFloat64Free(ctx, &d, val))
        return JS_EXCEPTION;
    flags = JS_DTOA_FORMAT_FREE;
    if (base != 10)
        flags |= JS_DTOA_EXP_DISABLED;
    return js_dtoa2(ctx, d, base, 0, flags);
 fail:
    JS_FreeValue(ctx, val);
    return JS_EXCEPTION;
}

static JSValue js_number_toFixed(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSValue val;
    int f, flags;
    double d;

    val = js_thisNumberValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    if (JS_ToFloat64Free(ctx, &d, val))
        return JS_EXCEPTION;
    if (JS_ToInt32Sat(ctx, &f, argv[0]))
        return JS_EXCEPTION;
    if (f < 0 || f > 100)
        return JS_ThrowRangeError(ctx, "invalid number of digits");
    if (fabs(d) >= 1e21)
        flags = JS_DTOA_FORMAT_FREE;
    else
        flags = JS_DTOA_FORMAT_FRAC;
    return js_dtoa2(ctx, d, 10, f, flags);
}

static JSValue js_number_toExponential(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValue val;
    int f, flags;
    double d;

    val = js_thisNumberValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    if (JS_ToFloat64Free(ctx, &d, val))
        return JS_EXCEPTION;
    if (JS_ToInt32Sat(ctx, &f, argv[0]))
        return JS_EXCEPTION;
    if (!isfinite(d)) {
        return JS_ToStringFree(ctx,  __JS_NewFloat64(ctx, d));
    }
    if (JS_IsUndefined(argv[0])) {
        flags = JS_DTOA_FORMAT_FREE;
        f = 0;
    } else {
        if (f < 0 || f > 100)
            return JS_ThrowRangeError(ctx, "invalid number of digits");
        f++;
        flags = JS_DTOA_FORMAT_FIXED;
    }
    return js_dtoa2(ctx, d, 10, f, flags | JS_DTOA_EXP_ENABLED);
}

static JSValue js_number_toPrecision(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSValue val;
    int p;
    double d;

    val = js_thisNumberValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    if (JS_ToFloat64Free(ctx, &d, val))
        return JS_EXCEPTION;
    if (JS_IsUndefined(argv[0]))
        goto to_string;
    if (JS_ToInt32Sat(ctx, &p, argv[0]))
        return JS_EXCEPTION;
    if (!isfinite(d)) {
    to_string:
        return JS_ToStringFree(ctx,  __JS_NewFloat64(ctx, d));
    }
    if (p < 1 || p > 100)
        return JS_ThrowRangeError(ctx, "invalid number of digits");
    return js_dtoa2(ctx, d, 10, p, JS_DTOA_FORMAT_FIXED);
}

const JSCFunctionListEntry js_number_proto_funcs[] = {
    JS_CFUNC_DEF("toExponential", 1, js_number_toExponential ),
    JS_CFUNC_DEF("toFixed", 1, js_number_toFixed ),
    JS_CFUNC_DEF("toPrecision", 1, js_number_toPrecision ),
    JS_CFUNC_MAGIC_DEF("toString", 1, js_number_toString, 0 ),
    JS_CFUNC_MAGIC_DEF("toLocaleString", 0, js_number_toString, 1 ),
    JS_CFUNC_DEF("valueOf", 0, js_number_valueOf ),
};

JSValue js_parseInt(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    const char *str, *p;
    int radix, flags;
    JSValue ret;

    str = JS_ToCString(ctx, argv[0]);
    if (!str)
        return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &radix, argv[1])) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }
    if (radix != 0 && (radix < 2 || radix > 36)) {
        ret = JS_NAN;
    } else {
        p = str;
        p += skip_spaces(p);
        flags = ATOD_INT_ONLY | ATOD_ACCEPT_PREFIX_AFTER_SIGN;
        ret = js_atof(ctx, p, NULL, radix, flags);
    }
    JS_FreeCString(ctx, str);
    return ret;
}

JSValue js_parseFloat(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    const char *str, *p;
    JSValue ret;

    str = JS_ToCString(ctx, argv[0]);
    if (!str)
        return JS_EXCEPTION;
    p = str;
    p += skip_spaces(p);
    ret = js_atof(ctx, p, NULL, 10, 0);
    JS_FreeCString(ctx, str);
    return ret;
}

/* Boolean */
JSValue js_boolean_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValue val, obj;
    val = JS_NewBool(ctx, JS_ToBool(ctx, argv[0]));
    if (!JS_IsUndefined(new_target)) {
        obj = js_create_from_ctor(ctx, new_target, JS_CLASS_BOOLEAN);
        if (!JS_IsException(obj))
            JS_SetObjectData(ctx, obj, val);
        return obj;
    } else {
        return val;
    }
}

static JSValue js_thisBooleanValue(JSContext *ctx, JSValueConst this_val)
{
    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_BOOL)
        return JS_DupValue(ctx, this_val);

    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(this_val);
        if (p->class_id == JS_CLASS_BOOLEAN) {
            if (JS_VALUE_GET_TAG(p->u.object_data) == JS_TAG_BOOL)
                return p->u.object_data;
        }
    }
    return JS_ThrowTypeError(ctx, "not a boolean");
}

static JSValue js_boolean_toString(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSValue val = js_thisBooleanValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    return JS_AtomToString(ctx, JS_VALUE_GET_BOOL(val) ?
                       JS_ATOM_true : JS_ATOM_false);
}

static JSValue js_boolean_valueOf(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    return js_thisBooleanValue(ctx, this_val);
}

const JSCFunctionListEntry js_boolean_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_boolean_toString ),
    JS_CFUNC_DEF("valueOf", 0, js_boolean_valueOf ),
};

/* String */

static int js_string_get_own_property(JSContext *ctx,
                                      JSPropertyDescriptor *desc,
                                      JSValueConst obj, JSAtom prop)
{
    JSObject *p;
    JSString *p1;
    uint32_t idx, ch;

    /* This is a class exotic method: obj class_id is JS_CLASS_STRING */
    if (__JS_AtomIsTaggedInt(prop)) {
        p = JS_VALUE_GET_OBJ(obj);
        if (JS_VALUE_GET_TAG(p->u.object_data) == JS_TAG_STRING) {
            p1 = JS_VALUE_GET_STRING(p->u.object_data);
            idx = __JS_AtomToUInt32(prop);
            if (idx < p1->len) {
                if (desc) {
                    ch = string_get(p1, idx);
                    desc->flags = JS_PROP_ENUMERABLE;
                    desc->value = js_new_string_char(ctx, ch);
                    desc->getter = JS_UNDEFINED;
                    desc->setter = JS_UNDEFINED;
                }
                return TRUE;
            }
        }
    }
    return FALSE;
}

static int js_string_define_own_property(JSContext *ctx,
                                         JSValueConst this_obj,
                                         JSAtom prop, JSValueConst val,
                                         JSValueConst getter,
                                         JSValueConst setter, int flags)
{
    uint32_t idx;
    JSObject *p;
    JSString *p1, *p2;

    if (__JS_AtomIsTaggedInt(prop)) {
        idx = __JS_AtomToUInt32(prop);
        p = JS_VALUE_GET_OBJ(this_obj);
        if (JS_VALUE_GET_TAG(p->u.object_data) != JS_TAG_STRING)
            goto def;
        p1 = JS_VALUE_GET_STRING(p->u.object_data);
        if (idx >= p1->len)
            goto def;
        if (!check_define_prop_flags(JS_PROP_ENUMERABLE, flags))
            goto fail;
        /* check that the same value is configured */
        if (flags & JS_PROP_HAS_VALUE) {
            if (JS_VALUE_GET_TAG(val) != JS_TAG_STRING)
                goto fail;
            p2 = JS_VALUE_GET_STRING(val);
            if (p2->len != 1)
                goto fail;
            if (string_get(p1, idx) != string_get(p2, 0)) {
            fail:
                return JS_ThrowTypeErrorOrFalse(ctx, flags, "property is not configurable");
            }
        }
        return TRUE;
    } else {
    def:
        return JS_DefineProperty(ctx, this_obj, prop, val, getter, setter,
                                 flags | JS_PROP_NO_EXOTIC);
    }
}

static int js_string_delete_property(JSContext *ctx,
                                     JSValueConst obj, JSAtom prop)
{
    uint32_t idx;

    if (__JS_AtomIsTaggedInt(prop)) {
        idx = __JS_AtomToUInt32(prop);
        if (idx < js_string_obj_get_length(ctx, obj)) {
            return FALSE;
        }
    }
    return TRUE;
}

const JSClassExoticMethods js_string_exotic_methods = {
    .get_own_property = js_string_get_own_property,
    .define_own_property = js_string_define_own_property,
    .delete_property = js_string_delete_property,
};

JSValue js_string_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValue val, obj;
    if (argc == 0) {
        val = JS_AtomToString(ctx, JS_ATOM_empty_string);
    } else {
        if (JS_IsUndefined(new_target) && JS_IsSymbol(argv[0])) {
            JSAtomStruct *p = JS_VALUE_GET_PTR(argv[0]);
            val = JS_ConcatString3(ctx, "Symbol(", JS_AtomToString(ctx, js_get_atom_index(ctx->rt, p)), ")");
        } else {
            val = JS_ToString(ctx, argv[0]);
        }
        if (JS_IsException(val))
            return val;
    }
    if (!JS_IsUndefined(new_target)) {
        JSString *p1 = JS_VALUE_GET_STRING(val);

        obj = js_create_from_ctor(ctx, new_target, JS_CLASS_STRING);
        if (JS_IsException(obj)) {
            JS_FreeValue(ctx, val);
        } else {
            JS_SetObjectData(ctx, obj, val);
            JS_DefinePropertyValue(ctx, obj, JS_ATOM_length, JS_NewInt32(ctx, p1->len), 0);
        }
        return obj;
    } else {
        return val;
    }
}

static JSValue js_thisStringValue(JSContext *ctx, JSValueConst this_val)
{
    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_STRING ||
        JS_VALUE_GET_TAG(this_val) == JS_TAG_STRING_ROPE)
        return JS_DupValue(ctx, this_val);

    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(this_val);
        if (p->class_id == JS_CLASS_STRING) {
            if (JS_VALUE_GET_TAG(p->u.object_data) == JS_TAG_STRING)
                return JS_DupValue(ctx, p->u.object_data);
        }
    }
    return JS_ThrowTypeError(ctx, "not a string");
}

static JSValue js_string_fromCharCode(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    int i;
    StringBuffer b_s, *b = &b_s;

    string_buffer_init(ctx, b, argc);

    for(i = 0; i < argc; i++) {
        int32_t c;
        if (JS_ToInt32(ctx, &c, argv[i]) || string_buffer_putc16(b, c & 0xffff)) {
            string_buffer_free(b);
            return JS_EXCEPTION;
        }
    }
    return string_buffer_end(b);
}

static JSValue js_string_fromCodePoint(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    double d;
    int i, c;
    StringBuffer b_s, *b = &b_s;

    /* XXX: could pre-compute string length if all arguments are JS_TAG_INT */

    if (string_buffer_init(ctx, b, argc))
        goto fail;
    for(i = 0; i < argc; i++) {
        if (JS_VALUE_GET_TAG(argv[i]) == JS_TAG_INT) {
            c = JS_VALUE_GET_INT(argv[i]);
            if (c < 0 || c > 0x10ffff)
                goto range_error;
        } else {
            if (JS_ToFloat64(ctx, &d, argv[i]))
                goto fail;
            if (isnan(d) || d < 0 || d > 0x10ffff || (c = (int)d) != d)
                goto range_error;
        }
        if (string_buffer_putc(b, c))
            goto fail;
    }
    return string_buffer_end(b);

 range_error:
    JS_ThrowRangeError(ctx, "invalid code point");
 fail:
    string_buffer_free(b);
    return JS_EXCEPTION;
}

static JSValue js_string_raw(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    // raw(temp,...a)
    JSValue cooked, val, raw;
    StringBuffer b_s, *b = &b_s;
    int64_t i, n;

    string_buffer_init(ctx, b, 0);
    raw = JS_UNDEFINED;
    cooked = JS_ToObject(ctx, argv[0]);
    if (JS_IsException(cooked))
        goto exception;
    raw = JS_ToObjectFree(ctx, JS_GetProperty(ctx, cooked, JS_ATOM_raw));
    if (JS_IsException(raw))
        goto exception;
    if (js_get_length64(ctx, &n, raw) < 0)
        goto exception;

    for (i = 0; i < n; i++) {
        val = JS_ToStringFree(ctx, JS_GetPropertyInt64(ctx, raw, i));
        if (JS_IsException(val))
            goto exception;
        string_buffer_concat_value_free(b, val);
        if (i < n - 1 && i + 1 < argc) {
            if (string_buffer_concat_value(b, argv[i + 1]))
                goto exception;
        }
    }
    JS_FreeValue(ctx, cooked);
    JS_FreeValue(ctx, raw);
    return string_buffer_end(b);

exception:
    JS_FreeValue(ctx, cooked);
    JS_FreeValue(ctx, raw);
    string_buffer_free(b);
    return JS_EXCEPTION;
}

/* only used in test262 */
JSValue js_string_codePointRange(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    uint32_t start, end, i, n;
    StringBuffer b_s, *b = &b_s;

    if (JS_ToUint32(ctx, &start, argv[0]) ||
        JS_ToUint32(ctx, &end, argv[1]))
        return JS_EXCEPTION;
    end = min_uint32(end, 0x10ffff + 1);

    if (start > end) {
        start = end;
    }
    n = end - start;
    if (end > 0x10000) {
        n += end - max_uint32(start, 0x10000);
    }
    if (string_buffer_init2(ctx, b, n, end >= 0x100))
        return JS_EXCEPTION;
    for(i = start; i < end; i++) {
        string_buffer_putc(b, i);
    }
    return string_buffer_end(b);
}

#if 0
static JSValue js_string___isSpace(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    int c;
    if (JS_ToInt32(ctx, &c, argv[0]))
        return JS_EXCEPTION;
    return JS_NewBool(ctx, lre_is_space(c));
}
#endif

static JSValue js_string_charCodeAt(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSValue val, ret;
    JSString *p;
    int idx, c;

    val = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(val))
        return val;
    p = JS_VALUE_GET_STRING(val);
    if (JS_ToInt32Sat(ctx, &idx, argv[0])) {
        JS_FreeValue(ctx, val);
        return JS_EXCEPTION;
    }
    if (idx < 0 || idx >= p->len) {
        ret = JS_NAN;
    } else {
        c = string_get(p, idx);
        ret = JS_NewInt32(ctx, c);
    }
    JS_FreeValue(ctx, val);
    return ret;
}

static JSValue js_string_charAt(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int is_at)
{
    JSValue val, ret;
    JSString *p;
    int idx, c;

    val = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(val))
        return val;
    p = JS_VALUE_GET_STRING(val);
    if (JS_ToInt32Sat(ctx, &idx, argv[0])) {
        JS_FreeValue(ctx, val);
        return JS_EXCEPTION;
    }
    if (idx < 0 && is_at)
        idx += p->len;
    if (idx < 0 || idx >= p->len) {
        if (is_at)
            ret = JS_UNDEFINED;
        else
            ret = JS_AtomToString(ctx, JS_ATOM_empty_string);
    } else {
        c = string_get(p, idx);
        ret = js_new_string_char(ctx, c);
    }
    JS_FreeValue(ctx, val);
    return ret;
}

static JSValue js_string_codePointAt(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSValue val, ret;
    JSString *p;
    int idx, c;

    val = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(val))
        return val;
    p = JS_VALUE_GET_STRING(val);
    if (JS_ToInt32Sat(ctx, &idx, argv[0])) {
        JS_FreeValue(ctx, val);
        return JS_EXCEPTION;
    }
    if (idx < 0 || idx >= p->len) {
        ret = JS_UNDEFINED;
    } else {
        c = string_getc(p, &idx);
        ret = JS_NewInt32(ctx, c);
    }
    JS_FreeValue(ctx, val);
    return ret;
}

static JSValue js_string_concat(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue r;
    int i;

    /* XXX: Use more efficient method */
    /* XXX: This method is OK if r has a single refcount */
    /* XXX: should use string_buffer? */
    r = JS_ToStringCheckObject(ctx, this_val);
    for (i = 0; i < argc; i++) {
        if (JS_IsException(r))
            break;
        r = JS_ConcatString(ctx, r, JS_DupValue(ctx, argv[i]));
    }
    return r;
}

static int string_cmp(JSString *p1, JSString *p2, int x1, int x2, int len)
{
    int i, c1, c2;
    for (i = 0; i < len; i++) {
        if ((c1 = string_get(p1, x1 + i)) != (c2 = string_get(p2, x2 + i)))
            return c1 - c2;
    }
    return 0;
}

int string_indexof_char(JSString *p, int c, int from)
{
    /* assuming 0 <= from <= p->len */
    int i, len = p->len;
    if (p->is_wide_char) {
        for (i = from; i < len; i++) {
            if (p->u.str16[i] == c)
                return i;
        }
    } else {
        if ((c & ~0xff) == 0) {
            for (i = from; i < len; i++) {
                if (p->u.str8[i] == (uint8_t)c)
                    return i;
            }
        }
    }
    return -1;
}

static int string_indexof(JSString *p1, JSString *p2, int from)
{
    /* assuming 0 <= from <= p1->len */
    int c, i, j, len1 = p1->len, len2 = p2->len;
    if (len2 == 0)
        return from;
    for (i = from, c = string_get(p2, 0); i + len2 <= len1; i = j + 1) {
        j = string_indexof_char(p1, c, i);
        if (j < 0 || j + len2 > len1)
            break;
        if (!string_cmp(p1, p2, j + 1, 1, len2 - 1))
            return j;
    }
    return -1;
}

int64_t string_advance_index(JSString *p, int64_t index, BOOL unicode)
{
    if (!unicode || index >= p->len || !p->is_wide_char) {
        index++;
    } else {
        int index32 = (int)index;
        string_getc(p, &index32);
        index = index32;
    }
    return index;
}

/* return the position of the first invalid character in the string or
   -1 if none */
int js_string_find_invalid_codepoint(JSString *p)
{
    int i;
    if (!p->is_wide_char)
        return -1;
    for(i = 0; i < p->len; i++) {
        uint32_t c = p->u.str16[i];
        if (is_surrogate(c)) {
            if (is_hi_surrogate(c) && (i + 1) < p->len
            &&  is_lo_surrogate(p->u.str16[i + 1])) {
                i++;
            } else {
                return i;
            }
        }
    }
    return -1;
}

static JSValue js_string_isWellFormed(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    JSValue str;
    JSString *p;
    BOOL ret;

    str = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(str))
        return JS_EXCEPTION;
    p = JS_VALUE_GET_STRING(str);
    ret = (js_string_find_invalid_codepoint(p) < 0);
    JS_FreeValue(ctx, str);
    return JS_NewBool(ctx, ret);
}

static JSValue js_string_toWellFormed(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    JSValue str, ret;
    JSString *p;
    int i;

    str = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(str))
        return JS_EXCEPTION;

    p = JS_VALUE_GET_STRING(str);
    /* avoid reallocating the string if it is well-formed */
    i = js_string_find_invalid_codepoint(p);
    if (i < 0)
        return str;

    ret = js_new_string16_len(ctx, p->u.str16, p->len);
    JS_FreeValue(ctx, str);
    if (JS_IsException(ret))
        return JS_EXCEPTION;

    p = JS_VALUE_GET_STRING(ret);
    for (; i < p->len; i++) {
        uint32_t c = p->u.str16[i];
        if (is_surrogate(c)) {
            if (is_hi_surrogate(c) && (i + 1) < p->len
            &&  is_lo_surrogate(p->u.str16[i + 1])) {
                i++;
            } else {
                p->u.str16[i] = 0xFFFD;
            }
        }
    }
    return ret;
}

static JSValue js_string_indexOf(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int lastIndexOf)
{
    JSValue str, v;
    int i, len, v_len, pos, start, stop, ret, inc;
    JSString *p;
    JSString *p1;

    str = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(str))
        return str;
    v = JS_ToString(ctx, argv[0]);
    if (JS_IsException(v))
        goto fail;
    p = JS_VALUE_GET_STRING(str);
    p1 = JS_VALUE_GET_STRING(v);
    len = p->len;
    v_len = p1->len;
    if (lastIndexOf) {
        pos = len - v_len;
        if (argc > 1) {
            double d;
            if (JS_ToFloat64(ctx, &d, argv[1]))
                goto fail;
            if (!isnan(d)) {
                if (d <= 0)
                    pos = 0;
                else if (d < pos)
                    pos = d;
            }
        }
        start = pos;
        stop = 0;
        inc = -1;
    } else {
        pos = 0;
        if (argc > 1) {
            if (JS_ToInt32Clamp(ctx, &pos, argv[1], 0, len, 0))
                goto fail;
        }
        start = pos;
        stop = len - v_len;
        inc = 1;
    }
    ret = -1;
    if (len >= v_len && inc * (stop - start) >= 0) {
        for (i = start;; i += inc) {
            if (!string_cmp(p, p1, i, 0, v_len)) {
                ret = i;
                break;
            }
            if (i == stop)
                break;
        }
    }
    JS_FreeValue(ctx, str);
    JS_FreeValue(ctx, v);
    return JS_NewInt32(ctx, ret);

fail:
    JS_FreeValue(ctx, str);
    JS_FreeValue(ctx, v);
    return JS_EXCEPTION;
}

/* return < 0 if exception or TRUE/FALSE */

static JSValue js_string_includes(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic)
{
    JSValue str, v = JS_UNDEFINED;
    int i, len, v_len, pos, start, stop, ret;
    JSString *p;
    JSString *p1;

    str = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(str))
        return str;
    ret = js_is_regexp(ctx, argv[0]);
    if (ret) {
        if (ret > 0)
            JS_ThrowTypeError(ctx, "regexp not supported");
        goto fail;
    }
    v = JS_ToString(ctx, argv[0]);
    if (JS_IsException(v))
        goto fail;
    p = JS_VALUE_GET_STRING(str);
    p1 = JS_VALUE_GET_STRING(v);
    len = p->len;
    v_len = p1->len;
    pos = (magic == 2) ? len : 0;
    if (argc > 1 && !JS_IsUndefined(argv[1])) {
        if (JS_ToInt32Clamp(ctx, &pos, argv[1], 0, len, 0))
            goto fail;
    }
    len -= v_len;
    ret = 0;
    if (magic == 0) {
        start = pos;
        stop = len;
    } else {
        if (magic == 1) {
            if (pos > len)
                goto done;
        } else {
            pos -= v_len;
        }
        start = stop = pos;
    }
    if (start >= 0 && start <= stop) {
        for (i = start;; i++) {
            if (!string_cmp(p, p1, i, 0, v_len)) {
                ret = 1;
                break;
            }
            if (i == stop)
                break;
        }
    }
 done:
    JS_FreeValue(ctx, str);
    JS_FreeValue(ctx, v);
    return JS_NewBool(ctx, ret);

fail:
    JS_FreeValue(ctx, str);
    JS_FreeValue(ctx, v);
    return JS_EXCEPTION;
}

static int check_regexp_g_flag(JSContext *ctx, JSValueConst regexp)
{
    int ret;
    JSValue flags;

    ret = js_is_regexp(ctx, regexp);
    if (ret < 0)
        return -1;
    if (ret) {
        flags = JS_GetProperty(ctx, regexp, JS_ATOM_flags);
        if (JS_IsException(flags))
            return -1;
        if (JS_IsUndefined(flags) || JS_IsNull(flags)) {
            JS_ThrowTypeError(ctx, "cannot convert to object");
            return -1;
        }
        flags = JS_ToStringFree(ctx, flags);
        if (JS_IsException(flags))
            return -1;
        ret = string_indexof_char(JS_VALUE_GET_STRING(flags), 'g', 0);
        JS_FreeValue(ctx, flags);
        if (ret < 0) {
            JS_ThrowTypeError(ctx, "regexp must have the 'g' flag");
            return -1;
        }
    }
    return 0;
}

static JSValue js_string_match(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int atom)
{
    // match(rx), search(rx), matchAll(rx)
    // atom is JS_ATOM_Symbol_match, JS_ATOM_Symbol_search, or JS_ATOM_Symbol_matchAll
    JSValueConst O = this_val, regexp = argv[0], args[2];
    JSValue matcher, S, rx, result, str;
    int args_len;

    if (JS_IsUndefined(O) || JS_IsNull(O))
        return JS_ThrowTypeError(ctx, "cannot convert to object");

    if (JS_IsObject(regexp)) {
        matcher = JS_GetProperty(ctx, regexp, atom);
        if (JS_IsException(matcher))
            return JS_EXCEPTION;
        if (atom == JS_ATOM_Symbol_matchAll) {
            if (check_regexp_g_flag(ctx, regexp) < 0) {
                JS_FreeValue(ctx, matcher);
                return JS_EXCEPTION;
            }
        }
        if (!JS_IsUndefined(matcher) && !JS_IsNull(matcher)) {
            return JS_CallFree(ctx, matcher, regexp, 1, &O);
        }
    }
    S = JS_ToString(ctx, O);
    if (JS_IsException(S))
        return JS_EXCEPTION;
    args_len = 1;
    args[0] = regexp;
    str = JS_UNDEFINED;
    if (atom == JS_ATOM_Symbol_matchAll) {
        str = js_new_string8(ctx, "g");
        if (JS_IsException(str))
            goto fail;
        args[args_len++] = (JSValueConst)str;
    }
    rx = JS_CallConstructor(ctx, ctx->regexp_ctor, args_len, args);
    JS_FreeValue(ctx, str);
    if (JS_IsException(rx)) {
    fail:
        JS_FreeValue(ctx, S);
        return JS_EXCEPTION;
    }
    result = JS_InvokeFree(ctx, rx, atom, 1, (JSValueConst *)&S);
    JS_FreeValue(ctx, S);
    return result;
}

/* if captures != NULL, captures_val and matched are ignored. Otherwise,
   captures_len is ignored */
int js_string_GetSubstitution(JSContext *ctx,
                                     StringBuffer *b,
                                     JSValueConst matched,
                                     JSString *sp,
                                     uint32_t position,
                                     JSValueConst captures_val,
                                     JSValueConst namedCaptures,
                                     JSValueConst rep,
                                     uint8_t **captures,
                                     uint32_t captures_len)
{
    JSValue capture, name, s;
    uint32_t len, matched_len;
    int i, j, j0, k, k1, shift;
    int c, c1;
    JSString *rp;

    if (JS_VALUE_GET_TAG(rep) != JS_TAG_STRING) {
        JS_ThrowTypeError(ctx, "not a string");
        goto exception;
    }
    shift = sp->is_wide_char;
    rp = JS_VALUE_GET_STRING(rep);

    if (captures) {
        matched_len = (captures[1] - captures[0]) >> shift;
    } else {
        captures_len = 0;
        if (!JS_IsUndefined(captures_val)) {
            if (js_get_length32(ctx, &captures_len, captures_val))
                goto exception;
        }
        if (js_get_length32(ctx, &matched_len, matched))
            goto exception;
    }

    len = rp->len;
    i = 0;
    for(;;) {
        j = string_indexof_char(rp, '$', i);
        if (j < 0 || j + 1 >= len)
            break;
        string_buffer_concat(b, rp, i, j);
        j0 = j++;
        c = string_get(rp, j++);
        if (c == '$') {
            string_buffer_putc8(b, '$');
        } else if (c == '&') {
            if (captures) {
                string_buffer_concat(b, sp, position, position + matched_len);
            } else {
                if (string_buffer_concat_value(b, matched))
                    goto exception;
            }
        } else if (c == '`') {
            string_buffer_concat(b, sp, 0, position);
        } else if (c == '\'') {
            string_buffer_concat(b, sp, position + matched_len, sp->len);
        } else if (c >= '0' && c <= '9') {
            k = c - '0';
            if (j < len) {
                c1 = string_get(rp, j);
                if (c1 >= '0' && c1 <= '9') {
                    /* This behavior is specified in ES6 and refined in ECMA 2019 */
                    /* ECMA 2019 does not have the extra test, but
                       Test262 S15.5.4.11_A3_T1..3 require this behavior */
                    k1 = k * 10 + c1 - '0';
                    if (k1 >= 1 && k1 < captures_len) {
                        k = k1;
                        j++;
                    }
                }
            }
            if (k >= 1 && k < captures_len) {
                if (captures) {
                    int start, end;
                    if (captures[2 * k] && captures[2 * k + 1]) {
                        start = (captures[2 * k] - sp->u.str8) >> shift;
                        end = (captures[2 * k + 1] - sp->u.str8) >> shift;
                        string_buffer_concat(b, sp, start, end);
                    }
                } else {
                    s = JS_GetPropertyInt64(ctx, captures_val, k);
                    if (JS_IsException(s))
                        goto exception;
                    if (!JS_IsUndefined(s)) {
                        if (string_buffer_concat_value_free(b, s))
                            goto exception;
                    }
                }
            } else {
                goto norep;
            }
        } else if (c == '<' && !JS_IsUndefined(namedCaptures)) {
            k = string_indexof_char(rp, '>', j);
            if (k < 0)
                goto norep;
            name = js_sub_string(ctx, rp, j, k);
            if (JS_IsException(name))
                goto exception;
            capture = JS_GetPropertyValue(ctx, namedCaptures, name);
            if (JS_IsException(capture))
                goto exception;
            if (!JS_IsUndefined(capture)) {
                if (string_buffer_concat_value_free(b, capture))
                    goto exception;
            }
            j = k + 1;
        } else {
        norep:
            string_buffer_concat(b, rp, j0, j);
        }
        i = j;
    }
    string_buffer_concat(b, rp, i, rp->len);
    return 0;
exception:
    return -1;
}

static JSValue js_string_replace(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv,
                                 int is_replaceAll)
{
    // replace(rx, rep)
    JSValueConst O = this_val, searchValue = argv[0], replaceValue = argv[1];
    JSValueConst args[3];
    JSValue str, search_str, replaceValue_str, repl_str;
    JSString *sp, *searchp;
    StringBuffer b_s, *b = &b_s;
    int pos, functionalReplace, endOfLastMatch;
    BOOL is_first;

    if (JS_IsUndefined(O) || JS_IsNull(O))
        return JS_ThrowTypeError(ctx, "cannot convert to object");

    search_str = JS_UNDEFINED;
    replaceValue_str = JS_UNDEFINED;
    repl_str = JS_UNDEFINED;

    if (JS_IsObject(searchValue)) {
        JSValue replacer;
        if (is_replaceAll) {
            if (check_regexp_g_flag(ctx, searchValue) < 0)
                return JS_EXCEPTION;
        }
        replacer = JS_GetProperty(ctx, searchValue, JS_ATOM_Symbol_replace);
        if (JS_IsException(replacer))
            return JS_EXCEPTION;
        if (!JS_IsUndefined(replacer) && !JS_IsNull(replacer)) {
            args[0] = O;
            args[1] = replaceValue;
            return JS_CallFree(ctx, replacer, searchValue, 2, args);
        }
    }
    string_buffer_init(ctx, b, 0);

    str = JS_ToString(ctx, O);
    if (JS_IsException(str))
        goto exception;
    search_str = JS_ToString(ctx, searchValue);
    if (JS_IsException(search_str))
        goto exception;
    functionalReplace = JS_IsFunction(ctx, replaceValue);
    if (!functionalReplace) {
        replaceValue_str = JS_ToString(ctx, replaceValue);
        if (JS_IsException(replaceValue_str))
            goto exception;
    }

    sp = JS_VALUE_GET_STRING(str);
    searchp = JS_VALUE_GET_STRING(search_str);
    endOfLastMatch = 0;
    is_first = TRUE;
    for(;;) {
        if (unlikely(searchp->len == 0)) {
            if (is_first)
                pos = 0;
            else if (endOfLastMatch >= sp->len)
                pos = -1;
            else
                pos = endOfLastMatch + 1;
        } else {
            pos = string_indexof(sp, searchp, endOfLastMatch);
        }
        if (pos < 0) {
            if (is_first) {
                string_buffer_free(b);
                JS_FreeValue(ctx, search_str);
                JS_FreeValue(ctx, replaceValue_str);
                return str;
            } else {
                break;
            }
        }

        string_buffer_concat(b, sp, endOfLastMatch, pos);

        if (functionalReplace) {
            args[0] = search_str;
            args[1] = JS_NewInt32(ctx, pos);
            args[2] = str;
            repl_str = JS_ToStringFree(ctx, JS_Call(ctx, replaceValue, JS_UNDEFINED, 3, args));
            if (JS_IsException(repl_str))
                goto exception;
            string_buffer_concat_value_free(b, repl_str);
        } else {
            if (js_string_GetSubstitution(ctx, b, search_str, sp, pos,
                                          JS_UNDEFINED, JS_UNDEFINED, replaceValue_str,
                                          NULL, 0)) {
                goto exception;
            }
        }

        endOfLastMatch = pos + searchp->len;
        is_first = FALSE;
        if (!is_replaceAll)
            break;
    }
    string_buffer_concat(b, sp, endOfLastMatch, sp->len);
    JS_FreeValue(ctx, search_str);
    JS_FreeValue(ctx, replaceValue_str);
    JS_FreeValue(ctx, str);
    return string_buffer_end(b);

exception:
    string_buffer_free(b);
    JS_FreeValue(ctx, search_str);
    JS_FreeValue(ctx, replaceValue_str);
    JS_FreeValue(ctx, str);
    return JS_EXCEPTION;
}

static JSValue js_string_split(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    // split(sep, limit)
    JSValueConst O = this_val, separator = argv[0], limit = argv[1];
    JSValueConst args[2];
    JSValue S, A, R, T;
    uint32_t lim, lengthA;
    int64_t p, q, s, r, e;
    JSString *sp, *rp;

    if (JS_IsUndefined(O) || JS_IsNull(O))
        return JS_ThrowTypeError(ctx, "cannot convert to object");

    S = JS_UNDEFINED;
    A = JS_UNDEFINED;
    R = JS_UNDEFINED;

    if (JS_IsObject(separator)) {
        JSValue splitter;
        splitter = JS_GetProperty(ctx, separator, JS_ATOM_Symbol_split);
        if (JS_IsException(splitter))
            return JS_EXCEPTION;
        if (!JS_IsUndefined(splitter) && !JS_IsNull(splitter)) {
            args[0] = O;
            args[1] = limit;
            return JS_CallFree(ctx, splitter, separator, 2, args);
        }
    }
    S = JS_ToString(ctx, O);
    if (JS_IsException(S))
        goto exception;
    A = JS_NewArray(ctx);
    if (JS_IsException(A))
        goto exception;
    lengthA = 0;
    if (JS_IsUndefined(limit)) {
        lim = 0xffffffff;
    } else {
        if (JS_ToUint32(ctx, &lim, limit) < 0)
            goto exception;
    }
    sp = JS_VALUE_GET_STRING(S);
    s = sp->len;
    R = JS_ToString(ctx, separator);
    if (JS_IsException(R))
        goto exception;
    rp = JS_VALUE_GET_STRING(R);
    r = rp->len;
    p = 0;
    if (lim == 0)
        goto done;
    if (JS_IsUndefined(separator))
        goto add_tail;
    if (s == 0) {
        if (r != 0)
            goto add_tail;
        goto done;
    }
    for (q = p; (q += !r) <= s - r - !r; q = p = e + r) {
        e = string_indexof(sp, rp, q);
        if (e < 0)
            break;
        T = js_sub_string(ctx, sp, p, e);
        if (JS_IsException(T))
            goto exception;
        if (JS_CreateDataPropertyUint32(ctx, A, lengthA++, T, 0) < 0)
            goto exception;
        if (lengthA == lim)
            goto done;
    }
add_tail:
    T = js_sub_string(ctx, sp, p, s);
    if (JS_IsException(T))
        goto exception;
    if (JS_CreateDataPropertyUint32(ctx, A, lengthA++, T,0 ) < 0)
        goto exception;
done:
    JS_FreeValue(ctx, S);
    JS_FreeValue(ctx, R);
    return A;

exception:
    JS_FreeValue(ctx, A);
    JS_FreeValue(ctx, S);
    JS_FreeValue(ctx, R);
    return JS_EXCEPTION;
}

static JSValue js_string_substring(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSValue str, ret;
    int a, b, start, end;
    JSString *p;

    str = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(str))
        return str;
    p = JS_VALUE_GET_STRING(str);
    if (JS_ToInt32Clamp(ctx, &a, argv[0], 0, p->len, 0)) {
        JS_FreeValue(ctx, str);
        return JS_EXCEPTION;
    }
    b = p->len;
    if (!JS_IsUndefined(argv[1])) {
        if (JS_ToInt32Clamp(ctx, &b, argv[1], 0, p->len, 0)) {
            JS_FreeValue(ctx, str);
            return JS_EXCEPTION;
        }
    }
    if (a < b) {
        start = a;
        end = b;
    } else {
        start = b;
        end = a;
    }
    ret = js_sub_string(ctx, p, start, end);
    JS_FreeValue(ctx, str);
    return ret;
}

static JSValue js_string_substr(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue str, ret;
    int a, len, n;
    JSString *p;

    str = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(str))
        return str;
    p = JS_VALUE_GET_STRING(str);
    len = p->len;
    if (JS_ToInt32Clamp(ctx, &a, argv[0], 0, len, len)) {
        JS_FreeValue(ctx, str);
        return JS_EXCEPTION;
    }
    n = len - a;
    if (!JS_IsUndefined(argv[1])) {
        if (JS_ToInt32Clamp(ctx, &n, argv[1], 0, len - a, 0)) {
            JS_FreeValue(ctx, str);
            return JS_EXCEPTION;
        }
    }
    ret = js_sub_string(ctx, p, a, a + n);
    JS_FreeValue(ctx, str);
    return ret;
}

static JSValue js_string_slice(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    JSValue str, ret;
    int len, start, end;
    JSString *p;

    str = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(str))
        return str;
    p = JS_VALUE_GET_STRING(str);
    len = p->len;
    if (JS_ToInt32Clamp(ctx, &start, argv[0], 0, len, len)) {
        JS_FreeValue(ctx, str);
        return JS_EXCEPTION;
    }
    end = len;
    if (!JS_IsUndefined(argv[1])) {
        if (JS_ToInt32Clamp(ctx, &end, argv[1], 0, len, len)) {
            JS_FreeValue(ctx, str);
            return JS_EXCEPTION;
        }
    }
    ret = js_sub_string(ctx, p, start, max_int(end, start));
    JS_FreeValue(ctx, str);
    return ret;
}

static JSValue js_string_pad(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int padEnd)
{
    JSValue str, v = JS_UNDEFINED;
    StringBuffer b_s, *b = &b_s;
    JSString *p, *p1 = NULL;
    int n, len, c = ' ';

    str = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(str))
        goto fail1;
    if (JS_ToInt32Sat(ctx, &n, argv[0]))
        goto fail2;
    p = JS_VALUE_GET_STRING(str);
    len = p->len;
    if (len >= n)
        return str;
    if (argc > 1 && !JS_IsUndefined(argv[1])) {
        v = JS_ToString(ctx, argv[1]);
        if (JS_IsException(v))
            goto fail2;
        p1 = JS_VALUE_GET_STRING(v);
        if (p1->len == 0) {
            JS_FreeValue(ctx, v);
            return str;
        }
        if (p1->len == 1) {
            c = string_get(p1, 0);
            p1 = NULL;
        }
    }
    if (n > JS_STRING_LEN_MAX) {
        JS_ThrowRangeError(ctx, "invalid string length");
        goto fail3;
    }
    if (string_buffer_init(ctx, b, n))
        goto fail3;
    n -= len;
    if (padEnd) {
        if (string_buffer_concat(b, p, 0, len))
            goto fail;
    }
    if (p1) {
        while (n > 0) {
            int chunk = min_int(n, p1->len);
            if (string_buffer_concat(b, p1, 0, chunk))
                goto fail;
            n -= chunk;
        }
    } else {
        if (string_buffer_fill(b, c, n))
            goto fail;
    }
    if (!padEnd) {
        if (string_buffer_concat(b, p, 0, len))
            goto fail;
    }
    JS_FreeValue(ctx, v);
    JS_FreeValue(ctx, str);
    return string_buffer_end(b);

fail:
    string_buffer_free(b);
fail3:
    JS_FreeValue(ctx, v);
fail2:
    JS_FreeValue(ctx, str);
fail1:
    return JS_EXCEPTION;
}

static JSValue js_string_repeat(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue str;
    StringBuffer b_s, *b = &b_s;
    JSString *p;
    int64_t val;
    int n, len;

    str = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(str))
        goto fail;
    if (JS_ToInt64Sat(ctx, &val, argv[0]))
        goto fail;
    if (val < 0 || val > 2147483647) {
        JS_ThrowRangeError(ctx, "invalid repeat count");
        goto fail;
    }
    n = val;
    p = JS_VALUE_GET_STRING(str);
    len = p->len;
    if (len == 0 || n == 1)
        return str;
    // XXX: potential arithmetic overflow
    if (val * len > JS_STRING_LEN_MAX) {
        JS_ThrowRangeError(ctx, "invalid string length");
        goto fail;
    }
    if (string_buffer_init2(ctx, b, n * len, p->is_wide_char))
        goto fail;
    if (len == 1) {
        string_buffer_fill(b, string_get(p, 0), n);
    } else {
        while (n-- > 0) {
            string_buffer_concat(b, p, 0, len);
        }
    }
    JS_FreeValue(ctx, str);
    return string_buffer_end(b);

fail:
    JS_FreeValue(ctx, str);
    return JS_EXCEPTION;
}

static JSValue js_string_trim(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic)
{
    JSValue str, ret;
    int a, b, len;
    JSString *p;

    str = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(str))
        return str;
    p = JS_VALUE_GET_STRING(str);
    a = 0;
    b = len = p->len;
    if (magic & 1) {
        while (a < len && lre_is_space(string_get(p, a)))
            a++;
    }
    if (magic & 2) {
        while (b > a && lre_is_space(string_get(p, b - 1)))
            b--;
    }
    ret = js_sub_string(ctx, p, a, b);
    JS_FreeValue(ctx, str);
    return ret;
}

/* return 0 if before the first char */
static int string_prevc(JSString *p, int *pidx)
{
    int idx, c, c1;

    idx = *pidx;
    if (idx <= 0)
        return 0;
    idx--;
    if (p->is_wide_char) {
        c = p->u.str16[idx];
        if (is_lo_surrogate(c) && idx > 0) {
            c1 = p->u.str16[idx - 1];
            if (is_hi_surrogate(c1)) {
                c = from_surrogate(c1, c);
                idx--;
            }
        }
    } else {
        c = p->u.str8[idx];
    }
    *pidx = idx;
    return c;
}

static BOOL test_final_sigma(JSString *p, int sigma_pos)
{
    int k, c1;

    /* before C: skip case ignorable chars and check there is
       a cased letter */
    k = sigma_pos;
    for(;;) {
        c1 = string_prevc(p, &k);
        if (!lre_is_case_ignorable(c1))
            break;
    }
    if (!lre_is_cased(c1))
        return FALSE;

    /* after C: skip case ignorable chars and check there is
       no cased letter */
    k = sigma_pos + 1;
    for(;;) {
        if (k >= p->len)
            return TRUE;
        c1 = string_getc(p, &k);
        if (!lre_is_case_ignorable(c1))
            break;
    }
    return !lre_is_cased(c1);
}

static JSValue js_string_toLowerCase(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv, int to_lower)
{
    JSValue val;
    StringBuffer b_s, *b = &b_s;
    JSString *p;
    int i, c, j, l;
    uint32_t res[LRE_CC_RES_LEN_MAX];

    val = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(val))
        return val;
    p = JS_VALUE_GET_STRING(val);
    if (p->len == 0)
        return val;
    if (string_buffer_init(ctx, b, p->len))
        goto fail;
    for(i = 0; i < p->len;) {
        c = string_getc(p, &i);
        if (c == 0x3a3 && to_lower && test_final_sigma(p, i - 1)) {
            res[0] = 0x3c2; /* final sigma */
            l = 1;
        } else {
            l = lre_case_conv(res, c, to_lower);
        }
        for(j = 0; j < l; j++) {
            if (string_buffer_putc(b, res[j]))
                goto fail;
        }
    }
    JS_FreeValue(ctx, val);
    return string_buffer_end(b);
 fail:
    JS_FreeValue(ctx, val);
    string_buffer_free(b);
    return JS_EXCEPTION;
}

#ifdef CONFIG_ALL_UNICODE

/* return (-1, NULL) if exception, otherwise (len, buf) */
static int JS_ToUTF32String(JSContext *ctx, uint32_t **pbuf, JSValueConst val1)
{
    JSValue val;
    JSString *p;
    uint32_t *buf;
    int i, j, len;

    val = JS_ToString(ctx, val1);
    if (JS_IsException(val))
        return -1;
    p = JS_VALUE_GET_STRING(val);
    len = p->len;
    /* UTF32 buffer length is len minus the number of correct surrogates pairs */
    buf = js_malloc(ctx, sizeof(buf[0]) * max_int(len, 1));
    if (!buf) {
        JS_FreeValue(ctx, val);
        goto fail;
    }
    for(i = j = 0; i < len;)
        buf[j++] = string_getc(p, &i);
    JS_FreeValue(ctx, val);
    *pbuf = buf;
    return j;
 fail:
    *pbuf = NULL;
    return -1;
}

static JSValue JS_NewUTF32String(JSContext *ctx, const uint32_t *buf, int len)
{
    int i;
    StringBuffer b_s, *b = &b_s;
    if (string_buffer_init(ctx, b, len))
        return JS_EXCEPTION;
    for(i = 0; i < len; i++) {
        if (string_buffer_putc(b, buf[i]))
            goto fail;
    }
    return string_buffer_end(b);
 fail:
    string_buffer_free(b);
    return JS_EXCEPTION;
}

static int js_string_normalize1(JSContext *ctx, uint32_t **pout_buf,
                                JSValueConst val,
                                UnicodeNormalizationEnum n_type)
{
    int buf_len, out_len;
    uint32_t *buf, *out_buf;

    buf_len = JS_ToUTF32String(ctx, &buf, val);
    if (buf_len < 0)
        return -1;
    out_len = unicode_normalize(&out_buf, buf, buf_len, n_type,
                                ctx->rt, (DynBufReallocFunc *)js_realloc_rt);
    js_free(ctx, buf);
    if (out_len < 0)
        return -1;
    *pout_buf = out_buf;
    return out_len;
}

static JSValue js_string_normalize(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    const char *form, *p;
    size_t form_len;
    int is_compat, out_len;
    UnicodeNormalizationEnum n_type;
    JSValue val;
    uint32_t *out_buf;

    val = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(val))
        return val;

    if (argc == 0 || JS_IsUndefined(argv[0])) {
        n_type = UNICODE_NFC;
    } else {
        form = JS_ToCStringLen(ctx, &form_len, argv[0]);
        if (!form)
            goto fail1;
        p = form;
        if (p[0] != 'N' || p[1] != 'F')
            goto bad_form;
        p += 2;
        is_compat = FALSE;
        if (*p == 'K') {
            is_compat = TRUE;
            p++;
        }
        if (*p == 'C' || *p == 'D') {
            n_type = UNICODE_NFC + is_compat * 2 + (*p - 'C');
            if ((p + 1 - form) != form_len)
                goto bad_form;
        } else {
        bad_form:
            JS_FreeCString(ctx, form);
            JS_ThrowRangeError(ctx, "bad normalization form");
        fail1:
            JS_FreeValue(ctx, val);
            return JS_EXCEPTION;
        }
        JS_FreeCString(ctx, form);
    }

    out_len = js_string_normalize1(ctx, &out_buf, val, n_type);
    JS_FreeValue(ctx, val);
    if (out_len < 0)
        return JS_EXCEPTION;
    val = JS_NewUTF32String(ctx, out_buf, out_len);
    js_free(ctx, out_buf);
    return val;
}

/* return < 0, 0 or > 0 */
static int js_UTF32_compare(const uint32_t *buf1, int buf1_len,
                            const uint32_t *buf2, int buf2_len)
{
    int i, len, c, res;
    len = min_int(buf1_len, buf2_len);
    for(i = 0; i < len; i++) {
        /* Note: range is limited so a subtraction is valid */
        c = buf1[i] - buf2[i];
        if (c != 0)
            return c;
    }
    if (buf1_len == buf2_len)
        res = 0;
    else if (buf1_len < buf2_len)
        res = -1;
    else
        res = 1;
    return res;
}

static JSValue js_string_localeCompare(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValue a, b;
    int cmp, a_len, b_len;
    uint32_t *a_buf, *b_buf;

    a = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(a))
        return JS_EXCEPTION;
    b = JS_ToString(ctx, argv[0]);
    if (JS_IsException(b)) {
        JS_FreeValue(ctx, a);
        return JS_EXCEPTION;
    }
    a_len = js_string_normalize1(ctx, &a_buf, a, UNICODE_NFC);
    JS_FreeValue(ctx, a);
    if (a_len < 0) {
        JS_FreeValue(ctx, b);
        return JS_EXCEPTION;
    }

    b_len = js_string_normalize1(ctx, &b_buf, b, UNICODE_NFC);
    JS_FreeValue(ctx, b);
    if (b_len < 0) {
        js_free(ctx, a_buf);
        return JS_EXCEPTION;
    }
    cmp = js_UTF32_compare(a_buf, a_len, b_buf, b_len);
    js_free(ctx, a_buf);
    js_free(ctx, b_buf);
    return JS_NewInt32(ctx, cmp);
}
#else /* CONFIG_ALL_UNICODE */
static JSValue js_string_localeCompare(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValue a, b;
    int cmp;

    a = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(a))
        return JS_EXCEPTION;
    b = JS_ToString(ctx, argv[0]);
    if (JS_IsException(b)) {
        JS_FreeValue(ctx, a);
        return JS_EXCEPTION;
    }
    cmp = js_string_compare(ctx, JS_VALUE_GET_STRING(a), JS_VALUE_GET_STRING(b));
    JS_FreeValue(ctx, a);
    JS_FreeValue(ctx, b);
    return JS_NewInt32(ctx, cmp);
}
#endif /* !CONFIG_ALL_UNICODE */

/* also used for String.prototype.valueOf */
static JSValue js_string_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    return js_thisStringValue(ctx, this_val);
}

/* String Iterator */

static JSValue js_string_iterator_next(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv,
                                       BOOL *pdone, int magic)
{
    JSArrayIteratorData *it;
    uint32_t idx, c, start;
    JSString *p;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_STRING_ITERATOR);
    if (!it) {
        *pdone = FALSE;
        return JS_EXCEPTION;
    }
    if (JS_IsUndefined(it->obj))
        goto done;
    p = JS_VALUE_GET_STRING(it->obj);
    idx = it->idx;
    if (idx >= p->len) {
        JS_FreeValue(ctx, it->obj);
        it->obj = JS_UNDEFINED;
    done:
        *pdone = TRUE;
        return JS_UNDEFINED;
    }

    start = idx;
    c = string_getc(p, (int *)&idx);
    it->idx = idx;
    *pdone = FALSE;
    if (c <= 0xffff) {
        return js_new_string_char(ctx, c);
    } else {
        return js_new_string16_len(ctx, p->u.str16 + start, 2);
    }
}

/* ES6 Annex B 2.3.2 etc. */
enum {
    magic_string_anchor,
    magic_string_big,
    magic_string_blink,
    magic_string_bold,
    magic_string_fixed,
    magic_string_fontcolor,
    magic_string_fontsize,
    magic_string_italics,
    magic_string_link,
    magic_string_small,
    magic_string_strike,
    magic_string_sub,
    magic_string_sup,
};

static JSValue js_string_CreateHTML(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv, int magic)
{
    JSValue str;
    const JSString *p;
    StringBuffer b_s, *b = &b_s;
    static struct { const char *tag, *attr; } const defs[] = {
        { "a", "name" }, { "big", NULL }, { "blink", NULL }, { "b", NULL },
        { "tt", NULL }, { "font", "color" }, { "font", "size" }, { "i", NULL },
        { "a", "href" }, { "small", NULL }, { "strike", NULL },
        { "sub", NULL }, { "sup", NULL },
    };

    str = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(str))
        return JS_EXCEPTION;
    string_buffer_init(ctx, b, 7);
    string_buffer_putc8(b, '<');
    string_buffer_puts8(b, defs[magic].tag);
    if (defs[magic].attr) {
        // r += " " + attr + "=\"" + value + "\"";
        JSValue value;
        int i;

        string_buffer_putc8(b, ' ');
        string_buffer_puts8(b, defs[magic].attr);
        string_buffer_puts8(b, "=\"");
        value = JS_ToStringCheckObject(ctx, argv[0]);
        if (JS_IsException(value)) {
            JS_FreeValue(ctx, str);
            string_buffer_free(b);
            return JS_EXCEPTION;
        }
        p = JS_VALUE_GET_STRING(value);
        for (i = 0; i < p->len; i++) {
            int c = string_get(p, i);
            if (c == '"') {
                string_buffer_puts8(b, "&quot;");
            } else {
                string_buffer_putc16(b, c);
            }
        }
        JS_FreeValue(ctx, value);
        string_buffer_putc8(b, '\"');
    }
    // return r + ">" + str + "</" + tag + ">";
    string_buffer_putc8(b, '>');
    string_buffer_concat_value_free(b, str);
    string_buffer_puts8(b, "</");
    string_buffer_puts8(b, defs[magic].tag);
    string_buffer_putc8(b, '>');
    return string_buffer_end(b);
}

const JSCFunctionListEntry js_string_funcs[] = {
    JS_CFUNC_DEF("fromCharCode", 1, js_string_fromCharCode ),
    JS_CFUNC_DEF("fromCodePoint", 1, js_string_fromCodePoint ),
    JS_CFUNC_DEF("raw", 1, js_string_raw ),
};

const JSCFunctionListEntry js_string_proto_funcs[] = {
    JS_PROP_INT32_DEF("length", 0, JS_PROP_CONFIGURABLE ),
    JS_CFUNC_MAGIC_DEF("at", 1, js_string_charAt, 1 ),
    JS_CFUNC_DEF("charCodeAt", 1, js_string_charCodeAt ),
    JS_CFUNC_MAGIC_DEF("charAt", 1, js_string_charAt, 0 ),
    JS_CFUNC_DEF("concat", 1, js_string_concat ),
    JS_CFUNC_DEF("codePointAt", 1, js_string_codePointAt ),
    JS_CFUNC_DEF("isWellFormed", 0, js_string_isWellFormed ),
    JS_CFUNC_DEF("toWellFormed", 0, js_string_toWellFormed ),
    JS_CFUNC_MAGIC_DEF("indexOf", 1, js_string_indexOf, 0 ),
    JS_CFUNC_MAGIC_DEF("lastIndexOf", 1, js_string_indexOf, 1 ),
    JS_CFUNC_MAGIC_DEF("includes", 1, js_string_includes, 0 ),
    JS_CFUNC_MAGIC_DEF("endsWith", 1, js_string_includes, 2 ),
    JS_CFUNC_MAGIC_DEF("startsWith", 1, js_string_includes, 1 ),
    JS_CFUNC_MAGIC_DEF("match", 1, js_string_match, JS_ATOM_Symbol_match ),
    JS_CFUNC_MAGIC_DEF("matchAll", 1, js_string_match, JS_ATOM_Symbol_matchAll ),
    JS_CFUNC_MAGIC_DEF("search", 1, js_string_match, JS_ATOM_Symbol_search ),
    JS_CFUNC_DEF("split", 2, js_string_split ),
    JS_CFUNC_DEF("substring", 2, js_string_substring ),
    JS_CFUNC_DEF("substr", 2, js_string_substr ),
    JS_CFUNC_DEF("slice", 2, js_string_slice ),
    JS_CFUNC_DEF("repeat", 1, js_string_repeat ),
    JS_CFUNC_MAGIC_DEF("replace", 2, js_string_replace, 0 ),
    JS_CFUNC_MAGIC_DEF("replaceAll", 2, js_string_replace, 1 ),
    JS_CFUNC_MAGIC_DEF("padEnd", 1, js_string_pad, 1 ),
    JS_CFUNC_MAGIC_DEF("padStart", 1, js_string_pad, 0 ),
    JS_CFUNC_MAGIC_DEF("trim", 0, js_string_trim, 3 ),
    JS_CFUNC_MAGIC_DEF("trimEnd", 0, js_string_trim, 2 ),
    JS_ALIAS_DEF("trimRight", "trimEnd" ),
    JS_CFUNC_MAGIC_DEF("trimStart", 0, js_string_trim, 1 ),
    JS_ALIAS_DEF("trimLeft", "trimStart" ),
    JS_CFUNC_DEF("toString", 0, js_string_toString ),
    JS_CFUNC_DEF("valueOf", 0, js_string_toString ),
    JS_CFUNC_MAGIC_DEF("toLowerCase", 0, js_string_toLowerCase, 1 ),
    JS_CFUNC_MAGIC_DEF("toUpperCase", 0, js_string_toLowerCase, 0 ),
    JS_CFUNC_MAGIC_DEF("toLocaleLowerCase", 0, js_string_toLowerCase, 1 ),
    JS_CFUNC_MAGIC_DEF("toLocaleUpperCase", 0, js_string_toLowerCase, 0 ),
    JS_CFUNC_MAGIC_DEF("[Symbol.iterator]", 0, js_create_array_iterator, JS_ITERATOR_KIND_VALUE | 4 ),
    /* ES6 Annex B 2.3.2 etc. */
    JS_CFUNC_MAGIC_DEF("anchor", 1, js_string_CreateHTML, magic_string_anchor ),
    JS_CFUNC_MAGIC_DEF("big", 0, js_string_CreateHTML, magic_string_big ),
    JS_CFUNC_MAGIC_DEF("blink", 0, js_string_CreateHTML, magic_string_blink ),
    JS_CFUNC_MAGIC_DEF("bold", 0, js_string_CreateHTML, magic_string_bold ),
    JS_CFUNC_MAGIC_DEF("fixed", 0, js_string_CreateHTML, magic_string_fixed ),
    JS_CFUNC_MAGIC_DEF("fontcolor", 1, js_string_CreateHTML, magic_string_fontcolor ),
    JS_CFUNC_MAGIC_DEF("fontsize", 1, js_string_CreateHTML, magic_string_fontsize ),
    JS_CFUNC_MAGIC_DEF("italics", 0, js_string_CreateHTML, magic_string_italics ),
    JS_CFUNC_MAGIC_DEF("link", 1, js_string_CreateHTML, magic_string_link ),
    JS_CFUNC_MAGIC_DEF("small", 0, js_string_CreateHTML, magic_string_small ),
    JS_CFUNC_MAGIC_DEF("strike", 0, js_string_CreateHTML, magic_string_strike ),
    JS_CFUNC_MAGIC_DEF("sub", 0, js_string_CreateHTML, magic_string_sub ),
    JS_CFUNC_MAGIC_DEF("sup", 0, js_string_CreateHTML, magic_string_sup ),
};

const JSCFunctionListEntry js_string_iterator_proto_funcs[] = {
    JS_ITERATOR_NEXT_DEF("next", 0, js_string_iterator_next, 0 ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "String Iterator", JS_PROP_CONFIGURABLE ),
};

static const JSCFunctionListEntry js_string_proto_normalize[] = {
#ifdef CONFIG_ALL_UNICODE
    JS_CFUNC_DEF("normalize", 0, js_string_normalize ),
#endif
    JS_CFUNC_DEF("localeCompare", 1, js_string_localeCompare ),
};

int JS_AddIntrinsicStringNormalize(JSContext *ctx)
{
    return JS_SetPropertyFunctionList(ctx, ctx->class_proto[JS_CLASS_STRING], js_string_proto_normalize,
                                      countof(js_string_proto_normalize));
}

/* Math */

/* precondition: a and b are not NaN */
static double js_fmin(double a, double b)
{
    if (a == 0 && b == 0) {
        JSFloat64Union a1, b1;
        a1.d = a;
        b1.d = b;
        a1.u64 |= b1.u64;
        return a1.d;
    } else {
        return fmin(a, b);
    }
}

/* precondition: a and b are not NaN */
static double js_fmax(double a, double b)
{
    if (a == 0 && b == 0) {
        JSFloat64Union a1, b1;
        a1.d = a;
        b1.d = b;
        a1.u64 &= b1.u64;
        return a1.d;
    } else {
        return fmax(a, b);
    }
}

static JSValue js_math_min_max(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic)
{
    BOOL is_max = magic;
    double r, a;
    int i;
    uint32_t tag;

    if (unlikely(argc == 0)) {
        return __JS_NewFloat64(ctx, is_max ? -1.0 / 0.0 : 1.0 / 0.0);
    }

    tag = JS_VALUE_GET_TAG(argv[0]);
    if (tag == JS_TAG_INT) {
        int a1, r1 = JS_VALUE_GET_INT(argv[0]);
        for(i = 1; i < argc; i++) {
            tag = JS_VALUE_GET_TAG(argv[i]);
            if (tag != JS_TAG_INT) {
                r = r1;
                goto generic_case;
            }
            a1 = JS_VALUE_GET_INT(argv[i]);
            if (is_max)
                r1 = max_int(r1, a1);
            else
                r1 = min_int(r1, a1);

        }
        return JS_NewInt32(ctx, r1);
    } else {
        if (JS_ToFloat64(ctx, &r, argv[0]))
            return JS_EXCEPTION;
        i = 1;
    generic_case:
        while (i < argc) {
            if (JS_ToFloat64(ctx, &a, argv[i]))
                return JS_EXCEPTION;
            if (!isnan(r)) {
                if (isnan(a)) {
                    r = a;
                } else {
                    if (is_max)
                        r = js_fmax(r, a);
                    else
                        r = js_fmin(r, a);
                }
            }
            i++;
        }
        return JS_NewFloat64(ctx, r);
    }
}

static double js_math_sign(double a)
{
    if (isnan(a) || a == 0.0)
        return a;
    if (a < 0)
        return -1;
    else
        return 1;
}

static double js_math_round(double a)
{
    JSFloat64Union u;
    uint64_t frac_mask, one;
    unsigned int e, s;

    u.d = a;
    e = (u.u64 >> 52) & 0x7ff;
    if (e < 1023) {
        /* abs(a) < 1 */
        if (e == (1023 - 1) && u.u64 != 0xbfe0000000000000) {
            /* abs(a) > 0.5 or a = 0.5: return +/-1.0 */
            u.u64 = (u.u64 & ((uint64_t)1 << 63)) | ((uint64_t)1023 << 52);
        } else {
            /* return +/-0.0 */
            u.u64 &= (uint64_t)1 << 63;
        }
    } else if (e < (1023 + 52)) {
        s = u.u64 >> 63;
        one = (uint64_t)1 << (52 - (e - 1023));
        frac_mask = one - 1;
        u.u64 += (one >> 1) - s;
        u.u64 &= ~frac_mask; /* truncate to an integer */
    }
    /* otherwise: abs(a) >= 2^52, or NaN, +/-Infinity: no change */
    return u.d;
}

static JSValue js_math_hypot(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    double r, a;
    int i;

    r = 0;
    if (argc > 0) {
        if (JS_ToFloat64(ctx, &r, argv[0]))
            return JS_EXCEPTION;
        if (argc == 1) {
            r = fabs(r);
        } else {
            /* use the built-in function to minimize precision loss */
            for (i = 1; i < argc; i++) {
                if (JS_ToFloat64(ctx, &a, argv[i]))
                    return JS_EXCEPTION;
                r = hypot(r, a);
            }
        }
    }
    return JS_NewFloat64(ctx, r);
}

static double js_math_f16round(double a)
{
    return fromfp16(tofp16(a));
}

static double js_math_fround(double a)
{
    return (float)a;
}

static JSValue js_math_imul(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    uint32_t a, b, c;
    int32_t d;

    if (JS_ToUint32(ctx, &a, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &b, argv[1]))
        return JS_EXCEPTION;
    c = a * b;
    memcpy(&d, &c, sizeof(d));
    return JS_NewInt32(ctx, d);
}

static JSValue js_math_clz32(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    uint32_t a, r;

    if (JS_ToUint32(ctx, &a, argv[0]))
        return JS_EXCEPTION;
    if (a == 0)
        r = 32;
    else
        r = clz32(a);
    return JS_NewInt32(ctx, r);
}

typedef enum {
    SUM_PRECISE_STATE_FINITE,
    SUM_PRECISE_STATE_INFINITY,
    SUM_PRECISE_STATE_MINUS_INFINITY, /* must be after SUM_PRECISE_STATE_INFINITY */
    SUM_PRECISE_STATE_NAN, /* must be after SUM_PRECISE_STATE_MINUS_INFINITY */
} SumPreciseStateEnum;

#define SP_LIMB_BITS 56
#define SP_RND_BITS (SP_LIMB_BITS - 53)
/* we add one extra limb to avoid having to test for overflows during the sum */
#define SUM_PRECISE_ACC_LEN 39

#define SUM_PRECISE_COUNTER_INIT 250

typedef struct {
    SumPreciseStateEnum state;
    uint32_t counter;
    int n_limbs; /* 'acc' contains n_limbs and is not necessarily
                    acc[n_limb - 1] may be 0. 0 indicates minus zero
                    result when state = SUM_PRECISE_STATE_FINITE */
    int64_t acc[SUM_PRECISE_ACC_LEN];
} SumPreciseState;

static void sum_precise_init(SumPreciseState *s)
{
    memset(s->acc, 0, sizeof(s->acc));
    s->state = SUM_PRECISE_STATE_FINITE;
    s->counter = SUM_PRECISE_COUNTER_INIT;
    s->n_limbs = 0;
}

static void sum_precise_renorm(SumPreciseState *s)
{
    int64_t v, carry;
    int i;
    
    carry = 0;
    for(i = 0; i < s->n_limbs; i++) {
        v = s->acc[i] + carry;
        s->acc[i] = v & (((uint64_t)1 << SP_LIMB_BITS) - 1);
        carry = v >> SP_LIMB_BITS;
    }
    /* we add a failsafe but it should be never reached in a
       reasonnable amount of time */
    if (carry != 0 && s->n_limbs < SUM_PRECISE_ACC_LEN)
        s->acc[s->n_limbs++] = carry;
}

static void sum_precise_add(SumPreciseState *s, double d)
{
    uint64_t a, m, a0, a1;
    int sgn, e, p;
    unsigned int shift;
    
    a = float64_as_uint64(d);
    sgn = a >> 63;
    e = (a >> 52) & ((1 << 11) - 1);
    m = a & (((uint64_t)1 << 52) - 1);
    if (unlikely(e == 2047)) {
        if (m == 0) {
            /* +/- infinity */
            if (s->state == SUM_PRECISE_STATE_NAN ||
                (s->state == SUM_PRECISE_STATE_MINUS_INFINITY && !sgn) ||
                (s->state == SUM_PRECISE_STATE_INFINITY && sgn)) {
                s->state = SUM_PRECISE_STATE_NAN;
            } else {
                s->state = SUM_PRECISE_STATE_INFINITY + sgn;
            }
        } else {
            /* NaN */
            s->state = SUM_PRECISE_STATE_NAN;
        }
    } else if (e == 0) {
        if (likely(m == 0)) {
            /* zero */
            if (s->n_limbs == 0 && !sgn)
                s->n_limbs = 1;
        } else {
            /* subnormal */
            p = 0;
            shift = 0;
            goto add;
        }
    } else {
        /* Note: we sum even if state != SUM_PRECISE_STATE_FINITE to
           avoid tests */
        m |= (uint64_t)1 << 52;
        shift = e - 1;
        /* 'p' is the position of a0 in acc. The division is normally
           implementation as a multiplication by the compiler. */
        p = shift / SP_LIMB_BITS;
        shift %= SP_LIMB_BITS;
    add:
        a0 = (m << shift) & (((uint64_t)1 << SP_LIMB_BITS) - 1);
        a1 = m >> (SP_LIMB_BITS - shift);
        if (!sgn) {
            s->acc[p] += a0;
            s->acc[p + 1] += a1;
        } else {
            s->acc[p] -= a0;
            s->acc[p + 1] -= a1;
        }
        s->n_limbs = max_int(s->n_limbs, p + 2);

        if (unlikely(--s->counter == 0)) {
            s->counter = SUM_PRECISE_COUNTER_INIT;
            sum_precise_renorm(s);
        }
    }
}

static double sum_precise_get_result(SumPreciseState *s)
{
    int n, shift, e, p, is_neg;
    uint64_t m, addend;
        
    if (s->state != SUM_PRECISE_STATE_FINITE) {
        switch(s->state) {
        default:
        case SUM_PRECISE_STATE_INFINITY:
            return INFINITY;
        case SUM_PRECISE_STATE_MINUS_INFINITY:
            return -INFINITY;
        case SUM_PRECISE_STATE_NAN:
            return NAN;
        }
    }

    sum_precise_renorm(s);

    /* extract the sign and absolute value */
#if 0
    {
        int i;
        printf("len=%d:", s->n_limbs);
        for(i = s->n_limbs - 1; i >= 0; i--)
            printf(" %014lx", s->acc[i]);
        printf("\n");
    }
#endif
    n = s->n_limbs;
    /* minus zero result */
    if (n == 0)
        return -0.0;
    
    /* normalize */
    while (n > 0 && s->acc[n - 1] == 0)
        n--;
    /* zero result. The spec tells it is always positive in the finite case */
    if (n == 0)
        return 0.0;
    is_neg = (s->acc[n - 1] < 0);
    if (is_neg) {
        uint64_t v, carry;
        int i;
        /* negate */
        /* XXX: do it only when needed */
        carry = 1;
        for(i = 0; i < n - 1; i++) {
            v = (((uint64_t)1 << SP_LIMB_BITS) - 1) - s->acc[i] + carry;
            carry = v >> SP_LIMB_BITS;
            s->acc[i] = v & (((uint64_t)1 << SP_LIMB_BITS) - 1);
        }
        s->acc[n - 1] = -s->acc[n - 1] + carry - 1;
        while (n > 1 && s->acc[n - 1] == 0)
            n--;
    }
    /* subnormal case */
    if (n == 1 && s->acc[0] < ((uint64_t)1 << 52))
        return uint64_as_float64(((uint64_t)is_neg << 63) | s->acc[0]); 
    /* normal case */
    e = n * SP_LIMB_BITS;
    p = n - 1;
    m = s->acc[p];
    shift = clz64(m) - (64 - SP_LIMB_BITS);
    e = e - shift - 52;
    if (shift != 0) {
        m <<= shift;
        if (p > 0) {
            int shift1;
            uint64_t nz;
            p--;
            shift1 = SP_LIMB_BITS - shift;
            nz = s->acc[p] & (((uint64_t)1 << shift1) - 1);
            m = m | (s->acc[p] >> shift1) | (nz != 0);
        }
    }
    if ((m & ((1 << SP_RND_BITS) - 1)) == (1 << (SP_RND_BITS - 1))) {
        /* see if the LSB part is non zero for the final rounding  */
        while (p > 0) {
            p--;
            if (s->acc[p] != 0) {
                m |= 1;
                break;
            }
        }
    }
    /* rounding to nearest with ties to even */
    addend = (1 << (SP_RND_BITS - 1)) - 1 + ((m >> SP_RND_BITS) & 1);
    m = (m + addend) >> SP_RND_BITS;
    /* handle overflow in the rounding */
    if (m == ((uint64_t)1 << 53))
        e++;
    if (unlikely(e >= 2047)) {
        /* infinity */
        return uint64_as_float64(((uint64_t)is_neg << 63) | ((uint64_t)2047 << 52));
    } else {
        m &= (((uint64_t)1 << 52) - 1);
        return uint64_as_float64(((uint64_t)is_neg << 63) | ((uint64_t)e << 52) | m);
    }
}

static JSValue js_math_sumPrecise(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValue iter, next, item, ret;
    uint32_t tag;
    int done;
    double d;
    SumPreciseState s_s, *s = &s_s;

    iter = JS_GetIterator(ctx, argv[0], FALSE);
    if (JS_IsException(iter))
        return JS_EXCEPTION;
    ret = JS_EXCEPTION;
    next = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next))
        goto fail;
    sum_precise_init(s);
    for (;;) {
        item = JS_IteratorNext(ctx, iter, next, 0, NULL, &done);
        if (JS_IsException(item))
            goto fail;
        if (done)
            break;
        tag = JS_VALUE_GET_TAG(item);
        if (JS_TAG_IS_FLOAT64(tag)) {
            d = JS_VALUE_GET_FLOAT64(item);
        } else if (tag == JS_TAG_INT) {
            d = JS_VALUE_GET_INT(item);
        } else {
            JS_FreeValue(ctx, item);
            JS_ThrowTypeError(ctx, "not a number");
            JS_IteratorClose(ctx, iter, TRUE);
            goto fail;
        }
        sum_precise_add(s, d);
    }
    ret = __JS_NewFloat64(ctx, sum_precise_get_result(s));
fail:
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, next);
    return ret;
}

/* xorshift* random number generator by Marsaglia */
static uint64_t xorshift64star(uint64_t *pstate)
{
    uint64_t x;
    x = *pstate;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *pstate = x;
    return x * 0x2545F4914F6CDD1D;
}

void js_random_init(JSContext *ctx)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ctx->random_state = ((int64_t)tv.tv_sec * 1000000) + tv.tv_usec;
    /* the state must be non zero */
    if (ctx->random_state == 0)
        ctx->random_state = 1;
}

static JSValue js_math_random(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSFloat64Union u;
    uint64_t v;

    v = xorshift64star(&ctx->random_state);
    /* 1.0 <= u.d < 2 */
    u.u64 = ((uint64_t)0x3ff << 52) | (v >> 12);
    return __JS_NewFloat64(ctx, u.d - 1.0);
}

static const JSCFunctionListEntry js_math_funcs[] = {
    JS_CFUNC_MAGIC_DEF("min", 2, js_math_min_max, 0 ),
    JS_CFUNC_MAGIC_DEF("max", 2, js_math_min_max, 1 ),
    JS_CFUNC_SPECIAL_DEF("abs", 1, f_f, fabs ),
    JS_CFUNC_SPECIAL_DEF("floor", 1, f_f, floor ),
    JS_CFUNC_SPECIAL_DEF("ceil", 1, f_f, ceil ),
    JS_CFUNC_SPECIAL_DEF("round", 1, f_f, js_math_round ),
    JS_CFUNC_SPECIAL_DEF("sqrt", 1, f_f, sqrt ),

    JS_CFUNC_SPECIAL_DEF("acos", 1, f_f, acos ),
    JS_CFUNC_SPECIAL_DEF("asin", 1, f_f, asin ),
    JS_CFUNC_SPECIAL_DEF("atan", 1, f_f, atan ),
    JS_CFUNC_SPECIAL_DEF("atan2", 2, f_f_f, atan2 ),
    JS_CFUNC_SPECIAL_DEF("cos", 1, f_f, cos ),
    JS_CFUNC_SPECIAL_DEF("exp", 1, f_f, exp ),
    JS_CFUNC_SPECIAL_DEF("log", 1, f_f, log ),
    JS_CFUNC_SPECIAL_DEF("pow", 2, f_f_f, js_pow ),
    JS_CFUNC_SPECIAL_DEF("sin", 1, f_f, sin ),
    JS_CFUNC_SPECIAL_DEF("tan", 1, f_f, tan ),
    /* ES6 */
    JS_CFUNC_SPECIAL_DEF("trunc", 1, f_f, trunc ),
    JS_CFUNC_SPECIAL_DEF("sign", 1, f_f, js_math_sign ),
    JS_CFUNC_SPECIAL_DEF("cosh", 1, f_f, cosh ),
    JS_CFUNC_SPECIAL_DEF("sinh", 1, f_f, sinh ),
    JS_CFUNC_SPECIAL_DEF("tanh", 1, f_f, tanh ),
    JS_CFUNC_SPECIAL_DEF("acosh", 1, f_f, acosh ),
    JS_CFUNC_SPECIAL_DEF("asinh", 1, f_f, asinh ),
    JS_CFUNC_SPECIAL_DEF("atanh", 1, f_f, atanh ),
    JS_CFUNC_SPECIAL_DEF("expm1", 1, f_f, expm1 ),
    JS_CFUNC_SPECIAL_DEF("log1p", 1, f_f, log1p ),
    JS_CFUNC_SPECIAL_DEF("log2", 1, f_f, log2 ),
    JS_CFUNC_SPECIAL_DEF("log10", 1, f_f, log10 ),
    JS_CFUNC_SPECIAL_DEF("cbrt", 1, f_f, cbrt ),
    JS_CFUNC_DEF("hypot", 2, js_math_hypot ),
    JS_CFUNC_DEF("random", 0, js_math_random ),
    JS_CFUNC_SPECIAL_DEF("f16round", 1, f_f, js_math_f16round ),
    JS_CFUNC_SPECIAL_DEF("fround", 1, f_f, js_math_fround ),
    JS_CFUNC_DEF("imul", 2, js_math_imul ),
    JS_CFUNC_DEF("clz32", 1, js_math_clz32 ),
    JS_CFUNC_DEF("sumPrecise", 1, js_math_sumPrecise ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Math", JS_PROP_CONFIGURABLE ),
    JS_PROP_DOUBLE_DEF("E", 2.718281828459045, 0 ),
    JS_PROP_DOUBLE_DEF("LN10", 2.302585092994046, 0 ),
    JS_PROP_DOUBLE_DEF("LN2", 0.6931471805599453, 0 ),
    JS_PROP_DOUBLE_DEF("LOG2E", 1.4426950408889634, 0 ),
    JS_PROP_DOUBLE_DEF("LOG10E", 0.4342944819032518, 0 ),
    JS_PROP_DOUBLE_DEF("PI", 3.141592653589793, 0 ),
    JS_PROP_DOUBLE_DEF("SQRT1_2", 0.7071067811865476, 0 ),
    JS_PROP_DOUBLE_DEF("SQRT2", 1.4142135623730951, 0 ),
};

const JSCFunctionListEntry js_math_obj[] = {
    JS_OBJECT_DEF("Math", js_math_funcs, countof(js_math_funcs), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE ),
};

