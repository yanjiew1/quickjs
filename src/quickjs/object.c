/*
 * QuickJS Object & Property Subsystem
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "cutils.h"
#include "list.h"
#include "quickjs.h"
#include "libregexp.h"
#include "libunicode.h"
#include "dtoa.h"

#include "quickjs/def.h"
#include "quickjs/string.h"
#include "quickjs/atom.h"
#include "quickjs/shape.h"
#include "quickjs/function.h"
#include "quickjs/value.h"
#include "quickjs/object.h"
#include "quickjs/module.h"
#include "quickjs/promise.h"
#include "quickjs/runtime.h"
#include "quickjs/conversion.h"
#include "quickjs/array.h"
#include "quickjs/bigint.h"
#include "quickjs/operator.h"
#include "quickjs/alloc.h"
#include "quickjs/gc.h"
#include "quickjs/builtin/builtin.h"
#include "quickjs/vm.h"

static JSValue js_instantiate_prototype(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);
static int JS_CreateProperty(JSContext *ctx, JSObject *p,
                             JSAtom prop, JSValueConst val,
                             JSValueConst getter, JSValueConst setter,
                             int flags);
static JSValue JS_InstantiateFunctionListItem2(JSContext *ctx, JSObject *p,
                                               JSAtom atom, void *opaque);
static int JS_InstantiateFunctionListItem(JSContext *ctx, JSValueConst obj,
                                         JSAtom atom, const JSCFunctionListEntry *e);
/* 'props[]' is used to initialized the object properties. The number
   of elements depends on the shape. */
JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *sh, JSClassID class_id,
                                     JSProperty *props)
{
    JSObject *p;
    int i;
    
    js_trigger_gc(ctx->rt, sizeof(JSObject));
    p = js_malloc(ctx, sizeof(JSObject));
    if (unlikely(!p))
        goto fail;
    p->class_id = class_id;
    p->is_std_array_prototype = 0;
    p->extensible = TRUE;
    p->free_mark = 0;
    p->is_exotic = 0;
    p->fast_array = 0;
    p->is_constructor = 0;
    p->has_immutable_prototype = 0;
    p->tmp_mark = 0;
    p->is_HTMLDDA = 0;
    p->weakref_count = 0;
    p->u.opaque = NULL;
    p->shape = sh;
    p->prop = js_malloc(ctx, sizeof(JSProperty) * sh->prop_size);
    if (unlikely(!p->prop)) {
        js_free(ctx, p);
    fail:
        if (props) {
            JSShapeProperty *prs = get_shape_prop(sh);
            for(i = 0; i < sh->prop_count; i++) {
                free_property(ctx->rt, &props[i], prs->flags);
                prs++;
            }
        }
        js_free_shape(ctx->rt, sh);
        return JS_EXCEPTION;
    }

    switch(class_id) {
    case JS_CLASS_OBJECT:
        break;
    case JS_CLASS_ARRAY:
        {
            JSProperty *pr;
            p->is_exotic = 1;
            p->fast_array = 1;
            p->u.array.u.values = NULL;
            p->u.array.count = 0;
            p->u.array.u1.size = 0;
            if (!props) {
                /* XXX: remove */
                /* the length property is always the first one */
                if (likely(sh == ctx->array_shape)) {
                    pr = &p->prop[0];
                } else {
                    /* only used for the first array */
                    /* cannot fail */
                    pr = add_property(ctx, p, JS_ATOM_length,
                                      JS_PROP_WRITABLE | JS_PROP_LENGTH);
                }
                pr->u.value = JS_NewInt32(ctx, 0);
            }
        }
        break;
    case JS_CLASS_C_FUNCTION:
        p->prop[0].u.value = JS_UNDEFINED;
        break;
    case JS_CLASS_ARGUMENTS:
    case JS_CLASS_MAPPED_ARGUMENTS:
    case JS_CLASS_UINT8C_ARRAY:
    case JS_CLASS_INT8_ARRAY:
    case JS_CLASS_UINT8_ARRAY:
    case JS_CLASS_INT16_ARRAY:
    case JS_CLASS_UINT16_ARRAY:
    case JS_CLASS_INT32_ARRAY:
    case JS_CLASS_UINT32_ARRAY:
    case JS_CLASS_BIG_INT64_ARRAY:
    case JS_CLASS_BIG_UINT64_ARRAY:
    case JS_CLASS_FLOAT16_ARRAY:
    case JS_CLASS_FLOAT32_ARRAY:
    case JS_CLASS_FLOAT64_ARRAY:
        p->is_exotic = 1;
        p->fast_array = 1;
        p->u.array.u.ptr = NULL;
        p->u.array.count = 0;
        break;
    case JS_CLASS_DATAVIEW:
        p->u.array.u.ptr = NULL;
        p->u.array.count = 0;
        break;
    case JS_CLASS_NUMBER:
    case JS_CLASS_STRING:
    case JS_CLASS_BOOLEAN:
    case JS_CLASS_SYMBOL:
    case JS_CLASS_DATE:
    case JS_CLASS_BIG_INT:
        p->u.object_data = JS_UNDEFINED;
        goto set_exotic;
    case JS_CLASS_REGEXP:
        p->u.regexp.pattern = NULL;
        p->u.regexp.bytecode = NULL;
        break;
    case JS_CLASS_GLOBAL_OBJECT:
        p->u.global_object.uninitialized_vars = JS_UNDEFINED;
        break;
    default:
    set_exotic:
        if (ctx->rt->class_array[class_id].exotic) {
            p->is_exotic = 1;
        }
        break;
    }
    js_rc(p)->ref_count = 1;
    add_gc_object(ctx->rt, &p->header, JS_GC_OBJ_TYPE_JS_OBJECT);
    if (props) {
        for(i = 0; i < sh->prop_count; i++)
            p->prop[i] = props[i];
    }
    return JS_MKPTR(JS_TAG_OBJECT, p);
}

JSObject *get_proto_obj(JSValueConst proto_val)
{
    if (JS_VALUE_GET_TAG(proto_val) != JS_TAG_OBJECT)
        return NULL;
    else
        return JS_VALUE_GET_OBJ(proto_val);
}

/* WARNING: proto must be an object or JS_NULL */
JSValue JS_NewObjectProtoClass(JSContext *ctx, JSValueConst proto_val,
                               JSClassID class_id)
{
    JSShape *sh;
    JSObject *proto;

    proto = get_proto_obj(proto_val);
    sh = find_hashed_shape_proto(ctx->rt, proto);
    if (likely(sh)) {
        sh = js_dup_shape(sh);
    } else {
        sh = js_new_shape(ctx, proto);
        if (!sh)
            return JS_EXCEPTION;
    }
    return JS_NewObjectFromShape(ctx, sh, class_id, NULL);
}

/* WARNING: the shape is not hashed. It is used for objects where
   factorizing the shape is not relevant (prototypes, constructors) */
JSValue JS_NewObjectProtoClassAlloc(JSContext *ctx, JSValueConst proto_val,
                                           JSClassID class_id, int n_alloc_props)
{
    JSShape *sh;
    JSObject *proto;
    int hash_size, hash_bits;
    
    if (n_alloc_props <= JS_PROP_INITIAL_SIZE) {
        n_alloc_props = JS_PROP_INITIAL_SIZE;
        hash_size = JS_PROP_INITIAL_HASH_SIZE;
    } else {
        hash_bits = 32 - clz32(n_alloc_props - 1); /* ceil(log2(radix)) */
        hash_size = 1 << hash_bits;
    }
    proto = get_proto_obj(proto_val);
    sh = js_new_shape_nohash(ctx, proto, hash_size, n_alloc_props);
    if (!sh)
        return JS_EXCEPTION;
    return JS_NewObjectFromShape(ctx, sh, class_id, NULL);
}

#if 0
JSValue JS_GetObjectData(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;

    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        switch(p->class_id) {
        case JS_CLASS_NUMBER:
        case JS_CLASS_STRING:
        case JS_CLASS_BOOLEAN:
        case JS_CLASS_SYMBOL:
        case JS_CLASS_DATE:
        case JS_CLASS_BIG_INT:
            return JS_DupValue(ctx, p->u.object_data);
        }
    }
    return JS_UNDEFINED;
}
#endif

int JS_SetObjectData(JSContext *ctx, JSValueConst obj, JSValue val)
{
    JSObject *p;

    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        switch(p->class_id) {
        case JS_CLASS_NUMBER:
        case JS_CLASS_STRING:
        case JS_CLASS_BOOLEAN:
        case JS_CLASS_SYMBOL:
        case JS_CLASS_DATE:
        case JS_CLASS_BIG_INT:
            JS_FreeValue(ctx, p->u.object_data);
            p->u.object_data = val; /* for JS_CLASS_STRING, 'val' must
                                       be JS_TAG_STRING (and not a
                                       rope) */
            return 0;
        }
    }
    JS_FreeValue(ctx, val);
    if (!JS_IsException(obj))
        JS_ThrowTypeError(ctx, "invalid object type");
    return -1;
}

JSValue JS_NewObjectClass(JSContext *ctx, int class_id)
{
    return JS_NewObjectProtoClass(ctx, ctx->class_proto[class_id], class_id);
}

JSValue JS_NewObjectProto(JSContext *ctx, JSValueConst proto)
{
    return JS_NewObjectProtoClass(ctx, proto, JS_CLASS_OBJECT);
}

/* (JS_NewArray moved to src/quickjs/array.c) */

JSValue JS_NewObject(JSContext *ctx)
{
    /* inline JS_NewObjectClass(ctx, JS_CLASS_OBJECT); */
    return JS_NewObjectProtoClass(ctx, ctx->class_proto[JS_CLASS_OBJECT], JS_CLASS_OBJECT);
}


void JS_SetImmutablePrototype(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return;
    p = JS_VALUE_GET_OBJ(obj);
    p->has_immutable_prototype = TRUE;
}

/* Return -1 (exception) or TRUE/FALSE. 'throw_flag' = FALSE indicates
   that it is called from Reflect.setPrototypeOf(). */
int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj,
                                   JSValueConst proto_val,
                                   BOOL throw_flag)
{
    JSObject *proto, *p, *p1;
    JSShape *sh;

    if (throw_flag) {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_NULL ||
            JS_VALUE_GET_TAG(obj) == JS_TAG_UNDEFINED)
            goto not_obj;
    } else {
        if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
            goto not_obj;
    }
    p = JS_VALUE_GET_OBJ(obj);
    if (JS_VALUE_GET_TAG(proto_val) != JS_TAG_OBJECT) {
        if (JS_VALUE_GET_TAG(proto_val) != JS_TAG_NULL) {
        not_obj:
            JS_ThrowTypeErrorNotAnObject(ctx);
            return -1;
        }
        proto = NULL;
    } else {
        proto = JS_VALUE_GET_OBJ(proto_val);
    }

    if (throw_flag && JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return TRUE;

    if (unlikely(p->is_exotic)) {
        const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
        int ret;
        if (em && em->set_prototype) {
            ret = em->set_prototype(ctx, obj, proto_val);
            if (ret == 0 && throw_flag) {
                JS_ThrowTypeError(ctx, "proxy: bad prototype");
                return -1;
            } else {
                return ret;
            }
        }
    }

    sh = p->shape;
    if (sh->proto == proto)
        return TRUE;
    if (unlikely(p->has_immutable_prototype)) {
        if (throw_flag) {
            JS_ThrowTypeError(ctx, "prototype is immutable");
            return -1;
        } else {
            return FALSE;
        }
    }
    if (unlikely(!p->extensible)) {
        if (throw_flag) {
            JS_ThrowTypeError(ctx, "object is not extensible");
            return -1;
        } else {
            return FALSE;
        }
    }
    if (proto) {
        /* check if there is a cycle */
        p1 = proto;
        do {
            if (p1 == p) {
                if (throw_flag) {
                    JS_ThrowTypeError(ctx, "circular prototype chain");
                    return -1;
                } else {
                    return FALSE;
                }
            }
            /* Note: for Proxy objects, proto is NULL */
            p1 = p1->shape->proto;
        } while (p1 != NULL);
        JS_DupValue(ctx, proto_val);
    }

    if (js_shape_prepare_update(ctx, p, NULL))
        return -1;
    sh = p->shape;
    if (sh->proto)
        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, sh->proto));
    sh->proto = proto;
    p->is_std_array_prototype = FALSE; 
    return TRUE;
}

/* return -1 (exception) or TRUE/FALSE */
int JS_SetPrototype(JSContext *ctx, JSValueConst obj, JSValueConst proto_val)
{
    return JS_SetPrototypeInternal(ctx, obj, proto_val, TRUE);
}

/* Only works for primitive types, otherwise return JS_NULL. */
static JSValueConst JS_GetPrototypePrimitive(JSContext *ctx, JSValueConst val)
{
    switch(JS_VALUE_GET_NORM_TAG(val)) {
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        val = ctx->class_proto[JS_CLASS_BIG_INT];
        break;
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
        val = ctx->class_proto[JS_CLASS_NUMBER];
        break;
    case JS_TAG_BOOL:
        val = ctx->class_proto[JS_CLASS_BOOLEAN];
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        val = ctx->class_proto[JS_CLASS_STRING];
        break;
    case JS_TAG_SYMBOL:
        val = ctx->class_proto[JS_CLASS_SYMBOL];
        break;
    case JS_TAG_OBJECT:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
    default:
        val = JS_NULL;
        break;
    }
    return val;
}

/* Return an Object, JS_NULL or JS_EXCEPTION in case of exotic object. */
JSValue JS_GetPrototype(JSContext *ctx, JSValueConst obj)
{
    JSValue val;
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        JSObject *p;
        p = JS_VALUE_GET_OBJ(obj);
        if (unlikely(p->is_exotic)) {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->get_prototype) {
                return em->get_prototype(ctx, obj);
            }
        }
        p = p->shape->proto;
        if (!p)
            val = JS_NULL;
        else
            val = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
    } else {
        val = JS_DupValue(ctx, JS_GetPrototypePrimitive(ctx, obj));
    }
    return val;
}

JSValue JS_GetPrototypeFree(JSContext *ctx, JSValue obj)
{
    JSValue obj1;
    obj1 = JS_GetPrototype(ctx, obj);
    JS_FreeValue(ctx, obj);
    return obj1;
}

/* return TRUE, FALSE or (-1) in case of exception */
int JS_OrdinaryIsInstanceOf(JSContext *ctx, JSValueConst val,
                                   JSValueConst obj)
{
    JSValue obj_proto;
    JSObject *proto;
    const JSObject *p, *proto1;
    BOOL ret;

    if (!JS_IsFunction(ctx, obj))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id == JS_CLASS_BOUND_FUNCTION) {
        JSBoundFunction *s = p->u.bound_function;
        return JS_IsInstanceOf(ctx, val, s->func_obj);
    }

    /* Only explicitly boxed values are instances of constructors */
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    obj_proto = JS_GetProperty(ctx, obj, JS_ATOM_prototype);
    if (JS_VALUE_GET_TAG(obj_proto) != JS_TAG_OBJECT) {
        if (!JS_IsException(obj_proto))
            JS_ThrowTypeError(ctx, "operand 'prototype' property is not an object");
        ret = -1;
        goto done;
    }
    proto = JS_VALUE_GET_OBJ(obj_proto);
    p = JS_VALUE_GET_OBJ(val);
    for(;;) {
        proto1 = p->shape->proto;
        if (!proto1) {
            /* slow case if exotic object in the prototype chain */
            if (unlikely(p->is_exotic && !p->fast_array)) {
                JSValue obj1;
                obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, (JSObject *)p));
                for(;;) {
                    obj1 = JS_GetPrototypeFree(ctx, obj1);
                    if (JS_IsException(obj1)) {
                        ret = -1;
                        break;
                    }
                    if (JS_IsNull(obj1)) {
                        ret = FALSE;
                        break;
                    }
                    if (proto == JS_VALUE_GET_OBJ(obj1)) {
                        JS_FreeValue(ctx, obj1);
                        ret = TRUE;
                        break;
                    }
                    /* must check for timeout to avoid infinite loop */
                    if (js_poll_interrupts(ctx)) {
                        JS_FreeValue(ctx, obj1);
                        ret = -1;
                        break;
                    }
                }
            } else {
                ret = FALSE;
            }
            break;
        }
        p = proto1;
        if (proto == p) {
            ret = TRUE;
            break;
        }
    }
done:
    JS_FreeValue(ctx, obj_proto);
    return ret;
}

/* return TRUE, FALSE or (-1) in case of exception */
int JS_IsInstanceOf(JSContext *ctx, JSValueConst val, JSValueConst obj)
{
    JSValue method;

    if (!JS_IsObject(obj))
        goto fail;
    method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_hasInstance);
    if (JS_IsException(method))
        return -1;
    if (!JS_IsNull(method) && !JS_IsUndefined(method)) {
        JSValue ret;
        ret = JS_CallFree(ctx, method, obj, 1, &val);
        return JS_ToBoolFree(ctx, ret);
    }

    /* legacy case */
    if (!JS_IsFunction(ctx, obj)) {
    fail:
        JS_ThrowTypeError(ctx, "invalid 'instanceof' right operand");
        return -1;
    }
    return JS_OrdinaryIsInstanceOf(ctx, val, obj);
}

/* return the value associated to the autoinit property or an exception */
typedef JSValue JSAutoInitFunc(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);

static JSAutoInitFunc *js_autoinit_func_table[] = {
    js_instantiate_prototype, /* JS_AUTOINIT_ID_PROTOTYPE */
    js_module_ns_autoinit, /* JS_AUTOINIT_ID_MODULE_NS */
    JS_InstantiateFunctionListItem2, /* JS_AUTOINIT_ID_PROP */
};

/* warning: 'prs' is reallocated after it */
int JS_AutoInitProperty(JSContext *ctx, JSObject *p, JSAtom prop,
                               JSProperty *pr, JSShapeProperty *prs)
{
    JSValue val;
    JSContext *realm;
    JSAutoInitFunc *func;
    JSAutoInitIDEnum id;
    
    if (js_shape_prepare_update(ctx, p, &prs))
        return -1;

    realm = js_autoinit_get_realm(pr);
    id = js_autoinit_get_id(pr);
    func = js_autoinit_func_table[id];
    /* 'func' shall not modify the object properties 'pr' */
    val = func(realm, p, prop, pr->u.init.opaque);
    js_autoinit_free(ctx->rt, pr);
    prs->flags &= ~JS_PROP_TMASK;
    pr->u.value = JS_UNDEFINED;
    if (JS_IsException(val))
        return -1;
    if (id == JS_AUTOINIT_ID_MODULE_NS &&
        JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        /* WARNING: a varref is returned as a string  ! */
        prs->flags |= JS_PROP_VARREF;
        pr->u.var_ref = JS_VALUE_GET_PTR(val);
        js_rc(pr->u.var_ref)->ref_count++;
    } else if (p->class_id == JS_CLASS_GLOBAL_OBJECT) {
        JSVarRef *var_ref;
        /* in the global object we use references */
        var_ref = js_create_var_ref(ctx, FALSE);
        if (!var_ref)
            return -1;
        prs->flags |= JS_PROP_VARREF;
        pr->u.var_ref = var_ref;
        var_ref->value = val; 
        var_ref->is_const = !(prs->flags & JS_PROP_WRITABLE);
    } else {
        pr->u.value = val;
    }
    return 0;
}

