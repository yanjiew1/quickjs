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
#ifndef QUICKJS_INTERNAL_PROPERTY_H
#define QUICKJS_INTERNAL_PROPERTY_H

#include "internal-object.h"

static inline JSShapeProperty *get_shape_prop(JSShape *shape)
{
    return (JSShapeProperty *)((uint32_t *)(shape + 1) +
                               shape->prop_hash_mask + 1);
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
QJS_INTERNAL int JS_GetOwnPropertyNamesInternal(
    JSContext *ctx, JSPropertyEnum **properties, uint32_t *count,
    JSObject *obj, int flags);
QJS_INTERNAL JSProperty *add_property(JSContext *ctx, JSObject *obj,
                                          JSAtom atom, int flags);
QJS_INTERNAL int __attribute__((format(printf, 3, 4)))
JS_ThrowTypeErrorOrFalse(JSContext *ctx, int flags, const char *fmt, ...);
QJS_INTERNAL int JS_DefineAutoInitProperty(JSContext *ctx,
                                               JSValueConst obj,
                                               JSAtom atom,
                                               JSAutoInitIDEnum id,
                                               void *opaque, int flags);
QJS_INTERNAL int js_update_property_flags(JSContext *ctx, JSObject *obj,
                                           JSShapeProperty **property,
                                           int flags);
QJS_INTERNAL JSValue JS_NewObjectProtoClassAlloc(
    JSContext *ctx, JSValueConst proto, JSClassID class_id, int prop_count);
QJS_INTERNAL void set_cycle_flag(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL JSValue JS_GetPropertyValue(JSContext *ctx,
                                            JSValueConst obj,
                                            JSValue property);
QJS_INTERNAL JSValue JS_GetPropertyInt64(JSContext *ctx,
                                            JSValueConst obj,
                                            int64_t index);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);
int JS_DefinePropertyValueInt64(
    JSContext *ctx, JSValueConst obj, int64_t index, JSValue value, int flags);
int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj,
                                           int64_t index, int flags);
QJS_INTERNAL int JS_SetPropertyValue(JSContext *ctx, JSValueConst obj,
                                        JSValue property, JSValue value,
                                        int flags);
QJS_INTERNAL int JS_AutoInitProperty(JSContext *ctx, JSObject *obj,
                                        JSAtom atom, JSProperty *property,
                                        JSShapeProperty *shape_property);
QJS_INTERNAL int JS_AddBrand(JSContext *ctx, JSValueConst obj,
                               JSValueConst home_obj);
QJS_INTERNAL int JS_CheckBrand(JSContext *ctx, JSValueConst obj,
                                 JSValueConst func);
QJS_INTERNAL int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom atom,
                                             int flags);
QJS_INTERNAL int JS_GetGlobalVarRef(JSContext *ctx, JSAtom atom,
                                        JSValue *stack);
QJS_INTERNAL int JS_DeleteGlobalVar(JSContext *ctx, JSAtom atom);
QJS_INTERNAL int JS_DefineObjectName(JSContext *ctx, JSValueConst obj,
                                        JSAtom name, int flags);
QJS_INTERNAL int JS_DefineObjectNameComputed(
    JSContext *ctx, JSValueConst obj, JSValueConst name, int flags);
QJS_INTERNAL int JS_DefinePrivateField(JSContext *ctx, JSValueConst obj,
                                          JSValueConst name,
                                          JSValue value);
QJS_INTERNAL JSValue JS_GetPrivateField(JSContext *ctx, JSValueConst obj,
                                           JSValueConst name);
QJS_INTERNAL int JS_SetPrivateField(JSContext *ctx, JSValueConst obj,
                                       JSValueConst name, JSValue value);
QJS_INTERNAL int JS_GetOwnPropertyInternal(
    JSContext *ctx, JSPropertyDescriptor *desc, JSObject *obj, JSAtom atom);
QJS_INTERNAL JSValue JS_GetPrototypeFree(JSContext *ctx, JSValue obj);
QJS_INTERNAL int JS_SetPrototypeInternal(JSContext *ctx,
                                            JSValueConst obj,
                                            JSValueConst proto,
                                            BOOL throw_flag);
QJS_INTERNAL JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *shape,
                                               JSClassID class_id,
                                               JSProperty *properties);
#ifndef QUICKJS_OBJECT_OWNER
static inline JSShape *js_dup_shape(JSShape *shape)
{
    js_rc(shape)->ref_count++;
    return shape;
}
#endif
QJS_INTERNAL int convert_fast_array_to_array(JSContext *ctx,
                                                 JSObject *obj);
QJS_INTERNAL int delete_property(JSContext *ctx, JSObject *obj,
                                     JSAtom atom);
QJS_INTERNAL void free_property(JSRuntime *rt, JSProperty *property,
                                    int flags);
QJS_INTERNAL JSValue js_create_array_free(JSContext *ctx, int len,
                                           JSValue *values);
int JS_DefinePropertyValueValue(
    JSContext *ctx, JSValueConst obj, JSValue property, JSValue value,
    int flags);
QJS_INTERNAL BOOL js_strict_eq2(JSContext *ctx, JSValueConst left,
                                   JSValueConst right, int mode);
#define QJS_EQ_STRICT 0
#define QJS_EQ_SAME_VALUE 1
#define QJS_EQ_SAME_VALUE_ZERO 2

#ifndef QUICKJS_NUMBER_OWNER
static inline BOOL js_same_value(JSContext *ctx, JSValueConst left,
                                  JSValueConst right)
{
    return js_strict_eq2(ctx, left, right, QJS_EQ_SAME_VALUE);
}

static inline BOOL js_same_value_zero(JSContext *ctx,
                                       JSValueConst left,
                                       JSValueConst right)
{
    return js_strict_eq2(ctx, left, right, QJS_EQ_SAME_VALUE_ZERO);
}
#endif

/* Existing consumer bridges, owned here because they expose property logic. */
QJS_INTERNAL JSShape *js_new_shape2(JSContext *ctx, JSObject *proto,
                                            int hash_size, int prop_size);
QJS_INTERNAL int add_shape_property(JSContext *ctx,
                                               JSShape **shape,
                                               JSObject *obj, JSAtom atom,
                                               int prop_flags);
QJS_INTERNAL JSObject *get_proto_obj(JSValueConst proto);
QJS_INTERNAL int JS_SetPrototypeInternal(JSContext *ctx,
                                                  JSValueConst obj,
                                                  JSValueConst proto,
                                                  BOOL throw_flag);
QJS_INTERNAL int JS_GetOwnPropertyInternal(
    JSContext *ctx, JSPropertyDescriptor *desc, JSObject *obj, JSAtom atom);
QJS_INTERNAL void js_free_desc(JSContext *ctx,
                                      JSPropertyDescriptor *desc);
QJS_INTERNAL BOOL check_define_prop_flags(int prop_flags, int flags);
QJS_INTERNAL int js_obj_to_desc(JSContext *ctx,
                                       JSPropertyDescriptor *desc,
                                       JSValueConst value);

#endif /* QUICKJS_INTERNAL_PROPERTY_H */
