/*
 * QuickJS Javascript Engine: String, RegExp, and Symbol Builtins
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

#include "quickjs-internal.h"
#include "builtins/js_builtins.h"

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

static JSValue js_string_constructor(JSContext *ctx, JSValueConst new_target,
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

static int string_indexof_char(JSString *p, int c, int from)
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

static int64_t string_advance_index(JSString *p, int64_t index, BOOL unicode)
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
static int js_is_regexp(JSContext *ctx, JSValueConst obj);

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
static int js_string_GetSubstitution(JSContext *ctx,
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

static const JSCFunctionListEntry js_string_funcs[] = {
    JS_CFUNC_DEF("fromCharCode", 1, js_string_fromCharCode ),
    JS_CFUNC_DEF("fromCodePoint", 1, js_string_fromCodePoint ),
    JS_CFUNC_DEF("raw", 1, js_string_raw ),
};

static const JSCFunctionListEntry js_string_proto_funcs[] = {
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

static const JSCFunctionListEntry js_string_iterator_proto_funcs[] = {
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


/* RegExp */

void js_regexp_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSRegExp *re = &p->u.regexp;
    if (re->bytecode != NULL)
        JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_STRING, re->bytecode));
    if (re->pattern != NULL)
        JS_FreeValueRT(rt, JS_MKPTR(JS_TAG_STRING, re->pattern));
}

/* create a string containing the RegExp bytecode */
static JSValue js_compile_regexp(JSContext *ctx, JSValueConst pattern,
                                 JSValueConst flags)
{
    const char *str;
    int re_flags, mask;
    uint8_t *re_bytecode_buf;
    size_t i, len;
    int re_bytecode_len;
    JSValue ret;
    char error_msg[64];

    re_flags = 0;
    if (!JS_IsUndefined(flags)) {
        str = JS_ToCStringLen(ctx, &len, flags);
        if (!str)
            return JS_EXCEPTION;
        /* XXX: re_flags = LRE_FLAG_OCTAL unless strict mode? */
        for (i = 0; i < len; i++) {
            switch(str[i]) {
            case 'd':
                mask = LRE_FLAG_INDICES;
                break;
            case 'g':
                mask = LRE_FLAG_GLOBAL;
                break;
            case 'i':
                mask = LRE_FLAG_IGNORECASE;
                break;
            case 'm':
                mask = LRE_FLAG_MULTILINE;
                break;
            case 's':
                mask = LRE_FLAG_DOTALL;
                break;
            case 'u':
                mask = LRE_FLAG_UNICODE;
                break;
            case 'v':
                mask = LRE_FLAG_UNICODE_SETS;
                break;
            case 'y':
                mask = LRE_FLAG_STICKY;
                break;
            default:
                goto bad_flags;
            }
            if ((re_flags & mask) != 0) {
            bad_flags:
                JS_FreeCString(ctx, str);
                goto bad_flags1;
            }
            re_flags |= mask;
        }
        JS_FreeCString(ctx, str);
    }

    /* 'u' and 'v' cannot be both set */
    if ((re_flags & LRE_FLAG_UNICODE_SETS) && (re_flags & LRE_FLAG_UNICODE)) {
    bad_flags1:
        return JS_ThrowSyntaxError(ctx, "invalid regular expression flags");
    }
    
    str = JS_ToCStringLen2(ctx, &len, pattern, !(re_flags & (LRE_FLAG_UNICODE | LRE_FLAG_UNICODE_SETS)));
    if (!str)
        return JS_EXCEPTION;
    re_bytecode_buf = lre_compile(&re_bytecode_len, error_msg,
                                  sizeof(error_msg), str, len, re_flags, ctx);
    JS_FreeCString(ctx, str);
    if (!re_bytecode_buf) {
        JS_ThrowSyntaxError(ctx, "%s", error_msg);
        return JS_EXCEPTION;
    }

    ret = js_new_string8_len(ctx, (const char *)re_bytecode_buf, re_bytecode_len);
    js_free(ctx, re_bytecode_buf);
    return ret;
}

/* fast regexp creation */
JSValue JS_NewRegexp(JSContext *ctx, JSValue pattern, JSValue bc)
{
    JSValue obj;
    JSProperty props[1];
    JSObject *p;
    JSRegExp *re;

    /* sanity check */
    if (unlikely(JS_VALUE_GET_TAG(bc) != JS_TAG_STRING ||
                 JS_VALUE_GET_TAG(pattern) != JS_TAG_STRING)) {
        JS_ThrowTypeError(ctx, "string expected");
        goto fail;
    }
    props[0].u.value = JS_NewInt32(ctx, 0); /* lastIndex */
    obj = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->regexp_shape), JS_CLASS_REGEXP, props);
    if (JS_IsException(obj))
        goto fail;
    p = JS_VALUE_GET_OBJ(obj);
    re = &p->u.regexp;
    re->pattern = JS_VALUE_GET_STRING(pattern);
    re->bytecode = JS_VALUE_GET_STRING(bc);
    return obj;
 fail:
    JS_FreeValue(ctx, bc);
    JS_FreeValue(ctx, pattern);
    return JS_EXCEPTION;
}

/* set the RegExp fields */
static JSValue js_regexp_set_internal(JSContext *ctx,
                                      JSValue obj,
                                      JSValue pattern, JSValue bc)
{
    JSObject *p;
    JSRegExp *re;

    /* sanity check */
    if (unlikely(JS_VALUE_GET_TAG(bc) != JS_TAG_STRING ||
                 JS_VALUE_GET_TAG(pattern) != JS_TAG_STRING)) {
        JS_ThrowTypeError(ctx, "string expected");
        JS_FreeValue(ctx, obj);
        JS_FreeValue(ctx, bc);
        JS_FreeValue(ctx, pattern);
        return JS_EXCEPTION;
    }

    p = JS_VALUE_GET_OBJ(obj);
    re = &p->u.regexp;
    re->pattern = JS_VALUE_GET_STRING(pattern);
    re->bytecode = JS_VALUE_GET_STRING(bc);
    /* Note: cannot fail because the field is preallocated */
    JS_DefinePropertyValue(ctx, obj, JS_ATOM_lastIndex, JS_NewInt32(ctx, 0),
                           JS_PROP_WRITABLE);
    return obj;
}

static JSRegExp *js_get_regexp(JSContext *ctx, JSValueConst obj, BOOL throw_error)
{
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(obj);
        if (p->class_id == JS_CLASS_REGEXP)
            return &p->u.regexp;
    }
    if (throw_error) {
        JS_ThrowTypeErrorInvalidClass(ctx, JS_CLASS_REGEXP);
    }
    return NULL;
}

/* return < 0 if exception or TRUE/FALSE */
static int js_is_regexp(JSContext *ctx, JSValueConst obj)
{
    JSValue m;

    if (!JS_IsObject(obj))
        return FALSE;
    m = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_match);
    if (JS_IsException(m))
        return -1;
    if (!JS_IsUndefined(m))
        return JS_ToBoolFree(ctx, m);
    return js_get_regexp(ctx, obj, FALSE) != NULL;
}