JSValue JS_GetPropertyInternal(JSContext *ctx, JSValueConst obj,
                               JSAtom prop, JSValueConst this_obj,
                               BOOL throw_ref_error)
{
    JSObject *p;
    JSProperty *pr;
    JSShapeProperty *prs;
    uint32_t tag;

    tag = JS_VALUE_GET_TAG(obj);
    if (unlikely(tag != JS_TAG_OBJECT)) {
        switch(tag) {
        case JS_TAG_NULL:
            return JS_ThrowTypeErrorAtom(ctx, "cannot read property '%s' of null", prop);
        case JS_TAG_UNDEFINED:
            return JS_ThrowTypeErrorAtom(ctx, "cannot read property '%s' of undefined", prop);
        case JS_TAG_EXCEPTION:
            return JS_EXCEPTION;
        case JS_TAG_STRING:
            {
                JSString *p1 = JS_VALUE_GET_STRING(obj);
                if (__JS_AtomIsTaggedInt(prop)) {
                    uint32_t idx;
                    idx = __JS_AtomToUInt32(prop);
                    if (idx < p1->len) {
                        return js_new_string_char(ctx, string_get(p1, idx));
                    }
                } else if (prop == JS_ATOM_length) {
                    return JS_NewInt32(ctx, p1->len);
                }
            }
            break;
        case JS_TAG_STRING_ROPE:
            {
                JSStringRope *p1 = JS_VALUE_GET_STRING_ROPE(obj);
                if (__JS_AtomIsTaggedInt(prop)) {
                    uint32_t idx;
                    idx = __JS_AtomToUInt32(prop);
                    if (idx < p1->len) {
                        return js_new_string_char(ctx, string_rope_get(obj, idx));
                    }
                } else if (prop == JS_ATOM_length) {
                    return JS_NewInt32(ctx, p1->len);
                }
            }
            break;
        default:
            break;
        }
        /* cannot raise an exception */
        p = JS_VALUE_GET_OBJ(JS_GetPrototypePrimitive(ctx, obj));
        if (!p)
            return JS_UNDEFINED;
    } else {
        p = JS_VALUE_GET_OBJ(obj);
    }

    for(;;) {
        prs = find_own_property(&pr, p, prop);
        if (prs) {
            /* found */
            if (unlikely(prs->flags & JS_PROP_TMASK)) {
                if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                    if (unlikely(!pr->u.getset.getter)) {
                        return JS_UNDEFINED;
                    } else {
                        JSValue func = JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter);
                        /* Note: the field could be removed in the getter */
                        func = JS_DupValue(ctx, func);
                        return JS_CallFree(ctx, func, this_obj, 0, NULL);
                    }
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                    JSValue val = *pr->u.var_ref->pvalue;
                    if (unlikely(JS_IsUninitialized(val)))
                        return JS_ThrowReferenceErrorUninitialized(ctx, prs->atom);
                    return JS_DupValue(ctx, val);
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                    /* Instantiate property and retry */
                    if (JS_AutoInitProperty(ctx, p, prop, pr, prs))
                        return JS_EXCEPTION;
                    continue;
                }
            } else {
                return JS_DupValue(ctx, pr->u.value);
            }
        }
        if (unlikely(p->is_exotic)) {
            /* exotic behaviors */
            if (p->fast_array) {
                if (__JS_AtomIsTaggedInt(prop)) {
                    uint32_t idx = __JS_AtomToUInt32(prop);
                    if (idx < p->u.array.count) {
                        /* we avoid duplicating the code */
                        return JS_GetPropertyUint32(ctx, JS_MKPTR(JS_TAG_OBJECT, p), idx);
                    } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
                               p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                        return JS_UNDEFINED;
                    }
                } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
                           p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                    int ret;
                    ret = JS_AtomIsNumericIndex(ctx, prop);
                    if (ret != 0) {
                        if (ret < 0)
                            return JS_EXCEPTION;
                        return JS_UNDEFINED;
                    }
                }
            } else {
                const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
                if (em) {
                    if (em->get_property) {
                        JSValue obj1, retval;
                        /* XXX: should pass throw_ref_error */
                        /* Note: if 'p' is a prototype, it can be
                           freed in the called function */
                        obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
                        retval = em->get_property(ctx, obj1, prop, this_obj);
                        JS_FreeValue(ctx, obj1);
                        return retval;
                    }
                    if (em->get_own_property) {
                        JSPropertyDescriptor desc;
                        int ret;
                        JSValue obj1;

                        /* Note: if 'p' is a prototype, it can be
                           freed in the called function */
                        obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
                        ret = em->get_own_property(ctx, &desc, obj1, prop);
                        JS_FreeValue(ctx, obj1);
                        if (ret < 0)
                            return JS_EXCEPTION;
                        if (ret) {
                            if (desc.flags & JS_PROP_GETSET) {
                                JS_FreeValue(ctx, desc.setter);
                                return JS_CallFree(ctx, desc.getter, this_obj, 0, NULL);
                            } else {
                                return desc.value;
                            }
                        }
                    }
                }
            }
        }
        p = p->shape->proto;
        if (!p)
            break;
    }
    if (unlikely(throw_ref_error)) {
        return JS_ThrowReferenceErrorNotDefined(ctx, prop);
    } else {
        return JS_UNDEFINED;
    }
}

static JSValue JS_ThrowTypeErrorPrivateNotFound(JSContext *ctx, JSAtom atom)
{
    return JS_ThrowTypeErrorAtom(ctx, "private class field '%s' does not exist",
                                 atom);
}

/* Private fields can be added even on non extensible objects or
   Proxies */
int JS_DefinePrivateField(JSContext *ctx, JSValueConst obj,
                                 JSValueConst name, JSValue val)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSAtom prop;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        goto fail;
    }
    /* safety check */
    if (unlikely(JS_VALUE_GET_TAG(name) != JS_TAG_SYMBOL)) {
        JS_ThrowTypeErrorNotASymbol(ctx);
        goto fail;
    }
    prop = js_symbol_to_atom(ctx, (JSValue)name);
    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property(&pr, p, prop);
    if (prs) {
        JS_ThrowTypeErrorAtom(ctx, "private class field '%s' already exists",
                              prop);
        goto fail;
    }
    pr = add_property(ctx, p, prop, JS_PROP_C_W_E);
    if (unlikely(!pr)) {
    fail:
        JS_FreeValue(ctx, val);
        return -1;
    }
    pr->u.value = val;
    return 0;
}

JSValue JS_GetPrivateField(JSContext *ctx, JSValueConst obj,
                                  JSValueConst name)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSAtom prop;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT))
        return JS_ThrowTypeErrorNotAnObject(ctx);
    /* safety check */
    if (unlikely(JS_VALUE_GET_TAG(name) != JS_TAG_SYMBOL))
        return JS_ThrowTypeErrorNotASymbol(ctx);
    prop = js_symbol_to_atom(ctx, (JSValue)name);
    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property(&pr, p, prop);
    if (!prs) {
        JS_ThrowTypeErrorPrivateNotFound(ctx, prop);
        return JS_EXCEPTION;
    }
    return JS_DupValue(ctx, pr->u.value);
}

int JS_SetPrivateField(JSContext *ctx, JSValueConst obj,
                              JSValueConst name, JSValue val)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSAtom prop;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        goto fail;
    }
    /* safety check */
    if (unlikely(JS_VALUE_GET_TAG(name) != JS_TAG_SYMBOL)) {
        JS_ThrowTypeErrorNotASymbol(ctx);
        goto fail;
    }
    prop = js_symbol_to_atom(ctx, (JSValue)name);
    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property(&pr, p, prop);
    if (!prs) {
        JS_ThrowTypeErrorPrivateNotFound(ctx, prop);
    fail:
        JS_FreeValue(ctx, val);
        return -1;
    }
    set_value(ctx, &pr->u.value, val);
    return 0;
}

/* add a private brand field to 'home_obj' if not already present and
   if obj is != null add a private brand to it */
int JS_AddBrand(JSContext *ctx, JSValueConst obj, JSValueConst home_obj)
{
    JSObject *p, *p1;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSValue brand;
    JSAtom brand_atom;

    if (unlikely(JS_VALUE_GET_TAG(home_obj) != JS_TAG_OBJECT)) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    p = JS_VALUE_GET_OBJ(home_obj);
    prs = find_own_property(&pr, p, JS_ATOM_Private_brand);
    if (!prs) {
        /* if the brand is not present, add it */
        brand = JS_NewSymbolFromAtom(ctx, JS_ATOM_brand, JS_ATOM_TYPE_PRIVATE);
        if (JS_IsException(brand))
            return -1;
        pr = add_property(ctx, p, JS_ATOM_Private_brand, JS_PROP_C_W_E);
        if (!pr) {
            JS_FreeValue(ctx, brand);
            return -1;
        }
        pr->u.value = JS_DupValue(ctx, brand);
    } else {
        brand = JS_DupValue(ctx, pr->u.value);
    }
    brand_atom = js_symbol_to_atom(ctx, brand);

    if (JS_IsObject(obj)) {
        p1 = JS_VALUE_GET_OBJ(obj);
        prs = find_own_property(&pr, p1, brand_atom);
        if (unlikely(prs)) {
            JS_FreeAtom(ctx, brand_atom);
            JS_ThrowTypeError(ctx, "private method is already present");
            return -1;
        }
        pr = add_property(ctx, p1, brand_atom, JS_PROP_C_W_E);
        JS_FreeAtom(ctx, brand_atom);
        if (!pr)
            return -1;
        pr->u.value = JS_UNDEFINED;
    } else {
        JS_FreeAtom(ctx, brand_atom);
    }
    return 0;
}

/* return a boolean telling if the brand of the home object of 'func'
   is present on 'obj' or -1 in case of exception */
int JS_CheckBrand(JSContext *ctx, JSValueConst obj, JSValueConst func)
{
    JSObject *p, *p1, *home_obj;
    JSShapeProperty *prs;
    JSProperty *pr;
    JSValueConst brand;

    /* get the home object of 'func' */
    if (unlikely(JS_VALUE_GET_TAG(func) != JS_TAG_OBJECT))
        goto not_obj;
    p1 = JS_VALUE_GET_OBJ(func);
    if (!js_class_has_bytecode(p1->class_id))
        goto not_obj;
    home_obj = p1->u.func.home_object;
    if (!home_obj)
        goto not_obj;
    prs = find_own_property(&pr, home_obj, JS_ATOM_Private_brand);
    if (!prs) {
        JS_ThrowTypeError(ctx, "expecting <brand> private field");
        return -1;
    }
    brand = pr->u.value;
    /* safety check */
    if (unlikely(JS_VALUE_GET_TAG(brand) != JS_TAG_SYMBOL))
        goto not_obj;

    /* get the brand array of 'obj' */
    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)) {
    not_obj:
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property(&pr, p, js_symbol_to_atom(ctx, (JSValue)brand));
    return (prs != NULL);
}

uint32_t js_string_obj_get_length(JSContext *ctx,
                                         JSValueConst obj)
{
    JSObject *p;
    uint32_t len = 0;

    /* This is a class exotic method: obj class_id is JS_CLASS_STRING */
    p = JS_VALUE_GET_OBJ(obj);
    if (JS_VALUE_GET_TAG(p->u.object_data) == JS_TAG_STRING) {
        JSString *p1 = JS_VALUE_GET_STRING(p->u.object_data);
        len = p1->len;
    }
    return len;
}

static int num_keys_cmp(const void *p1, const void *p2, void *opaque)
{
    JSContext *ctx = opaque;
    JSAtom atom1 = ((const JSPropertyEnum *)p1)->atom;
    JSAtom atom2 = ((const JSPropertyEnum *)p2)->atom;
    uint32_t v1, v2;
    BOOL atom1_is_integer, atom2_is_integer;

    atom1_is_integer = JS_AtomIsArrayIndex(ctx, &v1, atom1);
    atom2_is_integer = JS_AtomIsArrayIndex(ctx, &v2, atom2);
    assert(atom1_is_integer && atom2_is_integer);
    if (v1 < v2)
        return -1;
    else if (v1 == v2)
        return 0;
    else
        return 1;
}

void JS_FreePropertyEnum(JSContext *ctx, JSPropertyEnum *tab, uint32_t len)
{
    uint32_t i;
    if (tab) {
        for(i = 0; i < len; i++)
            JS_FreeAtom(ctx, tab[i].atom);
        js_free(ctx, tab);
    }
}

/* return < 0 in case if exception, 0 if OK. ptab and its atoms must
   be freed by the user. */
int __exception JS_GetOwnPropertyNamesInternal(JSContext *ctx,
                                                      JSPropertyEnum **ptab,
                                                      uint32_t *plen,
                                                      JSObject *p, int flags)
{
    int i, j;
    JSShape *sh;
    JSShapeProperty *prs;
    JSPropertyEnum *tab_atom, *tab_exotic;
    JSAtom atom;
    uint32_t num_keys_count, str_keys_count, sym_keys_count, atom_count;
    uint32_t num_index, str_index, sym_index, exotic_count, exotic_keys_count;
    BOOL is_enumerable, num_sorted;
    uint32_t num_key;
    JSAtomKindEnum kind;

    /* clear pointer for consistency in case of failure */
    *ptab = NULL;
    *plen = 0;

    /* compute the number of returned properties */
    num_keys_count = 0;
    str_keys_count = 0;
    sym_keys_count = 0;
    exotic_keys_count = 0;
    exotic_count = 0;
    tab_exotic = NULL;
    sh = p->shape;
    for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
        atom = prs->atom;
        if (atom != JS_ATOM_NULL) {
            is_enumerable = ((prs->flags & JS_PROP_ENUMERABLE) != 0);
            kind = JS_AtomGetKind(ctx, atom);
            if ((!(flags & JS_GPN_ENUM_ONLY) || is_enumerable) &&
                ((flags >> kind) & 1) != 0) {
                /* need to raise an exception in case of the module
                   name space (implicit GetOwnProperty) */
                if (unlikely((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) &&
                    (flags & (JS_GPN_SET_ENUM | JS_GPN_ENUM_ONLY))) {
                    JSVarRef *var_ref = p->prop[i].u.var_ref;
                    if (unlikely(JS_IsUninitialized(*var_ref->pvalue))) {
                        JS_ThrowReferenceErrorUninitialized(ctx, prs->atom);
                        return -1;
                    }
                }
                if (JS_AtomIsArrayIndex(ctx, &num_key, atom)) {
                    num_keys_count++;
                } else if (kind == JS_ATOM_KIND_STRING) {
                    str_keys_count++;
                } else {
                    sym_keys_count++;
                }
            }
        }
    }

    if (p->is_exotic) {
        if (p->fast_array) {
            if (flags & JS_GPN_STRING_MASK) {
                num_keys_count += p->u.array.count;
            }
        } else if (p->class_id == JS_CLASS_STRING) {
            if (flags & JS_GPN_STRING_MASK) {
                num_keys_count += js_string_obj_get_length(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
            }
        } else {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->get_own_property_names) {
                if (em->get_own_property_names(ctx, &tab_exotic, &exotic_count,
                                               JS_MKPTR(JS_TAG_OBJECT, p)))
                    return -1;
                for(i = 0; i < exotic_count; i++) {
                    atom = tab_exotic[i].atom;
                    kind = JS_AtomGetKind(ctx, atom);
                    if (((flags >> kind) & 1) != 0) {
                        is_enumerable = FALSE;
                        if (flags & (JS_GPN_SET_ENUM | JS_GPN_ENUM_ONLY)) {
                            JSPropertyDescriptor desc;
                            int res;
                            /* set the "is_enumerable" field if necessary */
                            res = JS_GetOwnPropertyInternal(ctx, &desc, p, atom);
                            if (res < 0) {
                                JS_FreePropertyEnum(ctx, tab_exotic, exotic_count);
                                return -1;
                            }
                            if (res) {
                                is_enumerable =
                                    ((desc.flags & JS_PROP_ENUMERABLE) != 0);
                                js_free_desc(ctx, &desc);
                            }
                            tab_exotic[i].is_enumerable = is_enumerable;
                        }
                        if (!(flags & JS_GPN_ENUM_ONLY) || is_enumerable) {
                            exotic_keys_count++;
                        }
                    }
                }
            }
        }
    }

    /* fill them */

    atom_count = num_keys_count + str_keys_count;
    if (atom_count < str_keys_count)
        goto add_overflow;
    atom_count += sym_keys_count;
    if (atom_count < sym_keys_count)
        goto add_overflow;
    atom_count += exotic_keys_count;
    if (atom_count < exotic_keys_count || atom_count > INT32_MAX) {
    add_overflow:
        JS_ThrowOutOfMemory(ctx);
        JS_FreePropertyEnum(ctx, tab_exotic, exotic_count);
        return -1;
    }
    /* XXX: need generic way to test for js_malloc(ctx, a * b) overflow */
    
    /* avoid allocating 0 bytes */
    tab_atom = js_malloc(ctx, sizeof(tab_atom[0]) * max_int(atom_count, 1));
    if (!tab_atom) {
        JS_FreePropertyEnum(ctx, tab_exotic, exotic_count);
        return -1;
    }

    num_index = 0;
    str_index = num_keys_count;
    sym_index = str_index + str_keys_count;

    num_sorted = TRUE;
    sh = p->shape;
    for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
        atom = prs->atom;
        if (atom != JS_ATOM_NULL) {
            is_enumerable = ((prs->flags & JS_PROP_ENUMERABLE) != 0);
            kind = JS_AtomGetKind(ctx, atom);
            if ((!(flags & JS_GPN_ENUM_ONLY) || is_enumerable) &&
                ((flags >> kind) & 1) != 0) {
                if (JS_AtomIsArrayIndex(ctx, &num_key, atom)) {
                    j = num_index++;
                    num_sorted = FALSE;
                } else if (kind == JS_ATOM_KIND_STRING) {
                    j = str_index++;
                } else {
                    j = sym_index++;
                }
                tab_atom[j].atom = JS_DupAtom(ctx, atom);
                tab_atom[j].is_enumerable = is_enumerable;
            }
        }
    }

    if (p->is_exotic) {
        int len;
        if (p->fast_array) {
            if (flags & JS_GPN_STRING_MASK) {
                len = p->u.array.count;
                goto add_array_keys;
            }
        } else if (p->class_id == JS_CLASS_STRING) {
            if (flags & JS_GPN_STRING_MASK) {
                len = js_string_obj_get_length(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
            add_array_keys:
                for(i = 0; i < len; i++) {
                    tab_atom[num_index].atom = __JS_AtomFromUInt32(i);
                    if (tab_atom[num_index].atom == JS_ATOM_NULL) {
                        JS_FreePropertyEnum(ctx, tab_atom, num_index);
                        return -1;
                    }
                    tab_atom[num_index].is_enumerable = TRUE;
                    num_index++;
                }
            }
        } else {
            /* Note: exotic keys are not reordered and comes after the object own properties. */
            for(i = 0; i < exotic_count; i++) {
                atom = tab_exotic[i].atom;
                is_enumerable = tab_exotic[i].is_enumerable;
                kind = JS_AtomGetKind(ctx, atom);
                if ((!(flags & JS_GPN_ENUM_ONLY) || is_enumerable) &&
                    ((flags >> kind) & 1) != 0) {
                    tab_atom[sym_index].atom = atom;
                    tab_atom[sym_index].is_enumerable = is_enumerable;
                    sym_index++;
                } else {
                    JS_FreeAtom(ctx, atom);
                }
            }
            js_free(ctx, tab_exotic);
        }
    }

    assert(num_index == num_keys_count);
    assert(str_index == num_keys_count + str_keys_count);
    assert(sym_index == atom_count);

    if (num_keys_count != 0 && !num_sorted) {
        rqsort(tab_atom, num_keys_count, sizeof(tab_atom[0]), num_keys_cmp,
               ctx);
    }
    *ptab = tab_atom;
    *plen = atom_count;
    return 0;
}

int JS_GetOwnPropertyNames(JSContext *ctx, JSPropertyEnum **ptab,
                           uint32_t *plen, JSValueConst obj, int flags)
{
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    return JS_GetOwnPropertyNamesInternal(ctx, ptab, plen,
                                          JS_VALUE_GET_OBJ(obj), flags);
}

/* Return -1 if exception,
   FALSE if the property does not exist, TRUE if it exists. If TRUE is
   returned, the property descriptor 'desc' is filled present. */
int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                                     JSObject *p, JSAtom prop)
{
    JSShapeProperty *prs;
    JSProperty *pr;

retry:
    prs = find_own_property(&pr, p, prop);
    if (prs) {
        if (desc) {
            desc->flags = prs->flags & JS_PROP_C_W_E;
            desc->getter = JS_UNDEFINED;
            desc->setter = JS_UNDEFINED;
            desc->value = JS_UNDEFINED;
            if (unlikely(prs->flags & JS_PROP_TMASK)) {
                if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                    desc->flags |= JS_PROP_GETSET;
                    if (pr->u.getset.getter)
                        desc->getter = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter));
                    if (pr->u.getset.setter)
                        desc->setter = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.setter));
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                    JSValue val = *pr->u.var_ref->pvalue;
                    if (unlikely(JS_IsUninitialized(val))) {
                        JS_ThrowReferenceErrorUninitialized(ctx, prs->atom);
                        return -1;
                    }
                    desc->value = JS_DupValue(ctx, val);
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                    /* Instantiate property and retry */
                    if (JS_AutoInitProperty(ctx, p, prop, pr, prs))
                        return -1;
                    goto retry;
                }
            } else {
                desc->value = JS_DupValue(ctx, pr->u.value);
            }
        } else {
            /* for consistency, send the exception even if desc is NULL */
            if (unlikely((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF)) {
                if (unlikely(JS_IsUninitialized(*pr->u.var_ref->pvalue))) {
                    JS_ThrowReferenceErrorUninitialized(ctx, prs->atom);
                    return -1;
                }
            } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                /* nothing to do: delay instantiation until actual value and/or attributes are read */
            }
        }
        return TRUE;
    }
    if (p->is_exotic) {
        if (p->fast_array) {
            /* specific case for fast arrays */
            if (__JS_AtomIsTaggedInt(prop)) {
                uint32_t idx;
                idx = __JS_AtomToUInt32(prop);
                if (idx < p->u.array.count) {
                    if (desc) {
                        desc->flags = JS_PROP_WRITABLE | JS_PROP_ENUMERABLE |
                            JS_PROP_CONFIGURABLE;
                        desc->getter = JS_UNDEFINED;
                        desc->setter = JS_UNDEFINED;
                        desc->value = JS_GetPropertyUint32(ctx, JS_MKPTR(JS_TAG_OBJECT, p), idx);
                    }
                    return TRUE;
                }
            }
        } else {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->get_own_property) {
                return em->get_own_property(ctx, desc,
                                            JS_MKPTR(JS_TAG_OBJECT, p), prop);
            }
        }
    }
    return FALSE;
}

