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
#ifndef QUICKJS_INTERNAL_PRIMITIVE_H
#define QUICKJS_INTERNAL_PRIMITIVE_H

#include "internal-global.h"
#include "internal-regexp.h"

#define JS_ToInt32Sat qjs_to_int32_sat
#define JS_ToInt32Clamp qjs_to_int32_clamp
#define JS_ToInt64Sat qjs_to_int64_sat
#define HINT_NUMBER 1

QJS_INTERNAL int qjs_add_intrinsic_number_boolean_string(JSContext *ctx);
QJS_INTERNAL int qjs_add_intrinsic_symbol(JSContext *ctx);
QJS_INTERNAL int qjs_add_intrinsic_bigint(JSContext *ctx);

QJS_INTERNAL int qjs_primitive_throw_not_configurable(JSContext *ctx,
                                                      int flags);
QJS_INTERNAL JSValue qjs_primitive_throw_not_constructor(
    JSContext *ctx, JSValueConst value);
QJS_INTERNAL uint32_t qjs_string_object_length(JSContext *ctx,
                                               JSValueConst value);
QJS_INTERNAL JSValue qjs_primitive_get_property_value(JSContext *ctx,
                                                      JSValueConst obj,
                                                      JSValue property);
QJS_INTERNAL JSValue qjs_primitive_get_property_int64(JSContext *ctx,
                                                      JSValueConst obj,
                                                      int64_t index);
QJS_INTERNAL BOOL qjs_primitive_check_define_flags(int prop_flags, int flags);
QJS_INTERNAL int qjs_primitive_create_data_property_uint32(
    JSContext *ctx, JSValueConst obj, uint32_t index, JSValue value,
    int flags);
QJS_INTERNAL JSValue qjs_primitive_to_primitive_free(JSContext *ctx,
                                                    JSValue value, int hint);
QJS_INTERNAL JSValue qjs_primitive_to_string_check_object(
    JSContext *ctx, JSValueConst value);
QJS_INTERNAL JSValue qjs_primitive_create_from_ctor(JSContext *ctx,
                                                    JSValueConst ctor,
                                                    JSClassID class_id);
QJS_INTERNAL JSValue qjs_primitive_invoke_free(JSContext *ctx, JSValue value,
                                               JSAtom atom, int argc,
                                               JSValueConst *argv);
QJS_INTERNAL JSValue qjs_primitive_to_object_free(JSContext *ctx,
                                                  JSValue value);
QJS_INTERNAL JSValue qjs_primitive_new_object_proto_list(
    JSContext *ctx, JSValueConst proto, const JSCFunctionListEntry *fields,
    int field_count);
QJS_INTERNAL JSValue qjs_primitive_new_c_constructor(
    JSContext *ctx, int class_id, const char *name, JSCFunction *func,
    int length, JSCFunctionEnum cproto, int magic, JSValueConst parent_ctor,
    const JSCFunctionListEntry *ctor_fields, int ctor_field_count,
    const JSCFunctionListEntry *proto_fields, int proto_field_count, int flags);
QJS_INTERNAL JSValue qjs_primitive_create_array_iterator(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
    int magic);

#endif /* QUICKJS_INTERNAL_PRIMITIVE_H */