static JSValue js_regexp_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValue pattern, flags, bc, val, obj = JS_UNDEFINED;
    JSValueConst pat, flags1;
    JSRegExp *re;
    int pat_is_regexp;

    pat = argv[0];
    flags1 = argv[1];
    pat_is_regexp = js_is_regexp(ctx, pat);
    if (pat_is_regexp < 0)
        return JS_EXCEPTION;
    if (JS_IsUndefined(new_target)) {
        /* called as a function */
        new_target = JS_GetActiveFunction(ctx);
        if (pat_is_regexp && JS_IsUndefined(flags1)) {
            JSValue ctor;
            BOOL res;
            ctor = JS_GetProperty(ctx, pat, JS_ATOM_constructor);
            if (JS_IsException(ctor))
                return ctor;
            res = js_same_value(ctx, ctor, new_target);
            JS_FreeValue(ctx, ctor);
            if (res)
                return JS_DupValue(ctx, pat);
        }
    }
    re = js_get_regexp(ctx, pat, FALSE);
    flags = JS_UNDEFINED;
    if (re) {
        pattern = JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, re->pattern));
        if (JS_IsUndefined(flags1)) {
            bc = JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, re->bytecode));
            obj = js_create_from_ctor(ctx, new_target, JS_CLASS_REGEXP);
            if (JS_IsException(obj))
                goto fail;
            goto no_compilation;
        } else {
            flags = JS_DupValue(ctx, flags1);
        }
    } else {
        if (pat_is_regexp) {
            pattern = JS_GetProperty(ctx, pat, JS_ATOM_source);
            if (JS_IsException(pattern))
                goto fail;
            if (JS_IsUndefined(flags1)) {
                flags = JS_GetProperty(ctx, pat, JS_ATOM_flags);
                if (JS_IsException(flags))
                    goto fail;
            } else {
                flags = JS_DupValue(ctx, flags1);
            }
        } else {
            pattern = JS_DupValue(ctx, pat);
            flags = JS_DupValue(ctx, flags1);
        }
        if (JS_IsUndefined(pattern)) {
            pattern = JS_AtomToString(ctx, JS_ATOM_empty_string);
        } else {
            val = pattern;
            pattern = JS_ToString(ctx, val);
            JS_FreeValue(ctx, val);
            if (JS_IsException(pattern))
                goto fail;
        }
    }
    obj = js_create_from_ctor(ctx, new_target, JS_CLASS_REGEXP);
    if (JS_IsException(obj))
        goto fail;
    bc = js_compile_regexp(ctx, pattern, flags);
    if (JS_IsException(bc))
        goto fail;
    JS_FreeValue(ctx, flags);
 no_compilation:
    return js_regexp_set_internal(ctx, obj, pattern, bc);
 fail:
    JS_FreeValue(ctx, pattern);
    JS_FreeValue(ctx, flags);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_regexp_compile(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSRegExp *re1, *re;
    JSValueConst pattern1, flags1;
    JSValue bc, pattern;

    re = js_get_regexp(ctx, this_val, TRUE);
    if (!re)
        return JS_EXCEPTION;
    pattern1 = argv[0];
    flags1 = argv[1];
    re1 = js_get_regexp(ctx, pattern1, FALSE);
    if (re1) {
        if (!JS_IsUndefined(flags1))
            return JS_ThrowTypeError(ctx, "flags must be undefined");
        pattern = JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, re1->pattern));
        bc = JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, re1->bytecode));
    } else {
        bc = JS_UNDEFINED;
        if (JS_IsUndefined(pattern1))
            pattern = JS_AtomToString(ctx, JS_ATOM_empty_string);
        else
            pattern = JS_ToString(ctx, pattern1);
        if (JS_IsException(pattern))
            goto fail;
        bc = js_compile_regexp(ctx, pattern, flags1);
        if (JS_IsException(bc))
            goto fail;
    }
    JS_FreeValue(ctx, JS_MKPTR(JS_TAG_STRING, re->pattern));
    JS_FreeValue(ctx, JS_MKPTR(JS_TAG_STRING, re->bytecode));
    re->pattern = JS_VALUE_GET_STRING(pattern);
    re->bytecode = JS_VALUE_GET_STRING(bc);
    if (JS_SetProperty(ctx, this_val, JS_ATOM_lastIndex,
                       JS_NewInt32(ctx, 0)) < 0)
        return JS_EXCEPTION;
    return JS_DupValue(ctx, this_val);
 fail:
    JS_FreeValue(ctx, pattern);
    JS_FreeValue(ctx, bc);
    return JS_EXCEPTION;
}

static JSValue js_regexp_get_source(JSContext *ctx, JSValueConst this_val)
{
    JSRegExp *re;
    JSString *p;
    StringBuffer b_s, *b = &b_s;
    int i, n, c, c2, bra;

    if (JS_VALUE_GET_TAG(this_val) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);

    if (js_same_value(ctx, this_val, ctx->class_proto[JS_CLASS_REGEXP]))
        goto empty_regex;

    re = js_get_regexp(ctx, this_val, TRUE);
    if (!re)
        return JS_EXCEPTION;

    p = re->pattern;

    if (p->len == 0) {
    empty_regex:
        return js_new_string8(ctx, "(?:)");
    }
    string_buffer_init2(ctx, b, p->len, p->is_wide_char);

    /* Escape '/' and newline sequences as needed */
    bra = 0;
    for (i = 0, n = p->len; i < n;) {
        c2 = -1;
        switch (c = string_get(p, i++)) {
        case '\\':
            if (i < n)
                c2 = string_get(p, i++);
            break;
        case ']':
            bra = 0;
            break;
        case '[':
            if (!bra) {
                if (i < n && string_get(p, i) == ']')
                    c2 = string_get(p, i++);
                bra = 1;
            }
            break;
        case '\n':
            c = '\\';
            c2 = 'n';
            break;
        case '\r':
            c = '\\';
            c2 = 'r';
            break;
        case '/':
            if (!bra) {
                c = '\\';
                c2 = '/';
            }
            break;
        }
        string_buffer_putc16(b, c);
        if (c2 >= 0)
            string_buffer_putc16(b, c2);
    }
    return string_buffer_end(b);
}

static JSValue js_regexp_get_flag(JSContext *ctx, JSValueConst this_val, int mask)
{
    JSRegExp *re;
    int flags;

    if (JS_VALUE_GET_TAG(this_val) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);

    re = js_get_regexp(ctx, this_val, FALSE);
    if (!re) {
        if (js_same_value(ctx, this_val, ctx->class_proto[JS_CLASS_REGEXP]))
            return JS_UNDEFINED;
        else
            return JS_ThrowTypeErrorInvalidClass(ctx, JS_CLASS_REGEXP);
    }

    flags = lre_get_flags(re->bytecode->u.str8);
    return JS_NewBool(ctx, flags & mask);
}

#define RE_FLAG_COUNT 8

static JSValue js_regexp_get_flags(JSContext *ctx, JSValueConst this_val)
{
    char str[RE_FLAG_COUNT], *p = str;
    int res, i;
    static const int flag_atom[RE_FLAG_COUNT] = {
        JS_ATOM_hasIndices,
        JS_ATOM_global,
        JS_ATOM_ignoreCase,
        JS_ATOM_multiline,
        JS_ATOM_dotAll,
        JS_ATOM_unicode,
        JS_ATOM_unicodeSets,
        JS_ATOM_sticky,
    };
    static const char flag_char[RE_FLAG_COUNT] = { 'd', 'g', 'i', 'm', 's', 'u', 'v', 'y' };
    
    if (JS_VALUE_GET_TAG(this_val) != JS_TAG_OBJECT)
        return JS_ThrowTypeErrorNotAnObject(ctx);

    for(i = 0; i < RE_FLAG_COUNT; i++) {
        res = JS_ToBoolFree(ctx, JS_GetProperty(ctx, this_val, flag_atom[i]));
        if (res < 0)
            goto exception;
        if (res)
            *p++ = flag_char[i];
    }
    return JS_NewStringLen(ctx, str, p - str);

exception:
    return JS_EXCEPTION;
}

static JSValue js_regexp_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValue pattern, flags;
    StringBuffer b_s, *b = &b_s;

    if (!JS_IsObject(this_val))
        return JS_ThrowTypeErrorNotAnObject(ctx);

    string_buffer_init(ctx, b, 0);
    string_buffer_putc8(b, '/');
    pattern = JS_GetProperty(ctx, this_val, JS_ATOM_source);
    if (string_buffer_concat_value_free(b, pattern))
        goto fail;
    string_buffer_putc8(b, '/');
    flags = JS_GetProperty(ctx, this_val, JS_ATOM_flags);
    if (string_buffer_concat_value_free(b, flags))
        goto fail;
    return string_buffer_end(b);

fail:
    string_buffer_free(b);
    return JS_EXCEPTION;
}

int lre_check_stack_overflow(void *opaque, size_t alloca_size)
{
    JSContext *ctx = opaque;
    return js_check_stack_overflow(ctx->rt, alloca_size);
}

int lre_check_timeout(void *opaque)
{
    JSContext *ctx = opaque;
    JSRuntime *rt = ctx->rt;
    return (rt->interrupt_handler && 
            rt->interrupt_handler(rt, rt->interrupt_opaque));
}

void *lre_realloc(void *opaque, void *ptr, size_t size)
{
    JSContext *ctx = opaque;
    /* No JS exception is raised here */
    return js_realloc_rt(ctx->rt, ptr, size);
}