int JS_GetOwnProperty(JSContext *ctx, JSPropertyDescriptor *desc,
                      JSValueConst obj, JSAtom prop)
{
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    return JS_GetOwnPropertyInternal(ctx, desc, JS_VALUE_GET_OBJ(obj), prop);
}

/* return -1 if exception (exotic object only) or TRUE/FALSE */
int JS_IsExtensible(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    if (unlikely(p->is_exotic)) {
        const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
        if (em && em->is_extensible) {
            return em->is_extensible(ctx, obj);
        }
    }
    return p->extensible;
}

/* return -1 if exception (exotic object only) or TRUE/FALSE */
int JS_PreventExtensions(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    if (unlikely(p->is_exotic)) {
        if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
            p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
            JSTypedArray *ta;
            JSArrayBuffer *abuf;
            /* resizable type arrays return FALSE */
            ta = p->u.typed_array;
            abuf = ta->buffer->u.array_buffer;
            if (ta->track_rab ||
                (array_buffer_is_resizable(abuf) && !abuf->shared))
                return FALSE;
        } else {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->prevent_extensions) {
                return em->prevent_extensions(ctx, obj);
            }
        }
    }
    p->extensible = FALSE;
    return TRUE;
}

/* return -1 if exception otherwise TRUE or FALSE */
int JS_HasProperty(JSContext *ctx, JSValueConst obj, JSAtom prop)
{
    JSObject *p;
    int ret;
    JSValue obj1;

    if (unlikely(JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT))
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    for(;;) {
        if (p->is_exotic) {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->has_property) {
                /* has_property can free the prototype */
                obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
                ret = em->has_property(ctx, obj1, prop);
                JS_FreeValue(ctx, obj1);
                return ret;
            }
        }
        /* JS_GetOwnPropertyInternal can free the prototype */
        JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
        ret = JS_GetOwnPropertyInternal(ctx, NULL, p, prop);
        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
        if (ret != 0)
            return ret;
        if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
            p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
            ret = JS_AtomIsNumericIndex(ctx, prop);
            if (ret != 0) {
                if (ret < 0)
                    return -1;
                return FALSE;
            }
        }
        p = p->shape->proto;
        if (!p)
            break;
    }
    return FALSE;
}

/* (js_symbol_to_atom and JS_ValueToAtom moved) */

JSValue JS_GetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                                   JSValue prop)
{
    JSAtom atom;
    JSValue ret;

    if (likely(JS_VALUE_GET_TAG(this_obj) == JS_TAG_OBJECT &&
               JS_VALUE_GET_TAG(prop) == JS_TAG_INT)) {
        JSObject *p;
        uint32_t idx;
        /* fast path for array access */
        p = JS_VALUE_GET_OBJ(this_obj);
        idx = JS_VALUE_GET_INT(prop);
        switch(p->class_id) {
        case JS_CLASS_ARRAY:
        case JS_CLASS_ARGUMENTS:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_DupValue(ctx, p->u.array.u.values[idx]);
        case JS_CLASS_MAPPED_ARGUMENTS:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_DupValue(ctx, *p->u.array.u.var_refs[idx]->pvalue);
        case JS_CLASS_INT8_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewInt32(ctx, p->u.array.u.int8_ptr[idx]);
        case JS_CLASS_UINT8C_ARRAY:
        case JS_CLASS_UINT8_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewInt32(ctx, p->u.array.u.uint8_ptr[idx]);
        case JS_CLASS_INT16_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewInt32(ctx, p->u.array.u.int16_ptr[idx]);
        case JS_CLASS_UINT16_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewInt32(ctx, p->u.array.u.uint16_ptr[idx]);
        case JS_CLASS_INT32_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewInt32(ctx, p->u.array.u.int32_ptr[idx]);
        case JS_CLASS_UINT32_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewUint32(ctx, p->u.array.u.uint32_ptr[idx]);
        case JS_CLASS_BIG_INT64_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewBigInt64(ctx, p->u.array.u.int64_ptr[idx]);
        case JS_CLASS_BIG_UINT64_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return JS_NewBigUint64(ctx, p->u.array.u.uint64_ptr[idx]);
        case JS_CLASS_FLOAT16_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return __JS_NewFloat64(ctx, fromfp16(p->u.array.u.fp16_ptr[idx]));
        case JS_CLASS_FLOAT32_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return __JS_NewFloat64(ctx, p->u.array.u.float_ptr[idx]);
        case JS_CLASS_FLOAT64_ARRAY:
            if (unlikely(idx >= p->u.array.count)) goto slow_path;
            return __JS_NewFloat64(ctx, p->u.array.u.double_ptr[idx]);
        default:
            goto slow_path;
        }
    } else {
    slow_path:
        /* ToObject() must be done before ToPropertyKey() */
        if (JS_IsNull(this_obj) || JS_IsUndefined(this_obj)) {
            JS_FreeValue(ctx, prop);
            return JS_ThrowTypeError(ctx, "cannot read property of %s", JS_IsNull(this_obj) ? "null" : "undefined");
        }
        atom = JS_ValueToAtom(ctx, prop);
        JS_FreeValue(ctx, prop);
        if (unlikely(atom == JS_ATOM_NULL))
            return JS_EXCEPTION;
        ret = JS_GetProperty(ctx, this_obj, atom);
        JS_FreeAtom(ctx, atom);
        return ret;
    }
}

JSValue JS_GetPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                             uint32_t idx)
{
    return JS_GetPropertyValue(ctx, this_obj, JS_NewUint32(ctx, idx));
}

/* Check if an object has a generalized numeric property. Return value:
   -1 for exception,
   TRUE if property exists, stored into *pval,
   FALSE if proprty does not exist.
 */
int JS_TryGetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, JSValue *pval)
{
    JSValue val = JS_UNDEFINED;
    JSAtom prop;
    int present;

    if (likely((uint64_t)idx <= JS_ATOM_MAX_INT)) {
        /* fast path */
        present = JS_HasProperty(ctx, obj, __JS_AtomFromUInt32(idx));
        if (present > 0) {
            val = JS_GetPropertyValue(ctx, obj, JS_NewInt32(ctx, idx));
            if (unlikely(JS_IsException(val)))
                present = -1;
        }
    } else {
        prop = JS_NewAtomInt64(ctx, idx);
        present = -1;
        if (likely(prop != JS_ATOM_NULL)) {
            present = JS_HasProperty(ctx, obj, prop);
            if (present > 0) {
                val = JS_GetProperty(ctx, obj, prop);
                if (unlikely(JS_IsException(val)))
                    present = -1;
            }
            JS_FreeAtom(ctx, prop);
        }
    }
    *pval = val;
    return present;
}

JSValue JS_GetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx)
{
    JSAtom prop;
    JSValue val;

    if ((uint64_t)idx <= INT32_MAX) {
        /* fast path for fast arrays */
        return JS_GetPropertyValue(ctx, obj, JS_NewInt32(ctx, idx));
    }
    prop = JS_NewAtomInt64(ctx, idx);
    if (prop == JS_ATOM_NULL)
        return JS_EXCEPTION;

    val = JS_GetProperty(ctx, obj, prop);
    JS_FreeAtom(ctx, prop);
    return val;
}

JSValue JS_GetPropertyStr(JSContext *ctx, JSValueConst this_obj,
                          const char *prop)
{
    JSAtom atom;
    JSValue ret;
    atom = JS_NewAtom(ctx, prop);
    if (atom == JS_ATOM_NULL)
        return JS_EXCEPTION;
    ret = JS_GetProperty(ctx, this_obj, atom);
    JS_FreeAtom(ctx, atom);
    return ret;
}

/* Note: the property value is not initialized. Return NULL if memory
   error. */
JSProperty *add_property(JSContext *ctx,
                                JSObject *p, JSAtom prop, int prop_flags)
{
    JSShape *sh, *new_sh;

    if (unlikely(__JS_AtomIsTaggedInt(prop))) {
        /* update is_std_array_prototype */
        if (unlikely(p->is_std_array_prototype)) {
            p->is_std_array_prototype = FALSE;
        } else if (unlikely(p->has_immutable_prototype)) {
            struct list_head *el;
            
            /* modifying Object.prototype : reset the corresponding is_std_array_prototype */
            list_for_each(el, &ctx->rt->context_list) {
                JSContext *ctx1 = list_entry(el, JSContext, link);
                if (JS_IsObject(ctx1->class_proto[JS_CLASS_OBJECT]) && 
                    JS_VALUE_GET_OBJ(ctx1->class_proto[JS_CLASS_OBJECT]) == p) {
                    if (JS_IsObject(ctx1->class_proto[JS_CLASS_ARRAY])) {
                        JSObject *p1 = JS_VALUE_GET_OBJ(ctx1->class_proto[JS_CLASS_ARRAY]);
                        p1->is_std_array_prototype = FALSE;
                    }
                    break;
                }
            }
        }
    }
    sh = p->shape;
    if (sh->is_hashed) {
        /* try to find an existing shape */
        new_sh = find_hashed_shape_prop(ctx->rt, sh, prop, prop_flags);
        if (new_sh) {
            /* matching shape found: use it */
            /*  the property array may need to be resized */
            if (new_sh->prop_size != sh->prop_size) {
                JSProperty *new_prop;
                new_prop = js_realloc(ctx, p->prop, sizeof(p->prop[0]) *
                                      new_sh->prop_size);
                if (!new_prop)
                    return NULL;
                p->prop = new_prop;
            }
            p->shape = js_dup_shape(new_sh);
            js_free_shape(ctx->rt, sh);
            return &p->prop[new_sh->prop_count - 1];
        } else if (js_rc(sh)->ref_count != 1) {
            /* if the shape is shared, clone it */
            new_sh = js_clone_shape(ctx, sh);
            if (!new_sh)
                return NULL;
            /* hash the cloned shape */
            new_sh->is_hashed = TRUE;
            js_shape_hash_link(ctx->rt, new_sh);
            js_free_shape(ctx->rt, p->shape);
            p->shape = new_sh;
        }
    }
    assert(js_rc(p->shape)->ref_count == 1);
    if (add_shape_property(ctx, &p->shape, p, prop, prop_flags))
        return NULL;
    return &p->prop[p->shape->prop_count - 1];
}

/* (convert_fast_array_to_array moved to src/quickjs/array.c) */


int remove_global_object_property(JSContext *ctx, JSObject *p,
                                         JSShapeProperty *prs, JSProperty *pr)
{
    JSVarRef *var_ref;
    JSObject *p1;
    JSProperty *pr1;
    
    var_ref = pr->u.var_ref;
    if (js_rc(var_ref)->ref_count == 1)
        return 0;
    p1 = JS_VALUE_GET_OBJ(p->u.global_object.uninitialized_vars);
    pr1 = add_property(ctx, p1, prs->atom, JS_PROP_C_W_E | JS_PROP_VARREF);
    if (!pr1)
        return -1;
    pr1->u.var_ref = var_ref;
    js_rc(var_ref)->ref_count++;
    JS_FreeValue(ctx, var_ref->value);
    var_ref->is_lexical = FALSE;
    var_ref->is_const = FALSE;
    var_ref->value = JS_UNINITIALIZED;
    return 0;
}

int delete_property(JSContext *ctx, JSObject *p, JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *lpr, *prop;
    JSProperty *pr1;
    uint32_t lpr_idx;
    intptr_t h, h1;

 redo:
    sh = p->shape;
    h1 = atom & sh->prop_hash_mask;
    h = sh->hash_table[h1];
    prop = get_shape_prop(sh);
    lpr = NULL;
    lpr_idx = 0;   /* prevent warning */
    while (h != 0) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            /* found ! */
            if (!(pr->flags & JS_PROP_CONFIGURABLE))
                return FALSE;
            /* realloc the shape if needed */
            if (lpr)
                lpr_idx = lpr - get_shape_prop(sh);
            if (js_shape_prepare_update(ctx, p, &pr))
                return -1;
            sh = p->shape;
            /* remove property */
            if (lpr) {
                lpr = get_shape_prop(sh) + lpr_idx;
                lpr->hash_next = pr->hash_next;
            } else {
                sh->hash_table[h1] = pr->hash_next;
            }
            sh->deleted_prop_count++;
            /* free the entry */
            pr1 = &p->prop[h - 1];
            if (unlikely(p->class_id == JS_CLASS_GLOBAL_OBJECT)) {
                if ((pr->flags & JS_PROP_TMASK) == JS_PROP_VARREF)
                    if (remove_global_object_property(ctx, p, pr, pr1))
                        return -1;
            }
            free_property(ctx->rt, pr1, pr->flags);
            JS_FreeAtom(ctx, pr->atom);
            /* put default values */
            pr->flags = 0;
            pr->atom = JS_ATOM_NULL;
            pr1->u.value = JS_UNDEFINED;

            /* compact the properties if too many deleted properties */
            if (sh->deleted_prop_count >= 8 &&
                sh->deleted_prop_count >= ((unsigned)sh->prop_count / 2)) {
                compact_properties(ctx, p);
            }
            return TRUE;
        }
        lpr = pr;
        h = pr->hash_next;
    }

    if (p->is_exotic) {
        if (p->fast_array) {
            uint32_t idx;
            if (JS_AtomIsArrayIndex(ctx, &idx, atom) &&
                idx < p->u.array.count) {
                if (p->class_id == JS_CLASS_ARRAY ||
                    p->class_id == JS_CLASS_ARGUMENTS ||
                    p->class_id == JS_CLASS_MAPPED_ARGUMENTS) {
                    /* Special case deleting the last element of a fast Array */
                    if (idx == p->u.array.count - 1) {
                        if (p->class_id == JS_CLASS_MAPPED_ARGUMENTS) {
                            free_var_ref(ctx->rt, p->u.array.u.var_refs[idx]);
                        } else {
                            JS_FreeValue(ctx, p->u.array.u.values[idx]);
                        }
                        p->u.array.count = idx;
                        return TRUE;
                    }
                    if (convert_fast_array_to_array(ctx, p))
                        return -1;
                    goto redo;
                } else {
                    return FALSE;
                }
            }
        } else {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em && em->delete_property) {
                return em->delete_property(ctx, JS_MKPTR(JS_TAG_OBJECT, p), atom);
            }
        }
    }
    /* not found */
    return TRUE;
}

static int call_setter(JSContext *ctx, JSObject *setter,
                       JSValueConst this_obj, JSValue val, int flags)
{
    JSValue ret, func;
    if (likely(setter)) {
        func = JS_MKPTR(JS_TAG_OBJECT, setter);
        /* Note: the field could be removed in the setter */
        func = JS_DupValue(ctx, func);
        ret = JS_CallFree(ctx, func, this_obj, 1, (JSValueConst *)&val);
        JS_FreeValue(ctx, val);
        if (JS_IsException(ret))
            return -1;
        JS_FreeValue(ctx, ret);
        return TRUE;
    } else {
        JS_FreeValue(ctx, val);
        if ((flags & JS_PROP_THROW) ||
            ((flags & JS_PROP_THROW_STRICT) && is_strict_mode(ctx))) {
            JS_ThrowTypeError(ctx, "no setter for property");
            return -1;
        }
        return FALSE;
    }
}

/* set the array length and remove the array elements if necessary. */
/* (set_array_length, expand_fast_array, add_fast_array_element, js_allocate_fast_array, js_create_array, js_create_array_free moved to src/quickjs/array.c) */


void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc)
{
    JS_FreeValue(ctx, desc->getter);
    JS_FreeValue(ctx, desc->setter);
    JS_FreeValue(ctx, desc->value);
}

/* return -1 in case of exception or TRUE or FALSE. Warning: 'val' is
   freed by the function. 'flags' is a bitmask of JS_PROP_THROW and
   JS_PROP_THROW_STRICT. 'this_obj' is the receiver. If obj !=
   this_obj, then obj must be an object (Reflect.set case). */
int JS_SetPropertyInternal(JSContext *ctx, JSValueConst obj,
                           JSAtom prop, JSValue val, JSValueConst this_obj, int flags)
{
    JSObject *p, *p1;
    JSShapeProperty *prs;
    JSProperty *pr;
    uint32_t tag;
    JSPropertyDescriptor desc;
    int ret;
#if 0
    printf("JS_SetPropertyInternal: "); print_atom(ctx, prop); printf("\n");
#endif
    tag = JS_VALUE_GET_TAG(this_obj);
    if (unlikely(tag != JS_TAG_OBJECT)) {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
            p = NULL;
            p1 = JS_VALUE_GET_OBJ(obj);
            goto prototype_lookup;
        } else {
            switch(tag) {
            case JS_TAG_NULL:
                JS_FreeValue(ctx, val);
                JS_ThrowTypeErrorAtom(ctx, "cannot set property '%s' of null", prop);
                return -1;
            case JS_TAG_UNDEFINED:
                JS_FreeValue(ctx, val);
                JS_ThrowTypeErrorAtom(ctx, "cannot set property '%s' of undefined", prop);
                return -1;
            default:
                /* even on a primitive type we can have setters on the prototype */
                p = NULL;
                p1 = JS_VALUE_GET_OBJ(JS_GetPrototypePrimitive(ctx, obj));
                goto prototype_lookup;
            }
        }
    } else {
        p = JS_VALUE_GET_OBJ(this_obj);
        p1 = JS_VALUE_GET_OBJ(obj);
        if (unlikely(p != p1))
            goto retry2;
    }

    /* fast path if obj == this_obj */
 retry:
    prs = find_own_property(&pr, p1, prop);
    if (prs) {
        if (likely((prs->flags & (JS_PROP_TMASK | JS_PROP_WRITABLE |
                                  JS_PROP_LENGTH)) == JS_PROP_WRITABLE)) {
            /* fast case */
            set_value(ctx, &pr->u.value, val);
            return TRUE;
        } else if (prs->flags & JS_PROP_LENGTH) {
            assert(p->class_id == JS_CLASS_ARRAY);
            assert(prop == JS_ATOM_length);
            return set_array_length(ctx, p, val, flags);
        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
            return call_setter(ctx, pr->u.getset.setter, this_obj, val, flags);
        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
            /* XXX: already use var_ref->is_const. Cannot simplify use the
               writable flag for JS_CLASS_MODULE_NS. */
            if (p->class_id == JS_CLASS_MODULE_NS || pr->u.var_ref->is_const)
                goto read_only_prop;
            set_value(ctx, pr->u.var_ref->pvalue, val);
            return TRUE;
        } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
            /* Instantiate property and retry (potentially useless) */
            if (JS_AutoInitProperty(ctx, p, prop, pr, prs)) {
                JS_FreeValue(ctx, val);
                return -1;
            }
            goto retry;
        } else {
            goto read_only_prop;
        }
    }

    for(;;) {
        if (p1->is_exotic) {
            if (p1->fast_array) {
                if (__JS_AtomIsTaggedInt(prop)) {
                    uint32_t idx = __JS_AtomToUInt32(prop);
                    if (idx < p1->u.array.count) {
                        if (unlikely(p == p1))
                            return JS_SetPropertyValue(ctx, this_obj, JS_NewInt32(ctx, idx), val, flags);
                        else
                            break;
                    } else if (p1->class_id >= JS_CLASS_UINT8C_ARRAY &&
                               p1->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                        goto typed_array_oob;
                    }
                } else if (p1->class_id >= JS_CLASS_UINT8C_ARRAY &&
                           p1->class_id <= JS_CLASS_FLOAT64_ARRAY) {
                    ret = JS_AtomIsNumericIndex(ctx, prop);
                    if (ret != 0) {
                        if (ret < 0) {
                            JS_FreeValue(ctx, val);
                            return -1;
                        }
                    typed_array_oob:
                        if (p == p1) {
                            /* must convert the argument even if out of bound access */
                            if (p1->class_id == JS_CLASS_BIG_INT64_ARRAY ||
                                p1->class_id == JS_CLASS_BIG_UINT64_ARRAY) {
                                int64_t v;
                                if (JS_ToBigInt64Free(ctx, &v, val))
                                    return -1;
                            } else {
                                val = JS_ToNumberFree(ctx, val);
                                JS_FreeValue(ctx, val);
                                if (JS_IsException(val))
                                    return -1;
                            }
                        } else {
                            JS_FreeValue(ctx, val);
                        }
                        return TRUE;
                    }
                }
            } else {
                const JSClassExoticMethods *em = ctx->rt->class_array[p1->class_id].exotic;
                if (em) {
                    JSValue obj1;
                    if (em->set_property) {
                        /* set_property can free the prototype */
                        obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p1));
                        ret = em->set_property(ctx, obj1, prop,
                                               val, this_obj, flags);
                        JS_FreeValue(ctx, obj1);
                        JS_FreeValue(ctx, val);
                        return ret;
                    }
                    if (em->get_own_property) {
                        /* get_own_property can free the prototype */
                        obj1 = JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, p1));
                        ret = em->get_own_property(ctx, &desc,
                                                   obj1, prop);
                        JS_FreeValue(ctx, obj1);
                        if (ret < 0) {
                            JS_FreeValue(ctx, val);
                            return ret;
                        }
                        if (ret) {
                            if (desc.flags & JS_PROP_GETSET) {
                                JSObject *setter;
                                if (JS_IsUndefined(desc.setter))
                                    setter = NULL;
                                else
                                    setter = JS_VALUE_GET_OBJ(desc.setter);
                                ret = call_setter(ctx, setter, this_obj, val, flags);
                                JS_FreeValue(ctx, desc.getter);
                                JS_FreeValue(ctx, desc.setter);
                                return ret;
                            } else {
                                JS_FreeValue(ctx, desc.value);
                                if (!(desc.flags & JS_PROP_WRITABLE))
                                    goto read_only_prop;
                                if (likely(p == p1)) {
                                    ret = JS_DefineProperty(ctx, this_obj, prop, val,
                                                            JS_UNDEFINED, JS_UNDEFINED,
                                                            JS_PROP_HAS_VALUE);
                                    JS_FreeValue(ctx, val);
                                    return ret;
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        p1 = p1->shape->proto;
    prototype_lookup:
        if (!p1)
            break;

    retry2:
        prs = find_own_property(&pr, p1, prop);
        if (prs) {
            if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                return call_setter(ctx, pr->u.getset.setter, this_obj, val, flags);
            } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                /* Instantiate property and retry (potentially useless) */
                if (JS_AutoInitProperty(ctx, p1, prop, pr, prs))
                    return -1;
                goto retry2;
            } else if (!(prs->flags & JS_PROP_WRITABLE)) {
                goto read_only_prop;
            } else {
                break;
            }
        }
    }

    if (unlikely(!p)) {
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeErrorOrFalse(ctx, flags, "not an object");
    }

    if (unlikely(!p->extensible)) {
        JS_FreeValue(ctx, val);
        return JS_ThrowTypeErrorOrFalse(ctx, flags, "object is not extensible");
    }

    if (likely(p == JS_VALUE_GET_OBJ(obj))) {
        if (p->is_exotic) {
            if (p->class_id == JS_CLASS_ARRAY && p->fast_array &&
                __JS_AtomIsTaggedInt(prop)) {
                uint32_t idx = __JS_AtomToUInt32(prop);
                if (idx == p->u.array.count) {
                    /* fast case */
                    return add_fast_array_element(ctx, p, val, flags);
                } else {
                    goto generic_create_prop;
                }
            } else {
                goto generic_create_prop;
            }
        } else {
            if (unlikely(p->class_id == JS_CLASS_GLOBAL_OBJECT))
                goto generic_create_prop;
            pr = add_property(ctx, p, prop, JS_PROP_C_W_E);
            if (unlikely(!pr)) {
                JS_FreeValue(ctx, val);
                return -1;
            }
            pr->u.value = val;
            return TRUE;
        }
    } else {
        /* generic case: modify the property in this_obj if it already exists */
        ret = JS_GetOwnPropertyInternal(ctx, &desc, p, prop);
        if (ret < 0) {
            JS_FreeValue(ctx, val);
            return ret;
        }
        if (ret) {
            if (desc.flags & JS_PROP_GETSET) {
                JS_FreeValue(ctx, desc.getter);
                JS_FreeValue(ctx, desc.setter);
                JS_FreeValue(ctx, val);
                return JS_ThrowTypeErrorOrFalse(ctx, flags, "setter is forbidden");
            } else {
                JS_FreeValue(ctx, desc.value);
                if (!(desc.flags & JS_PROP_WRITABLE) ||
                    p->class_id == JS_CLASS_MODULE_NS) {
                read_only_prop:
                    JS_FreeValue(ctx, val);
                    return JS_ThrowTypeErrorReadOnly(ctx, flags, prop);
                }
            }
            ret = JS_DefineProperty(ctx, this_obj, prop, val,
                                    JS_UNDEFINED, JS_UNDEFINED,
                                    JS_PROP_HAS_VALUE);
            JS_FreeValue(ctx, val);
            return ret;
        } else {
        generic_create_prop:
            ret = JS_CreateProperty(ctx, p, prop, val, JS_UNDEFINED, JS_UNDEFINED,
                                    flags |
                                    JS_PROP_HAS_VALUE |
                                    JS_PROP_HAS_ENUMERABLE |
                                    JS_PROP_HAS_WRITABLE |
                                    JS_PROP_HAS_CONFIGURABLE |
                                    JS_PROP_C_W_E);
            JS_FreeValue(ctx, val);
            return ret;
        }
    }
}


