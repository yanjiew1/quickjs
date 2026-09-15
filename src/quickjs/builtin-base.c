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

#include "internal-base.h"

static __exception int JS_DefinePropertyDesc(JSContext *ctx, JSValueConst obj,
                                             JSAtom prop, JSValueConst desc,
                                             int flags)
{
    JSPropertyDescriptor d;
    int ret;

    if (qjs_base_obj_to_desc(ctx, &d, desc) < 0)
        return -1;

    ret = JS_DefineProperty(ctx, obj, prop,
                            d.value, d.getter, d.setter, d.flags | flags);
    qjs_base_free_desc(ctx, &d);
    return ret;
}

static __exception int JS_ObjectDefineProperties(JSContext *ctx,
                                                 JSValueConst obj,
                                                 JSValueConst properties)
{
    JSValue props, desc;
    JSObject *p;
    JSPropertyEnum *atoms;
    uint32_t len, i;
    int ret = -1;

    if (!JS_IsObject(obj)) {
        qjs_throw_type_error_not_object(ctx);
        return -1;
    }
    desc = JS_UNDEFINED;
    props = qjs_to_object(ctx, properties);
    if (JS_IsException(props))
        return -1;
    p = JS_VALUE_GET_OBJ(props);
    /* XXX: not done in the same order as the spec */
    if (qjs_get_own_property_names_internal(ctx, &atoms, &len, p, JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK) < 0)
        goto exception;
    for(i = 0; i < len; i++) {
        JS_FreeValue(ctx, desc);
        desc = JS_GetProperty(ctx, props, atoms[i].atom);
        if (JS_IsException(desc))
            goto exception;
        if (JS_DefinePropertyDesc(ctx, obj, atoms[i].atom, desc, JS_PROP_THROW) < 0)
            goto exception;
    }
    ret = 0;

exception:
    JS_FreePropertyEnum(ctx, atoms, len);
    JS_FreeValue(ctx, props);
    JS_FreeValue(ctx, desc);
    return ret;
}


static JSValue js_object_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValue ret;
    if (!JS_IsUndefined(new_target) &&
        JS_VALUE_GET_OBJ(new_target) !=
        JS_VALUE_GET_OBJ(qjs_base_get_active_function(ctx))) {
        ret = qjs_base_create_from_ctor(ctx, new_target, JS_CLASS_OBJECT);
    } else {
        int tag = JS_VALUE_GET_NORM_TAG(argv[0]);
        switch(tag) {
        case JS_TAG_NULL:
        case JS_TAG_UNDEFINED:
            ret = JS_NewObject(ctx);
            break;
        default:
            ret = qjs_to_object(ctx, argv[0]);
            break;
        }
    }
    return ret;
}

static JSValue js_object_create(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValueConst proto, props;
    JSValue obj;

    proto = argv[0];
    if (!JS_IsObject(proto) && !JS_IsNull(proto))
        return JS_ThrowTypeError(ctx, "not a prototype");
    obj = JS_NewObjectProto(ctx, proto);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    props = argv[1];
    if (!JS_IsUndefined(props)) {
        if (JS_ObjectDefineProperties(ctx, obj, props)) {
            JS_FreeValue(ctx, obj);
            return JS_EXCEPTION;
        }
    }
    return obj;
}

static JSValue js_object_getPrototypeOf(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic)
{
    JSValueConst val;

    val = argv[0];
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT) {
        /* ES6 feature non compatible with ES5.1: primitive types are
           accepted */
        if (magic || JS_VALUE_GET_TAG(val) == JS_TAG_NULL ||
            JS_VALUE_GET_TAG(val) == JS_TAG_UNDEFINED)
            return qjs_throw_type_error_not_object(ctx);
    }
    return JS_GetPrototype(ctx, val);
}

static JSValue js_object_setPrototypeOf(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSValueConst obj;
    obj = argv[0];
    if (qjs_base_set_prototype_internal(ctx, obj, argv[1], TRUE) < 0)
        return JS_EXCEPTION;
    return JS_DupValue(ctx, obj);
}

/* magic = 1 if called as Reflect.defineProperty */
static JSValue js_object_defineProperty(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic)
{
    JSValueConst obj, prop, desc;
    int ret, flags;
    JSAtom atom;

    obj = argv[0];
    prop = argv[1];
    desc = argv[2];

    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return qjs_throw_type_error_not_object(ctx);
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    flags = 0;
    if (!magic)
        flags |= JS_PROP_THROW;
    ret = JS_DefinePropertyDesc(ctx, obj, atom, desc, flags);
    JS_FreeAtom(ctx, atom);
    if (ret < 0) {
        return JS_EXCEPTION;
    } else if (magic) {
        return JS_NewBool(ctx, ret);
    } else {
        return JS_DupValue(ctx, obj);
    }
}

static JSValue js_object_defineProperties(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    // defineProperties(obj, properties)
    JSValueConst obj = argv[0];

    if (JS_ObjectDefineProperties(ctx, obj, argv[1]))
        return JS_EXCEPTION;
    else
        return JS_DupValue(ctx, obj);
}

/* magic = 1 if called as __defineSetter__ */
static JSValue js_object___defineGetter__(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv, int magic)
{
    JSValue obj;
    JSValueConst prop, value, get, set;
    int ret, flags;
    JSAtom atom;

    prop = argv[0];
    value = argv[1];

    obj = qjs_to_object(ctx, this_val);
    if (JS_IsException(obj))
        return JS_EXCEPTION;

    if (qjs_check_function(ctx, value)) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL)) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    flags = JS_PROP_THROW |
        JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE |
        JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE;
    if (magic) {
        get = JS_UNDEFINED;
        set = value;
        flags |= JS_PROP_HAS_SET;
    } else {
        get = value;
        set = JS_UNDEFINED;
        flags |= JS_PROP_HAS_GET;
    }
    ret = JS_DefineProperty(ctx, obj, atom, JS_UNDEFINED, get, set, flags);
    JS_FreeValue(ctx, obj);
    JS_FreeAtom(ctx, atom);
    if (ret < 0) {
        return JS_EXCEPTION;
    } else {
        return JS_UNDEFINED;
    }
}

static JSValue js_object_getOwnPropertyDescriptor(JSContext *ctx, JSValueConst this_val,
                                                  int argc, JSValueConst *argv, int magic)
{
    JSValueConst prop;
    JSAtom atom;
    JSValue ret, obj;
    JSPropertyDescriptor desc;
    int res, flags;

    if (magic) {
        /* Reflect.getOwnPropertyDescriptor case */
        if (JS_VALUE_GET_TAG(argv[0]) != JS_TAG_OBJECT)
            return qjs_throw_type_error_not_object(ctx);
        obj = JS_DupValue(ctx, argv[0]);
    } else {
        obj = qjs_to_object(ctx, argv[0]);
        if (JS_IsException(obj))
            return obj;
    }
    prop = argv[1];
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL))
        goto exception;
    ret = JS_UNDEFINED;
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        res = qjs_base_get_own_property_internal(ctx, &desc, JS_VALUE_GET_OBJ(obj), atom);
        if (res < 0)
            goto exception;
        if (res) {
            ret = JS_NewObject(ctx);
            if (JS_IsException(ret))
                goto exception1;
            flags = JS_PROP_C_W_E | JS_PROP_THROW;
            if (desc.flags & JS_PROP_GETSET) {
                if (JS_DefinePropertyValue(ctx, ret, JS_ATOM_get, JS_DupValue(ctx, desc.getter), flags) < 0
                ||  JS_DefinePropertyValue(ctx, ret, JS_ATOM_set, JS_DupValue(ctx, desc.setter), flags) < 0)
                    goto exception1;
            } else {
                if (JS_DefinePropertyValue(ctx, ret, JS_ATOM_value, JS_DupValue(ctx, desc.value), flags) < 0
                ||  JS_DefinePropertyValue(ctx, ret, JS_ATOM_writable,
                                           JS_NewBool(ctx, desc.flags & JS_PROP_WRITABLE), flags) < 0)
                    goto exception1;
            }
            if (JS_DefinePropertyValue(ctx, ret, JS_ATOM_enumerable,
                                       JS_NewBool(ctx, desc.flags & JS_PROP_ENUMERABLE), flags) < 0
            ||  JS_DefinePropertyValue(ctx, ret, JS_ATOM_configurable,
                                       JS_NewBool(ctx, desc.flags & JS_PROP_CONFIGURABLE), flags) < 0)
                goto exception1;
            qjs_base_free_desc(ctx, &desc);
        }
    }
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, obj);
    return ret;