static JSValue js_regexp_escape(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue str;
    StringBuffer b_s, *b = &b_s;
    JSString *p;
    uint32_t c;
    char s[16];
    int i, i0;
    
    if (!JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "not a string");
    str = JS_ToString(ctx, argv[0]); /* must call it to linearlize ropes */
    if (JS_IsException(str))
        return JS_EXCEPTION;
    p = JS_VALUE_GET_STRING(str);
    string_buffer_init2(ctx, b, 0, p->is_wide_char);
    for (i = 0; i < p->len; ) {
        i0 = i;
        c = string_getc(p, &i);
        if (c < 33) {
            if (c >= 9 && c <= 13) {
                string_buffer_putc8(b, '\\');
                string_buffer_putc8(b, "tnvfr"[c - 9]);
            } else {
                goto hex2;
            }
        } else if (c < 128) {
            if ((c >= '0' && c <= '9')
             || (c >= 'A' && c <= 'Z')
             || (c >= 'a' && c <= 'z')) {
                if (i0 == 0)
                    goto hex2;
            } else if (strchr(",-=<>#&!%:;@~'`\"", c)) {
                goto hex2;
            } else if (c != '_') {
                string_buffer_putc8(b, '\\');
            }
            string_buffer_putc8(b, c);
        } else if (c < 256) {
        hex2:
            snprintf(s, sizeof(s), "\\x%02x", c);
            string_buffer_puts8(b, s);
        } else if (is_surrogate(c) || lre_is_space(c)) {
            snprintf(s, sizeof(s), "\\u%04x", c);
            string_buffer_puts8(b, s);
        } else {
            string_buffer_putc(b, c);
        }
    }
    JS_FreeValue(ctx, str);
    return string_buffer_end(b);
}

/* this_val must be of JS_CLASS_REGEXP */
static force_inline int js_regexp_get_lastIndex(JSContext *ctx, int64_t *plast_index,
                                                JSValueConst this_val)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    
    /* lastIndex is always the first property (it is not configurable) */
    if (likely(JS_VALUE_GET_TAG(p->prop[0].u.value) == JS_TAG_INT)) {
        *plast_index = max_int(JS_VALUE_GET_INT(p->prop[0].u.value), 0);
        return 0;
    } else {
        return JS_ToLengthFree(ctx, plast_index, JS_DupValue(ctx, p->prop[0].u.value));
    }
}

/* this_val must be of JS_CLASS_REGEXP */
static force_inline int js_regexp_set_lastIndex(JSContext *ctx, JSValueConst this_val,
                                                int last_index)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    
    /* lastIndex is always the first property (it is not configurable) */
    if (likely(JS_VALUE_GET_TAG(p->prop[0].u.value) == JS_TAG_INT &&
               (get_shape_prop(p->shape)->flags & JS_PROP_WRITABLE))) {
        set_value(ctx, &p->prop[0].u.value, JS_NewInt32(ctx, last_index));
    } else {
        if (JS_SetProperty(ctx, this_val, JS_ATOM_lastIndex,
                           JS_NewInt32(ctx, last_index)) < 0)
            return -1;
    }
    return 0;
}

static JSValue js_regexp_exec(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSRegExp *re = js_get_regexp(ctx, this_val, TRUE);
    JSString *str;
    JSValue t, ret, str_val, obj, groups;
    JSValue indices, indices_groups;
    uint8_t *re_bytecode;
    uint8_t **capture, *str_buf;
    int rc, capture_count, shift, i, re_flags, alloc_count;
    int64_t last_index;
    const char *group_name_ptr;
    JSObject *p_obj;
    JSAtom group_name;
    
    if (!re)
        return JS_EXCEPTION;

    str_val = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str_val))
        return JS_EXCEPTION;

    ret = JS_EXCEPTION;
    obj = JS_NULL;
    groups = JS_UNDEFINED;
    indices = JS_UNDEFINED;
    indices_groups = JS_UNDEFINED;
    capture = NULL;
    group_name = JS_ATOM_NULL;
    
    if (js_regexp_get_lastIndex(ctx, &last_index, this_val))
        goto fail;

    re_bytecode = re->bytecode->u.str8;
    re_flags = lre_get_flags(re_bytecode);
    if ((re_flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) == 0) {
        last_index = 0;
    }
    str = JS_VALUE_GET_STRING(str_val);
    alloc_count = lre_get_alloc_count(re_bytecode);
    if (alloc_count > 0) {
        capture = js_malloc(ctx, sizeof(capture[0]) * alloc_count);
        if (!capture)
            goto fail;
    }
    capture_count = lre_get_capture_count(re_bytecode);
    shift = str->is_wide_char;
    str_buf = str->u.str8;
    if (last_index > str->len) {
        rc = 2;
    } else {
        rc = lre_exec(capture, re_bytecode,
                      str_buf, last_index, str->len,
                      shift, ctx);
    }
    if (rc != 1) {
        if (rc >= 0) {
            if (rc == 2 || (re_flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY))) {
                if (js_regexp_set_lastIndex(ctx, this_val, 0) < 0)
                    goto fail;
            }
        } else {
            if (rc == LRE_RET_TIMEOUT) {
                JS_ThrowInterrupted(ctx);
            } else {
                JS_ThrowInternalError(ctx, "out of memory in regexp execution");
            }
            goto fail;
        }
    } else {
        int prop_flags;
        JSProperty props[4];
        
        if (re_flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) {
            if (js_regexp_set_lastIndex(ctx, this_val,
                                        (capture[1] - str_buf) >> shift) < 0)
                goto fail;
        }
        prop_flags = JS_PROP_C_W_E | JS_PROP_THROW;
        group_name_ptr = lre_get_groupnames(re_bytecode);
        if (group_name_ptr) {
            groups = JS_NewObjectProto(ctx, JS_NULL);
            if (JS_IsException(groups))
                goto fail;
        }
        if (re_flags & LRE_FLAG_INDICES) {
            indices = JS_NewArray(ctx);
            if (JS_IsException(indices))
                goto fail;
            if (group_name_ptr) {
                indices_groups = JS_NewObjectProto(ctx, JS_NULL);
                if (JS_IsException(indices_groups))
                    goto fail;
            }
        }

        props[0].u.value = JS_NewInt32(ctx, capture_count); /* length */
        props[1].u.value = JS_NewInt32(ctx, (capture[0] - str_buf) >> shift); /* index */
        props[2].u.value = str_val; /* input */
        props[3].u.value = JS_DupValue(ctx, groups); /* groups */

        str_val = JS_UNDEFINED;
        obj = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->regexp_result_shape),
                                    JS_CLASS_ARRAY, props);
        if (JS_IsException(obj))
            goto fail;

        p_obj = JS_VALUE_GET_OBJ(obj);
        if (expand_fast_array(ctx, p_obj, capture_count))
            goto fail;
        
        for(i = 0; i < capture_count; i++) {
            uint8_t **match = &capture[2 * i];
            int start = -1;
            int end = -1;
            JSValue val;

            if (group_name_ptr && i > 0) {
                if (*group_name_ptr) {
                    /* XXX: slow, should create a shape when the regexp is
                       compiled */
                    group_name = JS_NewAtom(ctx, group_name_ptr);
                    if (group_name == JS_ATOM_NULL)
                        goto fail;
                }
                group_name_ptr += strlen(group_name_ptr) + LRE_GROUP_NAME_TRAILER_LEN;
            }

            if (match[0] && match[1]) {
                start = (match[0] - str_buf) >> shift;
                end = (match[1] - str_buf) >> shift;
            }

            if (!JS_IsUndefined(indices)) {
                val = JS_UNDEFINED;
                if (start != -1) {
                    val = JS_NewArray(ctx);
                    if (JS_IsException(val))
                        goto fail;
                    if (JS_DefinePropertyValueUint32(ctx, val, 0,
                                                     JS_NewInt32(ctx, start),
                                                     prop_flags) < 0) {
                        JS_FreeValue(ctx, val);
                        goto fail;
                    }
                    if (JS_DefinePropertyValueUint32(ctx, val, 1,
                                                     JS_NewInt32(ctx, end),
                                                     prop_flags) < 0) {
                        JS_FreeValue(ctx, val);
                        goto fail;
                    }
                }
                if (group_name != JS_ATOM_NULL) {
                    /* JS_HasProperty() cannot fail here */
                    if (!JS_IsUndefined(val) ||
                        !JS_HasProperty(ctx, indices_groups, group_name)) {
                        if (JS_DefinePropertyValue(ctx, indices_groups,
                                                   group_name, JS_DupValue(ctx, val), prop_flags) < 0) {
                            JS_FreeValue(ctx, val);
                            goto fail;
                        }
                    }
                }
                if (JS_DefinePropertyValueUint32(ctx, indices, i, val,
                                                 prop_flags) < 0) {
                    goto fail;
                }
            }

            val = JS_UNDEFINED;
            if (start != -1) {
                val = js_sub_string(ctx, str, start, end);
                if (JS_IsException(val))
                    goto fail;
            }

            if (group_name != JS_ATOM_NULL) {
                /* JS_HasProperty() cannot fail here */
                if (!JS_IsUndefined(val) ||
                    !JS_HasProperty(ctx, groups, group_name)) {
                    if (JS_DefinePropertyValue(ctx, groups, group_name,
                                               JS_DupValue(ctx, val),
                                               prop_flags) < 0) {
                        JS_FreeValue(ctx, val);
                        goto fail;
                    }
                }
                JS_FreeAtom(ctx, group_name);
                group_name = JS_ATOM_NULL;
            }
            p_obj->u.array.u.values[p_obj->u.array.count++] = val;
        }

        if (!JS_IsUndefined(indices)) {
            t = indices_groups, indices_groups = JS_UNDEFINED;
            if (JS_DefinePropertyValue(ctx, indices, JS_ATOM_groups,
                                       t, prop_flags) < 0) {
                goto fail;
            }
            t = indices, indices = JS_UNDEFINED;
            if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_indices,
                                       t, prop_flags) < 0) {
                goto fail;
            }
        }
    }
    ret = obj;
    obj = JS_UNDEFINED;
