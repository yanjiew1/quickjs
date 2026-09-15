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
QJS_INTERNAL JSValue qjs_get_property_value(JSContext *ctx,
                                            JSValueConst obj,
                                            JSValue property);
QJS_INTERNAL JSValue qjs_get_property_int64(JSContext *ctx,
                                            JSValueConst obj,
                                            int64_t index);
QJS_INTERNAL int qjs_set_property_value(JSContext *ctx, JSValueConst obj,
                                        JSValue property, JSValue value,
                                        int flags);
QJS_INTERNAL BOOL qjs_strict_equal(JSContext *ctx, JSValueConst left,
                                   JSValueConst right, int mode);
QJS_INTERNAL BOOL qjs_same_value(JSContext *ctx, JSValueConst left,
                                 JSValueConst right);
#define QJS_EQ_STRICT 0
#define QJS_EQ_SAME_VALUE 1
#define QJS_EQ_SAME_VALUE_ZERO 2

/* Existing consumer bridges, owned here because they expose property logic. */
QJS_INTERNAL JSShape *qjs_regexp_new_shape2(JSContext *ctx, JSObject *proto,
                                            int hash_size, int prop_size);
QJS_INTERNAL JSShape *qjs_regexp_dup_shape(JSShape *shape);
QJS_INTERNAL int qjs_regexp_add_shape_property(JSContext *ctx,
                                               JSShape **shape,
                                               JSObject *obj, JSAtom atom,
                                               int prop_flags);
QJS_INTERNAL JSValue qjs_regexp_new_object_from_shape(JSContext *ctx,
                                                      JSShape *shape,
                                                      JSClassID class_id,
                                                      JSProperty *props);
QJS_INTERNAL JSObject *qjs_regexp_get_proto_obj(JSValueConst proto);
QJS_INTERNAL JSValue qjs_regexp_get_property_int64(JSContext *ctx,
                                                   JSValueConst obj,
                                                   int64_t index);
QJS_INTERNAL int qjs_regexp_expand_fast_array(JSContext *ctx, JSObject *obj,
                                              uint32_t new_len);
QJS_INTERNAL int qjs_regexp_define_property_value_int64(
    JSContext *ctx, JSValueConst obj, int64_t index, JSValue value, int flags);
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