exception1:
    qjs_base_free_desc(ctx, &desc);
    JS_FreeValue(ctx, ret);
exception:
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_object_getOwnPropertyDescriptors(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv)
{
    //getOwnPropertyDescriptors(obj)
    JSValue obj, r;
    JSObject *p;
    JSPropertyEnum *props;
    uint32_t len, i;

    r = JS_UNDEFINED;
    obj = qjs_to_object(ctx, argv[0]);
    if (JS_IsException(obj))
        return JS_EXCEPTION;

    p = JS_VALUE_GET_OBJ(obj);
    if (qjs_get_own_property_names_internal(ctx, &props, &len, p,
                               JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK))
        goto exception;
    r = JS_NewObject(ctx);
    if (JS_IsException(r))
        goto exception;
    for(i = 0; i < len; i++) {
        JSValue atomValue, desc;
        JSValueConst args[2];

        atomValue = JS_AtomToValue(ctx, props[i].atom);
        if (JS_IsException(atomValue))
            goto exception;
        args[0] = obj;
        args[1] = atomValue;
        desc = js_object_getOwnPropertyDescriptor(ctx, JS_UNDEFINED, 2, args, 0);
        JS_FreeValue(ctx, atomValue);
        if (JS_IsException(desc))
            goto exception;
        if (!JS_IsUndefined(desc)) {
            if (JS_DefinePropertyValue(ctx, r, props[i].atom, desc,
                                       JS_PROP_C_W_E | JS_PROP_THROW) < 0)
                goto exception;
        }
    }
    JS_FreePropertyEnum(ctx, props, len);
    JS_FreeValue(ctx, obj);
    return r;

exception:
    JS_FreePropertyEnum(ctx, props, len);
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, r);
    return JS_EXCEPTION;
}

static JSValue JS_GetOwnPropertyNames2(JSContext *ctx, JSValueConst obj1,
                                       int flags, int kind)
{
    JSValue obj, r, val, key, value;
    JSObject *p;
    JSPropertyEnum *atoms;
    uint32_t len, i, j;

    r = JS_UNDEFINED;
    val = JS_UNDEFINED;
    obj = qjs_to_object(ctx, obj1);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    p = JS_VALUE_GET_OBJ(obj);
    if (qjs_get_own_property_names_internal(ctx, &atoms, &len, p, flags & ~JS_GPN_ENUM_ONLY))
        goto exception;
    r = JS_NewArray(ctx);
    if (JS_IsException(r))
        goto exception;
    for(j = i = 0; i < len; i++) {
        JSAtom atom = atoms[i].atom;
        if (flags & JS_GPN_ENUM_ONLY) {
            JSPropertyDescriptor desc;
            int res;

            /* Check if property is still enumerable */
            res = qjs_base_get_own_property_internal(ctx, &desc, p, atom);
            if (res < 0)
                goto exception;
            if (!res)
                continue;
            qjs_base_free_desc(ctx, &desc);
            if (!(desc.flags & JS_PROP_ENUMERABLE))
                continue;
        }
        switch(kind) {
        default:
        case JS_ITERATOR_KIND_KEY:
            val = JS_AtomToValue(ctx, atom);
            if (JS_IsException(val))
                goto exception;
            break;
        case JS_ITERATOR_KIND_VALUE:
            val = JS_GetProperty(ctx, obj, atom);
            if (JS_IsException(val))
                goto exception;
            break;
        case JS_ITERATOR_KIND_KEY_AND_VALUE:
            val = JS_NewArray(ctx);
            if (JS_IsException(val))
                goto exception;
            key = JS_AtomToValue(ctx, atom);
            if (JS_IsException(key))
                goto exception1;
            if (qjs_base_create_data_property_uint32(ctx, val, 0, key, JS_PROP_THROW) < 0)
                goto exception1;
            value = JS_GetProperty(ctx, obj, atom);
            if (JS_IsException(value))
                goto exception1;
            if (qjs_base_create_data_property_uint32(ctx, val, 1, value, JS_PROP_THROW) < 0)
                goto exception1;
            break;
        }
        if (qjs_base_create_data_property_uint32(ctx, r, j++, val, 0) < 0)
            goto exception;
    }
    goto done;

exception1:
    JS_FreeValue(ctx, val);
exception:
    JS_FreeValue(ctx, r);
    r = JS_EXCEPTION;
done:
    JS_FreePropertyEnum(ctx, atoms, len);
    JS_FreeValue(ctx, obj);
    return r;
}

static JSValue js_object_getOwnPropertyNames(JSContext *ctx, JSValueConst this_val,
                                             int argc, JSValueConst *argv)
{
    return JS_GetOwnPropertyNames2(ctx, argv[0],
                                   JS_GPN_STRING_MASK, JS_ITERATOR_KIND_KEY);
}

static JSValue js_object_getOwnPropertySymbols(JSContext *ctx, JSValueConst this_val,
                                             int argc, JSValueConst *argv)
{
    return JS_GetOwnPropertyNames2(ctx, argv[0],
                                   JS_GPN_SYMBOL_MASK, JS_ITERATOR_KIND_KEY);
}

static JSValue js_object_keys(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int kind)
{
    return JS_GetOwnPropertyNames2(ctx, argv[0],
                                   JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK, kind);
}

static JSValue js_object_isExtensible(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int reflect)
{
    JSValueConst obj;
    int ret;

    obj = argv[0];
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        if (reflect)
            return qjs_throw_type_error_not_object(ctx);
        else
            return JS_FALSE;
    }
    ret = JS_IsExtensible(ctx, obj);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

static JSValue js_object_preventExtensions(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv, int reflect)
{
    JSValueConst obj;
    int ret;

    obj = argv[0];
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        if (reflect)
            return qjs_throw_type_error_not_object(ctx);
        else
            return JS_DupValue(ctx, obj);
    }
    ret = JS_PreventExtensions(ctx, obj);
    if (ret < 0)
        return JS_EXCEPTION;
    if (reflect) {
        return JS_NewBool(ctx, ret);
    } else {
        if (!ret)
            return JS_ThrowTypeError(ctx, "proxy preventExtensions handler returned false");
        return JS_DupValue(ctx, obj);
    }
}

static JSValue js_object_hasOwnProperty(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSValue obj;
    JSAtom atom;
    JSObject *p;
    BOOL ret;

    atom = JS_ValueToAtom(ctx, argv[0]); /* must be done first */
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    obj = qjs_to_object(ctx, this_val);
    if (JS_IsException(obj)) {
        JS_FreeAtom(ctx, atom);
        return obj;
    }
    p = JS_VALUE_GET_OBJ(obj);
    ret = qjs_base_get_own_property_internal(ctx, NULL, p, atom);
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, obj);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

static JSValue js_object_hasOwn(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSValue obj;
    JSAtom atom;
    JSObject *p;
    BOOL ret;

    obj = qjs_to_object(ctx, argv[0]);
    if (JS_IsException(obj))
        return obj;
    atom = JS_ValueToAtom(ctx, argv[1]);
    if (unlikely(atom == JS_ATOM_NULL)) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    p = JS_VALUE_GET_OBJ(obj);
    ret = qjs_base_get_own_property_internal(ctx, NULL, p, atom);
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, obj);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

static JSValue js_object_valueOf(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    return qjs_to_object(ctx, this_val);
}