/* flags can be JS_PROP_THROW or JS_PROP_THROW_STRICT */
int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                               JSValue prop, JSValue val, int flags)
{
    if (likely(JS_VALUE_GET_TAG(this_obj) == JS_TAG_OBJECT &&
               JS_VALUE_GET_TAG(prop) == JS_TAG_INT)) {
        JSObject *p;
        uint32_t idx;
        double d;
        int32_t v;

        /* fast path for array access */
        p = JS_VALUE_GET_OBJ(this_obj);
        idx = JS_VALUE_GET_INT(prop);
        switch(p->class_id) {
        case JS_CLASS_ARRAY:
            if (unlikely(idx >= (uint32_t)p->u.array.count)) {
                /* fast path to add an element to the array */
                if (unlikely(idx != (uint32_t)p->u.array.count ||
                             !p->fast_array ||
                             !can_extend_fast_array(p))) {
                    goto slow_path;
                }
                /* add element */
                return add_fast_array_element(ctx, p, val, flags);
            }
            set_value(ctx, &p->u.array.u.values[idx], val);
            break;
        case JS_CLASS_ARGUMENTS:
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto slow_path;
            set_value(ctx, &p->u.array.u.values[idx], val);
            break;
        case JS_CLASS_MAPPED_ARGUMENTS:
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto slow_path;
            set_value(ctx, p->u.array.u.var_refs[idx]->pvalue, val);
            break;
        case JS_CLASS_UINT8C_ARRAY:
            if (JS_ToUint8ClampFree(ctx, &v, val))
                return -1;
            /* Note: the conversion can detach the typed array, so the
               array bound check must be done after */
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint8_ptr[idx] = v;
            break;
        case JS_CLASS_INT8_ARRAY:
        case JS_CLASS_UINT8_ARRAY:
            if (JS_ToInt32Free(ctx, &v, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint8_ptr[idx] = v;
            break;
        case JS_CLASS_INT16_ARRAY:
        case JS_CLASS_UINT16_ARRAY:
            if (JS_ToInt32Free(ctx, &v, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint16_ptr[idx] = v;
            break;
        case JS_CLASS_INT32_ARRAY:
        case JS_CLASS_UINT32_ARRAY:
            if (JS_ToInt32Free(ctx, &v, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.uint32_ptr[idx] = v;
            break;
        case JS_CLASS_BIG_INT64_ARRAY:
        case JS_CLASS_BIG_UINT64_ARRAY:
            /* XXX: need specific conversion function */
            {
                int64_t v;
                if (JS_ToBigInt64Free(ctx, &v, val))
                    return -1;
                if (unlikely(idx >= (uint32_t)p->u.array.count))
                    goto ta_out_of_bound;
                p->u.array.u.uint64_ptr[idx] = v;
            }
            break;
        case JS_CLASS_FLOAT16_ARRAY:
            if (JS_ToFloat64Free(ctx, &d, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.fp16_ptr[idx] = tofp16(d);
            break;
        case JS_CLASS_FLOAT32_ARRAY:
            if (JS_ToFloat64Free(ctx, &d, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count))
                goto ta_out_of_bound;
            p->u.array.u.float_ptr[idx] = d;
            break;
        case JS_CLASS_FLOAT64_ARRAY:
            if (JS_ToFloat64Free(ctx, &d, val))
                return -1;
            if (unlikely(idx >= (uint32_t)p->u.array.count)) {
            ta_out_of_bound:
                return TRUE;
            }
            p->u.array.u.double_ptr[idx] = d;
            break;
        default:
            goto slow_path;
        }
        return TRUE;
    } else {
        JSAtom atom;
        int ret;
    slow_path:
        atom = JS_ValueToAtom(ctx, prop);
        JS_FreeValue(ctx, prop);
        if (unlikely(atom == JS_ATOM_NULL)) {
            JS_FreeValue(ctx, val);
            return -1;
        }
        ret = JS_SetPropertyInternal(ctx, this_obj, atom, val, this_obj, flags);
        JS_FreeAtom(ctx, atom);
        return ret;
    }
}

int JS_SetPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                         uint32_t idx, JSValue val)
{
    return JS_SetPropertyValue(ctx, this_obj, JS_NewUint32(ctx, idx), val,
                               JS_PROP_THROW);
}

int JS_SetPropertyInt64(JSContext *ctx, JSValueConst this_obj,
                        int64_t idx, JSValue val)
{
    JSAtom prop;
    int res;

    if ((uint64_t)idx <= INT32_MAX) {
        /* fast path for fast arrays */
        return JS_SetPropertyValue(ctx, this_obj, JS_NewInt32(ctx, idx), val,
                                   JS_PROP_THROW);
    }
    prop = JS_NewAtomInt64(ctx, idx);
    if (prop == JS_ATOM_NULL) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    res = JS_SetProperty(ctx, this_obj, prop, val);
    JS_FreeAtom(ctx, prop);
    return res;
}

int JS_SetPropertyStr(JSContext *ctx, JSValueConst this_obj,
                      const char *prop, JSValue val)
{
    JSAtom atom;
    int ret;
    atom = JS_NewAtom(ctx, prop);
    if (atom == JS_ATOM_NULL) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    ret = JS_SetPropertyInternal(ctx, this_obj, atom, val, this_obj, JS_PROP_THROW);
    JS_FreeAtom(ctx, atom);
    return ret;
}

/* compute the property flags. For each flag: (JS_PROP_HAS_x forces
   it, otherwise def_flags is used)
   Note: makes assumption about the bit pattern of the flags
*/
static int get_prop_flags(int flags, int def_flags)
{
    int mask;
    mask = (flags >> JS_PROP_HAS_SHIFT) & JS_PROP_C_W_E;
    return (flags & mask) | (def_flags & ~mask);
}

static int JS_CreateProperty(JSContext *ctx, JSObject *p,
                             JSAtom prop, JSValueConst val,
                             JSValueConst getter, JSValueConst setter,
                             int flags)
{
    JSProperty *pr;
    int ret, prop_flags;
    JSVarRef *var_ref;
    JSObject *delete_obj;
    
    /* add a new property or modify an existing exotic one */
    if (p->is_exotic) {
        if (p->class_id == JS_CLASS_ARRAY) {
            uint32_t idx, len;

            if (p->fast_array) {
                if (__JS_AtomIsTaggedInt(prop)) {
                    idx = __JS_AtomToUInt32(prop);
                    if (idx == p->u.array.count) {
                        if (!p->extensible)
                            goto not_extensible;
                        if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET))
                            goto convert_to_array;
                        prop_flags = get_prop_flags(flags, 0);
                        if (prop_flags != JS_PROP_C_W_E)
                            goto convert_to_array;
                        return add_fast_array_element(ctx, p,
                                                      JS_DupValue(ctx, val), flags);
                    } else {
                        goto convert_to_array;
                    }
                } else if (JS_AtomIsArrayIndex(ctx, &idx, prop)) {
                    /* convert the fast array to normal array */
                convert_to_array:
                    if (convert_fast_array_to_array(ctx, p))
                        return -1;
                    goto generic_array;
                }
            } else if (JS_AtomIsArrayIndex(ctx, &idx, prop)) {
                JSProperty *plen;
                JSShapeProperty *pslen;
            generic_array:
                /* update the length field */
                plen = &p->prop[0];
                JS_ToUint32(ctx, &len, plen->u.value);
                if ((idx + 1) > len) {
                    pslen = get_shape_prop(p->shape);
                    if (unlikely(!(pslen->flags & JS_PROP_WRITABLE)))
                        return JS_ThrowTypeErrorReadOnly(ctx, flags, JS_ATOM_length);
                    /* XXX: should update the length after defining
                       the property */
                    len = idx + 1;
                    set_value(ctx, &plen->u.value, JS_NewUint32(ctx, len));
                }
            }
        } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
                   p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
            ret = JS_AtomIsNumericIndex(ctx, prop);
            if (ret != 0) {
                if (ret < 0)
                    return -1;
                return JS_ThrowTypeErrorOrFalse(ctx, flags, "cannot create numeric index in typed array");
            }
        } else if (!(flags & JS_PROP_NO_EXOTIC)) {
            const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
            if (em) {
                if (em->define_own_property) {
                    return em->define_own_property(ctx, JS_MKPTR(JS_TAG_OBJECT, p),
                                                   prop, val, getter, setter, flags);
                }
                ret = JS_IsExtensible(ctx, JS_MKPTR(JS_TAG_OBJECT, p));
                if (ret < 0)
                    return -1;
                if (!ret)
                    goto not_extensible;
            }
        }
    }

    if (!p->extensible) {
    not_extensible:
        return JS_ThrowTypeErrorOrFalse(ctx, flags, "object is not extensible");
    }

    var_ref = NULL;
    delete_obj = NULL;
    if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
        prop_flags = (flags & (JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE)) |
            JS_PROP_GETSET;
    } else {
        prop_flags = flags & JS_PROP_C_W_E;
        if (p->class_id == JS_CLASS_GLOBAL_OBJECT) {
            JSObject *p1 = JS_VALUE_GET_OBJ(p->u.global_object.uninitialized_vars);
            JSShapeProperty *prs1;
            JSProperty *pr1;
            prs1 = find_own_property(&pr1, p1, prop);
            if (prs1) {
                delete_obj = p1;
                var_ref = pr1->u.var_ref;
                js_rc(var_ref)->ref_count++;
            } else {
                var_ref = js_create_var_ref(ctx, FALSE);
                if (!var_ref)
                    return -1;
            }
            var_ref->is_const = !(prop_flags & JS_PROP_WRITABLE);
            prop_flags |= JS_PROP_VARREF;
        }
    }
    pr = add_property(ctx, p, prop, prop_flags);
    if (unlikely(!pr)) {
        if (var_ref)
            free_var_ref(ctx->rt, var_ref);
        return -1;
    }
    if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
        pr->u.getset.getter = NULL;
        if ((flags & JS_PROP_HAS_GET) && JS_IsFunction(ctx, getter)) {
            pr->u.getset.getter =
                JS_VALUE_GET_OBJ(JS_DupValue(ctx, getter));
        }
        pr->u.getset.setter = NULL;
        if ((flags & JS_PROP_HAS_SET) && JS_IsFunction(ctx, setter)) {
            pr->u.getset.setter =
                JS_VALUE_GET_OBJ(JS_DupValue(ctx, setter));
        }
    } else if (p->class_id == JS_CLASS_GLOBAL_OBJECT) {
        if (delete_obj)
            delete_property(ctx, delete_obj, prop);
        pr->u.var_ref = var_ref;
        if (flags & JS_PROP_HAS_VALUE) {
            *var_ref->pvalue = JS_DupValue(ctx, val);
        } else {
            *var_ref->pvalue = JS_UNDEFINED;
        }
    } else {
        if (flags & JS_PROP_HAS_VALUE) {
            pr->u.value = JS_DupValue(ctx, val);
        } else {
            pr->u.value = JS_UNDEFINED;
        }
    }
    return TRUE;
}

/* return FALSE if not OK */
BOOL check_define_prop_flags(int prop_flags, int flags)
{
    BOOL has_accessor, is_getset;

    if (!(prop_flags & JS_PROP_CONFIGURABLE)) {
        if ((flags & (JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE)) ==
            (JS_PROP_HAS_CONFIGURABLE | JS_PROP_CONFIGURABLE)) {
            return FALSE;
        }
        if ((flags & JS_PROP_HAS_ENUMERABLE) &&
            (flags & JS_PROP_ENUMERABLE) != (prop_flags & JS_PROP_ENUMERABLE))
            return FALSE;
        if (flags & (JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE |
                     JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
            has_accessor = ((flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) != 0);
            is_getset = ((prop_flags & JS_PROP_TMASK) == JS_PROP_GETSET);
            if (has_accessor != is_getset)
                return FALSE;
            if (!is_getset && !(prop_flags & JS_PROP_WRITABLE)) {
                /* not writable: cannot set the writable bit */
                if ((flags & (JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE)) ==
                    (JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE))
                    return FALSE;
            }
        }
    }
    return TRUE;
}

/* ensure that the shape can be safely modified */
int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                                  JSShapeProperty **pprs)
{
    JSShape *sh;
    uint32_t idx = 0;    /* prevent warning */

    sh = p->shape;
    if (sh->is_hashed) {
        if (js_rc(sh)->ref_count != 1) {
            if (pprs)
                idx = *pprs - get_shape_prop(sh);
            /* clone the shape (the resulting one is no longer hashed) */
            sh = js_clone_shape(ctx, sh);
            if (!sh)
                return -1;
            js_free_shape(ctx->rt, p->shape);
            p->shape = sh;
            if (pprs)
                *pprs = get_shape_prop(sh) + idx;
        } else {
            js_shape_hash_unlink(ctx->rt, sh);
            sh->is_hashed = FALSE;
        }
    }
    return 0;
}

int js_update_property_flags(JSContext *ctx, JSObject *p,
                                    JSShapeProperty **pprs, int flags)
{
    if (flags != (*pprs)->flags) {
        if (js_shape_prepare_update(ctx, p, pprs))
            return -1;
        (*pprs)->flags = flags;
    }
    return 0;
}

/* allowed flags:
   JS_PROP_CONFIGURABLE, JS_PROP_WRITABLE, JS_PROP_ENUMERABLE
   JS_PROP_HAS_GET, JS_PROP_HAS_SET, JS_PROP_HAS_VALUE,
   JS_PROP_HAS_CONFIGURABLE, JS_PROP_HAS_WRITABLE, JS_PROP_HAS_ENUMERABLE,
   JS_PROP_THROW, JS_PROP_NO_EXOTIC.
   If JS_PROP_THROW is set, return an exception instead of FALSE.
   if JS_PROP_NO_EXOTIC is set, do not call the exotic
   define_own_property callback.
   return -1 (exception), FALSE or TRUE.
*/
int JS_DefineProperty(JSContext *ctx, JSValueConst this_obj,
                      JSAtom prop, JSValueConst val,
                      JSValueConst getter, JSValueConst setter, int flags)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    int mask, res;

    if (JS_VALUE_GET_TAG(this_obj) != JS_TAG_OBJECT) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    p = JS_VALUE_GET_OBJ(this_obj);

 redo_prop_update:
    prs = find_own_property(&pr, p, prop);
    if (prs) {
        /* the range of the Array length property is always tested before */
        if ((prs->flags & JS_PROP_LENGTH) && (flags & JS_PROP_HAS_VALUE)) {
            uint32_t array_length;
            if (JS_ToArrayLengthFree(ctx, &array_length,
                                     JS_DupValue(ctx, val), FALSE)) {
                return -1;
            }
            /* this code relies on the fact that Uint32 are never allocated */
            val = (JSValueConst)JS_NewUint32(ctx, array_length);
            /* prs may have been modified */
            prs = find_own_property(&pr, p, prop);
            assert(prs != NULL);
        }
        /* property already exists */
        if (!check_define_prop_flags(prs->flags, flags)) {
        not_configurable:
            return JS_ThrowTypeErrorOrFalse(ctx, flags, "property is not configurable");
        }

        if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
            /* Instantiate property and retry */
            if (JS_AutoInitProperty(ctx, p, prop, pr, prs))
                return -1;
            goto redo_prop_update;
        }

        if (flags & (JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE |
                     JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
            if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
                JSObject *new_getter, *new_setter;

                if (JS_IsFunction(ctx, getter)) {
                    new_getter = JS_VALUE_GET_OBJ(getter);
                } else {
                    new_getter = NULL;
                }
                if (JS_IsFunction(ctx, setter)) {
                    new_setter = JS_VALUE_GET_OBJ(setter);
                } else {
                    new_setter = NULL;
                }

                if ((prs->flags & JS_PROP_TMASK) != JS_PROP_GETSET) {
                    if (js_shape_prepare_update(ctx, p, &prs))
                        return -1;
                    /* convert to getset */
                    if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                        if (unlikely(p->class_id == JS_CLASS_GLOBAL_OBJECT)) {
                            if (remove_global_object_property(ctx, p, prs, pr))
                                return -1;
                        }
                        free_var_ref(ctx->rt, pr->u.var_ref);
                    } else {
                        JS_FreeValue(ctx, pr->u.value);
                    }
                    prs->flags = (prs->flags &
                                  (JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE)) |
                        JS_PROP_GETSET;
                    pr->u.getset.getter = NULL;
                    pr->u.getset.setter = NULL;
                } else {
                    if (!(prs->flags & JS_PROP_CONFIGURABLE)) {
                        if ((flags & JS_PROP_HAS_GET) &&
                            new_getter != pr->u.getset.getter) {
                            goto not_configurable;
                        }
                        if ((flags & JS_PROP_HAS_SET) &&
                            new_setter != pr->u.getset.setter) {
                            goto not_configurable;
                        }
                    }
                }
                if (flags & JS_PROP_HAS_GET) {
                    if (pr->u.getset.getter)
                        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter));
                    if (new_getter)
                        JS_DupValue(ctx, getter);
                    pr->u.getset.getter = new_getter;
                }
                if (flags & JS_PROP_HAS_SET) {
                    if (pr->u.getset.setter)
                        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.setter));
                    if (new_setter)
                        JS_DupValue(ctx, setter);
                    pr->u.getset.setter = new_setter;
                }
            } else {
                if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                    /* convert to data descriptor */
                    JSVarRef *var_ref;
                    if (unlikely(p->class_id == JS_CLASS_GLOBAL_OBJECT)) {
                        var_ref = js_global_object_find_uninitialized_var(ctx, p, prop, FALSE);
                        if (!var_ref)
                            return -1;
                    } else {
                        var_ref = NULL;
                    }
                    if (js_shape_prepare_update(ctx, p, &prs)) {
                        if (var_ref)
                            free_var_ref(ctx->rt, var_ref);
                        return -1;
                    }
                    if (pr->u.getset.getter)
                        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.getter));
                    if (pr->u.getset.setter)
                        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_OBJECT, pr->u.getset.setter));
                    if (var_ref) {
                        prs->flags = (prs->flags & ~JS_PROP_TMASK) |
                            JS_PROP_VARREF | JS_PROP_WRITABLE;
                        pr->u.var_ref = var_ref;
                    } else {
                        prs->flags &= ~(JS_PROP_TMASK | JS_PROP_WRITABLE);
                        pr->u.value = JS_UNDEFINED;
                    }
                } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                    /* Note: JS_PROP_VARREF is always writable */
                } else {
                    if ((prs->flags & (JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE)) == 0 &&
                        (flags & JS_PROP_HAS_VALUE)) {
                        if (!js_same_value(ctx, val, pr->u.value)) {
                            goto not_configurable;
                        } else {
                            return TRUE;
                        }
                    }
                }
                if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                    if (flags & JS_PROP_HAS_VALUE) {
                        if (p->class_id == JS_CLASS_MODULE_NS) {
                            /* JS_PROP_WRITABLE is always true for variable
                               references, but they are write protected in module name
                               spaces. */
                            if (!js_same_value(ctx, val, *pr->u.var_ref->pvalue))
                                goto not_configurable;
                        } else {
                            /* update the reference */
                            set_value(ctx, pr->u.var_ref->pvalue,
                                      JS_DupValue(ctx, val));
                        }
                    }
                    if ((flags & (JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE)) == JS_PROP_HAS_WRITABLE) {
                        JSValue val1;
                        if (p->class_id == JS_CLASS_MODULE_NS) {
                            return JS_ThrowTypeErrorOrFalse(ctx, flags, "module namespace properties have writable = false");
                        }
                        if (js_shape_prepare_update(ctx, p, &prs))
                            return -1;
                        if (p->class_id == JS_CLASS_GLOBAL_OBJECT) {
                            pr->u.var_ref->is_const = TRUE; /* mark as read-only */
                            prs->flags &= ~JS_PROP_WRITABLE;
                        } else {
                            /* if writable is set to false, no longer a
                               reference (for mapped arguments) */
                            val1 = JS_DupValue(ctx, *pr->u.var_ref->pvalue);
                            free_var_ref(ctx->rt, pr->u.var_ref);
                            pr->u.value = val1;
                            prs->flags &= ~(JS_PROP_TMASK | JS_PROP_WRITABLE);
                        }
                    }
                } else if (prs->flags & JS_PROP_LENGTH) {
                    if (flags & JS_PROP_HAS_VALUE) {
                        /* Note: no JS code is executable because
                           'val' is guaranted to be a Uint32 */
                        res = set_array_length(ctx, p, JS_DupValue(ctx, val),
                                               flags);
                    } else {
                        res = TRUE;
                    }
                    /* still need to reset the writable flag if
                       needed.  The JS_PROP_LENGTH is kept because the
                       Uint32 test is still done if the length
                       property is read-only. */
                    if ((flags & (JS_PROP_HAS_WRITABLE | JS_PROP_WRITABLE)) ==
                        JS_PROP_HAS_WRITABLE) {
                        prs = get_shape_prop(p->shape);
                        if (js_update_property_flags(ctx, p, &prs,
                                                     prs->flags & ~JS_PROP_WRITABLE))
                            return -1;
                    }
                    return res;
                } else {
                    if (flags & JS_PROP_HAS_VALUE) {
                        JS_FreeValue(ctx, pr->u.value);
                        pr->u.value = JS_DupValue(ctx, val);
                    }
                    if (flags & JS_PROP_HAS_WRITABLE) {
                        if (js_update_property_flags(ctx, p, &prs,
                                                     (prs->flags & ~JS_PROP_WRITABLE) |
                                                     (flags & JS_PROP_WRITABLE)))
                            return -1;
                    }
                }
            }
        }
        mask = 0;
        if (flags & JS_PROP_HAS_CONFIGURABLE)
            mask |= JS_PROP_CONFIGURABLE;
        if (flags & JS_PROP_HAS_ENUMERABLE)
            mask |= JS_PROP_ENUMERABLE;
        if (js_update_property_flags(ctx, p, &prs,
                                     (prs->flags & ~mask) | (flags & mask)))
            return -1;
        return TRUE;
    }

    /* handle modification of fast array elements */
    if (p->fast_array) {
        uint32_t idx;
        uint32_t prop_flags;
        if (p->class_id == JS_CLASS_ARRAY) {
            if (__JS_AtomIsTaggedInt(prop)) {
                idx = __JS_AtomToUInt32(prop);
                if (idx < p->u.array.count) {
                    prop_flags = get_prop_flags(flags, JS_PROP_C_W_E);
                    if (prop_flags != JS_PROP_C_W_E)
                        goto convert_to_slow_array;
                    if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET)) {
                    convert_to_slow_array:
                        if (convert_fast_array_to_array(ctx, p))
                            return -1;
                        else
                            goto redo_prop_update;
                    }
                    if (flags & JS_PROP_HAS_VALUE) {
                        set_value(ctx, &p->u.array.u.values[idx], JS_DupValue(ctx, val));
                    }
                    return TRUE;
                }
            }
        } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY &&
                   p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
            JSValue num;
            int ret;

            if (!__JS_AtomIsTaggedInt(prop)) {
                /* slow path with to handle all numeric indexes */
                num = JS_AtomIsNumericIndex1(ctx, prop);
                if (JS_IsUndefined(num))
                    goto typed_array_done;
                if (JS_IsException(num))
                    return -1;
                ret = JS_NumberIsInteger(ctx, num);
                if (ret < 0) {
                    JS_FreeValue(ctx, num);
                    return -1;
                }
                if (!ret) {
                    JS_FreeValue(ctx, num);
                    return JS_ThrowTypeErrorOrFalse(ctx, flags, "non integer index in typed array");
                }
                ret = JS_NumberIsNegativeOrMinusZero(ctx, num);
                JS_FreeValue(ctx, num);
                if (ret) {
                    return JS_ThrowTypeErrorOrFalse(ctx, flags, "negative index in typed array");
                }
                if (!__JS_AtomIsTaggedInt(prop))
                    goto typed_array_oob;
            }
            idx = __JS_AtomToUInt32(prop);
            /* if the typed array is detached, p->u.array.count = 0 */
            if (idx >= p->u.array.count) {
            typed_array_oob:
                return JS_ThrowTypeErrorOrFalse(ctx, flags, "out-of-bound index in typed array");
            }
            prop_flags = get_prop_flags(flags, JS_PROP_ENUMERABLE | JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
            if (flags & (JS_PROP_HAS_GET | JS_PROP_HAS_SET) ||
                prop_flags != (JS_PROP_ENUMERABLE | JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE)) {
                return JS_ThrowTypeErrorOrFalse(ctx, flags, "invalid descriptor flags");
            }
            if (flags & JS_PROP_HAS_VALUE) {
                return JS_SetPropertyValue(ctx, this_obj, JS_NewInt32(ctx, idx), JS_DupValue(ctx, val), flags);
            }
            return TRUE;
        typed_array_done: ;
        }
    }

    return JS_CreateProperty(ctx, p, prop, val, getter, setter, flags);
}