fail:
    JS_FreeAtom(ctx, group_name);
    JS_FreeValue(ctx, indices_groups);
    JS_FreeValue(ctx, indices);
    JS_FreeValue(ctx, str_val);
    JS_FreeValue(ctx, groups);
    JS_FreeValue(ctx, obj);
    js_free(ctx, capture);
    return ret;
}

/* XXX: add group names support */
static JSValue js_regexp_replace(JSContext *ctx, JSValueConst this_val, JSValueConst arg,
                                 JSValueConst rep_val)
{
    JSRegExp *re = js_get_regexp(ctx, this_val, TRUE);
    JSString *str;
    JSValue str_val;
    uint8_t *re_bytecode;
    int ret;
    uint8_t **capture, *str_buf;
    int capture_count, alloc_count, shift, re_flags;
    int next_src_pos, start, end;
    int64_t last_index;
    StringBuffer b_s, *b = &b_s;
    JSString *rp = JS_VALUE_GET_STRING(rep_val);
    const char *group_name_ptr;
    BOOL fullUnicode;
    
    if (!re)
        return JS_EXCEPTION;
    re_bytecode = re->bytecode->u.str8;
    group_name_ptr = lre_get_groupnames(re_bytecode);
    if (group_name_ptr)
        return JS_UNDEFINED; /* group names are not supported yet */
    
    string_buffer_init(ctx, b, 0);

    capture = NULL;
    str_val = JS_ToString(ctx, arg);
    if (JS_IsException(str_val))
        goto fail;
    str = JS_VALUE_GET_STRING(str_val);
    re_flags = lre_get_flags(re_bytecode);

    if (re_flags & LRE_FLAG_GLOBAL) {
        if (js_regexp_set_lastIndex(ctx, this_val, 0))
            goto fail;
    }
    if ((re_flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) == 0) {
        last_index = 0;
    } else {
        if (js_regexp_get_lastIndex(ctx, &last_index, this_val))
            goto fail;
    }
    alloc_count = lre_get_alloc_count(re_bytecode);
    if (alloc_count > 0) {
        capture = js_malloc(ctx, sizeof(capture[0]) * alloc_count);
        if (!capture)
            goto fail;
    }
    capture_count = lre_get_capture_count(re_bytecode);
    fullUnicode = ((re_flags & (LRE_FLAG_UNICODE | LRE_FLAG_UNICODE_SETS)) != 0);
    shift = str->is_wide_char;
    str_buf = str->u.str8;
    next_src_pos = 0;
    for (;;) {
        if (last_index > str->len) {
            ret = 0;
        } else {
            ret = lre_exec(capture, re_bytecode,
                           str_buf, last_index, str->len, shift, ctx);
        }
        if (ret != 1) {
            if (ret >= 0) {
                if (ret == 2 || (re_flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY))) {
                    if (js_regexp_set_lastIndex(ctx, this_val, 0) < 0)
                        goto fail;
                }
            } else {
                if (ret == LRE_RET_TIMEOUT) {
                    JS_ThrowInterrupted(ctx);
                } else {
                    JS_ThrowInternalError(ctx, "out of memory in regexp execution");
                }
                goto fail;
            }
            break;
        }
        start = (capture[0] - str_buf) >> shift;
        end = (capture[1] - str_buf) >> shift;
        last_index = end;
        if (next_src_pos < start) {
            if (string_buffer_concat(b, str, next_src_pos, start))
                goto fail;
        }
        if (rp->len != 0) {
            if (js_string_GetSubstitution(ctx, b, JS_UNDEFINED, str, start,
                                          JS_UNDEFINED, JS_UNDEFINED, rep_val,
                                          capture, capture_count)) {
                goto fail;
            }
        }
        next_src_pos = end;
        if (!(re_flags & LRE_FLAG_GLOBAL)) {
            if (re_flags & LRE_FLAG_STICKY) {
                if (js_regexp_set_lastIndex(ctx, this_val, end) < 0)
                    goto fail;
            }
            break;
        }
        if (end == start) {
            end = string_advance_index(str, end, fullUnicode);
        }
        last_index = end;
    }
    if (string_buffer_concat(b, str, next_src_pos, str->len))
        goto fail;
    JS_FreeValue(ctx, str_val);
    js_free(ctx, capture);
    return string_buffer_end(b);
fail:
    JS_FreeValue(ctx, str_val);
    js_free(ctx, capture);
    string_buffer_free(b);
    return JS_EXCEPTION;
}

static JSValue JS_RegExpExec(JSContext *ctx, JSValueConst r, JSValueConst s)
{
    JSValue method, ret;

    method = JS_GetProperty(ctx, r, JS_ATOM_exec);
    if (JS_IsException(method))
        return method;
    if (JS_IsFunction(ctx, method)) {
        ret = JS_CallFree(ctx, method, r, 1, &s);
        if (JS_IsException(ret))
            return ret;
        if (!JS_IsObject(ret) && !JS_IsNull(ret)) {
            JS_FreeValue(ctx, ret);
            return JS_ThrowTypeError(ctx, "RegExp exec method must return an object or null");
        }
        return ret;
    }
    JS_FreeValue(ctx, method);
    return js_regexp_exec(ctx, r, 1, &s);
}

static JSValue js_regexp_test(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValue val;
    BOOL ret;

    val = JS_RegExpExec(ctx, this_val, argv[0]);
    if (JS_IsException(val))
        return JS_EXCEPTION;
    ret = !JS_IsNull(val);
    JS_FreeValue(ctx, val);
    return JS_NewBool(ctx, ret);
}

static JSValue js_regexp_Symbol_match(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    // [Symbol.match](str)
    JSValueConst rx = this_val;
    JSValue A, S, flags, result, matchStr;
    int global, n, fullUnicode, isEmpty;
    JSString *p;

    if (!JS_IsObject(rx))
        return JS_ThrowTypeErrorNotAnObject(ctx);

    A = JS_UNDEFINED;
    flags = JS_UNDEFINED;
    result = JS_UNDEFINED;
    matchStr = JS_UNDEFINED;
    S = JS_ToString(ctx, argv[0]);
    if (JS_IsException(S))
        goto exception;

    flags = JS_GetProperty(ctx, rx, JS_ATOM_flags);
    if (JS_IsException(flags))
        goto exception;
    flags = JS_ToStringFree(ctx, flags);
    if (JS_IsException(flags))
        goto exception;
    p = JS_VALUE_GET_STRING(flags);

    global = (-1 != string_indexof_char(p, 'g', 0));
    if (!global) {
        A = JS_RegExpExec(ctx, rx, S);
    } else {
        fullUnicode = (string_indexof_char(p, 'u', 0) >= 0 ||
                       string_indexof_char(p, 'v', 0) >= 0);

        if (JS_SetProperty(ctx, rx, JS_ATOM_lastIndex, JS_NewInt32(ctx, 0)) < 0)
            goto exception;
        A = JS_NewArray(ctx);
        if (JS_IsException(A))
            goto exception;
        n = 0;
        for(;;) {
            JS_FreeValue(ctx, result);
            result = JS_RegExpExec(ctx, rx, S);
            if (JS_IsException(result))
                goto exception;
            if (JS_IsNull(result))
                break;
            matchStr = JS_ToStringFree(ctx, JS_GetPropertyInt64(ctx, result, 0));
            if (JS_IsException(matchStr))
                goto exception;
            isEmpty = JS_IsEmptyString(matchStr);
            if (JS_DefinePropertyValueInt64(ctx, A, n++, matchStr, JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                goto exception;
            if (isEmpty) {
                int64_t thisIndex, nextIndex;
                if (JS_ToLengthFree(ctx, &thisIndex,
                                    JS_GetProperty(ctx, rx, JS_ATOM_lastIndex)) < 0)
                    goto exception;
                p = JS_VALUE_GET_STRING(S);
                nextIndex = string_advance_index(p, thisIndex, fullUnicode);
                if (JS_SetProperty(ctx, rx, JS_ATOM_lastIndex, JS_NewInt64(ctx, nextIndex)) < 0)
                    goto exception;
            }
        }
        if (n == 0) {
            JS_FreeValue(ctx, A);
            A = JS_NULL;
        }
    }
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, flags);
    JS_FreeValue(ctx, S);
    return A;

exception:
    JS_FreeValue(ctx, A);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, flags);
    JS_FreeValue(ctx, S);
    return JS_EXCEPTION;
}

