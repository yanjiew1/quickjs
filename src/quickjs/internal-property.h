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

static inline JSShapeProperty *qjs_get_shape_prop(JSShape *shape)
{
    return (JSShapeProperty *)((uint32_t *)(shape + 1) +
                               shape->prop_hash_mask + 1);
}

static force_inline JSShapeProperty *qjs_find_own_property_fast(
    JSProperty **value, JSObject *obj, JSAtom atom)
{
    JSShape *shape = obj->shape;
    JSShapeProperty *prop = qjs_get_shape_prop(shape);
    intptr_t hash = (uintptr_t)atom & shape->prop_hash_mask;

    hash = shape->hash_table[hash];
    while (hash) {
        JSShapeProperty *shape_prop = &prop[hash - 1];
        if (likely(shape_prop->atom == atom)) {
            *value = &obj->prop[hash - 1];
            return shape_prop;
        }
        hash = shape_prop->hash_next;
    }
    *value = NULL;
    return NULL;
}

QJS_INTERNAL JSShapeProperty *qjs_find_own_property(JSProperty **value,
                                                    JSObject *obj,
                                                    JSAtom atom);
QJS_INTERNAL JSShapeProperty *qjs_find_own_property1(JSObject *obj,
                                                     JSAtom atom);
QJS_INTERNAL int qjs_get_own_property_names_internal(
    JSContext *ctx, JSPropertyEnum **properties, uint32_t *count,
    JSObject *obj, int flags);
QJS_INTERNAL JSProperty *qjs_add_property(JSContext *ctx, JSObject *obj,
                                          JSAtom atom, int flags);
QJS_INTERNAL int qjs_define_auto_init_property(JSContext *ctx,
                                               JSValueConst obj,
                                               JSAtom atom,
                                               JSAutoInitIDEnum id,
                                               void *opaque, int flags);
QJS_INTERNAL int qjs_update_property_flags(JSContext *ctx, JSObject *obj,
                                           JSShapeProperty **property,
                                           int flags);
QJS_INTERNAL JSValue qjs_object_proto_class_alloc(
    JSContext *ctx, JSValueConst proto, JSClassID class_id, int prop_count);
QJS_INTERNAL void qjs_set_cycle_flag(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL JSValue JS_GetPropertyValue(JSContext *ctx,
                                            JSValueConst obj,
                                            JSValue property);
QJS_INTERNAL JSValue qjs_get_property_int64(JSContext *ctx,
                                            JSValueConst obj,
                                            int64_t index);
QJS_INTERNAL JSValue qjs_throw_type_error_not_object(JSContext *ctx);
QJS_INTERNAL int qjs_define_property_value_int64(
    JSContext *ctx, JSValueConst obj, int64_t index, JSValue value, int flags);
QJS_INTERNAL int qjs_delete_property_int64(JSContext *ctx, JSValueConst obj,
                                           int64_t index, int flags);
QJS_INTERNAL int JS_SetPropertyValue(JSContext *ctx, JSValueConst obj,
                                        JSValue property, JSValue value,
                                        int flags);
QJS_INTERNAL int qjs_auto_init_property(JSContext *ctx, JSObject *obj,
                                        JSAtom atom, JSProperty *property,
                                        JSShapeProperty *shape_property);
QJS_INTERNAL int qjs_add_brand(JSContext *ctx, JSValueConst obj,
                               JSValueConst home_obj);
QJS_INTERNAL int qjs_check_brand(JSContext *ctx, JSValueConst obj,
                                 JSValueConst func);
QJS_INTERNAL int qjs_check_define_global_var(JSContext *ctx, JSAtom atom,
                                             int flags);
QJS_INTERNAL int qjs_get_global_var_ref(JSContext *ctx, JSAtom atom,
                                        JSValue *stack);
QJS_INTERNAL int qjs_delete_global_var(JSContext *ctx, JSAtom atom);
QJS_INTERNAL int qjs_define_object_name(JSContext *ctx, JSValueConst obj,
                                        JSAtom name, int flags);
QJS_INTERNAL int qjs_define_object_name_computed(
    JSContext *ctx, JSValueConst obj, JSValueConst name, int flags);
QJS_INTERNAL int qjs_define_private_field(JSContext *ctx, JSValueConst obj,
                                          JSValueConst name,
                                          JSValue value);
QJS_INTERNAL JSValue qjs_get_private_field(JSContext *ctx, JSValueConst obj,
                                           JSValueConst name);
QJS_INTERNAL int qjs_set_private_field(JSContext *ctx, JSValueConst obj,
                                       JSValueConst name, JSValue value);
QJS_INTERNAL int qjs_get_own_property_internal(
    JSContext *ctx, JSPropertyDescriptor *desc, JSObject *obj, JSAtom atom);
QJS_INTERNAL JSValue qjs_get_prototype_free(JSContext *ctx, JSValue obj);
QJS_INTERNAL int qjs_set_prototype_internal(JSContext *ctx,
                                            JSValueConst obj,
                                            JSValueConst proto,
                                            BOOL throw_flag);
QJS_INTERNAL JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *shape,
                                               JSClassID class_id,
                                               JSProperty *properties);