static JSValue js_object_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSValue obj, tag;
    int is_array;
    JSAtom atom;
    JSObject *p;

    if (JS_IsNull(this_val)) {
        tag = qjs_new_string8(ctx, "Null");
    } else if (JS_IsUndefined(this_val)) {
        tag = qjs_new_string8(ctx, "Undefined");
    } else {
        obj = qjs_to_object(ctx, this_val);
        if (JS_IsException(obj))
            return obj;
        is_array = JS_IsArray(ctx, obj);
        if (is_array < 0) {
            JS_FreeValue(ctx, obj);
            return JS_EXCEPTION;
        }
        if (is_array) {
            atom = JS_ATOM_Array;
        } else if (JS_IsFunction(ctx, obj)) {
            atom = JS_ATOM_Function;
        } else {
            p = JS_VALUE_GET_OBJ(obj);
            switch(p->class_id) {
            case JS_CLASS_STRING:
            case JS_CLASS_ARGUMENTS:
            case JS_CLASS_MAPPED_ARGUMENTS:
            case JS_CLASS_ERROR:
            case JS_CLASS_BOOLEAN:
            case JS_CLASS_NUMBER:
            case JS_CLASS_DATE:
            case JS_CLASS_REGEXP:
                atom = ctx->rt->class_array[p->class_id].class_name;
                break;
            default:
                atom = JS_ATOM_Object;
                break;
            }
        }
        tag = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_toStringTag);
        JS_FreeValue(ctx, obj);
        if (JS_IsException(tag))
            return JS_EXCEPTION;
        if (!JS_IsString(tag)) {
            JS_FreeValue(ctx, tag);
            tag = JS_AtomToString(ctx, atom);
        }
    }
    return qjs_concat_string3(ctx, "[object ", tag, "]");
}

static JSValue js_object_toLocaleString(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    return JS_Invoke(ctx, this_val, JS_ATOM_toString, 0, NULL);
}

static JSValue js_object_assign(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    // Object.assign(obj, source1)
    JSValue obj, s;
    int i;

    s = JS_UNDEFINED;
    obj = qjs_to_object(ctx, argv[0]);
    if (JS_IsException(obj))
        goto exception;
    for (i = 1; i < argc; i++) {
        if (!JS_IsNull(argv[i]) && !JS_IsUndefined(argv[i])) {
            s = qjs_to_object(ctx, argv[i]);
            if (JS_IsException(s))
                goto exception;
            if (qjs_copy_data_properties(ctx, obj, s, JS_UNDEFINED, TRUE))
                goto exception;
            JS_FreeValue(ctx, s);
        }
    }
    return obj;
exception:
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, s);
    return JS_EXCEPTION;
}

static JSValue js_object_seal(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int freeze_flag)
{
    JSValueConst obj = argv[0];
    JSObject *p;
    JSPropertyEnum *props;
    uint32_t len, i;
    int flags, desc_flags, res;

    if (!JS_IsObject(obj))
        return JS_DupValue(ctx, obj);

    res = JS_PreventExtensions(ctx, obj);
    if (res < 0)
        return JS_EXCEPTION;
    if (!res) {
        return JS_ThrowTypeError(ctx, "proxy preventExtensions handler returned false");
    }

    p = JS_VALUE_GET_OBJ(obj);
    flags = JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK;
    if (qjs_get_own_property_names_internal(ctx, &props, &len, p, flags))
        return JS_EXCEPTION;

    for(i = 0; i < len; i++) {
        JSPropertyDescriptor desc;
        JSAtom prop = props[i].atom;

        desc_flags = JS_PROP_THROW | JS_PROP_HAS_CONFIGURABLE;
        if (freeze_flag) {
            res = qjs_base_get_own_property_internal(ctx, &desc, p, prop);
            if (res < 0)
                goto exception;
            if (res) {
                if (desc.flags & JS_PROP_WRITABLE)
                    desc_flags |= JS_PROP_HAS_WRITABLE;
                qjs_base_free_desc(ctx, &desc);
            }
        }
        if (JS_DefineProperty(ctx, obj, prop, JS_UNDEFINED,
                              JS_UNDEFINED, JS_UNDEFINED, desc_flags) < 0)
            goto exception;
    }
    JS_FreePropertyEnum(ctx, props, len);
    return JS_DupValue(ctx, obj);

 exception:
    JS_FreePropertyEnum(ctx, props, len);
    return JS_EXCEPTION;
}

static JSValue js_object_isSealed(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int is_frozen)
{
    JSValueConst obj = argv[0];
    JSObject *p;
    JSPropertyEnum *props;
    uint32_t len, i;
    int flags, res;

    if (!JS_IsObject(obj))
        return JS_TRUE;

    p = JS_VALUE_GET_OBJ(obj);
    flags = JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK;
    if (qjs_get_own_property_names_internal(ctx, &props, &len, p, flags))
        return JS_EXCEPTION;

    for(i = 0; i < len; i++) {
        JSPropertyDescriptor desc;
        JSAtom prop = props[i].atom;

        res = qjs_base_get_own_property_internal(ctx, &desc, p, prop);
        if (res < 0)
            goto exception;
        if (res) {
            qjs_base_free_desc(ctx, &desc);
            if ((desc.flags & JS_PROP_CONFIGURABLE)
            ||  (is_frozen && (desc.flags & JS_PROP_WRITABLE))) {
                res = FALSE;
                goto done;
            }
        }
    }
    res = JS_IsExtensible(ctx, obj);
    if (res < 0)
        return JS_EXCEPTION;
    res ^= 1;
done:
    JS_FreePropertyEnum(ctx, props, len);
    return JS_NewBool(ctx, res);

exception:
    JS_FreePropertyEnum(ctx, props, len);
    return JS_EXCEPTION;
}

static JSValue js_object_fromEntries(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSValue obj, iter, next_method = JS_UNDEFINED;
    JSValueConst iterable;
    BOOL done;

    /*  RequireObjectCoercible() not necessary because it is tested in
        qjs_get_iterator() by JS_GetProperty() */
    iterable = argv[0];

    obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        return obj;

    iter = qjs_get_iterator(ctx, iterable, FALSE);
    if (JS_IsException(iter))
        goto fail;
    next_method = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next_method))
        goto fail;

    for(;;) {
        JSValue key, value, item;
        item = qjs_iterator_next(ctx, iter, next_method, 0, NULL, &done);
        if (JS_IsException(item))
            goto fail;
        if (done)
            break;

        key = JS_UNDEFINED;
        value = JS_UNDEFINED;
        if (!JS_IsObject(item)) {
            qjs_throw_type_error_not_object(ctx);
            goto fail1;
        }
        key = JS_GetPropertyUint32(ctx, item, 0);
        if (JS_IsException(key))
            goto fail1;
        value = JS_GetPropertyUint32(ctx, item, 1);
        if (JS_IsException(value)) {
            JS_FreeValue(ctx, key);
            goto fail1;
        }
        if (qjs_base_define_property_value(ctx, obj, key, value,
                                        JS_PROP_C_W_E | JS_PROP_THROW) < 0) {
        fail1:
            JS_FreeValue(ctx, item);
            goto fail;
        }
        JS_FreeValue(ctx, item);
    }
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    return obj;
 fail:
    if (JS_IsObject(iter)) {
        /* close the iterator object, preserving pending exception */
        qjs_iterator_close(ctx, iter, TRUE);
    }
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_object_is(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    return JS_NewBool(ctx, qjs_same_value(ctx, argv[0], argv[1]));
}

QJS_INTERNAL JSValue qjs_base_species_constructor(
    JSContext *ctx, JSValueConst obj, JSValueConst defaultConstructor)
{
    JSValue ctor, species;

    if (!JS_IsObject(obj))
        return qjs_throw_type_error_not_object(ctx);
    ctor = JS_GetProperty(ctx, obj, JS_ATOM_constructor);
    if (JS_IsException(ctor))
        return ctor;
    if (JS_IsUndefined(ctor))
        return JS_DupValue(ctx, defaultConstructor);
    if (!JS_IsObject(ctor)) {
        JS_FreeValue(ctx, ctor);
        return qjs_throw_type_error_not_object(ctx);
    }
    species = JS_GetProperty(ctx, ctor, JS_ATOM_Symbol_species);
    JS_FreeValue(ctx, ctor);
    if (JS_IsException(species))
        return species;
    if (JS_IsUndefined(species) || JS_IsNull(species))
        return JS_DupValue(ctx, defaultConstructor);
    if (!JS_IsConstructor(ctx, species)) {
        qjs_base_throw_not_constructor(ctx, species);
        JS_FreeValue(ctx, species);
        return JS_EXCEPTION;
    }
    return species;
}

static JSValue js_object_get___proto__(JSContext *ctx, JSValueConst this_val)
{
    JSValue val, ret;

    val = qjs_to_object(ctx, this_val);
    if (JS_IsException(val))
        return val;
    ret = JS_GetPrototype(ctx, val);
    JS_FreeValue(ctx, val);
    return ret;
}