void js_regexp_string_iterator_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSRegExpStringIteratorData *it = p->u.regexp_string_iterator_data;
    if (it) {
        JS_FreeValueRT(rt, it->iterating_regexp);
        JS_FreeValueRT(rt, it->iterated_string);
        js_free_rt(rt, it);
    }
}

void js_regexp_string_iterator_mark(JSRuntime *rt, JSValueConst val,
                                      JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JSRegExpStringIteratorData *it = p->u.regexp_string_iterator_data;
    if (it) {
        JS_MarkValue(rt, it->iterating_regexp, mark_func);
        JS_MarkValue(rt, it->iterated_string, mark_func);
    }
}

static JSValue js_regexp_string_iterator_next(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              BOOL *pdone, int magic)
{
    JSRegExpStringIteratorData *it;
    JSValueConst R, S;
    JSValue matchStr = JS_UNDEFINED, match = JS_UNDEFINED;
    JSString *sp;

    it = JS_GetOpaque2(ctx, this_val, JS_CLASS_REGEXP_STRING_ITERATOR);
    if (!it)
        goto exception;
    if (it->done) {
        *pdone = TRUE;
        return JS_UNDEFINED;
    }
    R = it->iterating_regexp;
    S = it->iterated_string;
    match = JS_RegExpExec(ctx, R, S);
    if (JS_IsException(match))
        goto exception;
    if (JS_IsNull(match)) {
        it->done = TRUE;
        *pdone = TRUE;
        return JS_UNDEFINED;
    } else if (it->global) {
        matchStr = JS_ToStringFree(ctx, JS_GetPropertyInt64(ctx, match, 0));
        if (JS_IsException(matchStr))
            goto exception;
        if (JS_IsEmptyString(matchStr)) {
            int64_t thisIndex, nextIndex;
            if (JS_ToLengthFree(ctx, &thisIndex,
                                JS_GetProperty(ctx, R, JS_ATOM_lastIndex)) < 0)
                goto exception;
            sp = JS_VALUE_GET_STRING(S);
            nextIndex = string_advance_index(sp, thisIndex, it->unicode);
            if (JS_SetProperty(ctx, R, JS_ATOM_lastIndex,
                               JS_NewInt64(ctx, nextIndex)) < 0)
                goto exception;
        }
        JS_FreeValue(ctx, matchStr);
    } else {
        it->done = TRUE;
    }
    *pdone = FALSE;
    return match;
 exception:
    JS_FreeValue(ctx, match);
    JS_FreeValue(ctx, matchStr);
    *pdone = FALSE;
    return JS_EXCEPTION;
}

static JSValue js_regexp_Symbol_matchAll(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    // [Symbol.matchAll](str)
    JSValueConst R = this_val;
    JSValue S, C, flags, matcher, iter;
    JSValueConst args[2];
    JSString *strp;
    int64_t lastIndex;
    JSRegExpStringIteratorData *it;

    if (!JS_IsObject(R))
        return JS_ThrowTypeErrorNotAnObject(ctx);

    C = JS_UNDEFINED;
    flags = JS_UNDEFINED;
    matcher = JS_UNDEFINED;
    iter = JS_UNDEFINED;

    S = JS_ToString(ctx, argv[0]);
    if (JS_IsException(S))
        goto exception;
    C = JS_SpeciesConstructor(ctx, R, ctx->regexp_ctor);
    if (JS_IsException(C))
        goto exception;
    flags = JS_ToStringFree(ctx, JS_GetProperty(ctx, R, JS_ATOM_flags));
    if (JS_IsException(flags))
        goto exception;
    args[0] = R;
    args[1] = flags;
    matcher = JS_CallConstructor(ctx, C, 2, args);
    if (JS_IsException(matcher))
        goto exception;
    if (JS_ToLengthFree(ctx, &lastIndex,
                        JS_GetProperty(ctx, R, JS_ATOM_lastIndex)))
        goto exception;
    if (JS_SetProperty(ctx, matcher, JS_ATOM_lastIndex,
                       JS_NewInt64(ctx, lastIndex)) < 0)
        goto exception;

    iter = JS_NewObjectClass(ctx, JS_CLASS_REGEXP_STRING_ITERATOR);
    if (JS_IsException(iter))
        goto exception;
    it = js_malloc(ctx, sizeof(*it));
    if (!it)
        goto exception;
    it->iterating_regexp = matcher;
    it->iterated_string = S;
    strp = JS_VALUE_GET_STRING(flags);
    it->global = string_indexof_char(strp, 'g', 0) >= 0;
    it->unicode = (string_indexof_char(strp, 'u', 0) >= 0 ||
                   string_indexof_char(strp, 'v', 0) >= 0);
    it->done = FALSE;
    JS_SetOpaque(iter, it);

    JS_FreeValue(ctx, C);
    JS_FreeValue(ctx, flags);
    return iter;
 exception:
    JS_FreeValue(ctx, S);
    JS_FreeValue(ctx, C);
    JS_FreeValue(ctx, flags);
    JS_FreeValue(ctx, matcher);
    JS_FreeValue(ctx, iter);
    return JS_EXCEPTION;
}

typedef struct ValueBuffer {
    JSContext *ctx;
    JSValue *arr;
    JSValue def[4];
    int len;
    int size;
    int error_status;
} ValueBuffer;

static int value_buffer_init(JSContext *ctx, ValueBuffer *b)
{
    b->ctx = ctx;
    b->len = 0;
    b->size = 4;
    b->error_status = 0;
    b->arr = b->def;
    return 0;
}

static void value_buffer_free(ValueBuffer *b)
{
    while (b->len > 0)
        JS_FreeValue(b->ctx, b->arr[--b->len]);
    if (b->arr != b->def)
        js_free(b->ctx, b->arr);
    b->arr = b->def;
    b->size = 4;
}

static int value_buffer_append(ValueBuffer *b, JSValue val)
{
    if (b->error_status)
        return -1;

    if (b->len >= b->size) {
        int new_size = (b->len + (b->len >> 1) + 31) & ~16;
        size_t slack;
        JSValue *new_arr;

        if (b->arr == b->def) {
            new_arr = js_realloc2(b->ctx, NULL, sizeof(*b->arr) * new_size, &slack);
            if (new_arr)
                memcpy(new_arr, b->def, sizeof b->def);
        } else {
            new_arr = js_realloc2(b->ctx, b->arr, sizeof(*b->arr) * new_size, &slack);
        }
        if (!new_arr) {
            value_buffer_free(b);
            JS_FreeValue(b->ctx, val);
            b->error_status = -1;
            return -1;
        }
        new_size += slack / sizeof(*new_arr);
        b->arr = new_arr;
        b->size = new_size;
    }
    b->arr[b->len++] = val;
    return 0;
}

/* find in 'p' or its prototypes */
static JSShapeProperty *find_property_regexp(JSProperty **ppr,
                                             JSObject *p, JSAtom atom)
{
    JSShapeProperty *prs;

    for(;;) {
        prs = find_own_property(ppr, p, atom);
        if (prs)
            return prs;
        p = p->shape->proto;
        if (!p)
            return NULL;
        if (p->is_exotic)
            return NULL;
    }
}

static BOOL check_regexp_getter(JSContext *ctx,
                                JSObject *p, JSAtom atom,
                                JSCFunction *func, int magic)
{
    JSProperty *pr;
    JSShapeProperty *prs;

    prs = find_property_regexp(&pr, p, atom);
    if (!prs)
        return FALSE;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_GETSET)
        return FALSE;
    if (!pr->u.getset.getter)
        return FALSE;
    return JS_IsCFunction(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter),
                          func, magic);
}

static BOOL js_is_standard_regexp(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    JSProperty *pr;
    JSShapeProperty *prs;
    JSCFunctionType ft;
    
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id != JS_CLASS_REGEXP)
        return FALSE;
    /* check that the lastIndex is a number (no side effect while getting it) */
    prs = find_own_property(&pr, p, JS_ATOM_lastIndex);
    if (!prs)
        return FALSE;
    if (!JS_IsNumber(pr->u.value))
        return FALSE;

    /* check the 'exec' method. */
    prs = find_property_regexp(&pr, p, JS_ATOM_exec);
    if (!prs)
        return FALSE;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return FALSE;
    if (!JS_IsCFunction(ctx, pr->u.value, js_regexp_exec, 0))
        return FALSE;
    /* check the flag getters */
    ft.getter = js_regexp_get_flags;
    if (!check_regexp_getter(ctx, p, JS_ATOM_flags, ft.generic, 0))
        return FALSE;
    ft.getter_magic = js_regexp_get_flag;
    if (!check_regexp_getter(ctx, p, JS_ATOM_global, ft.generic, LRE_FLAG_GLOBAL))
        return FALSE;
    if (!check_regexp_getter(ctx, p, JS_ATOM_unicode, ft.generic, LRE_FLAG_UNICODE))
        return FALSE;
    /* XXX: need to check all accessors, need a faster way.  */
    return TRUE;
}