int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj,
                                     JSAtom prop, JSAutoInitIDEnum id,
                                     void *opaque, int flags)
{
    JSObject *p;
    JSProperty *pr;

    if (JS_VALUE_GET_TAG(this_obj) != JS_TAG_OBJECT)
        return FALSE;

    p = JS_VALUE_GET_OBJ(this_obj);

    if (find_own_property(&pr, p, prop)) {
        /* property already exists */
        abort();
        return FALSE;
    }

    /* Specialized CreateProperty */
    pr = add_property(ctx, p, prop, (flags & JS_PROP_C_W_E) | JS_PROP_AUTOINIT);
    if (unlikely(!pr))
        return -1;
    pr->u.init.realm_and_id = (uintptr_t)JS_DupContext(ctx);
    assert((pr->u.init.realm_and_id & 3) == 0);
    assert(id <= 3);
    pr->u.init.realm_and_id |= id;
    pr->u.init.opaque = opaque;
    return TRUE;
}

/* shortcut to add or redefine a new property value */
int JS_DefinePropertyValue(JSContext *ctx, JSValueConst this_obj,
                           JSAtom prop, JSValue val, int flags)
{
    int ret;
    ret = JS_DefineProperty(ctx, this_obj, prop, val, JS_UNDEFINED, JS_UNDEFINED,
                            flags | JS_PROP_HAS_VALUE | JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_WRITABLE | JS_PROP_HAS_ENUMERABLE);
    JS_FreeValue(ctx, val);
    return ret;
}

int JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj,
                                JSValue prop, JSValue val, int flags)
{
    JSAtom atom;
    int ret;
    atom = JS_ValueToAtom(ctx, prop);
    JS_FreeValue(ctx, prop);
    if (unlikely(atom == JS_ATOM_NULL)) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    ret = JS_DefinePropertyValue(ctx, this_obj, atom, val, flags);
    JS_FreeAtom(ctx, atom);
    return ret;
}

int JS_DefinePropertyValueUint32(JSContext *ctx, JSValueConst this_obj,
                                 uint32_t idx, JSValue val, int flags)
{
    return JS_DefinePropertyValueValue(ctx, this_obj, JS_NewUint32(ctx, idx),
                                       val, flags);
}

int JS_DefinePropertyValueInt64(JSContext *ctx, JSValueConst this_obj,
                                int64_t idx, JSValue val, int flags)
{
    return JS_DefinePropertyValueValue(ctx, this_obj, JS_NewInt64(ctx, idx),
                                       val, flags);
}

int JS_DefinePropertyValueStr(JSContext *ctx, JSValueConst this_obj,
                              const char *prop, JSValue val, int flags)
{
    JSAtom atom;
    int ret;
    atom = JS_NewAtom(ctx, prop);
    if (atom == JS_ATOM_NULL) {
        JS_FreeValue(ctx, val);
        return -1;
    }
    ret = JS_DefinePropertyValue(ctx, this_obj, atom, val, flags);
    JS_FreeAtom(ctx, atom);
    return ret;
}

/* shortcut to add getter & setter */
int JS_DefinePropertyGetSet(JSContext *ctx, JSValueConst this_obj,
                            JSAtom prop, JSValue getter, JSValue setter,
                            int flags)
{
    int ret;
    ret = JS_DefineProperty(ctx, this_obj, prop, JS_UNDEFINED, getter, setter,
                            flags | JS_PROP_HAS_GET | JS_PROP_HAS_SET |
                            JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_ENUMERABLE);
    JS_FreeValue(ctx, getter);
    JS_FreeValue(ctx, setter);
    return ret;
}

int JS_CreateDataPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                       int64_t idx, JSValue val, int flags)
{
    return JS_DefinePropertyValueValue(ctx, this_obj, JS_NewInt64(ctx, idx),
                                       val, flags | JS_PROP_CONFIGURABLE |
                                       JS_PROP_ENUMERABLE | JS_PROP_WRITABLE);
}


/* return TRUE if 'obj' has a non empty 'name' string */
static BOOL js_object_has_name(JSContext *ctx, JSValueConst obj)
{
    JSProperty *pr;
    JSShapeProperty *prs;
    JSValueConst val;
    JSString *p;

    prs = find_own_property(&pr, JS_VALUE_GET_OBJ(obj), JS_ATOM_name);
    if (!prs)
        return FALSE;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return TRUE;
    val = pr->u.value;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_STRING)
        return TRUE;
    p = JS_VALUE_GET_STRING(val);
    return (p->len != 0);
}

int JS_DefineObjectName(JSContext *ctx, JSValueConst obj,
                               JSAtom name, int flags)
{
    if (name != JS_ATOM_NULL
    &&  JS_IsObject(obj)
    &&  !js_object_has_name(ctx, obj)
    &&  JS_DefinePropertyValue(ctx, obj, JS_ATOM_name, JS_AtomToString(ctx, name), flags) < 0) {
        return -1;
    }
    return 0;
}

int JS_DefineObjectNameComputed(JSContext *ctx, JSValueConst obj,
                                       JSValueConst str, int flags)
{
    if (JS_IsObject(obj) &&
        !js_object_has_name(ctx, obj)) {
        JSAtom prop;
        JSValue name_str;
        prop = JS_ValueToAtom(ctx, str);
        if (prop == JS_ATOM_NULL)
            return -1;
        name_str = js_get_function_name(ctx, prop);
        JS_FreeAtom(ctx, prop);
        if (JS_IsException(name_str))
            return -1;
        if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_name, name_str, flags) < 0)
            return -1;
    }
    return 0;
}

#define DEFINE_GLOBAL_LEX_VAR (1 << 7)
#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)

/* JS_ThrowSyntaxErrorVarRedeclaration defined inline in runtime.h */

/* flags is 0, DEFINE_GLOBAL_LEX_VAR or DEFINE_GLOBAL_FUNC_VAR */
/* XXX: could support exotic global object. */
int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom prop, int flags)
{
    JSObject *p;
    JSShapeProperty *prs;

    p = JS_VALUE_GET_OBJ(ctx->global_obj);
    prs = find_own_property1(p, prop);
    /* XXX: should handle JS_PROP_AUTOINIT */
    if (flags & DEFINE_GLOBAL_LEX_VAR) {
        if (prs && !(prs->flags & JS_PROP_CONFIGURABLE))
            goto fail_redeclaration;
    } else {
        if (!prs && !p->extensible)
            goto define_error;
        if (flags & DEFINE_GLOBAL_FUNC_VAR) {
            if (prs) {
                if (!(prs->flags & JS_PROP_CONFIGURABLE) &&
                    ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET ||
                     ((prs->flags & (JS_PROP_WRITABLE | JS_PROP_ENUMERABLE)) !=
                      (JS_PROP_WRITABLE | JS_PROP_ENUMERABLE)))) {
                define_error:
                    JS_ThrowTypeErrorAtom(ctx, "cannot define variable '%s'",
                                          prop);
                    return -1;
                }
            }
        }
    }
    /* check if there already is a lexical declaration */
    p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
    prs = find_own_property1(p, prop);
    if (prs) {
    fail_redeclaration:
        JS_ThrowSyntaxErrorVarRedeclaration(ctx, prop);
        return -1;
    }
    return 0;
}

/* construct a reference to a global variable */
int JS_GetGlobalVarRef(JSContext *ctx, JSAtom prop, JSValue *sp)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;

    /* no exotic behavior is possible in global_var_obj */
    p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
    prs = find_own_property(&pr, p, prop);
    if (prs) {
        /* XXX: conformance: do these tests in
           OP_put_var_ref/OP_get_var_ref ? */
        if (unlikely(JS_IsUninitialized(*pr->u.var_ref->pvalue))) {
            JS_ThrowReferenceErrorUninitialized(ctx, prs->atom);
            return -1;
        }
        if (unlikely(!(prs->flags & JS_PROP_WRITABLE))) {
            return JS_ThrowTypeErrorReadOnly(ctx, JS_PROP_THROW, prop);
        }
        sp[0] = JS_DupValue(ctx, ctx->global_var_obj);
    } else {
        int ret;
        ret = JS_HasProperty(ctx, ctx->global_obj, prop);
        if (ret < 0)
            return -1;
        if (ret) {
            sp[0] = JS_DupValue(ctx, ctx->global_obj);
        } else {
            sp[0] = JS_UNDEFINED;
        }
    }
    sp[1] = JS_AtomToValue(ctx, prop);
    return 0;
}

/* return -1, FALSE or TRUE */
int JS_DeleteGlobalVar(JSContext *ctx, JSAtom prop)
{
    JSObject *p;
    JSShapeProperty *prs;
    JSProperty *pr;
    int ret;

    /* 9.1.1.4.7 DeleteBinding ( N ) */
    p = JS_VALUE_GET_OBJ(ctx->global_var_obj);
    prs = find_own_property(&pr, p, prop);
    if (prs)
        return FALSE; /* lexical variables cannot be deleted */
    ret = JS_HasProperty(ctx, ctx->global_obj, prop);
    if (ret < 0)
        return -1;
    if (ret) {
        return JS_DeleteProperty(ctx, ctx->global_obj, prop, 0);
    } else {
        return TRUE;
    }
}

/* return -1, FALSE or TRUE. return FALSE if not configurable or
   invalid object. return -1 in case of exception.
   flags can be 0, JS_PROP_THROW or JS_PROP_THROW_STRICT */
int JS_DeleteProperty(JSContext *ctx, JSValueConst obj, JSAtom prop, int flags)
{
    JSValue obj1;
    JSObject *p;
    int res;

    obj1 = JS_ToObject(ctx, obj);
    if (JS_IsException(obj1))
        return -1;
    p = JS_VALUE_GET_OBJ(obj1);
    res = delete_property(ctx, p, prop);
    JS_FreeValue(ctx, obj1);
    if (res != FALSE)
        return res;
    if ((flags & JS_PROP_THROW) ||
        ((flags & JS_PROP_THROW_STRICT) && is_strict_mode(ctx))) {
        JS_ThrowTypeError(ctx, "could not delete property");
        return -1;
    }
    return FALSE;
}

int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, int flags)
{
    JSAtom prop;
    int res;

    if ((uint64_t)idx <= JS_ATOM_MAX_INT) {
        /* fast path for fast arrays */
        return JS_DeleteProperty(ctx, obj, __JS_AtomFromUInt32(idx), flags);
    }
    prop = JS_NewAtomInt64(ctx, idx);
    if (prop == JS_ATOM_NULL)
        return -1;
    res = JS_DeleteProperty(ctx, obj, prop, flags);
    JS_FreeAtom(ctx, prop);
    return res;
}


BOOL JS_IsError(JSContext *ctx, JSValueConst val)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(val);
    return (p->class_id == JS_CLASS_ERROR);
}

/* must be called after JS_Throw() */
void JS_SetUncatchableException(JSContext *ctx, BOOL flag)
{
    ctx->rt->current_exception_is_uncatchable = flag;
}

void JS_SetOpaque(JSValue obj, void *opaque)
{
   JSObject *p;
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        p = JS_VALUE_GET_OBJ(obj);
        p->u.opaque = opaque;
    }
}

/* return NULL if not an object of class class_id */
void *JS_GetOpaque(JSValueConst obj, JSClassID class_id)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return NULL;
    p = JS_VALUE_GET_OBJ(obj);
    if (p->class_id != class_id)
        return NULL;
    return p->u.opaque;
}

void *JS_GetOpaque2(JSContext *ctx, JSValueConst obj, JSClassID class_id)
{
    void *p = JS_GetOpaque(obj, class_id);
    if (unlikely(!p)) {
        JS_ThrowTypeErrorInvalidClass(ctx, class_id);
    }
    return p;
}

void *JS_GetAnyOpaque(JSValueConst obj, JSClassID *class_id)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT) {
        *class_id = 0;
        return NULL;
    }
    p = JS_VALUE_GET_OBJ(obj);
    *class_id = p->class_id;
    return p->u.opaque;
}

/* (Conversion and BigInt part 1 moved) */

#define JS_PRINT_MAX_DEPTH 8

typedef struct {
    JSRuntime *rt;
    JSContext *ctx; /* may be NULL */
    JSPrintValueOptions options;
    JSPrintValueWrite *write_func;
    void *write_opaque;
    int level;
    JSObject *print_stack[JS_PRINT_MAX_DEPTH]; /* level values */
} JSPrintValueState;

static void js_print_value(JSPrintValueState *s, JSValueConst val);


static void js_putc(JSPrintValueState *s, char c)
{
    s->write_func(s->write_opaque, &c, 1);
}

static void js_puts(JSPrintValueState *s, const char *str)
{
    s->write_func(s->write_opaque, str, strlen(str));
}

static void __attribute__((format(printf, 2, 3))) js_printf(JSPrintValueState *s, const char *fmt, ...)
{
    va_list ap;
    char buf[256];
    
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    s->write_func(s->write_opaque, buf, strlen(buf));
}

static void js_print_float64(JSPrintValueState *s, double d)
{
    JSDTOATempMem dtoa_mem;
    char buf[32];
    int len;
    len = js_dtoa(buf, d, 10, 0, JS_DTOA_FORMAT_FREE | JS_DTOA_MINUS_ZERO, &dtoa_mem);
    s->write_func(s->write_opaque, buf, len);
}

static uint32_t js_string_get_length(JSValueConst val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        return p->len;
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING_ROPE) {
        JSStringRope *r = JS_VALUE_GET_PTR(val);
        return r->len;
    } else {
        return 0;
    }
}