static JSValue js_object_set___proto__(JSContext *ctx, JSValueConst this_val,
                                       JSValueConst proto)
{
    if (JS_IsUndefined(this_val) || JS_IsNull(this_val))
        return qjs_throw_type_error_not_object(ctx);
    if (!JS_IsObject(proto) && !JS_IsNull(proto))
        return JS_UNDEFINED;
    if (qjs_base_set_prototype_internal(ctx, this_val, proto, TRUE) < 0)
        return JS_EXCEPTION;
    else
        return JS_UNDEFINED;
}

static JSValue js_object_isPrototypeOf(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValue obj, v1;
    JSValueConst v;
    int res;

    v = argv[0];
    if (!JS_IsObject(v))
        return JS_FALSE;
    obj = qjs_to_object(ctx, this_val);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    v1 = JS_DupValue(ctx, v);
    for(;;) {
        v1 = qjs_base_get_prototype_free(ctx, v1);
        if (JS_IsException(v1))
            goto exception;
        if (JS_IsNull(v1)) {
            res = FALSE;
            break;
        }
        if (JS_VALUE_GET_OBJ(obj) == JS_VALUE_GET_OBJ(v1)) {
            res = TRUE;
            break;
        }
        /* avoid infinite loop (possible with proxies) */
        if (qjs_base_poll_interrupts(ctx))
            goto exception;
    }
    JS_FreeValue(ctx, v1);
    JS_FreeValue(ctx, obj);
    return JS_NewBool(ctx, res);

exception:
    JS_FreeValue(ctx, v1);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_object_propertyIsEnumerable(JSContext *ctx, JSValueConst this_val,
                                              int argc, JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED, res = JS_EXCEPTION;
    JSAtom prop;
    JSPropertyDescriptor desc;
    int has_prop;

    prop = JS_ValueToAtom(ctx, argv[0]);
    if (unlikely(prop == JS_ATOM_NULL))
        goto exception;
    obj = qjs_to_object(ctx, this_val);
    if (JS_IsException(obj))
        goto exception;

    has_prop = qjs_base_get_own_property_internal(ctx, &desc, JS_VALUE_GET_OBJ(obj), prop);
    if (has_prop < 0)
        goto exception;
    if (has_prop) {
        res = JS_NewBool(ctx, desc.flags & JS_PROP_ENUMERABLE);
        qjs_base_free_desc(ctx, &desc);
    } else {
        res = JS_FALSE;
    }

exception:
    JS_FreeAtom(ctx, prop);
    JS_FreeValue(ctx, obj);
    return res;
}

static JSValue js_object___lookupGetter__(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv, int setter)
{
    JSValue obj, res = JS_EXCEPTION;
    JSAtom prop = JS_ATOM_NULL;
    JSPropertyDescriptor desc;
    int has_prop;

    obj = qjs_to_object(ctx, this_val);
    if (JS_IsException(obj))
        goto exception;
    prop = JS_ValueToAtom(ctx, argv[0]);
    if (unlikely(prop == JS_ATOM_NULL))
        goto exception;

    for (;;) {
        has_prop = qjs_base_get_own_property_internal(ctx, &desc, JS_VALUE_GET_OBJ(obj), prop);
        if (has_prop < 0)
            goto exception;
        if (has_prop) {
            if (desc.flags & JS_PROP_GETSET)
                res = JS_DupValue(ctx, setter ? desc.setter : desc.getter);
            else
                res = JS_UNDEFINED;
            qjs_base_free_desc(ctx, &desc);
            break;
        }
        obj = qjs_base_get_prototype_free(ctx, obj);
        if (JS_IsException(obj))
            goto exception;
        if (JS_IsNull(obj)) {
            res = JS_UNDEFINED;
            break;
        }
        /* avoid infinite loop (possible with proxies) */
        if (qjs_base_poll_interrupts(ctx))
            goto exception;
    }

exception:
    JS_FreeAtom(ctx, prop);
    JS_FreeValue(ctx, obj);
    return res;
}

static const JSCFunctionListEntry js_object_funcs[] = {
    JS_CFUNC_DEF("create", 2, js_object_create ),
    JS_CFUNC_MAGIC_DEF("getPrototypeOf", 1, js_object_getPrototypeOf, 0 ),
    JS_CFUNC_DEF("setPrototypeOf", 2, js_object_setPrototypeOf ),
    JS_CFUNC_MAGIC_DEF("defineProperty", 3, js_object_defineProperty, 0 ),
    JS_CFUNC_DEF("defineProperties", 2, js_object_defineProperties ),
    JS_CFUNC_DEF("getOwnPropertyNames", 1, js_object_getOwnPropertyNames ),
    JS_CFUNC_DEF("getOwnPropertySymbols", 1, js_object_getOwnPropertySymbols ),
    JS_CFUNC_MAGIC_DEF("groupBy", 2, qjs_object_group_by, 0 ),
    JS_CFUNC_MAGIC_DEF("keys", 1, js_object_keys, JS_ITERATOR_KIND_KEY ),
    JS_CFUNC_MAGIC_DEF("values", 1, js_object_keys, JS_ITERATOR_KIND_VALUE ),
    JS_CFUNC_MAGIC_DEF("entries", 1, js_object_keys, JS_ITERATOR_KIND_KEY_AND_VALUE ),
    JS_CFUNC_MAGIC_DEF("isExtensible", 1, js_object_isExtensible, 0 ),
    JS_CFUNC_MAGIC_DEF("preventExtensions", 1, js_object_preventExtensions, 0 ),
    JS_CFUNC_MAGIC_DEF("getOwnPropertyDescriptor", 2, js_object_getOwnPropertyDescriptor, 0 ),
    JS_CFUNC_DEF("getOwnPropertyDescriptors", 1, js_object_getOwnPropertyDescriptors ),
    JS_CFUNC_DEF("is", 2, js_object_is ),
    JS_CFUNC_DEF("assign", 2, js_object_assign ),
    JS_CFUNC_MAGIC_DEF("seal", 1, js_object_seal, 0 ),
    JS_CFUNC_MAGIC_DEF("freeze", 1, js_object_seal, 1 ),
    JS_CFUNC_MAGIC_DEF("isSealed", 1, js_object_isSealed, 0 ),
    JS_CFUNC_MAGIC_DEF("isFrozen", 1, js_object_isSealed, 1 ),
    JS_CFUNC_DEF("fromEntries", 1, js_object_fromEntries ),
    JS_CFUNC_DEF("hasOwn", 2, js_object_hasOwn ),
};

static const JSCFunctionListEntry js_object_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_object_toString ),
    JS_CFUNC_DEF("toLocaleString", 0, js_object_toLocaleString ),
    JS_CFUNC_DEF("valueOf", 0, js_object_valueOf ),
    JS_CFUNC_DEF("hasOwnProperty", 1, js_object_hasOwnProperty ),
    JS_CFUNC_DEF("isPrototypeOf", 1, js_object_isPrototypeOf ),
    JS_CFUNC_DEF("propertyIsEnumerable", 1, js_object_propertyIsEnumerable ),
    JS_CGETSET_DEF("__proto__", js_object_get___proto__, js_object_set___proto__ ),
    JS_CFUNC_MAGIC_DEF("__defineGetter__", 2, js_object___defineGetter__, 0 ),
    JS_CFUNC_MAGIC_DEF("__defineSetter__", 2, js_object___defineGetter__, 1 ),
    JS_CFUNC_MAGIC_DEF("__lookupGetter__", 1, js_object___lookupGetter__, 0 ),
    JS_CFUNC_MAGIC_DEF("__lookupSetter__", 1, js_object___lookupGetter__, 1 ),
};

/* Function class */

static JSValue js_function_proto(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    return JS_UNDEFINED;
}

/* XXX: add a specific eval mode so that Function("}), ({") is rejected */
static JSFunctionBytecode *js_base_get_function_bytecode(JSValueConst value)
{
    JSObject *obj;

    if (!JS_IsObject(value))
        return NULL;
    obj = JS_VALUE_GET_OBJ(value);
    if (!qjs_class_has_bytecode(obj->class_id))
        return NULL;
    return obj->u.func.function_bytecode;
}

