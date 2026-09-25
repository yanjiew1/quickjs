/*
 * QuickJS weak reference support and WeakRef builtin
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
#include "../internal/string.h"
#include "../internal/object.h"
#include "../internal/function-list.h"
#include "weakref.h"

BOOL js_weakref_is_target(JSValueConst val)
{
    switch (JS_VALUE_GET_TAG(val)) {
    case JS_TAG_OBJECT:
        return TRUE;
    case JS_TAG_SYMBOL:
        {
            JSAtomStruct *p = JS_VALUE_GET_PTR(val);
            if (p->atom_type == JS_ATOM_TYPE_SYMBOL &&
                p->hash != JS_ATOM_HASH_PRIVATE)
                return TRUE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

/* JS_UNDEFINED is considered as a live weakref */
/* XXX: add a specific JSWeakRef value type ? */
BOOL js_weakref_is_live(JSValueConst val)
{
    void *p;
    if (JS_IsUndefined(val))
        return TRUE;
    p = JS_VALUE_GET_PTR(val);
    return (js_rc(p)->ref_count != 0);
}

/* 'val' can be JS_UNDEFINED */
void js_weakref_free(JSRuntime *rt, JSValue val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(val);
        assert(p->weakref_count >= 1);
        p->weakref_count--;
        /* 'mark' is tested to avoid freeing the object structure when
           it is about to be freed in a cycle or in
           free_zero_refcount() */
        if (p->weakref_count == 0 && js_rc(p)->ref_count == 0 &&
            js_rc(p)->mark == 0) {
            js_free_rt(rt, p);
        }
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_SYMBOL) {
        JSString *p = JS_VALUE_GET_STRING(val);
        assert(p->hash >= 1);
        p->hash--;
        if (p->hash == 0 && js_rc(p)->ref_count == 0) {
            /* can remove the dummy structure */
            js_free_rt(rt, p);
        }
    }
}

/* val must be an object, a symbol or undefined (see
   js_weakref_is_target). */
JSValue js_weakref_new(JSContext *ctx, JSValueConst val)
{
    if (JS_VALUE_GET_TAG(val) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(val);
        p->weakref_count++;
    } else if (JS_VALUE_GET_TAG(val) == JS_TAG_SYMBOL) {
        JSString *p = JS_VALUE_GET_STRING(val);
        /* XXX: could return an exception if too many references */
        assert(p->hash < JS_ATOM_HASH_MASK - 2);
        p->hash++;
    } else {
        assert(JS_IsUndefined(val));
    }
    return (JSValue)val;
}

/* WeakRef */

typedef struct JSWeakRefData {
    JSWeakRefHeader weakref_header;
    JSValue target;
} JSWeakRefData;

static void js_weakref_finalizer(JSRuntime *rt, JSValue val)
{
    JSWeakRefData *wrd = JS_GetOpaque(val, JS_CLASS_WEAK_REF);
    if (!wrd)
        return;
    js_weakref_free(rt, wrd->target);
    list_del(&wrd->weakref_header.link);
    js_free_rt(rt, wrd);
}

void weakref_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh)
{
    JSWeakRefData *wrd = container_of(wh, JSWeakRefData, weakref_header);

    if (!js_weakref_is_live(wrd->target)) {
        js_weakref_free(rt, wrd->target);
        wrd->target = JS_UNDEFINED;
    }
}

static JSValue js_weakref_constructor(JSContext *ctx, JSValueConst new_target,
                                      int argc, JSValueConst *argv)
{
    JSValueConst arg;
    JSValue obj;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "constructor requires 'new'");
    arg = argv[0];
    if (!js_weakref_is_target(arg))
        return JS_ThrowTypeError(ctx, "invalid target");
    obj = js_create_from_ctor(ctx, new_target, JS_CLASS_WEAK_REF);
    if (JS_IsException(obj))
        return JS_EXCEPTION;
    JSWeakRefData *wrd = js_mallocz(ctx, sizeof(*wrd));
    if (!wrd) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    wrd->target = js_weakref_new(ctx, arg);
    wrd->weakref_header.weakref_type = JS_WEAKREF_TYPE_WEAKREF;
    list_add_tail(&wrd->weakref_header.link, &ctx->rt->weakref_list);
    JS_SetOpaque(obj, wrd);
    return obj;
}

static JSValue js_weakref_deref(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSWeakRefData *wrd = JS_GetOpaque2(ctx, this_val, JS_CLASS_WEAK_REF);
    if (!wrd)
        return JS_EXCEPTION;
    if (js_weakref_is_live(wrd->target))
        return JS_DupValue(ctx, wrd->target);
    else
        return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_weakref_proto_funcs[] = {
    JS_CFUNC_DEF("deref", 0, js_weakref_deref ),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "WeakRef", JS_PROP_CONFIGURABLE ),
};

static const JSClassShortDef js_weakref_class_def[] = {
    { JS_ATOM_WeakRef, js_weakref_finalizer, NULL }, /* JS_CLASS_WEAK_REF */
};

int js_add_intrinsic_weakref(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    JSValue obj;

    /* WeakRef */
    if (!JS_IsRegisteredClass(rt, JS_CLASS_WEAK_REF)) {
        if (init_class_range(rt, js_weakref_class_def, JS_CLASS_WEAK_REF,
                             countof(js_weakref_class_def)))
            return -1;
    }
    obj = JS_NewCConstructor(ctx, JS_CLASS_WEAK_REF, "WeakRef",
                             js_weakref_constructor, 1, JS_CFUNC_constructor_or_func, 0,
                             JS_UNDEFINED,
                             NULL, 0,
                             js_weakref_proto_funcs, countof(js_weakref_proto_funcs),
                             0);
    if (JS_IsException(obj))
        return -1;
    JS_FreeValue(ctx, obj);

    return 0;
}