/* pretty print the first 'len' characters of 'p' */
static void js_print_string1(JSPrintValueState *s, JSString *p, int len, int sep)
{
    uint8_t buf[UTF8_CHAR_LEN_MAX];
    int l, i, c, c1;

    for(i = 0; i < len; i++) {
        c = string_get(p, i);
        switch(c) {
        case '\t':
            c = 't';
            goto quote;
        case '\r':
            c = 'r';
            goto quote;
        case '\n':
            c = 'n';
            goto quote;
        case '\b':
            c = 'b';
            goto quote;
        case '\f':
            c = 'f';
            goto quote;
        case '\\':
        quote:
            js_putc(s, '\\');
            js_putc(s, c);
            break;
        default:
            if (c == sep)
                goto quote;
            if (c >= 32 && c <= 126) {
                js_putc(s, c);
            } else if (c < 32 || 
                       (c >= 0x7f && c <= 0x9f)) {
            escape:
                js_printf(s, "\\u%04x", c);
            } else {
                if (is_hi_surrogate(c)) {
                    if ((i + 1) >= len)
                        goto escape;
                    c1 = string_get(p, i + 1);
                    if (!is_lo_surrogate(c1))
                        goto escape;
                    i++;
                    c = from_surrogate(c, c1);
                } else if (is_lo_surrogate(c)) {
                    goto escape;
                }
                l = unicode_to_utf8(buf, c);
                s->write_func(s->write_opaque, (char *)buf, l);
            }
            break;
        }
    }
}

static void js_print_string_rec(JSPrintValueState *s, JSValueConst val,
                                int sep, uint32_t pos)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        uint32_t len;
        if (pos < s->options.max_string_length) {
            len = min_uint32(p->len, s->options.max_string_length - pos);
            js_print_string1(s, p, len, sep);
        }
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING_ROPE) {
        JSStringRope *r = JS_VALUE_GET_PTR(val);
        js_print_string_rec(s, r->left, sep, pos);
        js_print_string_rec(s, r->right, sep, pos + js_string_get_length(r->left));
    } else {
        js_printf(s, "<invalid string tag %d>", (int)JS_VALUE_GET_TAG(val));
    }
}

static void js_print_string(JSPrintValueState *s, JSValueConst val)
{
    int sep;
    if (s->options.raw_dump && JS_VALUE_GET_TAG(val) == JS_TAG_STRING) {
        JSString *p = JS_VALUE_GET_STRING(val);
        js_printf(s, "%d", js_rc(p)->ref_count);
        sep = (js_rc(p)->ref_count == 1) ? '\"' : '\'';
    } else {
        sep = '\"';
    }
    js_putc(s, sep);
    js_print_string_rec(s, val, sep, 0);
    js_putc(s, sep);
    if (js_string_get_length(val) > s->options.max_string_length) {
        uint32_t n = js_string_get_length(val) - s->options.max_string_length;
        js_printf(s, "... %u more character%s", n, n > 1 ? "s" : "");
    }
}

static void js_print_raw_string(JSPrintValueState *s, JSValueConst val)
{
    const char *cstr;
    size_t len;
    cstr = JS_ToCStringLen(s->ctx, &len, val);
    if (cstr) {
        s->write_func(s->write_opaque, cstr, len);
        JS_FreeCString(s->ctx, cstr);
    }
}

static BOOL is_ascii_ident(const JSString *p)
{
    int i, c;

    if (p->len == 0)
        return FALSE;
    for(i = 0; i < p->len; i++) {
        c = string_get(p, i);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c == '_' || c == '$') || (c >= '0' && c <= '9' && i > 0)))
            return FALSE;
    }
    return TRUE;
}

static void js_print_atom(JSPrintValueState *s, JSAtom atom)
{
    int i;
    if (__JS_AtomIsTaggedInt(atom)) {
        js_printf(s, "%u", __JS_AtomToUInt32(atom));
    } else if (atom == JS_ATOM_NULL) {
        js_puts(s, "<null>");
    } else {
        assert(atom < s->rt->atom_size);
        JSString *p;
        p = s->rt->atom_array[atom];
        if (is_ascii_ident(p)) {
            for(i = 0; i < p->len; i++) {
                js_putc(s, string_get(p, i));
            }
        } else {
            js_putc(s, '"');
            js_print_string1(s, p, p->len, '\"');
            js_putc(s, '"');
        }
    }
}

/* return 0 if invalid length */
static uint32_t js_print_array_get_length(JSObject *p)
{
    JSProperty *pr;
    JSShapeProperty *prs;
    JSValueConst val;

    prs = find_own_property(&pr, p, JS_ATOM_length);
    if (!prs)
        return 0;
    if ((prs->flags & JS_PROP_TMASK) != JS_PROP_NORMAL)
        return 0;
    val = pr->u.value;
    switch(JS_VALUE_GET_NORM_TAG(val)) {
    case JS_TAG_INT:
        return JS_VALUE_GET_INT(val);
    case JS_TAG_FLOAT64:
        return (uint32_t)JS_VALUE_GET_FLOAT64(val);
    default:
        return 0;
    }
}

static void js_print_comma(JSPrintValueState *s, int *pcomma_state)
{
    switch(*pcomma_state) {
    case 0:
        break;
    case 1:
        js_printf(s, ", ");
        break;
    case 2:
        js_printf(s, " { ");
        break;
    }
    *pcomma_state = 1;
}

static void js_print_more_items(JSPrintValueState *s, int *pcomma_state,
                                uint32_t n)
{
    js_print_comma(s, pcomma_state);
    js_printf(s, "... %u more item%s", n, n > 1 ? "s" : "");
}

/* similar to js_regexp_toString() but without side effect */
static void js_print_regexp(JSPrintValueState *s, JSObject *p1)
{
    JSRegExp *re = &p1->u.regexp;
    JSString *p;
    int i, n, c, c2, bra, flags;
    static const char regexp_flags[] = { 'g', 'i', 'm', 's', 'u', 'y', 'd', 'v' };

    if (!re->pattern || !re->bytecode) {
        /* the regexp fields are zeroed at init */
        js_puts(s, "[uninitialized_regexp]");
        return;
    }
    p = re->pattern;
    js_putc(s, '/');
    if (p->len == 0) {
        js_puts(s, "(?:)");
    } else {
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
            js_putc(s, c);
            if (c2 >= 0)
                js_putc(s, c2);
        }
    }
    js_putc(s, '/');

    flags = lre_get_flags(re->bytecode->u.str8);
    for(i = 0; i < countof(regexp_flags); i++) {
        if ((flags >> i) & 1) {
            js_putc(s, regexp_flags[i]);
        }
    }
}

/* similar to js_error_toString() but without side effect */
static void js_print_error(JSPrintValueState *s, JSObject *p)
{
    const char *str;
    size_t len;

    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_name);
    if (!str) {
        js_puts(s, "Error");
    } else {
        js_puts(s, str);
        JS_FreeCString(s->ctx, str);
    }
    
    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_message);
    if (str && str[0] != '\0') {
        js_puts(s, ": ");
        js_puts(s, str);
    }
    JS_FreeCString(s->ctx, str);

    /* dump the stack if present */
    str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_stack);
    if (str) {
        js_putc(s, '\n');
        
        /* XXX: should remove the last '\n' in stack as
           v8. SpiderMonkey does not do it */
        len = strlen(str);
        if (len > 0 && str[len - 1] == '\n')
            len--;
        s->write_func(s->write_opaque, str, len);
        
        JS_FreeCString(s->ctx, str);
    }
}

static void js_print_object(JSPrintValueState *s, JSObject *p)
{
    JSRuntime *rt = s->rt;
    JSShape *sh;
    JSShapeProperty *prs;
    JSProperty *pr;
    int comma_state;
    BOOL is_array;
    uint32_t i;
    
    comma_state = 0;
    is_array = FALSE;
    if (p->class_id == JS_CLASS_ARRAY) {
        is_array = TRUE;
        js_printf(s, "[ ");
        /* XXX: print array like properties even if not fast array */
        if (p->fast_array) {
            uint32_t len, n, len1;
            len = js_print_array_get_length(p);

            len1 = min_uint32(p->u.array.count, s->options.max_item_count);
            for(i = 0; i < len1; i++) {
                js_print_comma(s, &comma_state);
                js_print_value(s, p->u.array.u.values[i]);
            }
            if (len1 < p->u.array.count)
                js_print_more_items(s, &comma_state, p->u.array.count - len1);
            if (p->u.array.count < len) {
                n = len - p->u.array.count;
                js_print_comma(s, &comma_state);
                js_printf(s, "<%u empty item%s>", n, n > 1 ? "s" : "");
            }
        }
    } else if (p->class_id >= JS_CLASS_UINT8C_ARRAY && p->class_id <= JS_CLASS_FLOAT64_ARRAY) {
        uint32_t size = 1 << typed_array_size_log2(p->class_id);
        uint32_t len1;
        int64_t v;

        js_print_atom(s, rt->class_array[p->class_id].class_name);
        js_printf(s, "(%u) [ ", p->u.array.count);
        
        is_array = TRUE;
        len1 = min_uint32(p->u.array.count, s->options.max_item_count);
        for(i = 0; i < len1; i++) {
            const uint8_t *ptr = p->u.array.u.uint8_ptr + i * size;
            js_print_comma(s, &comma_state);
            switch(p->class_id) {
            case JS_CLASS_UINT8C_ARRAY:
            case JS_CLASS_UINT8_ARRAY:
                v = *ptr;
                goto ta_int64;
            case JS_CLASS_INT8_ARRAY:
                v = *(int8_t *)ptr;
                goto ta_int64;
            case JS_CLASS_INT16_ARRAY:
                v = *(int16_t *)ptr;
                goto ta_int64;
            case JS_CLASS_UINT16_ARRAY:
                v = *(uint16_t *)ptr;
                goto ta_int64;
            case JS_CLASS_INT32_ARRAY:
                v = *(int32_t *)ptr;
                goto ta_int64;
            case JS_CLASS_UINT32_ARRAY:
                v = *(uint32_t *)ptr;
                goto ta_int64;
            case JS_CLASS_BIG_INT64_ARRAY:
                v = *(int64_t *)ptr;
            ta_int64:
                js_printf(s, "%" PRId64, v);
                break;
            case JS_CLASS_BIG_UINT64_ARRAY:
                js_printf(s, "%" PRIu64, *(uint64_t *)ptr);
                break;
            case JS_CLASS_FLOAT16_ARRAY:
                js_print_float64(s, fromfp16(*(uint16_t *)ptr));
                break;
            case JS_CLASS_FLOAT32_ARRAY:
                js_print_float64(s, *(float *)ptr);
                break;
            case JS_CLASS_FLOAT64_ARRAY:
                js_print_float64(s, *(double *)ptr);
                break;
            }
        }
        if (len1 < p->u.array.count)
            js_print_more_items(s, &comma_state, p->u.array.count - len1);
    } else if (p->class_id == JS_CLASS_BYTECODE_FUNCTION ||
               (rt->class_array[p->class_id].call != NULL &&
                p->class_id != JS_CLASS_PROXY)) {
        js_printf(s, "[Function");
        /* XXX: allow dump without ctx */
        if (!s->options.raw_dump && s->ctx) {
            const char *func_name_str;
            js_putc(s, ' ');
            func_name_str = get_prop_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), JS_ATOM_name);
            if (!func_name_str || func_name_str[0] == '\0')
                js_puts(s, "(anonymous)");
            else
                js_puts(s, func_name_str);
            JS_FreeCString(s->ctx, func_name_str);
        }
        js_printf(s, "]");
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_MAP || p->class_id == JS_CLASS_SET) {
        JSMapState *ms = p->u.opaque;
        struct list_head *el;
        
        if (!ms)
            goto default_obj;
        js_print_atom(s, rt->class_array[p->class_id].class_name);
        js_printf(s, "(%u) { ", ms->record_count);
        i = 0;
        list_for_each(el, &ms->records) {
            JSMapRecord *mr = list_entry(el, JSMapRecord, link);
            js_print_comma(s, &comma_state);
            if (mr->empty)
                continue;
            js_print_value(s, mr->key);
            if (p->class_id == JS_CLASS_MAP) {
                js_printf(s, " => ");
                js_print_value(s, mr->value);
            }
            i++;
            if (i >= s->options.max_item_count)
                break;
        }
        if (i < ms->record_count)
            js_print_more_items(s, &comma_state, ms->record_count - i);
    } else if (p->class_id == JS_CLASS_REGEXP && s->ctx) {
        js_print_regexp(s, p);
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_DATE && s->ctx) {
        /* get_date_string() has no side effect */
        JSValue str = get_date_string(s->ctx, JS_MKPTR(JS_TAG_OBJECT, p), 0, NULL, 0x23); /* toISOString() */
        if (JS_IsException(str))
            goto default_obj;
        js_print_raw_string(s, str);
        JS_FreeValueRT(s->rt, str);
        comma_state = 2;
    } else if (p->class_id == JS_CLASS_ERROR && s->ctx) {
        js_print_error(s, p);
        comma_state = 2;
    } else {
        default_obj:
        if (p->class_id != JS_CLASS_OBJECT) {
            js_print_atom(s, rt->class_array[p->class_id].class_name);
            js_printf(s, " ");
        }
        js_printf(s, "{ ");
    }
    
    sh = p->shape; /* the shape can be NULL while freeing an object */
    if (sh) {
        uint32_t j;
        
        j = 0;
        for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
            if (prs->atom != JS_ATOM_NULL) {
                if (!(prs->flags & JS_PROP_ENUMERABLE) &&
                    !s->options.show_hidden) {
                    continue;
                }
                if (j < s->options.max_item_count) {
                    pr = &p->prop[i];
                    js_print_comma(s, &comma_state);
                    js_print_atom(s, prs->atom);
                    js_printf(s, ": ");
                    
                    /* XXX: autoinit property */
                    if ((prs->flags & JS_PROP_TMASK) == JS_PROP_GETSET) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[Getter %p Setter %p]",
                                    pr->u.getset.getter, pr->u.getset.setter);
                        } else {
                            if (pr->u.getset.getter && pr->u.getset.setter) {
                                js_printf(s, "[Getter/Setter]");
                            } else if (pr->u.getset.setter) {
                                js_printf(s, "[Setter]");
                            } else {
                                js_printf(s, "[Getter]");
                            }
                        }
                    } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_VARREF) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[varref %p]", (void *)pr->u.var_ref);
                        } else {
                            js_print_value(s, *pr->u.var_ref->pvalue);
                        }
                    } else if ((prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT) {
                        if (s->options.raw_dump) {
                            js_printf(s, "[autoinit %p %d %p]",
                                    (void *)js_autoinit_get_realm(pr),
                                    js_autoinit_get_id(pr),
                                    (void *)pr->u.init.opaque);
                        } else {
                            /* XXX: could autoinit but need to restart
                               the iteration */
                            js_printf(s, "[autoinit]");
                        }
                    } else {
                        js_print_value(s, pr->u.value);
                    }
                }
                j++;
            }
        }
        if (j > s->options.max_item_count)
            js_print_more_items(s, &comma_state, j - s->options.max_item_count);
    }
    if (s->options.raw_dump && js_class_has_bytecode(p->class_id)) {
        JSFunctionBytecode *b = p->u.func.function_bytecode;
        if (b->closure_var_count) {
            JSVarRef **var_refs;
            var_refs = p->u.func.var_refs;
            
            js_print_comma(s, &comma_state);
            js_printf(s, "[[Closure]]: [");
            for(i = 0; i < b->closure_var_count; i++) {
                if (i != 0)
                    js_printf(s, ", ");
                js_print_value(s, var_refs[i]->value);
            }
            js_printf(s, " ]");
        }
        if (p->u.func.home_object) {
            js_print_comma(s, &comma_state);
            js_printf(s, "[[HomeObject]]: ");
            js_print_value(s, JS_MKPTR(JS_TAG_OBJECT, p->u.func.home_object));
        }
    }

    if (!is_array) {
        if (comma_state != 2) {
            js_printf(s, " }");
        }
    } else {
        js_printf(s, " ]");
    }
}

static int js_print_stack_index(JSPrintValueState *s, JSObject *p)
{
    int i;
    for(i = 0; i < s->level; i++)
        if (s->print_stack[i] == p)
            return i;
    return -1;
}

static void js_print_value(JSPrintValueState *s, JSValueConst val)
{
    uint32_t tag = JS_VALUE_GET_NORM_TAG(val);
    const char *str;

    switch(tag) {
    case JS_TAG_INT:
        js_printf(s, "%d", JS_VALUE_GET_INT(val));
        break;
    case JS_TAG_BOOL:
        if (JS_VALUE_GET_BOOL(val))
            str = "true";
        else
            str = "false";
        goto print_str;
    case JS_TAG_NULL:
        str = "null";
        goto print_str;
    case JS_TAG_EXCEPTION:
        str = "exception";
        goto print_str;
    case JS_TAG_UNINITIALIZED:
        str = "uninitialized";
        goto print_str;
    case JS_TAG_UNDEFINED:
        str = "undefined";
    print_str:
        js_puts(s, str);
        break;
    case JS_TAG_FLOAT64:
        js_print_float64(s, JS_VALUE_GET_FLOAT64(val));
        break;
    case JS_TAG_SHORT_BIG_INT:
        js_printf(s, "%" PRId64 "n", (int64_t)JS_VALUE_GET_SHORT_BIG_INT(val));
        break;
    case JS_TAG_BIG_INT:
        if (!s->options.raw_dump && s->ctx) {
            JSValue str = js_bigint_to_string(s->ctx, val);
            if (JS_IsException(str))
                goto raw_bigint;
            js_print_raw_string(s, str);
            js_putc(s, 'n');
            JS_FreeValueRT(s->rt, str);
        } else {
            JSBigInt *p;
            int sgn, i;
        raw_bigint:
            p = JS_VALUE_GET_PTR(val);
            /* In order to avoid allocations we just dump the limbs */
            sgn = js_bigint_sign(p);
            if (sgn)
                js_printf(s, "BigInt.asIntN(%d,", p->len * JS_LIMB_BITS);
            js_printf(s, "0x");
            for(i = p->len - 1; i >= 0; i--) {
                if (i != p->len - 1)
                    js_putc(s, '_');
#if JS_LIMB_BITS == 32
                js_printf(s, "%08x", p->tab[i]);
#else
                js_printf(s, "%016" PRIx64, p->tab[i]);
#endif
            }
            js_putc(s, 'n');
            if (sgn)
                js_putc(s, ')');
        }
        break;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        if (s->options.raw_dump && tag == JS_TAG_STRING_ROPE) {
            JSStringRope *r = JS_VALUE_GET_STRING_ROPE(val);
            js_printf(s, "[rope len=%d depth=%d]", r->len, r->depth);
        } else {
            js_print_string(s, val);
        }
        break;
    case JS_TAG_FUNCTION_BYTECODE:
        {
            JSFunctionBytecode *b = JS_VALUE_GET_PTR(val);
            js_puts(s, "[bytecode ");
            js_print_atom(s, b->func_name);
            js_putc(s, ']');
        }
        break;
    case JS_TAG_OBJECT:
        {
            JSObject *p = JS_VALUE_GET_OBJ(val);
            int idx;
            idx = js_print_stack_index(s, p);
            if (idx >= 0) {
                js_printf(s, "[circular %d]", idx);
            } else if (s->level < s->options.max_depth) {
                s->print_stack[s->level++] = p;
                js_print_object(s, JS_VALUE_GET_OBJ(val));
                s->level--;
            } else {
                JSAtom atom = s->rt->class_array[p->class_id].class_name;
                js_putc(s, '[');
                js_print_atom(s, atom);
                if (s->options.raw_dump) {
                    js_printf(s, " %p", (void *)p);
                }
                js_putc(s, ']');
            }
        }
        break;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p = JS_VALUE_GET_PTR(val);
            js_puts(s, "Symbol(");
            js_print_atom(s, js_get_atom_index(s->rt, p));
            js_putc(s, ')');
        }
        break;
    case JS_TAG_MODULE:
        js_puts(s, "[module]");
        break;
    default:
        js_printf(s, "[unknown tag %d]", tag);
        break;
    }
}

void JS_PrintValueSetDefaultOptions(JSPrintValueOptions *options)
{
    memset(options, 0, sizeof(*options));
    options->max_depth = 2;
    options->max_string_length = 1000;
    options->max_item_count = 100;
}

static void JS_PrintValueInternal(JSRuntime *rt, JSContext *ctx, 
                                  JSPrintValueWrite *write_func, void *write_opaque,
                                  JSValueConst val, const JSPrintValueOptions *options)
{
    JSPrintValueState ss, *s = &ss;
    if (options)
        s->options = *options;
    else
        JS_PrintValueSetDefaultOptions(&s->options);
    if (s->options.max_depth <= 0)
        s->options.max_depth = JS_PRINT_MAX_DEPTH;
    else
        s->options.max_depth = min_int(s->options.max_depth, JS_PRINT_MAX_DEPTH);
    if (s->options.max_string_length == 0)
        s->options.max_string_length = UINT32_MAX;
    if (s->options.max_item_count == 0)
        s->options.max_item_count = UINT32_MAX;
    s->rt = rt;
    s->ctx = ctx;
    s->write_func = write_func;
    s->write_opaque = write_opaque;
    s->level = 0;
    js_print_value(s, val);
}