static JSValue js_throw_type_error(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSFunctionBytecode *b = js_base_get_function_bytecode(this_val);
    if (!b || (b->js_mode & JS_MODE_STRICT) || !b->has_prototype || argc >= 1) {
        return JS_ThrowTypeError(ctx, "invalid property access");
    }
    return JS_UNDEFINED;
}

static JSValue js_function_proto_fileName(
    JSContext *ctx, JSValueConst this_val)
{
    JSFunctionBytecode *b = js_base_get_function_bytecode(this_val);
    if (b && b->has_debug) {
        return JS_AtomToString(ctx, b->debug.filename);
    }
    return JS_UNDEFINED;
}

static JSValue js_function_proto_lineNumber(
    JSContext *ctx, JSValueConst this_val, int is_col)
{
    JSFunctionBytecode *b = js_base_get_function_bytecode(this_val);
    if (b && b->has_debug) {
        int line_num, col_num;
        line_num = qjs_find_line_num(ctx, b, -1, &col_num);
        if (is_col)
            return JS_NewInt32(ctx, col_num);
        else
            return JS_NewInt32(ctx, line_num);
    }
    return JS_UNDEFINED;
}

QJS_INTERNAL JSValue qjs_base_function_constructor(
    JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv,
    int magic)
{
    JSFunctionKindEnum func_kind = magic;
    int i, n, ret;
    JSValue s, proto, obj = JS_UNDEFINED;
    StringBuffer b_s, *b = &b_s;

    qjs_string_buffer_init(ctx, b, 0);
    qjs_string_buffer_putc8(b, '(');

    if (func_kind == JS_FUNC_ASYNC || func_kind == JS_FUNC_ASYNC_GENERATOR) {
        qjs_string_buffer_puts8(b, "async ");
    }
    qjs_string_buffer_puts8(b, "function");

    if (func_kind == JS_FUNC_GENERATOR || func_kind == JS_FUNC_ASYNC_GENERATOR) {
        qjs_string_buffer_putc8(b, '*');
    }
    qjs_string_buffer_puts8(b, " anonymous(");

    n = argc - 1;
    for(i = 0; i < n; i++) {
        if (i != 0) {
            qjs_string_buffer_putc8(b, ',');
        }
        if (qjs_string_buffer_concat_value(b, argv[i]))
            goto fail;
    }
    qjs_string_buffer_puts8(b, "\n) {\n");
    if (n >= 0) {
        if (qjs_string_buffer_concat_value(b, argv[n]))
            goto fail;
    }
    qjs_string_buffer_puts8(b, "\n})");
    s = qjs_string_buffer_end(b);
    if (JS_IsException(s))
        goto fail1;

    obj = qjs_eval_object(ctx, ctx->global_obj, s,
                          JS_EVAL_TYPE_INDIRECT, -1);
    JS_FreeValue(ctx, s);
    if (JS_IsException(obj))
        goto fail1;
    if (!JS_IsUndefined(new_target)) {
        /* set the prototype */
        proto = JS_GetProperty(ctx, new_target, JS_ATOM_prototype);
        if (JS_IsException(proto))
            goto fail1;
        if (!JS_IsObject(proto)) {
            JSContext *realm;
            JS_FreeValue(ctx, proto);
            realm = qjs_get_function_realm(ctx, new_target);
            if (!realm)
                goto fail1;
            proto = JS_DupValue(ctx, realm->class_proto[qjs_base_function_class_id(func_kind)]);
        }
        ret = qjs_base_set_prototype_internal(ctx, obj, proto, TRUE);
        JS_FreeValue(ctx, proto);
        if (ret < 0)
            goto fail1;
    }
    return obj;

 fail:
    qjs_string_buffer_free(b);
 fail1:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

QJS_INTERNAL int qjs_get_length32(JSContext *ctx, uint32_t *pres,
                                  JSValueConst obj)
{
    JSValue len_val;
    len_val = JS_GetProperty(ctx, obj, JS_ATOM_length);
    if (JS_IsException(len_val)) {
        *pres = 0;
        return -1;
    }
    return qjs_to_uint32_free(ctx, pres, len_val);
}

QJS_INTERNAL int qjs_get_length64(JSContext *ctx, int64_t *pres,
                                  JSValueConst obj)
{
    JSValue len_val;
    len_val = JS_GetProperty(ctx, obj, JS_ATOM_length);
    if (JS_IsException(len_val)) {
        *pres = 0;
        return -1;
    }
    return qjs_to_length_free(ctx, pres, len_val);
}

QJS_INTERNAL void qjs_base_free_arg_list(JSContext *ctx, JSValue *tab,
                                         uint32_t len)
{
    uint32_t i;
    for(i = 0; i < len; i++) {
        JS_FreeValue(ctx, tab[i]);
    }
    js_free(ctx, tab);
}

/* XXX: should use ValueArray */
QJS_INTERNAL JSValue *qjs_base_build_arg_list(
    JSContext *ctx, uint32_t *plen, JSValueConst array_arg)
{
    uint32_t len, i;
    int64_t len64;
    JSValue *tab, ret;
    JSObject *p;

    if (JS_VALUE_GET_TAG(array_arg) != JS_TAG_OBJECT) {
        JS_ThrowTypeError(ctx, "not a object");
        return NULL;
    }
    if (qjs_get_length64(ctx, &len64, array_arg))
        return NULL;
    if (len64 > JS_MAX_LOCAL_VARS) {
        // XXX: check for stack overflow?
        JS_ThrowRangeError(ctx, "too many arguments in function call (only %d allowed)",
                           JS_MAX_LOCAL_VARS);
        return NULL;
    }
    len = len64;
    /* avoid allocating 0 bytes */
    tab = js_mallocz(ctx, sizeof(tab[0]) * max_uint32(1, len));
    if (!tab)
        return NULL;
    p = JS_VALUE_GET_OBJ(array_arg);
    if ((p->class_id == JS_CLASS_ARRAY || p->class_id == JS_CLASS_ARGUMENTS || p->class_id == JS_CLASS_MAPPED_ARGUMENTS) &&
        p->fast_array &&
        len == p->u.array.count) {
        if (p->class_id == JS_CLASS_MAPPED_ARGUMENTS) {
            for(i = 0; i < len; i++) {
                tab[i] = JS_DupValue(ctx, *p->u.array.u.var_refs[i]->pvalue);
            }
        } else {
            for(i = 0; i < len; i++) {
                tab[i] = JS_DupValue(ctx, p->u.array.u.values[i]);
            }
        }
    } else {
        for(i = 0; i < len; i++) {
            ret = JS_GetPropertyUint32(ctx, array_arg, i);
            if (JS_IsException(ret)) {
                qjs_base_free_arg_list(ctx, tab, i);
                return NULL;
            }
            tab[i] = ret;
        }
    }
    *plen = len;
    return tab;
}

/* magic value: 0 = normal apply, 1 = apply for constructor, 2 =
   Reflect.apply */
QJS_INTERNAL JSValue qjs_base_function_apply(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
    int magic)
{
    JSValueConst this_arg, array_arg;
    uint32_t len;
    JSValue *tab, ret;

    if (qjs_check_function(ctx, this_val))
        return JS_EXCEPTION;
    this_arg = argv[0];
    array_arg = argv[1];
    if ((JS_VALUE_GET_TAG(array_arg) == JS_TAG_UNDEFINED ||
         JS_VALUE_GET_TAG(array_arg) == JS_TAG_NULL) && magic != 2) {
        return JS_Call(ctx, this_val, this_arg, 0, NULL);
    }
    tab = qjs_base_build_arg_list(ctx, &len, array_arg);
    if (!tab)
        return JS_EXCEPTION;
    if (magic & 1) {
        ret = JS_CallConstructor2(ctx, this_val, this_arg, len, (JSValueConst *)tab);
    } else {
        ret = JS_Call(ctx, this_val, this_arg, len, (JSValueConst *)tab);
    }
    qjs_base_free_arg_list(ctx, tab, len);
    return ret;
}

static JSValue js_function_call(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    if (argc <= 0) {
        return JS_Call(ctx, this_val, JS_UNDEFINED, 0, NULL);
    } else {
        return JS_Call(ctx, this_val, argv[0], argc - 1, argv + 1);
    }
}

static JSValue js_function_bind(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSBoundFunction *bf;
    JSValue func_obj, name1, len_val;
    JSObject *p;
    int arg_count, i, ret;

    if (qjs_check_function(ctx, this_val))
        return JS_EXCEPTION;

    func_obj = JS_NewObjectProtoClass(ctx, ctx->function_proto,
                                 JS_CLASS_BOUND_FUNCTION);
    if (JS_IsException(func_obj))
        return JS_EXCEPTION;
    p = JS_VALUE_GET_OBJ(func_obj);
    p->is_constructor = JS_IsConstructor(ctx, this_val);
    arg_count = max_int(0, argc - 1);
    bf = js_malloc(ctx, sizeof(*bf) + arg_count * sizeof(JSValue));
    if (!bf)
        goto exception;
    bf->func_obj = JS_DupValue(ctx, this_val);
    bf->this_val = JS_DupValue(ctx, argv[0]);
    bf->argc = arg_count;
    for(i = 0; i < arg_count; i++) {
        bf->argv[i] = JS_DupValue(ctx, argv[i + 1]);
    }
    p->u.bound_function = bf;

    /* XXX: the spec could be simpler by only using GetOwnProperty */
    ret = JS_GetOwnProperty(ctx, NULL, this_val, JS_ATOM_length);
    if (ret < 0)
        goto exception;
    if (!ret) {
        len_val = JS_NewInt32(ctx, 0);
    } else {
        len_val = JS_GetProperty(ctx, this_val, JS_ATOM_length);
        if (JS_IsException(len_val))
            goto exception;
        if (JS_VALUE_GET_TAG(len_val) == JS_TAG_INT) {
            /* most common case */
            int len1 = JS_VALUE_GET_INT(len_val);
            if (len1 <= arg_count)
                len1 = 0;
            else
                len1 -= arg_count;
            len_val = JS_NewInt32(ctx, len1);
        } else if (JS_VALUE_GET_NORM_TAG(len_val) == JS_TAG_FLOAT64) {
            double d = JS_VALUE_GET_FLOAT64(len_val);
            if (isnan(d)) {
                d = 0.0;
            } else {
                d = trunc(d);
                if (d <= (double)arg_count)
                    d = 0.0;
                else
                    d -= (double)arg_count; /* also converts -0 to +0 */
            }
            len_val = JS_NewFloat64(ctx, d);
        } else {
            JS_FreeValue(ctx, len_val);
            len_val = JS_NewInt32(ctx, 0);
        }
    }
    JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_length,
                           len_val, JS_PROP_CONFIGURABLE);

    name1 = JS_GetProperty(ctx, this_val, JS_ATOM_name);
    if (JS_IsException(name1))
        goto exception;
    if (!JS_IsString(name1)) {
        JS_FreeValue(ctx, name1);
        name1 = JS_AtomToString(ctx, JS_ATOM_empty_string);
    }
    name1 = qjs_concat_string3(ctx, "bound ", name1, "");
    if (JS_IsException(name1))
        goto exception;
    JS_DefinePropertyValue(ctx, func_obj, JS_ATOM_name, name1,
                           JS_PROP_CONFIGURABLE);
    return func_obj;
 exception:
    JS_FreeValue(ctx, func_obj);
    return JS_EXCEPTION;
}

