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
#ifndef QUICKJS_INTERNAL_REGEXP_H
#define QUICKJS_INTERNAL_REGEXP_H

#include "internal-frontend.h"

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
QJS_INTERNAL JSValue qjs_regexp_throw_type_error_not_object(JSContext *ctx);
QJS_INTERNAL JSValue qjs_regexp_throw_type_error_invalid_class(JSContext *ctx,
                                                               int class_id);
QJS_INTERNAL void qjs_regexp_throw_interrupted(JSContext *ctx);
QJS_INTERNAL JSValue qjs_regexp_get_property_int64(JSContext *ctx,
                                                   JSValueConst obj,
                                                   int64_t index);
QJS_INTERNAL int qjs_regexp_expand_fast_array(JSContext *ctx, JSObject *obj,
                                              uint32_t new_len);
QJS_INTERNAL int qjs_regexp_define_property_value_int64(
    JSContext *ctx, JSValueConst obj, int64_t index, JSValue value, int flags);
QJS_INTERNAL BOOL qjs_regexp_is_c_function(JSContext *ctx, JSValueConst value,
                                           JSCFunction *func, int magic);
QJS_INTERNAL int qjs_regexp_to_bool_free(JSContext *ctx, JSValue value);
QJS_INTERNAL int qjs_regexp_to_length_free(JSContext *ctx, int64_t *length,
                                           JSValue value);
QJS_INTERNAL JSValue qjs_regexp_to_string_free(JSContext *ctx, JSValue value);
QJS_INTERNAL BOOL qjs_regexp_same_value(JSContext *ctx, JSValueConst left,
                                        JSValueConst right);
QJS_INTERNAL JSValueConst qjs_regexp_get_active_function(JSContext *ctx);
QJS_INTERNAL JSValue qjs_regexp_create_from_ctor(JSContext *ctx,
                                                 JSValueConst ctor,
                                                 JSClassID class_id);
QJS_INTERNAL JSValue qjs_regexp_new_object_proto_list(
    JSContext *ctx, JSValueConst proto, const JSCFunctionListEntry *fields,
    int field_count);
QJS_INTERNAL JSValue qjs_regexp_new_c_constructor(
    JSContext *ctx, int class_id, const char *name, JSCFunction *func,
    int length, JSCFunctionEnum cproto, int magic, JSValueConst parent_ctor,
    const JSCFunctionListEntry *ctor_fields, int ctor_field_count,
    const JSCFunctionListEntry *proto_fields, int proto_field_count, int flags);
QJS_INTERNAL JSValue qjs_regexp_species_constructor(JSContext *ctx,
                                                    JSValueConst obj,
                                                    JSValueConst default_ctor);
QJS_INTERNAL JSValue qjs_regexp_function_apply(JSContext *ctx,
                                               JSValueConst this_val,
                                               int argc,
                                               JSValueConst *argv, int magic);
QJS_INTERNAL JSValue qjs_regexp_get_this(JSContext *ctx,
                                         JSValueConst this_val);
QJS_INTERNAL int qjs_regexp_get_length64(JSContext *ctx, int64_t *length,
                                         JSValueConst obj);
QJS_INTERNAL void qjs_regexp_finalizer(JSRuntime *rt, JSValue value);
QJS_INTERNAL void qjs_regexp_string_iterator_finalizer(JSRuntime *rt,
                                                       JSValue value);
QJS_INTERNAL void qjs_regexp_string_iterator_mark(JSRuntime *rt,
                                                  JSValueConst value,
                                                  JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue qjs_new_regexp(JSContext *ctx, JSValue pattern,
                                    JSValue bytecode);
QJS_INTERNAL int qjs_is_regexp(JSContext *ctx, JSValueConst value);

#endif /* QUICKJS_INTERNAL_REGEXP_H */