void JS_PrintValueRT(JSRuntime *rt, JSPrintValueWrite *write_func, void *write_opaque,
                     JSValueConst val, const JSPrintValueOptions *options)
{
    JS_PrintValueInternal(rt, NULL, write_func, write_opaque, val, options);
}

void JS_PrintValue(JSContext *ctx, JSPrintValueWrite *write_func, void *write_opaque,
                   JSValueConst val, const JSPrintValueOptions *options)
{
    JS_PrintValueInternal(ctx->rt, ctx, write_func, write_opaque, val, options);
}

void js_dump_value_write(void *opaque, const char *buf, size_t len)
{
    FILE *fo = opaque;
    fwrite(buf, 1, len, fo);
}

__maybe_unused void print_atom(JSContext *ctx, JSAtom atom)
{
    JSPrintValueState ss, *s = &ss;
    memset(s, 0, sizeof(*s));
    s->rt = ctx->rt;
    s->ctx = ctx;
    s->write_func = js_dump_value_write;
    s->write_opaque = stdout;
    js_print_atom(s, atom);
}

void JS_DumpAtom(JSContext *ctx, const char *str, JSAtom atom)
{
    printf("%s=", str);
    print_atom(ctx, atom);
    printf("\n");
}

void JS_DumpValue(JSContext *ctx, const char *str, JSValueConst val)
{
    printf("%s=", str);
    JS_PrintValue(ctx, js_dump_value_write, stdout, val, NULL);
    printf("\n");
}

void JS_DumpValueRT(JSRuntime *rt, const char *str, JSValueConst val)
{
    printf("%s=", str);
    JS_PrintValueRT(rt, js_dump_value_write, stdout, val, NULL);
    printf("\n");
}

void JS_DumpObjectHeader(JSRuntime *rt)
{
    printf("%14s %4s %4s %14s %s\n",
           "ADDRESS", "REFS", "SHRF", "PROTO", "CONTENT");
}

/* for debug only: dump an object without side effect */
void JS_DumpObject(JSRuntime *rt, JSObject *p)
{
    JSShape *sh;
    JSPrintValueOptions options;
    
    /* XXX: should encode atoms with special characters */
    sh = p->shape; /* the shape can be NULL while freeing an object */
    printf("%14p %4d ",
           (void *)p,
           js_rc(p)->ref_count);
    if (sh) {
        printf("%3d%c %14p ",
               js_rc(sh)->ref_count,
               " *"[sh->is_hashed],
               (void *)sh->proto);
    } else {
        printf("%3s  %14s ", "-", "-");
    }

    JS_PrintValueSetDefaultOptions(&options);
    options.max_depth = 1;
    options.show_hidden = TRUE;
    options.raw_dump = TRUE;
    JS_PrintValueRT(rt, js_dump_value_write, stdout, JS_MKPTR(JS_TAG_OBJECT, p), &options);

    printf("\n");
}

void JS_DumpGCObject(JSRuntime *rt, JSGCObjectHeader *p)
{
    if (js_rc(p)->gc_obj_type == JS_GC_OBJ_TYPE_JS_OBJECT) {
        JS_DumpObject(rt, (JSObject *)p);
    } else {
        printf("%14p %4d ",
               (void *)p,
               js_rc(p)->ref_count);
        switch(js_rc(p)->gc_obj_type) {
        case JS_GC_OBJ_TYPE_FUNCTION_BYTECODE:
            printf("[function bytecode]");
            break;
        case JS_GC_OBJ_TYPE_SHAPE:
            printf("[shape]");
            break;
        case JS_GC_OBJ_TYPE_VAR_REF:
            printf("[var_ref]");
            break;
        case JS_GC_OBJ_TYPE_ASYNC_FUNCTION:
            printf("[async_function]");
            break;
        case JS_GC_OBJ_TYPE_JS_CONTEXT:
            printf("[js_context]");
            break;
        case JS_GC_OBJ_TYPE_MODULE:
            printf("[module]");
            break;
        default:
            printf("[unknown %d]", js_rc(p)->gc_obj_type);
            break;
        }
        printf("\n");
    }
}

/* (JS_IsArray moved to src/quickjs/array.c) */

/* (js_pow, BigInt part 2, and Operator subsystem moved) */

/* XXX: not 100% compatible, but mozilla seems to use a similar
   implementation to ensure that caller in non strict mode does not
   throw (ES5 compatibility) */

JSValue build_for_in_iterator(JSContext *ctx, JSValue obj)
{
    JSObject *p, *p1;
    JSPropertyEnum *tab_atom;
    int i;
    JSValue enum_obj;
    JSForInIterator *it;
    uint32_t tag, tab_atom_count;

    tag = JS_VALUE_GET_TAG(obj);
    if (tag != JS_TAG_OBJECT && tag != JS_TAG_NULL && tag != JS_TAG_UNDEFINED) {
        obj = JS_ToObjectFree(ctx, obj);
    }

    it = js_malloc(ctx, sizeof(*it));
    if (!it) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    enum_obj = JS_NewObjectProtoClass(ctx, JS_NULL, JS_CLASS_FOR_IN_ITERATOR);
    if (JS_IsException(enum_obj)) {
        js_free(ctx, it);
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    it->is_array = FALSE;
    it->obj = obj;
    it->idx = 0;
    it->tab_atom = NULL;
    it->atom_count = 0;
    it->in_prototype_chain = FALSE;
    p1 = JS_VALUE_GET_OBJ(enum_obj);
    p1->u.for_in_iterator = it;

    if (tag == JS_TAG_NULL || tag == JS_TAG_UNDEFINED)
        return enum_obj;

    p = JS_VALUE_GET_OBJ(obj);
    if (p->fast_array) {
        JSShape *sh;
        JSShapeProperty *prs;
        /* check that there are no enumerable normal fields */
        sh = p->shape;
        for(i = 0, prs = get_shape_prop(sh); i < sh->prop_count; i++, prs++) {
            if (prs->flags & JS_PROP_ENUMERABLE)
                goto normal_case;
        }
        /* for fast arrays, we only store the number of elements */
        it->is_array = TRUE;
        it->atom_count = p->u.array.count;
    } else {
    normal_case:
        if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count, p,
                                           JS_GPN_STRING_MASK | JS_GPN_SET_ENUM)) {
            JS_FreeValue(ctx, enum_obj);
            return JS_EXCEPTION;
        }
        it->tab_atom = tab_atom;
        it->atom_count = tab_atom_count;
    }
    return enum_obj;
}

/* obj -> enum_obj */
__exception int js_for_in_start(JSContext *ctx, JSValue *sp)
{
    sp[-1] = build_for_in_iterator(ctx, sp[-1]);
    if (JS_IsException(sp[-1]))
        return -1;
    return 0;
}

/* return -1 if exception, 0 if slow case, 1 if the enumeration is finished */
static __exception int js_for_in_prepare_prototype_chain_enum(JSContext *ctx,
                                                              JSValueConst enum_obj)
{
    JSObject *p;
    JSForInIterator *it;
    JSPropertyEnum *tab_atom;
    uint32_t tab_atom_count, i;
    JSValue obj1;

    p = JS_VALUE_GET_OBJ(enum_obj);
    it = p->u.for_in_iterator;

    /* check if there are enumerable properties in the prototype chain (fast path) */
    obj1 = JS_DupValue(ctx, it->obj);
    for(;;) {
        obj1 = JS_GetPrototypeFree(ctx, obj1);
        if (JS_IsNull(obj1))
            break;
        if (JS_IsException(obj1))
            goto fail;
        if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count,
                                           JS_VALUE_GET_OBJ(obj1),
                                           JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY)) {
            JS_FreeValue(ctx, obj1);
            goto fail;
        }
        JS_FreePropertyEnum(ctx, tab_atom, tab_atom_count);
        if (tab_atom_count != 0) {
            JS_FreeValue(ctx, obj1);
            goto slow_path;
        }
        /* must check for timeout to avoid infinite loop */
        if (js_poll_interrupts(ctx)) {
            JS_FreeValue(ctx, obj1);
            goto fail;
        }
    }
    JS_FreeValue(ctx, obj1);
    return 1;

 slow_path:
    /* add the visited properties, even if they are not enumerable */
    if (it->is_array) {
        if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count,
                                           JS_VALUE_GET_OBJ(it->obj),
                                           JS_GPN_STRING_MASK | JS_GPN_SET_ENUM)) {
            goto fail;
        }
        it->is_array = FALSE;
        it->tab_atom = tab_atom;
        it->atom_count = tab_atom_count;
    }

    for(i = 0; i < it->atom_count; i++) {
        if (JS_DefinePropertyValue(ctx, enum_obj, it->tab_atom[i].atom, JS_NULL, JS_PROP_ENUMERABLE) < 0)
            goto fail;
    }
    return 0;
 fail:
    return -1;
}

/* enum_obj -> enum_obj value done */
__exception int js_for_in_next(JSContext *ctx, JSValue *sp)
{
    JSValueConst enum_obj;
    JSObject *p;
    JSAtom prop;
    JSForInIterator *it;
    JSPropertyEnum *tab_atom;
    uint32_t tab_atom_count;
    int ret;

    enum_obj = sp[-1];
    /* fail safe */
    if (JS_VALUE_GET_TAG(enum_obj) != JS_TAG_OBJECT)
        goto done;
    p = JS_VALUE_GET_OBJ(enum_obj);
    if (p->class_id != JS_CLASS_FOR_IN_ITERATOR)
        goto done;
    it = p->u.for_in_iterator;

    for(;;) {
        if (it->idx >= it->atom_count) {
            if (JS_IsNull(it->obj) || JS_IsUndefined(it->obj))
                goto done; /* not an object */
            /* no more property in the current object: look in the prototype */
            if (!it->in_prototype_chain) {
                ret = js_for_in_prepare_prototype_chain_enum(ctx, enum_obj);
                if (ret < 0)
                    return -1;
                if (ret)
                    goto done;
                it->in_prototype_chain = TRUE;
            }
            it->obj = JS_GetPrototypeFree(ctx, it->obj);
            if (JS_IsException(it->obj))
                return -1;
            if (JS_IsNull(it->obj))
                goto done; /* no more prototype */

            /* must check for timeout to avoid infinite loop */
            if (js_poll_interrupts(ctx))
                return -1;

            if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count,
                                               JS_VALUE_GET_OBJ(it->obj),
                                               JS_GPN_STRING_MASK | JS_GPN_SET_ENUM)) {
                return -1;
            }
            JS_FreePropertyEnum(ctx, it->tab_atom, it->atom_count);
            it->tab_atom = tab_atom;
            it->atom_count = tab_atom_count;
            it->idx = 0;
        } else {
            if (it->is_array) {
                prop = __JS_AtomFromUInt32(it->idx);
                it->idx++;
            } else {
                BOOL is_enumerable;
                prop = it->tab_atom[it->idx].atom;
                is_enumerable = it->tab_atom[it->idx].is_enumerable;
                it->idx++;
                if (it->in_prototype_chain) {
                    /* slow case: we are in the prototype chain */
                    ret = JS_GetOwnPropertyInternal(ctx, NULL, JS_VALUE_GET_OBJ(enum_obj), prop);
                    if (ret < 0)
                        return ret;
                    if (ret)
                        continue; /* already visited */
                    /* add to the visited property list */
                    if (JS_DefinePropertyValue(ctx, enum_obj, prop, JS_NULL,
                                               JS_PROP_ENUMERABLE) < 0)
                        return -1;
                }
                if (!is_enumerable)
                    continue;
            }
            /* check if the property was deleted */
            ret = JS_GetOwnPropertyInternal(ctx, NULL, JS_VALUE_GET_OBJ(it->obj), prop);
            if (ret < 0)
                return ret;
            if (ret)
                break;
        }
    }
    /* return the property */
    sp[0] = JS_AtomToValue(ctx, prop);
    sp[1] = JS_FALSE;
    return 0;
 done:
    /* return the end */
    sp[0] = JS_UNDEFINED;
    sp[1] = JS_TRUE;
    return 0;
}

JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj,
                               JSValueConst method)
{
    JSValue enum_obj;

    enum_obj = JS_Call(ctx, method, obj, 0, NULL);
    if (JS_IsException(enum_obj))
        return enum_obj;
    if (!JS_IsObject(enum_obj)) {
        JS_FreeValue(ctx, enum_obj);
        return JS_ThrowTypeErrorNotAnObject(ctx);
    }
    return enum_obj;
}

JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async)
{
    JSValue method, ret, sync_iter;

    if (is_async) {
        method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_asyncIterator);
        if (JS_IsException(method))
            return method;
        if (JS_IsUndefined(method) || JS_IsNull(method)) {
            method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_iterator);
            if (JS_IsException(method))
                return method;
            sync_iter = JS_GetIterator2(ctx, obj, method);
            JS_FreeValue(ctx, method);
            if (JS_IsException(sync_iter))
                return sync_iter;
            ret = JS_CreateAsyncFromSyncIterator(ctx, sync_iter);
            JS_FreeValue(ctx, sync_iter);
            return ret;
        }
    } else {
        method = JS_GetProperty(ctx, obj, JS_ATOM_Symbol_iterator);
        if (JS_IsException(method))
            return method;
    }
    if (!JS_IsFunction(ctx, method)) {
        JS_FreeValue(ctx, method);
        return JS_ThrowTypeError(ctx, "value is not iterable");
    }
    ret = JS_GetIterator2(ctx, obj, method);
    JS_FreeValue(ctx, method);
    return ret;
}

/* return *pdone = 2 if the iterator object is not parsed */
JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj,
                                JSValueConst method,
                                int argc, JSValueConst *argv, int *pdone)
{
    JSValue obj;

    /* fast path for the built-in iterators (avoid creating the
       intermediate result object) */
    if (JS_IsObject(method)) {
        JSObject *p = JS_VALUE_GET_OBJ(method);
        if (p->class_id == JS_CLASS_C_FUNCTION &&
            p->u.cfunc.cproto == JS_CFUNC_iterator_next) {
            JSCFunctionType func;
            JSValueConst args[1];

            /* in case the function expects one argument */
            if (argc == 0) {
                args[0] = JS_UNDEFINED;
                argv = args;
            }
            func = p->u.cfunc.c_function;
            return func.iterator_next(ctx, enum_obj, argc, argv,
                                      pdone, p->u.cfunc.magic);
        }
    }
    obj = JS_Call(ctx, method, enum_obj, argc, argv);
    if (JS_IsException(obj))
        goto fail;
    if (!JS_IsObject(obj)) {
        JS_FreeValue(ctx, obj);
        JS_ThrowTypeError(ctx, "iterator must return an object");
        goto fail;
    }
    *pdone = 2;
    return obj;
 fail:
    *pdone = FALSE;
    return JS_EXCEPTION;
}

/* Note: always return JS_UNDEFINED when *pdone = TRUE. */
JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
                               JSValueConst method,
                               int argc, JSValueConst *argv, BOOL *pdone)
{
    JSValue obj, value, done_val;
    int done;

    obj = JS_IteratorNext2(ctx, enum_obj, method, argc, argv, &done);
    if (JS_IsException(obj))
        goto fail;
    if (likely(done == 0)) {
        *pdone = FALSE;
        return obj;
    } else if (done != 2) {
        JS_FreeValue(ctx, obj);
        *pdone = TRUE;
        return JS_UNDEFINED;
    } else {
        done_val = JS_GetProperty(ctx, obj, JS_ATOM_done);
        if (JS_IsException(done_val))
            goto fail;
        *pdone = JS_ToBoolFree(ctx, done_val);
        value = JS_UNDEFINED;
        if (!*pdone) {
            value = JS_GetProperty(ctx, obj, JS_ATOM_value);
        }
        JS_FreeValue(ctx, obj);
        return value;
    }
 fail:
    JS_FreeValue(ctx, obj);
    *pdone = FALSE;
    return JS_EXCEPTION;
}

/* return < 0 in case of exception */
int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj,
                            BOOL is_exception_pending)
{
    JSValue method, ret, ex_obj;
    int res;

    if (is_exception_pending) {
        ex_obj = ctx->rt->current_exception;
        ctx->rt->current_exception = JS_UNINITIALIZED;
        res = -1;
    } else {
        ex_obj = JS_UNDEFINED;
        res = 0;
    }
    method = JS_GetProperty(ctx, enum_obj, JS_ATOM_return);
    if (JS_IsException(method)) {
        res = -1;
        goto done;
    }
    if (JS_IsUndefined(method) || JS_IsNull(method)) {
        goto done;
    }
    ret = JS_CallFree(ctx, method, enum_obj, 0, NULL);
    if (!is_exception_pending) {
        if (JS_IsException(ret)) {
            res = -1;
        } else if (!JS_IsObject(ret)) {
            JS_ThrowTypeErrorNotAnObject(ctx);
            res = -1;
        }
    }
    JS_FreeValue(ctx, ret);
 done:
    if (is_exception_pending) {
        JS_Throw(ctx, ex_obj);
    }
    return res;
}

/* obj -> enum_rec (3 slots) */
__exception int js_for_of_start(JSContext *ctx, JSValue *sp,
                                       BOOL is_async)
{
    JSValue op1, obj, method;
    op1 = sp[-1];
    obj = JS_GetIterator(ctx, op1, is_async);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, op1);
    sp[-1] = obj;
    method = JS_GetProperty(ctx, obj, JS_ATOM_next);
    if (JS_IsException(method))
        return -1;
    sp[0] = method;
    return 0;
}

/* enum_rec [objs] -> enum_rec [objs] value done. There are 'offset'
   objs. If 'done' is true or in case of exception, 'enum_rec' is set
   to undefined. If 'done' is true, 'value' is always set to
   undefined. */
__exception int js_for_of_next(JSContext *ctx, JSValue *sp, int offset)
{
    JSValue value = JS_UNDEFINED;
    int done = 1;

    if (likely(!JS_IsUndefined(sp[offset]))) {
        value = JS_IteratorNext(ctx, sp[offset], sp[offset + 1], 0, NULL, &done);
        if (JS_IsException(value))
            done = -1;
        if (done) {
            /* value is JS_UNDEFINED or JS_EXCEPTION */
            /* replace the iteration object with undefined */
            JS_FreeValue(ctx, sp[offset]);
            sp[offset] = JS_UNDEFINED;
            if (done < 0) {
                return -1;
            } else {
                JS_FreeValue(ctx, value);
                value = JS_UNDEFINED;
            }
        }
    }
    sp[0] = value;
    sp[1] = JS_NewBool(ctx, done);
    return 0;
}

__exception int js_for_await_of_next(JSContext *ctx, JSValue *sp)
{
    JSValue obj, iter, next;

    sp[-1] = JS_UNDEFINED; /* disable the catch offset so that
                              exceptions do not close the iterator */
    iter = sp[-3];
    next = sp[-2];
    obj = JS_Call(ctx, next, iter, 0, NULL);
    if (JS_IsException(obj))
        return -1;
    sp[0] = obj;
    return 0;
}

JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj,
                                           BOOL *pdone)
{
    JSValue done_val, value;
    BOOL done;
    done_val = JS_GetProperty(ctx, obj, JS_ATOM_done);
    if (JS_IsException(done_val))
        goto fail;
    done = JS_ToBoolFree(ctx, done_val);
    value = JS_GetProperty(ctx, obj, JS_ATOM_value);
    if (JS_IsException(value))
        goto fail;
    *pdone = done;
    return value;
 fail:
    *pdone = FALSE;
    return JS_EXCEPTION;
}

__exception int js_iterator_get_value_done(JSContext *ctx, JSValue *sp)
{
    JSValue obj, value;
    BOOL done;
    obj = sp[-1];
    if (!JS_IsObject(obj)) {
        JS_ThrowTypeError(ctx, "iterator must return an object");
        return -1;
    }
    value = JS_IteratorGetCompleteValue(ctx, obj, &done);
    if (JS_IsException(value))
        return -1;
    JS_FreeValue(ctx, obj);
    /* put again the catch offset so that exceptions close the
       iterator */
    sp[-2] = JS_NewCatchOffset(ctx, 0); 
    sp[-1] = value;
    sp[0] = JS_NewBool(ctx, done);
    return 0;
}

JSValue js_create_iterator_result(JSContext *ctx,
                                         JSValue val,
                                         BOOL done)
{
    JSValue obj;
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj)) {
        JS_FreeValue(ctx, val);
        return obj;
    }
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_value,
                               val, JS_PROP_C_W_E) < 0) {
        goto fail;
    }
    if (JS_DefinePropertyValue(ctx, obj, JS_ATOM_done,
                               JS_NewBool(ctx, done), JS_PROP_C_W_E) < 0) {
    fail:
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    return obj;
}



/* Access an Array's internal JSValue array if available */