static JSValue js_function_toString(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSObject *p;
    JSFunctionKindEnum func_kind = JS_FUNC_NORMAL;

    if (qjs_check_function(ctx, this_val))
        return JS_EXCEPTION;

    p = JS_VALUE_GET_OBJ(this_val);
    if (qjs_class_has_bytecode(p->class_id)) {
        JSFunctionBytecode *b = p->u.func.function_bytecode;
        if (b->has_debug && b->debug.source) {
            return JS_NewStringLen(ctx, b->debug.source, b->debug.source_len);
        }
        func_kind = b->func_kind;
    }
    {
        JSValue name;
        const char *pref, *suff;

        switch(func_kind) {
        default:
        case JS_FUNC_NORMAL:
            pref = "function ";
            break;
        case JS_FUNC_GENERATOR:
            pref = "function *";
            break;
        case JS_FUNC_ASYNC:
            pref = "async function ";
            break;
        case JS_FUNC_ASYNC_GENERATOR:
            pref = "async function *";
            break;
        }
        suff = "() {\n    [native code]\n}";
        name = JS_GetProperty(ctx, this_val, JS_ATOM_name);
        if (JS_IsUndefined(name))
            name = JS_AtomToString(ctx, JS_ATOM_empty_string);
        return qjs_concat_string3(ctx, pref, name, suff);
    }
}

