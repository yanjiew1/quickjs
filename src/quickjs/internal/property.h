/*
 * QuickJS internal property interfaces
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
#ifndef QUICKJS_PRIVATE_PROPERTY_H
#define QUICKJS_PRIVATE_PROPERTY_H

/* Internal implementation detail; not part of the public QuickJS API. */
__exception int js_get_length32(JSContext *ctx, uint32_t *pres,
                                       JSValueConst obj);

static inline JSShapeProperty *get_shape_prop(JSShape *sh)
{
    return (JSShapeProperty *)((uint32_t *)(sh + 1) + sh->prop_hash_mask + 1);
}

static force_inline JSShapeProperty *find_own_property(JSProperty **ppr,
                                                       JSObject *p,
                                                       JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *prop;
    intptr_t h;
    sh = p->shape;
    h = (uintptr_t)atom & sh->prop_hash_mask;
    h = sh->hash_table[h];
    prop = get_shape_prop(sh);
    while (h) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            *ppr = &p->prop[h - 1];
            /* the compiler should be able to assume that pr != NULL here */
            return pr;
        }
        h = pr->hash_next;
    }
    *ppr = NULL;
    return NULL;
}

static force_inline JSShapeProperty *find_own_property1(JSObject *p,
                                                        JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *prop;
    intptr_t h;
    sh = p->shape;
    h = (uintptr_t)atom & sh->prop_hash_mask;
    h = sh->hash_table[h];
    prop = get_shape_prop(sh);
    while (h) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            return pr;
        }
        h = pr->hash_next;
    }
    return NULL;
}

/* Internal implementation detail; not part of the public QuickJS API. */
int js_update_property_flags(JSContext *ctx, JSObject *p,
                                    JSShapeProperty **pprs, int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
JSProperty *add_property(JSContext *ctx,
                                JSObject *p, JSAtom prop, int prop_flags);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj,
                                     JSAtom prop, JSAutoInitIDEnum id,
                                     void *opaque, int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
int __exception JS_GetOwnPropertyNamesInternal(JSContext *ctx,
                                                      JSPropertyEnum **ptab,
                                                      uint32_t *plen,
                                                      JSObject *p, int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_GetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *sh, JSClassID class_id,
                                     JSProperty *props);

/* Internal implementation detail; not part of the public QuickJS API. */
int add_shape_property(JSContext *ctx, JSShape **psh,
                              JSObject *p, JSAtom atom, int prop_flags);

/* Internal implementation detail; not part of the public QuickJS API. */
int expand_fast_array(JSContext *ctx, JSObject *p, uint32_t new_len);

/* Internal implementation detail; not part of the public QuickJS API. */
JSObject *get_proto_obj(JSValueConst proto_val);

/* Internal implementation detail; not part of the public QuickJS API. */
JSShape *js_dup_shape(JSShape *sh);

/* Internal implementation detail; not part of the public QuickJS API. */
no_inline JSShape *js_new_shape2(JSContext *ctx, JSObject *proto,
                                        int hash_size, int prop_size);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_DefinePropertyValueInt64(JSContext *ctx, JSValueConst this_obj,
                                int64_t idx, JSValue val, int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                   int class_id);

/* Internal implementation detail; not part of the public QuickJS API. */
__exception int JS_CopyDataProperties(JSContext *ctx,
                                             JSValueConst target,
                                             JSValueConst source,
                                             JSValueConst excluded,
                                             BOOL setprop);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_CreateDataPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                       int64_t idx, JSValue val, int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj,
                                JSValue prop, JSValue val, int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                                     JSObject *p, JSAtom prop);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_GetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                                   JSValue prop);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_GetPrototypeFree(JSContext *ctx, JSValue obj);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_NewObjectProtoClassAlloc(JSContext *ctx, JSValueConst proto_val,
                                           JSClassID class_id, int n_alloc_props);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_OrdinaryIsInstanceOf(JSContext *ctx, JSValueConst val,
                                   JSValueConst obj);

/* Internal implementation detail; not part of the public QuickJS API. */
void JS_SetImmutablePrototype(JSContext *ctx, JSValueConst obj);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                               JSValue prop, JSValue val, int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj,
                                   JSValueConst proto_val,
                                   BOOL throw_flag);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_TryGetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, JSValue *pval);

static force_inline BOOL can_extend_fast_array(JSObject *p)
{
    JSObject *proto;
    if (!p->extensible)
        return FALSE;
    proto = p->shape->proto;
    if (!proto)
        return TRUE;
    return proto->is_std_array_prototype;
}

/* Internal implementation detail; not part of the public QuickJS API. */
BOOL check_define_prop_flags(int prop_flags, int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_allocate_fast_array(JSContext *ctx, int64_t len);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_create_array(JSContext *ctx, int len, JSValueConst *tab);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);

/* Internal implementation detail; not part of the public QuickJS API. */
BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj,
                              JSValue **arrpp, uint32_t *countp);

#endif /* QUICKJS_PRIVATE_PROPERTY_H */