__exception int js_append_enumerate(JSContext *ctx, JSValue *sp)
{
    JSValue iterator, enumobj, method, value;
    int is_array_iterator;
    JSValue *arrp;
    uint32_t i, count32, pos;
    JSCFunctionType ft;

    if (JS_VALUE_GET_TAG(sp[-2]) != JS_TAG_INT) {
        JS_ThrowInternalError(ctx, "invalid index for append");
        return -1;
    }

    pos = JS_VALUE_GET_INT(sp[-2]);

    /* XXX: further optimisations:
       - use ctx->array_proto_values?
       - check if array_iterator_prototype next method is built-in and
         avoid constructing actual iterator object?
       - build this into js_for_of_start and use in all `for (x of o)` loops
     */
    iterator = JS_GetProperty(ctx, sp[-1], JS_ATOM_Symbol_iterator);
    if (JS_IsException(iterator))
        return -1;
    ft.generic_magic = js_create_array_iterator;
    is_array_iterator = JS_IsCFunction(ctx, iterator, ft.generic,
                                       JS_ITERATOR_KIND_VALUE);
    JS_FreeValue(ctx, iterator);

    enumobj = JS_GetIterator(ctx, sp[-1], FALSE);
    if (JS_IsException(enumobj))
        return -1;
    method = JS_GetProperty(ctx, enumobj, JS_ATOM_next);
    if (JS_IsException(method)) {
        JS_FreeValue(ctx, enumobj);
        return -1;
    }

    ft.iterator_next = js_array_iterator_next;
    if (is_array_iterator
    &&  JS_IsCFunction(ctx, method, ft.generic, 0)
    &&  js_get_fast_array(ctx, sp[-1], &arrp, &count32)) {
        uint32_t len;
        if (js_get_length32(ctx, &len, sp[-1]))
            goto exception;
        /* if len > count32, the elements >= count32 might be read in
           the prototypes and might have side effects */
        if (len != count32)
            goto general_case;
        /* Handle fast arrays explicitly */
        for (i = 0; i < count32; i++) {
            if (JS_DefinePropertyValueUint32(ctx, sp[-3], pos++,
                                             JS_DupValue(ctx, arrp[i]), JS_PROP_C_W_E) < 0)
                goto exception;
        }
    } else {
    general_case:
        for (;;) {
            BOOL done;
            value = JS_IteratorNext(ctx, enumobj, method, 0, NULL, &done);
            if (JS_IsException(value))
                goto exception;
            if (done) {
                /* value is JS_UNDEFINED */
                break;
            }
            if (JS_DefinePropertyValueUint32(ctx, sp[-3], pos++, value, JS_PROP_C_W_E) < 0)
                goto exception;
        }
    }
    /* Note: could raise an error if too many elements */
    sp[-2] = JS_NewInt32(ctx, pos);
    JS_FreeValue(ctx, enumobj);
    JS_FreeValue(ctx, method);
    return 0;

exception:
    JS_IteratorClose(ctx, enumobj, TRUE);
    JS_FreeValue(ctx, enumobj);
    JS_FreeValue(ctx, method);
    return -1;
}

int JS_CopyDataProperties(JSContext *ctx,
                                             JSValueConst target,
                                             JSValueConst source,
                                             JSValueConst excluded,
                                             BOOL setprop)
{
    JSPropertyEnum *tab_atom;
    JSValue val;
    uint32_t i, tab_atom_count;
    JSObject *p;
    JSObject *pexcl = NULL;
    int ret, gpn_flags;
    JSPropertyDescriptor desc;
    BOOL is_enumerable;

    if (JS_VALUE_GET_TAG(source) != JS_TAG_OBJECT)
        return 0;

    if (JS_VALUE_GET_TAG(excluded) == JS_TAG_OBJECT)
        pexcl = JS_VALUE_GET_OBJ(excluded);

    p = JS_VALUE_GET_OBJ(source);

    gpn_flags = JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK | JS_GPN_ENUM_ONLY;
    if (p->is_exotic) {
        const JSClassExoticMethods *em = ctx->rt->class_array[p->class_id].exotic;
        /* cannot use JS_GPN_ENUM_ONLY with e.g. proxies because it
           introduces a visible change */
        if (em && em->get_own_property_names) {
            gpn_flags &= ~JS_GPN_ENUM_ONLY;
        }
    }
    if (JS_GetOwnPropertyNamesInternal(ctx, &tab_atom, &tab_atom_count, p,
                                       gpn_flags))
        return -1;

    for (i = 0; i < tab_atom_count; i++) {
        if (pexcl) {
            ret = JS_GetOwnPropertyInternal(ctx, NULL, pexcl, tab_atom[i].atom);
            if (ret) {
                if (ret < 0)
                    goto exception;
                continue;
            }
        }
        if (!(gpn_flags & JS_GPN_ENUM_ONLY)) {
            /* test if the property is enumerable */
            ret = JS_GetOwnPropertyInternal(ctx, &desc, p, tab_atom[i].atom);
            if (ret < 0)
                goto exception;
            if (!ret)
                continue;
            is_enumerable = (desc.flags & JS_PROP_ENUMERABLE) != 0;
            js_free_desc(ctx, &desc);
            if (!is_enumerable)
                continue;
        }
        val = JS_GetProperty(ctx, source, tab_atom[i].atom);
        if (JS_IsException(val))
            goto exception;
        if (setprop)
            ret = JS_SetProperty(ctx, target, tab_atom[i].atom, val);
        else
            ret = JS_DefinePropertyValue(ctx, target, tab_atom[i].atom, val,
                                         JS_PROP_C_W_E);
        if (ret < 0)
            goto exception;
    }
    JS_FreePropertyEnum(ctx, tab_atom, tab_atom_count);
    return 0;
 exception:
    JS_FreePropertyEnum(ctx, tab_atom, tab_atom_count);
    return -1;
}

/* only valid inside C functions */

void js_global_object_finalizer(JSRuntime *rt, JSValue obj)
{
    JSObject *p = JS_VALUE_GET_OBJ(obj);
    JS_FreeValueRT(rt, p->u.global_object.uninitialized_vars);
}

void js_global_object_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    JS_MarkValue(rt, p->u.global_object.uninitialized_vars, mark_func);
}


static JSValue js_instantiate_prototype(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque)
{
    JSValue obj, this_val;
    int ret;

    this_val = JS_MKPTR(JS_TAG_OBJECT, p);
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    set_cycle_flag(ctx, obj);
    set_cycle_flag(ctx, this_val);
    ret = JS_DefinePropertyValue(ctx, obj, JS_ATOM_constructor,
                                 JS_DupValue(ctx, this_val),
                                 JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    if (ret < 0) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    return obj;
}


JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                   int class_id)
{
    JSValue proto, obj;
    JSContext *realm;

    if (JS_IsUndefined(ctor)) {
        proto = JS_DupValue(ctx, ctx->class_proto[class_id]);
    } else {
        proto = JS_GetProperty(ctx, ctor, JS_ATOM_prototype);
        if (JS_IsException(proto))
            return proto;
        if (!JS_IsObject(proto)) {
            JS_FreeValue(ctx, proto);
            realm = JS_GetFunctionRealm(ctx, ctor);
            if (!realm)
                return JS_EXCEPTION;
            proto = JS_DupValue(ctx, realm->class_proto[class_id]);
        }
    }
    obj = JS_NewObjectProtoClass(ctx, proto, class_id);
    JS_FreeValue(ctx, proto);
    return obj;
}


JSAtom find_atom(JSContext *ctx, const char *name)
{
    JSAtom atom;
    int len;

    if (*name == '[') {
        name++;
        len = strlen(name) - 1;
        /* We assume 8 bit non null strings, which is the case for these
           symbols */
        for(atom = JS_ATOM_Symbol_toPrimitive; atom < JS_ATOM_END; atom++) {
            JSAtomStruct *p = ctx->rt->atom_array[atom];
            JSString *str = p;
            if (str->len == len && !memcmp(str->u.str8, name, len))
                return JS_DupAtom(ctx, atom);
        }
        abort();
    } else {
        atom = JS_NewAtom(ctx, name);
    }
    return atom;
}

JSValue JS_NewObjectProtoList(JSContext *ctx, JSValueConst proto,
                                     const JSCFunctionListEntry *fields, int n_fields)
{
    JSValue obj;
    obj = JS_NewObjectProtoClassAlloc(ctx, proto, JS_CLASS_OBJECT, n_fields);
    if (JS_IsException(obj))
        return obj;
    if (JS_SetPropertyFunctionList(ctx, obj, fields, n_fields)) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    return obj;
}

static JSValue JS_InstantiateFunctionListItem2(JSContext *ctx, JSObject *p,
                                               JSAtom atom, void *opaque)
{
    const JSCFunctionListEntry *e = opaque;
    JSValue val, proto;

    switch(e->def_type) {
    case JS_DEF_CFUNC:
        val = JS_NewCFunction2(ctx, e->u.func.cfunc.generic,
                               e->name, e->u.func.length, e->u.func.cproto, e->magic);
        break;
    case JS_DEF_PROP_STRING:
        val = JS_NewAtomString(ctx, e->u.str);
        break;
    case JS_DEF_OBJECT:
        /* XXX: could add a flag */
        if (atom == JS_ATOM_Symbol_unscopables)
            proto = JS_NULL;
        else
            proto = ctx->class_proto[JS_CLASS_OBJECT];
        val = JS_NewObjectProtoList(ctx, proto,
                                    e->u.prop_list.tab, e->u.prop_list.len);
        break;
    default:
        abort();
    }
    return val;
}

static int JS_InstantiateFunctionListItem(JSContext *ctx, JSValueConst obj,
                                          JSAtom atom,
                                          const JSCFunctionListEntry *e)
{
    JSValue val;
    int prop_flags = e->prop_flags;

    switch(e->def_type) {
    case JS_DEF_ALIAS: /* using autoinit for aliases is not safe */
        {
            JSAtom atom1 = find_atom(ctx, e->u.alias.name);
            switch (e->u.alias.base) {
            case -1:
                val = JS_GetProperty(ctx, obj, atom1);
                break;
            case 0:
                val = JS_GetProperty(ctx, ctx->global_obj, atom1);
                break;
            case 1:
                val = JS_GetProperty(ctx, ctx->class_proto[JS_CLASS_ARRAY], atom1);
                break;
            default:
                abort();
            }
            JS_FreeAtom(ctx, atom1);
            if (JS_IsException(val))
                return -1;
            if (atom == JS_ATOM_Symbol_toPrimitive) {
                /* Symbol.toPrimitive functions are not writable */
                prop_flags = JS_PROP_CONFIGURABLE;
            } else if (atom == JS_ATOM_Symbol_hasInstance) {
                /* Function.prototype[Symbol.hasInstance] is not writable nor configurable */
                prop_flags = 0;
            }
        }
        break;
    case JS_DEF_CFUNC:
        if (atom == JS_ATOM_Symbol_toPrimitive) {
            /* Symbol.toPrimitive functions are not writable */
            prop_flags = JS_PROP_CONFIGURABLE;
        } else if (atom == JS_ATOM_Symbol_hasInstance) {
            /* Function.prototype[Symbol.hasInstance] is not writable nor configurable */
            prop_flags = 0;
        }
        if (JS_DefineAutoInitProperty(ctx, obj, atom, JS_AUTOINIT_ID_PROP,
                                      (void *)e, prop_flags) < 0)
            return -1;
        return 0;
    case JS_DEF_CGETSET: /* XXX: use autoinit again ? */
    case JS_DEF_CGETSET_MAGIC:
        {
            JSValue getter, setter;
            char buf[64];

            getter = JS_UNDEFINED;
            if (e->u.getset.get.generic) {
                snprintf(buf, sizeof(buf), "get %s", e->name);
                getter = JS_NewCFunction2(ctx, e->u.getset.get.generic,
                                          buf, 0, e->def_type == JS_DEF_CGETSET_MAGIC ? JS_CFUNC_getter_magic : JS_CFUNC_getter,
                                          e->magic);
                if (JS_IsException(getter))
                    return -1;
            }
            setter = JS_UNDEFINED;
            if (e->u.getset.set.generic) {
                snprintf(buf, sizeof(buf), "set %s", e->name);
                setter = JS_NewCFunction2(ctx, e->u.getset.set.generic,
                                          buf, 1, e->def_type == JS_DEF_CGETSET_MAGIC ? JS_CFUNC_setter_magic : JS_CFUNC_setter,
                                          e->magic);
                if (JS_IsException(setter)) {
                    JS_FreeValue(ctx, getter);
                    return -1;
                }
            }
            if (JS_DefinePropertyGetSet(ctx, obj, atom, getter, setter, prop_flags) < 0)
                return -1;
            return 0;
        }
        break;
    case JS_DEF_PROP_INT32:
        val = JS_NewInt32(ctx, e->u.i32);
        break;
    case JS_DEF_PROP_INT64:
        val = JS_NewInt64(ctx, e->u.i64);
        break;
    case JS_DEF_PROP_DOUBLE:
        val = __JS_NewFloat64(ctx, e->u.f64);
        break;
    case JS_DEF_PROP_UNDEFINED:
        val = JS_UNDEFINED;
        break;
    case JS_DEF_PROP_ATOM:
        val = JS_AtomToValue(ctx, e->u.i32);
        break;
    case JS_DEF_PROP_BOOL:
        val = JS_NewBool(ctx, e->u.i32);
        break;
    case JS_DEF_PROP_STRING:
    case JS_DEF_OBJECT:
        if (JS_DefineAutoInitProperty(ctx, obj, atom, JS_AUTOINIT_ID_PROP,
                                      (void *)e, prop_flags) < 0)
            return -1;
        return 0;
    default:
        abort();
    }
    if (JS_DefinePropertyValue(ctx, obj, atom, val, prop_flags) < 0)
        return -1;
    return 0;
}

int JS_SetPropertyFunctionList(JSContext *ctx, JSValueConst obj,
                               const JSCFunctionListEntry *tab, int len)
{
    int i, ret;

    for (i = 0; i < len; i++) {
        const JSCFunctionListEntry *e = &tab[i];
        JSAtom atom = find_atom(ctx, e->name);
        if (atom == JS_ATOM_NULL)
            return -1;
        ret = JS_InstantiateFunctionListItem(ctx, obj, atom, e);
        JS_FreeAtom(ctx, atom);
        if (ret)
            return -1;
    }
    return 0;
}

int JS_AddModuleExportList(JSContext *ctx, JSModuleDef *m,
                           const JSCFunctionListEntry *tab, int len)
{
    int i;
    for(i = 0; i < len; i++) {
        if (JS_AddModuleExport(ctx, m, tab[i].name))
            return -1;
    }
    return 0;
}

int JS_SetModuleExportList(JSContext *ctx, JSModuleDef *m,
                           const JSCFunctionListEntry *tab, int len)
{
    int i;
    JSValue val;

    for(i = 0; i < len; i++) {
        const JSCFunctionListEntry *e = &tab[i];
        switch(e->def_type) {
        case JS_DEF_CFUNC:
            val = JS_NewCFunction2(ctx, e->u.func.cfunc.generic,
                                   e->name, e->u.func.length, e->u.func.cproto, e->magic);
            break;
        case JS_DEF_PROP_STRING:
            val = JS_NewString(ctx, e->u.str);
            break;
        case JS_DEF_PROP_INT32:
            val = JS_NewInt32(ctx, e->u.i32);
            break;
        case JS_DEF_PROP_INT64:
            val = JS_NewInt64(ctx, e->u.i64);
            break;
        case JS_DEF_PROP_DOUBLE:
            val = __JS_NewFloat64(ctx, e->u.f64);
            break;
        case JS_DEF_OBJECT:
            val = JS_NewObjectProtoList(ctx, ctx->class_proto[JS_CLASS_OBJECT],
                                        e->u.prop_list.tab, e->u.prop_list.len);
            break;
        default:
            abort();
        }
        if (JS_SetModuleExport(ctx, m, e->name, val))
            return -1;
    }
    return 0;
}

/* (Constructor helpers moved to src/quickjs/builtin/builtin.c) */

/* Object class */

JSValue JS_ToObject(JSContext *ctx, JSValueConst val)
{
    int tag = JS_VALUE_GET_NORM_TAG(val);
    JSValue obj;

    switch(tag) {
    default:
    case JS_TAG_NULL:
    case JS_TAG_UNDEFINED:
        return JS_ThrowTypeError(ctx, "cannot convert to object");
    case JS_TAG_OBJECT:
    case JS_TAG_EXCEPTION:
        return JS_DupValue(ctx, val);
    case JS_TAG_SHORT_BIG_INT:
    case JS_TAG_BIG_INT:
        obj = JS_NewObjectClass(ctx, JS_CLASS_BIG_INT);
        goto set_value;
    case JS_TAG_INT:
    case JS_TAG_FLOAT64:
        obj = JS_NewObjectClass(ctx, JS_CLASS_NUMBER);
        goto set_value;
    case JS_TAG_STRING:
    case JS_TAG_STRING_ROPE:
        /* XXX: should call the string constructor */
        {
            JSValue str;
            str = JS_ToString(ctx, val); /* ensure that we never store a rope */
            if (JS_IsException(str))
                return JS_EXCEPTION;
            obj = JS_NewObjectClass(ctx, JS_CLASS_STRING);
            if (!JS_IsException(obj)) {
                JS_DefinePropertyValue(ctx, obj, JS_ATOM_length,
                                       JS_NewInt32(ctx, JS_VALUE_GET_STRING(str)->len), 0);
                JS_SetObjectData(ctx, obj, JS_DupValue(ctx, str));
            }
            JS_FreeValue(ctx, str);
            return obj;
        }
    case JS_TAG_BOOL:
        obj = JS_NewObjectClass(ctx, JS_CLASS_BOOLEAN);
        goto set_value;
    case JS_TAG_SYMBOL:
        obj = JS_NewObjectClass(ctx, JS_CLASS_SYMBOL);
    set_value:
        if (!JS_IsException(obj))
            JS_SetObjectData(ctx, obj, JS_DupValue(ctx, val));
        return obj;
    }
}

JSValue JS_ToObjectFree(JSContext *ctx, JSValue val)
{
    JSValue obj = JS_ToObject(ctx, val);
    JS_FreeValue(ctx, val);
    return obj;
}

int js_obj_to_desc(JSContext *ctx, JSPropertyDescriptor *d,
                          JSValueConst desc)
{
    JSValue val, getter, setter;
    int flags;

    if (!JS_IsObject(desc)) {
        JS_ThrowTypeErrorNotAnObject(ctx);
        return -1;
    }
    flags = 0;
    val = JS_UNDEFINED;
    getter = JS_UNDEFINED;
    setter = JS_UNDEFINED;
    if (JS_HasProperty(ctx, desc, JS_ATOM_enumerable)) {
        JSValue prop = JS_GetProperty(ctx, desc, JS_ATOM_enumerable);
        if (JS_IsException(prop))
            goto fail;
        flags |= JS_PROP_HAS_ENUMERABLE;
        if (JS_ToBoolFree(ctx, prop))
            flags |= JS_PROP_ENUMERABLE;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_configurable)) {
        JSValue prop = JS_GetProperty(ctx, desc, JS_ATOM_configurable);
        if (JS_IsException(prop))
            goto fail;
        flags |= JS_PROP_HAS_CONFIGURABLE;
        if (JS_ToBoolFree(ctx, prop))
            flags |= JS_PROP_CONFIGURABLE;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_value)) {
        flags |= JS_PROP_HAS_VALUE;
        val = JS_GetProperty(ctx, desc, JS_ATOM_value);
        if (JS_IsException(val))
            goto fail;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_writable)) {
        JSValue prop = JS_GetProperty(ctx, desc, JS_ATOM_writable);
        if (JS_IsException(prop))
            goto fail;
        flags |= JS_PROP_HAS_WRITABLE;
        if (JS_ToBoolFree(ctx, prop))
            flags |= JS_PROP_WRITABLE;
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_get)) {
        flags |= JS_PROP_HAS_GET;
        getter = JS_GetProperty(ctx, desc, JS_ATOM_get);
        if (JS_IsException(getter) ||
            !(JS_IsUndefined(getter) || JS_IsFunction(ctx, getter))) {
            JS_ThrowTypeError(ctx, "invalid getter");
            goto fail;
        }
    }
    if (JS_HasProperty(ctx, desc, JS_ATOM_set)) {
        flags |= JS_PROP_HAS_SET;
        setter = JS_GetProperty(ctx, desc, JS_ATOM_set);
        if (JS_IsException(setter) ||
            !(JS_IsUndefined(setter) || JS_IsFunction(ctx, setter))) {
            JS_ThrowTypeError(ctx, "invalid setter");
            goto fail;
        }
    }
    if ((flags & (JS_PROP_HAS_SET | JS_PROP_HAS_GET)) &&
        (flags & (JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE))) {
        JS_ThrowTypeError(ctx, "cannot have setter/getter and value or writable");
        goto fail;
    }
    d->flags = flags;
    d->value = val;
    d->getter = getter;
    d->setter = setter;
    return 0;
 fail:
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, getter);
    JS_FreeValue(ctx, setter);
    return -1;
}

/* (Object moved to src/quickjs/builtin/builtin_object.c) */


/* (Function moved to src/quickjs/builtin/builtin_function.c) */


/* (Error, Generator, BigInt, and Basic/Base object intrinsics moved to src/quickjs/builtin/builtin.c) */