static JSValue js_regexp_Symbol_replace(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    // [Symbol.replace](str, rep)
    JSValueConst rx = this_val, rep = argv[1];
    JSValueConst args[6];
    JSValue flags, str, rep_val, matched, tab, rep_str, namedCaptures, res;
    JSString *p, *sp;
    StringBuffer b_s, *b = &b_s;
    ValueBuffer v_b, *results = &v_b;
    int nextSourcePosition, n, j, functionalReplace, is_global, fullUnicode;
    uint32_t nCaptures;
    int64_t position;

    if (!JS_IsObject(rx))
        return JS_ThrowTypeErrorNotAnObject(ctx);

    string_buffer_init(ctx, b, 0);
    value_buffer_init(ctx, results);

    rep_val = JS_UNDEFINED;
    matched = JS_UNDEFINED;
    tab = JS_UNDEFINED;
    flags = JS_UNDEFINED;
    rep_str = JS_UNDEFINED;
    namedCaptures = JS_UNDEFINED;

    str = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str))
        goto exception;

    sp = JS_VALUE_GET_STRING(str);
    functionalReplace = JS_IsFunction(ctx, rep);
    if (!functionalReplace) {
        rep_val = JS_ToString(ctx, rep);
        if (JS_IsException(rep_val))
            goto exception;
    }

    if (!functionalReplace && js_is_standard_regexp(ctx, rx)) {
        /* use faster version for simple cases */
        res = js_regexp_replace(ctx, rx, str, rep_val);
        if (!JS_IsUndefined(res))
            goto done;
    }
    
    flags = JS_GetProperty(ctx, rx, JS_ATOM_flags);
    if (JS_IsException(flags))
        goto exception;
    flags = JS_ToStringFree(ctx, flags);
    if (JS_IsException(flags))
        goto exception;
    p = JS_VALUE_GET_STRING(flags);

    fullUnicode = 0;
    is_global = (-1 != string_indexof_char(p, 'g', 0));
    if (is_global) {
        fullUnicode = (string_indexof_char(p, 'u', 0) >= 0 ||
                       string_indexof_char(p, 'v', 0) >= 0);
        if (JS_SetProperty(ctx, rx, JS_ATOM_lastIndex, JS_NewInt32(ctx, 0)) < 0)
            goto exception;
    }

    for(;;) {
        JSValue result;
        result = JS_RegExpExec(ctx, rx, str);
        if (JS_IsException(result))
            goto exception;
        if (JS_IsNull(result))
            break;
        if (value_buffer_append(results, result) < 0)
            goto exception;
        if (!is_global)
            break;
        JS_FreeValue(ctx, matched);
        matched = JS_ToStringFree(ctx, JS_GetPropertyInt64(ctx, result, 0));
        if (JS_IsException(matched))
            goto exception;
        if (JS_IsEmptyString(matched)) {
            /* always advance of at least one char */
            int64_t thisIndex, nextIndex;
            if (JS_ToLengthFree(ctx, &thisIndex, JS_GetProperty(ctx, rx, JS_ATOM_lastIndex)) < 0)
                goto exception;
            nextIndex = string_advance_index(sp, thisIndex, fullUnicode);
            if (JS_SetProperty(ctx, rx, JS_ATOM_lastIndex, JS_NewInt64(ctx, nextIndex)) < 0)
                goto exception;
        }
    }
    nextSourcePosition = 0;
    for(j = 0; j < results->len; j++) {
        JSValueConst result;
        result = results->arr[j];
        if (js_get_length32(ctx, &nCaptures, result) < 0)
            goto exception;
        JS_FreeValue(ctx, matched);
        matched = JS_ToStringFree(ctx, JS_GetPropertyInt64(ctx, result, 0));
        if (JS_IsException(matched))
            goto exception;
        if (JS_ToLengthFree(ctx, &position, JS_GetProperty(ctx, result, JS_ATOM_index)))
            goto exception;
        if (position > sp->len)
            position = sp->len;
        else if (position < 0)
            position = 0;
        /* ignore substition if going backward (can happen
           with custom regexp object) */
        JS_FreeValue(ctx, tab);
        tab = JS_NewArray(ctx);
        if (JS_IsException(tab))
            goto exception;
        if (JS_DefinePropertyValueInt64(ctx, tab, 0, JS_DupValue(ctx, matched),
                                        JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto exception;
        for(n = 1; n < nCaptures; n++) {
            JSValue capN;
            capN = JS_GetPropertyInt64(ctx, result, n);
            if (JS_IsException(capN))
                goto exception;
            if (!JS_IsUndefined(capN)) {
                capN = JS_ToStringFree(ctx, capN);
                if (JS_IsException(capN))
                    goto exception;
            }
            if (JS_DefinePropertyValueInt64(ctx, tab, n, capN,
                                            JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                goto exception;
        }
        JS_FreeValue(ctx, namedCaptures);
        namedCaptures = JS_GetProperty(ctx, result, JS_ATOM_groups);
        if (JS_IsException(namedCaptures))
            goto exception;
        if (functionalReplace) {
            if (JS_DefinePropertyValueInt64(ctx, tab, n++, JS_NewInt32(ctx, position), JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                goto exception;
            if (JS_DefinePropertyValueInt64(ctx, tab, n++, JS_DupValue(ctx, str), JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                goto exception;
            if (!JS_IsUndefined(namedCaptures)) {
                if (JS_DefinePropertyValueInt64(ctx, tab, n++, JS_DupValue(ctx, namedCaptures), JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                    goto exception;
            }
            args[0] = JS_UNDEFINED;
            args[1] = tab;
            JS_FreeValue(ctx, rep_str);
            rep_str = JS_ToStringFree(ctx, js_function_apply(ctx, rep, 2, args, 0));
        } else {
            JSValue namedCaptures1;
            StringBuffer b1_s, *b1 = &b1_s;
            int ret;
            
            if (!JS_IsUndefined(namedCaptures)) {
                namedCaptures1 = JS_ToObject(ctx, namedCaptures);
                if (JS_IsException(namedCaptures1))
                    goto exception;
            } else {
                namedCaptures1 = JS_UNDEFINED;
            }
            JS_FreeValue(ctx, rep_str);
            
            string_buffer_init(ctx, b1, 0);
            ret = js_string_GetSubstitution(ctx, b1, matched, sp, position,
                                            tab, namedCaptures1, rep_val,
                                            NULL, 0);
            rep_str = string_buffer_end(b1);
            JS_FreeValue(ctx, namedCaptures1);
            if (ret)
                goto exception;
        }
        if (JS_IsException(rep_str))
            goto exception;
        if (position >= nextSourcePosition) {
            string_buffer_concat(b, sp, nextSourcePosition, position);
            string_buffer_concat_value(b, rep_str);
            nextSourcePosition = position + JS_VALUE_GET_STRING(matched)->len;
        }
    }
    string_buffer_concat(b, sp, nextSourcePosition, sp->len);
    res = string_buffer_end(b);
    goto done1;

exception:
    res = JS_EXCEPTION;
done:
    string_buffer_free(b);
done1:
    value_buffer_free(results);
    JS_FreeValue(ctx, rep_val);
    JS_FreeValue(ctx, matched);
    JS_FreeValue(ctx, flags);
    JS_FreeValue(ctx, tab);
    JS_FreeValue(ctx, rep_str);
    JS_FreeValue(ctx, namedCaptures);
    JS_FreeValue(ctx, str);
    return res;
}

static JSValue js_regexp_Symbol_search(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValueConst rx = this_val;
    JSValue str, previousLastIndex, currentLastIndex, result, index;

    if (!JS_IsObject(rx))
        return JS_ThrowTypeErrorNotAnObject(ctx);

    result = JS_UNDEFINED;
    currentLastIndex = JS_UNDEFINED;
    previousLastIndex = JS_UNDEFINED;
    str = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str))
        goto exception;

    previousLastIndex = JS_GetProperty(ctx, rx, JS_ATOM_lastIndex);
    if (JS_IsException(previousLastIndex))
        goto exception;

    if (!js_same_value(ctx, previousLastIndex, JS_NewInt32(ctx, 0))) {
        if (JS_SetProperty(ctx, rx, JS_ATOM_lastIndex, JS_NewInt32(ctx, 0)) < 0) {
            goto exception;
        }
    }
    result = JS_RegExpExec(ctx, rx, str);
    if (JS_IsException(result))
        goto exception;
    currentLastIndex = JS_GetProperty(ctx, rx, JS_ATOM_lastIndex);
    if (JS_IsException(currentLastIndex))
        goto exception;
    if (js_same_value(ctx, currentLastIndex, previousLastIndex)) {
        JS_FreeValue(ctx, previousLastIndex);
    } else {
        if (JS_SetProperty(ctx, rx, JS_ATOM_lastIndex, previousLastIndex) < 0) {
            previousLastIndex = JS_UNDEFINED;
            goto exception;
        }
    }
    JS_FreeValue(ctx, str);
    JS_FreeValue(ctx, currentLastIndex);

    if (JS_IsNull(result)) {
        return JS_NewInt32(ctx, -1);
    } else {
        index = JS_GetProperty(ctx, result, JS_ATOM_index);
        JS_FreeValue(ctx, result);
        return index;
    }

exception:
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, str);
    JS_FreeValue(ctx, currentLastIndex);
    JS_FreeValue(ctx, previousLastIndex);
    return JS_EXCEPTION;
}

static JSValue js_regexp_Symbol_split(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    // [Symbol.split](str, limit)
    JSValueConst rx = this_val;
    JSValueConst args[2];
    JSValue str, ctor, splitter, A, flags, z, sub;
    JSString *strp;
    uint32_t lim, size, p, q;
    int unicodeMatching;
    int64_t lengthA, e, numberOfCaptures, i;

    if (!JS_IsObject(rx))
        return JS_ThrowTypeErrorNotAnObject(ctx);

    ctor = JS_UNDEFINED;
    splitter = JS_UNDEFINED;
    A = JS_UNDEFINED;
    flags = JS_UNDEFINED;
    z = JS_UNDEFINED;
    str = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str))
        goto exception;
    ctor = JS_SpeciesConstructor(ctx, rx, ctx->regexp_ctor);
    if (JS_IsException(ctor))
        goto exception;
    flags = JS_ToStringFree(ctx, JS_GetProperty(ctx, rx, JS_ATOM_flags));
    if (JS_IsException(flags))
        goto exception;
    strp = JS_VALUE_GET_STRING(flags);
    unicodeMatching = (string_indexof_char(strp, 'u', 0) >= 0 ||
                       string_indexof_char(strp, 'v', 0) >= 0);
    if (string_indexof_char(strp, 'y', 0) < 0) {
        flags = JS_ConcatString3(ctx, "", flags, "y");
        if (JS_IsException(flags))
            goto exception;
    }
    args[0] = rx;
    args[1] = flags;
    splitter = JS_CallConstructor(ctx, ctor, 2, args);
    if (JS_IsException(splitter))
        goto exception;
    A = JS_NewArray(ctx);
    if (JS_IsException(A))
        goto exception;
    lengthA = 0;
    if (JS_IsUndefined(argv[1])) {
        lim = 0xffffffff;
    } else {
        if (JS_ToUint32(ctx, &lim, argv[1]) < 0)
            goto exception;
        if (lim == 0)
            goto done;
    }
    strp = JS_VALUE_GET_STRING(str);
    p = q = 0;
    size = strp->len;
    if (size == 0) {
        z = JS_RegExpExec(ctx, splitter, str);
        if (JS_IsException(z))
            goto exception;
        if (JS_IsNull(z))
            goto add_tail;
        goto done;
    }
    while (q < size) {
        if (JS_SetProperty(ctx, splitter, JS_ATOM_lastIndex, JS_NewInt32(ctx, q)) < 0)
            goto exception;
        JS_FreeValue(ctx, z);
        z = JS_RegExpExec(ctx, splitter, str);
        if (JS_IsException(z))
            goto exception;
        if (JS_IsNull(z)) {
            q = string_advance_index(strp, q, unicodeMatching);
        } else {
            if (JS_ToLengthFree(ctx, &e, JS_GetProperty(ctx, splitter, JS_ATOM_lastIndex)))
                goto exception;
            if (e > size)
                e = size;
            if (e == p) {
                q = string_advance_index(strp, q, unicodeMatching);
            } else {
                sub = js_sub_string(ctx, strp, p, q);
                if (JS_IsException(sub))
                    goto exception;
                if (JS_DefinePropertyValueInt64(ctx, A, lengthA++, sub,
                                                JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                    goto exception;
                if (lengthA == lim)
                    goto done;
                p = e;
                if (js_get_length64(ctx, &numberOfCaptures, z))
                    goto exception;
                for(i = 1; i < numberOfCaptures; i++) {
                    sub = JS_GetPropertyInt64(ctx, z, i);
                    if (JS_IsException(sub))
                        goto exception;
                    if (JS_DefinePropertyValueInt64(ctx, A, lengthA++, sub, JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                        goto exception;
                    if (lengthA == lim)
                        goto done;
                }
                q = p;
            }
        }
    }
add_tail:
    if (p > size)
        p = size;
    sub = js_sub_string(ctx, strp, p, size);
    if (JS_IsException(sub))
        goto exception;
    if (JS_DefinePropertyValueInt64(ctx, A, lengthA++, sub, JS_PROP_C_W_E | JS_PROP_THROW) < 0)
        goto exception;
    goto done;
exception:
    JS_FreeValue(ctx, A);
    A = JS_EXCEPTION;
done:
    JS_FreeValue(ctx, str);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, splitter);
    JS_FreeValue(ctx, flags);
    JS_FreeValue(ctx, z);
    return A;
}

static const JSCFunctionListEntry js_regexp_funcs[] = {
    JS_CFUNC_DEF("escape", 1, js_regexp_escape ),
    JS_CGETSET_DEF("[Symbol.species]", js_get_this, NULL ),
};

static const JSCFunctionListEntry js_regexp_proto_funcs[] = {
    JS_CGETSET_DEF("flags", js_regexp_get_flags, NULL ),
    JS_CGETSET_DEF("source", js_regexp_get_source, NULL ),
    JS_CGETSET_MAGIC_DEF("global", js_regexp_get_flag, NULL, LRE_FLAG_GLOBAL ),
    JS_CGETSET_MAGIC_DEF("ignoreCase", js_regexp_get_flag, NULL, LRE_FLAG_IGNORECASE ),
    JS_CGETSET_MAGIC_DEF("multiline", js_regexp_get_flag, NULL, LRE_FLAG_MULTILINE ),
    JS_CGETSET_MAGIC_DEF("dotAll", js_regexp_get_flag, NULL, LRE_FLAG_DOTALL ),
    JS_CGETSET_MAGIC_DEF("unicode", js_regexp_get_flag, NULL, LRE_FLAG_UNICODE ),
    JS_CGETSET_MAGIC_DEF("unicodeSets", js_regexp_get_flag, NULL, LRE_FLAG_UNICODE_SETS ),
    JS_CGETSET_MAGIC_DEF("sticky", js_regexp_get_flag, NULL, LRE_FLAG_STICKY ),
    JS_CGETSET_MAGIC_DEF("hasIndices", js_regexp_get_flag, NULL, LRE_FLAG_INDICES ),
    JS_CFUNC_DEF("exec", 1, js_regexp_exec ),
    JS_CFUNC_DEF("compile", 2, js_regexp_compile ),
    JS_CFUNC_DEF("test", 1, js_regexp_test ),
    JS_CFUNC_DEF("toString", 0, js_regexp_toString ),
    JS_CFUNC_DEF("[Symbol.replace]", 2, js_regexp_Symbol_replace ),
    JS_CFUNC_DEF("[Symbol.match]", 1, js_regexp_Symbol_match ),
    JS_CFUNC_DEF("[Symbol.matchAll]", 1, js_regexp_Symbol_matchAll ),
    JS_CFUNC_DEF("[Symbol.search]", 1, js_regexp_Symbol_search ),
    JS_CFUNC_DEF("[Symbol.split]", 2, js_regexp_Symbol_split ),
};

static const JSCFunctionListEntry js_regexp_string_iterator_proto_funcs[] = {
    JS_ITERATOR_NEXT_DEF("next", 0, js_regexp_string_iterator_next, 0 ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "RegExp String Iterator", JS_PROP_CONFIGURABLE ),
};

void JS_AddIntrinsicRegExpCompiler(JSContext *ctx)
{
    ctx->compile_regexp = js_compile_regexp;
}

int JS_AddIntrinsicRegExp(JSContext *ctx)
{
    JSValue obj;

    JS_AddIntrinsicRegExpCompiler(ctx);

    obj = JS_NewCConstructor(ctx, JS_CLASS_REGEXP, "RegExp",
                                    js_regexp_constructor, 2, JS_CFUNC_constructor_or_func, 0,
                                    JS_UNDEFINED,
                                    js_regexp_funcs, countof(js_regexp_funcs),
                                    js_regexp_proto_funcs, countof(js_regexp_proto_funcs),
                                    0);
    if (JS_IsException(obj))
        return -1;
    ctx->regexp_ctor = obj;
    
    ctx->class_proto[JS_CLASS_REGEXP_STRING_ITERATOR] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR],
                              js_regexp_string_iterator_proto_funcs,
                              countof(js_regexp_string_iterator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_REGEXP_STRING_ITERATOR]))
        return -1;

    ctx->regexp_shape = js_new_shape2(ctx, get_proto_obj(ctx->class_proto[JS_CLASS_REGEXP]),
                                     JS_PROP_INITIAL_HASH_SIZE, 1);
    if (!ctx->regexp_shape)
        return -1;
    if (add_shape_property(ctx, &ctx->regexp_shape, NULL,
                           JS_ATOM_lastIndex, JS_PROP_WRITABLE))
        return -1;

    ctx->regexp_result_shape = js_new_shape2(ctx, get_proto_obj(ctx->class_proto[JS_CLASS_ARRAY]),
                                     JS_PROP_INITIAL_HASH_SIZE, 4);
    if (!ctx->regexp_result_shape)
        return -1;
    if (add_shape_property(ctx, &ctx->regexp_result_shape, NULL,
                           JS_ATOM_length, JS_PROP_WRITABLE | JS_PROP_LENGTH))
        return -1;
    if (add_shape_property(ctx, &ctx->regexp_result_shape, NULL,
                           JS_ATOM_index, JS_PROP_C_W_E))
        return -1;
    if (add_shape_property(ctx, &ctx->regexp_result_shape, NULL,
                           JS_ATOM_input, JS_PROP_C_W_E))
        return -1;
    if (add_shape_property(ctx, &ctx->regexp_result_shape, NULL,
                           JS_ATOM_groups, JS_PROP_C_W_E))
        return -1;

    return 0;
}


/* Symbol */

static JSValue js_symbol_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValue str;
    JSString *p;

    if (!JS_IsUndefined(new_target))
        return JS_ThrowTypeErrorNotAConstructor(ctx, new_target);
    if (argc == 0 || JS_IsUndefined(argv[0])) {
        p = NULL;
    } else {
        str = JS_ToString(ctx, argv[0]);
        if (JS_IsException(str))
            return JS_EXCEPTION;
        p = JS_VALUE_GET_STRING(str);
    }
    return JS_NewSymbol(ctx, p, JS_ATOM_TYPE_SYMBOL);
}

static JSValue js_thisSymbolValue(JSContext *ctx, JSValueConst this_val)
{
    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_SYMBOL)
        return JS_DupValue(ctx, this_val);

    if (JS_VALUE_GET_TAG(this_val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(this_val);
        if (p->class_id == JS_CLASS_SYMBOL) {
            if (JS_VALUE_GET_TAG(p->u.object_data) == JS_TAG_SYMBOL)
                return JS_DupValue(ctx, p->u.object_data);
        }
    }
    return JS_ThrowTypeError(ctx, "not a symbol");
}

static JSValue js_symbol_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValue val, ret;
    val = js_thisSymbolValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    /* XXX: use JS_ToStringInternal() with a flags */
    ret = js_string_constructor(ctx, JS_UNDEFINED, 1, (JSValueConst *)&val);
    JS_FreeValue(ctx, val);
    return ret;
}

static JSValue js_symbol_valueOf(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    return js_thisSymbolValue(ctx, this_val);
}

static JSValue js_symbol_get_description(JSContext *ctx, JSValueConst this_val)
{
    JSValue val, ret;
    JSAtomStruct *p;

    val = js_thisSymbolValue(ctx, this_val);
    if (JS_IsException(val))
        return val;
    p = JS_VALUE_GET_PTR(val);
    if (p->len == 0 && p->is_wide_char != 0) {
        ret = JS_UNDEFINED;
    } else {
        ret = JS_AtomToString(ctx, js_get_atom_index(ctx->rt, p));
    }
    JS_FreeValue(ctx, val);
    return ret;
}

static const JSCFunctionListEntry js_symbol_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_symbol_toString ),
    JS_CFUNC_DEF("valueOf", 0, js_symbol_valueOf ),
    // XXX: should have writable: false
    JS_CFUNC_DEF("[Symbol.toPrimitive]", 1, js_symbol_valueOf ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Symbol", JS_PROP_CONFIGURABLE ),
    JS_CGETSET_DEF("description", js_symbol_get_description, NULL ),
};

static JSValue js_symbol_for(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSValue str;

    str = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str))
        return JS_EXCEPTION;
    return JS_NewSymbol(ctx, JS_VALUE_GET_STRING(str), JS_ATOM_TYPE_GLOBAL_SYMBOL);
}