static JSValue js_function_hasInstance(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    int ret;
    ret = qjs_ordinary_is_instance_of(ctx, argv[0], this_val);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

static const JSCFunctionListEntry js_function_proto_funcs[] = {
    JS_CFUNC_DEF("call", 1, js_function_call ),
    JS_CFUNC_MAGIC_DEF("apply", 2, qjs_base_function_apply, 0 ),
    JS_CFUNC_DEF("bind", 1, js_function_bind ),
    JS_CFUNC_DEF("toString", 0, js_function_toString ),
    JS_CFUNC_DEF("[Symbol.hasInstance]", 1, js_function_hasInstance ),
    JS_CGETSET_DEF("fileName", js_function_proto_fileName, NULL ),
    JS_CGETSET_MAGIC_DEF("lineNumber", js_function_proto_lineNumber, NULL, 0 ),
    JS_CGETSET_MAGIC_DEF("columnNumber", js_function_proto_lineNumber, NULL, 1 ),
};

/* Error class */

static JSValue iterator_to_array(JSContext *ctx, JSValueConst items)
{
    JSValue iter, next_method = JS_UNDEFINED;
    JSValue v, r = JS_UNDEFINED;
    int64_t k;
    BOOL done;

    iter = qjs_get_iterator(ctx, items, FALSE);
    if (JS_IsException(iter))
        goto exception;
    next_method = JS_GetProperty(ctx, iter, JS_ATOM_next);
    if (JS_IsException(next_method))
        goto exception;
    r = JS_NewArray(ctx);
    if (JS_IsException(r))
        goto exception;
    for (k = 0;; k++) {
        v = qjs_iterator_next(ctx, iter, next_method, 0, NULL, &done);
        if (JS_IsException(v))
            goto exception_close;
        if (done)
            break;
        if (qjs_define_property_value_int64(ctx, r, k, v,
                                        JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto exception_close;
    }
 done:
    JS_FreeValue(ctx, next_method);
    JS_FreeValue(ctx, iter);
    return r;
 exception_close:
    qjs_iterator_close(ctx, iter, TRUE);
 exception:
    JS_FreeValue(ctx, r);
    r = JS_EXCEPTION;
    goto done;
}

static JSValue js_error_constructor(JSContext *ctx, JSValueConst new_target,
                                    int argc, JSValueConst *argv, int magic)
{
    JSValue obj, msg, proto;
    JSValueConst message, options;
    int arg_index;

    if (JS_IsUndefined(new_target))
        new_target = qjs_base_get_active_function(ctx);
    proto = JS_GetProperty(ctx, new_target, JS_ATOM_prototype);
    if (JS_IsException(proto))
        return proto;
    if (!JS_IsObject(proto)) {
        JSContext *realm;
        JSValueConst proto1;

        JS_FreeValue(ctx, proto);
        realm = qjs_get_function_realm(ctx, new_target);
        if (!realm)
            return JS_EXCEPTION;
        if (magic < 0) {
            proto1 = realm->class_proto[JS_CLASS_ERROR];
        } else {
            proto1 = realm->native_error_proto[magic];
        }
        proto = JS_DupValue(ctx, proto1);
    }
    obj = JS_NewObjectProtoClass(ctx, proto, JS_CLASS_ERROR);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        return obj;
    arg_index = (magic == JS_AGGREGATE_ERROR);

    message = argv[arg_index++];
    if (!JS_IsUndefined(message)) {
        msg = JS_ToString(ctx, message);
        if (unlikely(JS_IsException(msg)))
            goto exception;
        JS_DefinePropertyValue(ctx, obj, JS_ATOM_message, msg,
                               JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    }

    if (arg_index < argc) {
        options = argv[arg_index];
        if (JS_IsObject(options)) {
            int present = JS_HasProperty(ctx, options, JS_ATOM_cause);
            if (present < 0)
                goto exception;
            if (present) {
                JSValue cause = JS_GetProperty(ctx, options, JS_ATOM_cause);
                if (JS_IsException(cause))
                    goto exception;
                JS_DefinePropertyValue(ctx, obj, JS_ATOM_cause, cause,
                                       JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
            }
        }
    }

    if (magic == JS_AGGREGATE_ERROR) {
        JSValue error_list = iterator_to_array(ctx, argv[0]);
        if (JS_IsException(error_list))
            goto exception;
        JS_DefinePropertyValue(ctx, obj, JS_ATOM_errors, error_list,
                               JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    }

    /* skip the Error() function in the backtrace */
    qjs_build_backtrace(ctx, obj, NULL, 0, 0, QJS_BACKTRACE_SKIP_FIRST_LEVEL);
    return obj;
 exception:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

QJS_INTERNAL JSValue qjs_base_error_to_string(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue name, msg;

    if (!JS_IsObject(this_val))
        return qjs_throw_type_error_not_object(ctx);
    name = JS_GetProperty(ctx, this_val, JS_ATOM_name);
    if (JS_IsUndefined(name))
        name = JS_AtomToString(ctx, JS_ATOM_Error);
    else
        name = qjs_to_string_free(ctx, name);
    if (JS_IsException(name))
        return JS_EXCEPTION;

    msg = JS_GetProperty(ctx, this_val, JS_ATOM_message);
    if (JS_IsUndefined(msg))
        msg = JS_AtomToString(ctx, JS_ATOM_empty_string);
    else
        msg = qjs_to_string_free(ctx, msg);
    if (JS_IsException(msg)) {
        JS_FreeValue(ctx, name);
        return JS_EXCEPTION;
    }
    if (!qjs_is_empty_string(name) && !qjs_is_empty_string(msg))
        name = qjs_concat_string3(ctx, "", name, ": ");
    return qjs_concat_string(ctx, name, msg);
}

static const JSCFunctionListEntry js_error_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, qjs_base_error_to_string ),
    JS_PROP_STRING_DEF("name", "Error", JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE ),
    JS_PROP_STRING_DEF("message", "", JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE ),
};

/* 2 entries for each native error class */
/* Note: we use an atom to avoid the autoinit definition which does
   not work in get_prop_string() */
static const JSCFunctionListEntry js_native_error_proto_funcs[] = {
#define DEF(name) \
    JS_PROP_ATOM_DEF("name", name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE ),\
    JS_PROP_STRING_DEF("message", "", JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE ),

    DEF(JS_ATOM_EvalError)
    DEF(JS_ATOM_RangeError)
    DEF(JS_ATOM_ReferenceError)
    DEF(JS_ATOM_SyntaxError)
    DEF(JS_ATOM_TypeError)
    DEF(JS_ATOM_URIError)
    DEF(JS_ATOM_InternalError)
    DEF(JS_ATOM_AggregateError)
#undef DEF
};

static JSValue js_error_isError(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    return JS_NewBool(ctx, JS_IsError(ctx, argv[0]));
}

static const JSCFunctionListEntry js_error_funcs[] = {
    JS_CFUNC_DEF("isError", 1, js_error_isError),
};

/* AggregateError */

/* used by C code. */
QJS_INTERNAL JSValue qjs_base_aggregate_error(JSContext *ctx,
                                              JSValueConst errors)
{
    JSValue obj;

    obj = JS_NewObjectProtoClass(ctx,
                                 ctx->native_error_proto[JS_AGGREGATE_ERROR],
                                 JS_CLASS_ERROR);
    if (JS_IsException(obj))
        return obj;
    JS_DefinePropertyValue(ctx, obj, JS_ATOM_errors, JS_DupValue(ctx, errors),
                           JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    return obj;
}

/* Reflect */

static JSValue js_reflect_apply(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    return qjs_base_function_apply(ctx, argv[0], max_int(0, argc - 1), argv + 1, 2);
}

static JSValue js_reflect_construct(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSValueConst func, array_arg, new_target;
    JSValue *tab, ret;
    uint32_t len;

    func = argv[0];
    array_arg = argv[1];
    if (argc > 2) {
        new_target = argv[2];
        if (!JS_IsConstructor(ctx, new_target))
            return qjs_base_throw_not_constructor(ctx, new_target);
    } else {
        new_target = func;
    }
    tab = qjs_base_build_arg_list(ctx, &len, array_arg);
    if (!tab)
        return JS_EXCEPTION;
    ret = JS_CallConstructor2(ctx, func, new_target, len, (JSValueConst *)tab);
    qjs_base_free_arg_list(ctx, tab, len);
    return ret;
}

static JSValue js_reflect_deleteProperty(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    JSValueConst obj;
    JSAtom atom;
    int ret;

    obj = argv[0];
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return qjs_throw_type_error_not_object(ctx);
    atom = JS_ValueToAtom(ctx, argv[1]);
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    ret = JS_DeleteProperty(ctx, obj, atom, 0);
    JS_FreeAtom(ctx, atom);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

static JSValue js_reflect_get(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValueConst obj, prop, receiver;
    JSAtom atom;
    JSValue ret;

    obj = argv[0];
    prop = argv[1];
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return qjs_throw_type_error_not_object(ctx);
    if (argc > 2)
        receiver = argv[2];
    else
        receiver = obj;
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    ret = JS_GetPropertyInternal(ctx, obj, atom, receiver, FALSE);
    JS_FreeAtom(ctx, atom);
    return ret;
}

static JSValue js_reflect_has(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValueConst obj, prop;
    JSAtom atom;
    int ret;

    obj = argv[0];
    prop = argv[1];
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return qjs_throw_type_error_not_object(ctx);
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    ret = JS_HasProperty(ctx, obj, atom);
    JS_FreeAtom(ctx, atom);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

static JSValue js_reflect_set(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValueConst obj, prop, val, receiver;
    int ret;
    JSAtom atom;

    obj = argv[0];
    prop = argv[1];
    val = argv[2];
    if (argc > 3)
        receiver = argv[3];
    else
        receiver = obj;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return qjs_throw_type_error_not_object(ctx);
    atom = JS_ValueToAtom(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL))
        return JS_EXCEPTION;
    ret = JS_SetPropertyInternal(ctx, obj, atom,
                                 JS_DupValue(ctx, val), receiver, 0);
    JS_FreeAtom(ctx, atom);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

static JSValue js_reflect_setPrototypeOf(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    int ret;
    ret = qjs_base_set_prototype_internal(ctx, argv[0], argv[1], FALSE);
    if (ret < 0)
        return JS_EXCEPTION;
    else
        return JS_NewBool(ctx, ret);
}

static JSValue js_reflect_ownKeys(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    if (JS_VALUE_GET_TAG(argv[0]) != JS_TAG_OBJECT)
        return qjs_throw_type_error_not_object(ctx);
    return JS_GetOwnPropertyNames2(ctx, argv[0],
                                   JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK,
                                   JS_ITERATOR_KIND_KEY);
}

static const JSCFunctionListEntry js_reflect_funcs[] = {
    JS_CFUNC_DEF("apply", 3, js_reflect_apply ),
    JS_CFUNC_DEF("construct", 2, js_reflect_construct ),
    JS_CFUNC_MAGIC_DEF("defineProperty", 3, js_object_defineProperty, 1 ),
    JS_CFUNC_DEF("deleteProperty", 2, js_reflect_deleteProperty ),
    JS_CFUNC_DEF("get", 2, js_reflect_get ),
    JS_CFUNC_MAGIC_DEF("getOwnPropertyDescriptor", 2, js_object_getOwnPropertyDescriptor, 1 ),
    JS_CFUNC_MAGIC_DEF("getPrototypeOf", 1, js_object_getPrototypeOf, 1 ),
    JS_CFUNC_DEF("has", 2, js_reflect_has ),
    JS_CFUNC_MAGIC_DEF("isExtensible", 1, js_object_isExtensible, 1 ),
    JS_CFUNC_DEF("ownKeys", 1, js_reflect_ownKeys ),
    JS_CFUNC_MAGIC_DEF("preventExtensions", 1, js_object_preventExtensions, 1 ),
    JS_CFUNC_DEF("set", 3, js_reflect_set ),
    JS_CFUNC_DEF("setPrototypeOf", 2, js_reflect_setPrototypeOf ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Reflect", JS_PROP_CONFIGURABLE ),
};

static const JSCFunctionListEntry js_reflect_obj[] = {
    JS_OBJECT_DEF("Reflect", js_reflect_funcs, countof(js_reflect_funcs), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE ),
};
/* Minimum amount of objects to be able to compile code and display
   error messages. */
QJS_INTERNAL int qjs_add_intrinsic_basic_objects(JSContext *ctx)
{
    JSValue obj;
    JSCFunctionType ft;
    int i;

    /* warning: ordering is tricky */
    ctx->class_proto[JS_CLASS_OBJECT] =
        qjs_object_proto_class_alloc(ctx, JS_NULL, JS_CLASS_OBJECT,
                                    countof(js_object_proto_funcs) + 1);
    if (JS_IsException(ctx->class_proto[JS_CLASS_OBJECT]))
        return -1;
    qjs_base_set_immutable_prototype(ctx, ctx->class_proto[JS_CLASS_OBJECT]);

    /* 2 more properties: caller and arguments */
    ctx->function_proto = qjs_new_c_function3(ctx, js_function_proto, "", 0,
                                           JS_CFUNC_generic, 0,
                                           ctx->class_proto[JS_CLASS_OBJECT],
                                           countof(js_function_proto_funcs) + 3 + 2);
    if (JS_IsException(ctx->function_proto))
        return -1;
    ctx->class_proto[JS_CLASS_BYTECODE_FUNCTION] = JS_DupValue(ctx, ctx->function_proto);

    ctx->global_obj = qjs_object_proto_class_alloc(ctx, ctx->class_proto[JS_CLASS_OBJECT],
                                                  JS_CLASS_GLOBAL_OBJECT, 64);
    if (JS_IsException(ctx->global_obj))
        return -1;
    {
        JSObject *p;
        obj = qjs_object_proto_class_alloc(ctx, JS_NULL, JS_CLASS_OBJECT, 4);
        p = JS_VALUE_GET_OBJ(ctx->global_obj);
        p->u.global_object.uninitialized_vars = obj;
    }
    ctx->global_var_obj = qjs_object_proto_class_alloc(ctx, JS_NULL,
                                                      JS_CLASS_OBJECT, 16);
    if (JS_IsException(ctx->global_var_obj))
        return -1;

    /* Error */
    ft.generic_magic = js_error_constructor;
    obj = qjs_new_c_constructor(ctx, JS_CLASS_ERROR, "Error",
                                    ft.generic, 1, JS_CFUNC_constructor_or_func_magic, -1,
                                    JS_UNDEFINED,
                                    js_error_funcs, countof(js_error_funcs),
                                    js_error_proto_funcs, countof(js_error_proto_funcs),
                                    0);
    if (JS_IsException(obj))
        return -1;

    for(i = 0; i < JS_NATIVE_ERROR_COUNT; i++) {
        JSValue func_obj;
        const JSCFunctionListEntry *funcs;
        int n_args;
        char buf[ATOM_GET_STR_BUF_SIZE];
        const char *name = qjs_atom_get_str(ctx, buf, sizeof(buf),
                                         JS_ATOM_EvalError + i);
        n_args = 1 + (i == JS_AGGREGATE_ERROR);
        funcs = js_native_error_proto_funcs + 2 * i;
        func_obj = qjs_new_c_constructor(ctx, -1, name,
                                      ft.generic, n_args, JS_CFUNC_constructor_or_func_magic, i,
                                      obj,
                                      NULL, 0,
                                      funcs, 2,
                                      0);
        if (JS_IsException(func_obj)) {
            JS_FreeValue(ctx, obj);
            return -1;
        }
        ctx->native_error_proto[i] = JS_GetProperty(ctx, func_obj, JS_ATOM_prototype);
        JS_FreeValue(ctx, func_obj);
        if (JS_IsException(ctx->native_error_proto[i])) {
            JS_FreeValue(ctx, obj);
            return -1;
        }
    }
    JS_FreeValue(ctx, obj);

    if (qjs_add_intrinsic_array_basic(ctx))
        return -1;
    return 0;
}

int JS_AddIntrinsicBaseObjects(JSContext *ctx)
{
    JSValue obj1;
    JSCFunctionType ft;

    ctx->throw_type_error = JS_NewCFunction(ctx, js_throw_type_error, NULL, 0);
    if (JS_IsException(ctx->throw_type_error))
        return -1;
    /* add caller and arguments properties to throw a TypeError */
    if (JS_DefineProperty(ctx, ctx->function_proto, JS_ATOM_caller, JS_UNDEFINED,
                          ctx->throw_type_error, ctx->throw_type_error,
                          JS_PROP_HAS_GET | JS_PROP_HAS_SET |
                          JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE) < 0)
        return -1;
    if (JS_DefineProperty(ctx, ctx->function_proto, JS_ATOM_arguments, JS_UNDEFINED,
                          ctx->throw_type_error, ctx->throw_type_error,
                          JS_PROP_HAS_GET | JS_PROP_HAS_SET |
                          JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE) < 0)
        return -1;
    JS_FreeValue(ctx, js_object_seal(ctx, JS_UNDEFINED, 1, (JSValueConst *)&ctx->throw_type_error, 1));

    /* Object */
    obj1 = qjs_new_c_constructor(ctx, JS_CLASS_OBJECT, "Object",
                              js_object_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                              JS_UNDEFINED,
                              js_object_funcs, countof(js_object_funcs),
                              js_object_proto_funcs, countof(js_object_proto_funcs),
                              JS_NEW_CTOR_PROTO_EXIST);
    if (JS_IsException(obj1))
        return -1;
    JS_FreeValue(ctx, obj1);

    /* Function */
    ft.generic_magic = qjs_base_function_constructor;
    obj1 = qjs_new_c_constructor(ctx, JS_CLASS_BYTECODE_FUNCTION, "Function",
                              ft.generic, 1, JS_CFUNC_constructor_or_func_magic, JS_FUNC_NORMAL,
                              JS_UNDEFINED,
                              NULL, 0,
                              js_function_proto_funcs, countof(js_function_proto_funcs),
                              JS_NEW_CTOR_PROTO_EXIST);
    if (JS_IsException(obj1))
        return -1;
    ctx->function_ctor = obj1;

    if (qjs_add_intrinsic_iterators(ctx))
        return -1;

    /* parseFloat and parseInteger must be defined before Number
       because of the Number.parseFloat and Number.parseInteger
       aliases */
    if (qjs_add_intrinsic_global(ctx))
        return -1;

    if (qjs_add_intrinsic_number_boolean_string(ctx))
        return -1;

    /* Math: create as autoinit object */
    if (qjs_add_intrinsic_math(ctx))
        return -1;

    /* ES6 Reflect: create as autoinit object */
    if (JS_SetPropertyFunctionList(ctx, ctx->global_obj, js_reflect_obj, countof(js_reflect_obj)))
        return -1;

    if (qjs_add_intrinsic_symbol(ctx))
        return -1;

    /* ES6 Generator */
    if (qjs_add_intrinsic_generator(ctx))
        return -1;

    /* global properties */
    ctx->eval_obj = JS_GetProperty(ctx, ctx->global_obj, JS_ATOM_eval);
    if (JS_IsException(ctx->eval_obj))
        return -1;

    if (JS_DefinePropertyValue(ctx, ctx->global_obj, JS_ATOM_globalThis,
                               JS_DupValue(ctx, ctx->global_obj),
                               JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE) < 0)
        return -1;

    if (qjs_add_intrinsic_bigint(ctx))
        return -1;
    return 0;
}

QJS_INTERNAL JSValue qjs_json_object_keys(JSContext *ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst *argv,
                                          int magic)
{
    return js_object_keys(ctx, this_val, argc, argv, magic);
}

QJS_INTERNAL JSValue qjs_array_object_to_string(JSContext *ctx,
                                                JSValueConst this_val,
                                                int argc,
                                                JSValueConst *argv)
{
    return js_object_toString(ctx, this_val, argc, argv);
}