static inline JSShape *qjs_dup_shape(JSShape *shape)
{
    qjs_get_ref_header(shape)->ref_count++;
    return shape;
}
QJS_INTERNAL int qjs_convert_fast_array_to_array(JSContext *ctx,
                                                 JSObject *obj);
QJS_INTERNAL int qjs_delete_property(JSContext *ctx, JSObject *obj,
                                     JSAtom atom);
QJS_INTERNAL void qjs_free_property(JSRuntime *rt, JSProperty *property,
                                    int flags);
QJS_INTERNAL JSValue qjs_create_array_free(JSContext *ctx, int len,
                                           JSValue *values);
QJS_INTERNAL int qjs_define_property_value_value(
    JSContext *ctx, JSValueConst obj, JSValue property, JSValue value,
    int flags);
QJS_INTERNAL BOOL js_strict_eq2(JSContext *ctx, JSValueConst left,
                                   JSValueConst right, int mode);
#define QJS_EQ_STRICT 0
#define QJS_EQ_SAME_VALUE 1
#define QJS_EQ_SAME_VALUE_ZERO 2

static inline BOOL qjs_same_value(JSContext *ctx, JSValueConst left,
                                  JSValueConst right)
{
    return js_strict_eq2(ctx, left, right, QJS_EQ_SAME_VALUE);
}

static inline BOOL qjs_same_value_zero(JSContext *ctx,
                                       JSValueConst left,
                                       JSValueConst right)
{
    return js_strict_eq2(ctx, left, right, QJS_EQ_SAME_VALUE_ZERO);
}

/* Existing consumer bridges, owned here because they expose property logic. */
QJS_INTERNAL JSShape *qjs_regexp_new_shape2(JSContext *ctx, JSObject *proto,
                                            int hash_size, int prop_size);
QJS_INTERNAL int qjs_regexp_add_shape_property(JSContext *ctx,
                                               JSShape **shape,
                                               JSObject *obj, JSAtom atom,
                                               int prop_flags);
QJS_INTERNAL JSObject *qjs_regexp_get_proto_obj(JSValueConst proto);
QJS_INTERNAL int qjs_proxy_set_prototype_internal(JSContext *ctx,
                                                  JSValueConst obj,
                                                  JSValueConst proto,
                                                  BOOL throw_flag);
QJS_INTERNAL int qjs_proxy_get_own_property_internal(
    JSContext *ctx, JSPropertyDescriptor *desc, JSObject *obj, JSAtom atom);
QJS_INTERNAL void qjs_proxy_free_desc(JSContext *ctx,
                                      JSPropertyDescriptor *desc);
QJS_INTERNAL BOOL qjs_proxy_check_define_prop_flags(int prop_flags, int flags);
QJS_INTERNAL int qjs_proxy_obj_to_desc(JSContext *ctx,
                                       JSPropertyDescriptor *desc,
                                       JSValueConst value);

#endif /* QUICKJS_INTERNAL_PROPERTY_H */
