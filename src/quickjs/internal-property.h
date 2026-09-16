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

QJS_INTERNAL JSShapeProperty *get_shape_prop(JSShape *sh);

QJS_INTERNAL JSShapeProperty *find_own_property1(JSObject *p,
                                                        JSAtom atom);

QJS_INTERNAL JSShapeProperty *find_own_property(JSProperty **ppr,
                                                       JSObject *p,
                                                       JSAtom atom);
QJS_INTERNAL int __exception JS_GetOwnPropertyNamesInternal(JSContext *ctx,
                                                      JSPropertyEnum **ptab,
                                                      uint32_t *plen,
                                                      JSObject *p, int flags);
QJS_INTERNAL JSProperty *add_property(JSContext *ctx,
                                JSObject *p, JSAtom prop, int prop_flags);
QJS_INTERNAL int __attribute__((format(printf, 3, 4))) JS_ThrowTypeErrorOrFalse(JSContext *ctx, int flags, const char *fmt, ...);
QJS_INTERNAL int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj,
                                     JSAtom prop, JSAutoInitIDEnum id,
                                     void *opaque, int flags);
QJS_INTERNAL int js_update_property_flags(JSContext *ctx, JSObject *p,
                                    JSShapeProperty **pprs, int flags);
QJS_INTERNAL JSValue JS_NewObjectProtoClassAlloc(JSContext *ctx, JSValueConst proto_val,
                                           JSClassID class_id, int n_alloc_props);
QJS_INTERNAL void set_cycle_flag(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL JSValue JS_GetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                                   JSValue prop);
QJS_INTERNAL JSValue JS_GetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);
int JS_DefinePropertyValueInt64(JSContext *ctx, JSValueConst this_obj,
                                int64_t idx, JSValue val, int flags);
int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, int flags);
QJS_INTERNAL int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                               JSValue prop, JSValue val, int flags);
QJS_INTERNAL int JS_AutoInitProperty(JSContext *ctx, JSObject *p, JSAtom prop,
                               JSProperty *pr, JSShapeProperty *prs);
QJS_INTERNAL int JS_AddBrand(JSContext *ctx, JSValueConst obj, JSValueConst home_obj);
QJS_INTERNAL int JS_CheckBrand(JSContext *ctx, JSValueConst obj, JSValueConst func);
QJS_INTERNAL int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom prop, int flags);
QJS_INTERNAL int JS_GetGlobalVarRef(JSContext *ctx, JSAtom prop, JSValue *sp);
QJS_INTERNAL int JS_DeleteGlobalVar(JSContext *ctx, JSAtom prop);
QJS_INTERNAL int JS_DefineObjectName(JSContext *ctx, JSValueConst obj,
                               JSAtom name, int flags);
QJS_INTERNAL int JS_DefineObjectNameComputed(JSContext *ctx, JSValueConst obj,
                                       JSValueConst str, int flags);
QJS_INTERNAL int JS_DefinePrivateField(JSContext *ctx, JSValueConst obj,
                                 JSValueConst name, JSValue val);
QJS_INTERNAL JSValue JS_GetPrivateField(JSContext *ctx, JSValueConst obj,
                                  JSValueConst name);
QJS_INTERNAL int JS_SetPrivateField(JSContext *ctx, JSValueConst obj,
                              JSValueConst name, JSValue val);
QJS_INTERNAL int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                                     JSObject *p, JSAtom prop);
QJS_INTERNAL JSValue JS_GetPrototypeFree(JSContext *ctx, JSValue obj);
QJS_INTERNAL int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj,
                                   JSValueConst proto_val,
                                   BOOL throw_flag);
QJS_INTERNAL JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *sh, JSClassID class_id,
                                     JSProperty *props);
QJS_INTERNAL JSShape *js_dup_shape(JSShape *sh);
QJS_INTERNAL no_inline __exception int convert_fast_array_to_array(JSContext *ctx,
                                                             JSObject *p);
QJS_INTERNAL int delete_property(JSContext *ctx, JSObject *p, JSAtom atom);
QJS_INTERNAL void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
QJS_INTERNAL JSValue js_create_array_free(JSContext *ctx, int len, JSValue *tab);
int JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj,
                                JSValue prop, JSValue val, int flags);
QJS_INTERNAL BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2,
                          JSStrictEqModeEnum eq_mode);

QJS_INTERNAL BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2);

QJS_INTERNAL BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2);

/* Existing consumer bridges, owned here because they expose property logic. */
QJS_INTERNAL no_inline JSShape *js_new_shape2(JSContext *ctx, JSObject *proto,
                                        int hash_size, int prop_size);
QJS_INTERNAL int add_shape_property(JSContext *ctx, JSShape **psh,
                              JSObject *p, JSAtom atom, int prop_flags);
QJS_INTERNAL JSObject *get_proto_obj(JSValueConst proto_val);
QJS_INTERNAL int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj,
                                   JSValueConst proto_val,
                                   BOOL throw_flag);
QJS_INTERNAL int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                                     JSObject *p, JSAtom prop);
QJS_INTERNAL void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);
QJS_INTERNAL BOOL check_define_prop_flags(int prop_flags, int flags);
QJS_INTERNAL int js_obj_to_desc(JSContext *ctx, JSPropertyDescriptor *d,
                          JSValueConst desc);

#endif /* QUICKJS_INTERNAL_PROPERTY_H */