static JSValue js_symbol_keyFor(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSAtomStruct *p;

    if (!JS_IsSymbol(argv[0]))
        return JS_ThrowTypeError(ctx, "not a symbol");
    p = JS_VALUE_GET_PTR(argv[0]);
    if (p->atom_type != JS_ATOM_TYPE_GLOBAL_SYMBOL)
        return JS_UNDEFINED;
    return JS_DupValue(ctx, JS_MKPTR(JS_TAG_STRING, p));
}

static const JSCFunctionListEntry js_symbol_funcs[] = {
    JS_CFUNC_DEF("for", 1, js_symbol_for ),
    JS_CFUNC_DEF("keyFor", 1, js_symbol_keyFor ),
    JS_PROP_ATOM_DEF("toPrimitive", JS_ATOM_Symbol_toPrimitive, 0),
    JS_PROP_ATOM_DEF("iterator", JS_ATOM_Symbol_iterator, 0),
    JS_PROP_ATOM_DEF("match", JS_ATOM_Symbol_match, 0),
    JS_PROP_ATOM_DEF("matchAll", JS_ATOM_Symbol_matchAll, 0),
    JS_PROP_ATOM_DEF("replace", JS_ATOM_Symbol_replace, 0),
    JS_PROP_ATOM_DEF("search", JS_ATOM_Symbol_search, 0),
    JS_PROP_ATOM_DEF("split", JS_ATOM_Symbol_split, 0),
    JS_PROP_ATOM_DEF("toStringTag", JS_ATOM_Symbol_toStringTag, 0),
    JS_PROP_ATOM_DEF("isConcatSpreadable", JS_ATOM_Symbol_isConcatSpreadable, 0),
    JS_PROP_ATOM_DEF("hasInstance", JS_ATOM_Symbol_hasInstance, 0),
    JS_PROP_ATOM_DEF("species", JS_ATOM_Symbol_species, 0),
    JS_PROP_ATOM_DEF("unscopables", JS_ATOM_Symbol_unscopables, 0),
    JS_PROP_ATOM_DEF("asyncIterator", JS_ATOM_Symbol_asyncIterator, 0),
};



int JS_AddIntrinsicString(JSContext *ctx)
{
    JSValue obj1;

    obj1 = JS_NewCConstructor(ctx, JS_CLASS_STRING, "String",
                              js_string_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                              JS_UNDEFINED,
                              js_string_funcs, countof(js_string_funcs),
                              js_string_proto_funcs, countof(js_string_proto_funcs),
                              JS_NEW_CTOR_PROTO_CLASS);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);
    if (JS_SetObjectData(ctx, ctx->class_proto[JS_CLASS_STRING], JS_AtomToString(ctx, JS_ATOM_empty_string)))
        return -1;

    ctx->class_proto[JS_CLASS_STRING_ITERATOR] =
        JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_ITERATOR], 
                              js_string_iterator_proto_funcs,
                              countof(js_string_iterator_proto_funcs));
    if (JS_IsException(ctx->class_proto[JS_CLASS_STRING_ITERATOR]))
        return -1;
    return 0;
}

int JS_AddIntrinsicSymbol(JSContext *ctx)
{
    JSValue obj1;

    obj1 = JS_NewCConstructor(ctx, JS_CLASS_SYMBOL, "Symbol",
                              js_symbol_constructor, 0, JS_CFUNC_constructor_or_func, 0,
                              JS_UNDEFINED,
                              js_symbol_funcs, countof(js_symbol_funcs),
                              js_symbol_proto_funcs, countof(js_symbol_proto_funcs),
                              0);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);
    return 0;
}

