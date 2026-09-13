/*
 * QuickJS Javascript Engine
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef QUICKJS_INTERNAL_BASE_H
#define QUICKJS_INTERNAL_BASE_H

#include "internal-array.h"
#include "internal-async.h"
#include "internal-global.h"
#include "internal-collection.h"

#define QJS_BACKTRACE_SKIP_FIRST_LEVEL (1 << 0)

QJS_INTERNAL JSValueConst qjs_base_get_active_function(JSContext *ctx);
QJS_INTERNAL JSValue qjs_base_create_from_ctor(JSContext *ctx,
                                               JSValueConst ctor,
                                               JSClassID class_id);
QJS_INTERNAL int qjs_base_set_prototype_internal(
    JSContext *ctx, JSValueConst obj, JSValueConst proto, BOOL throw_flag);
QJS_INTERNAL int qjs_base_get_own_property_internal(
    JSContext *ctx, JSPropertyDescriptor *desc, JSObject *obj, JSAtom atom);
QJS_INTERNAL int qjs_base_obj_to_desc(JSContext *ctx,
                                      JSPropertyDescriptor *desc,
                                      JSValueConst value);
QJS_INTERNAL void qjs_base_free_desc(JSContext *ctx,
                                     JSPropertyDescriptor *desc);
QJS_INTERNAL int qjs_base_create_data_property_uint32(
    JSContext *ctx, JSValueConst obj, int64_t index, JSValue value,
    int flags);
QJS_INTERNAL int qjs_base_define_property_value(
    JSContext *ctx, JSValueConst obj, JSValue property, JSValue value,
    int flags);
QJS_INTERNAL JSValue qjs_base_throw_not_constructor(
    JSContext *ctx, JSValueConst value);
QJS_INTERNAL JSValue qjs_base_get_prototype_free(JSContext *ctx,
                                                 JSValue object);
QJS_INTERNAL int qjs_base_poll_interrupts(JSContext *ctx);
QJS_INTERNAL JSClassID qjs_base_function_class_id(int function_kind);
QJS_INTERNAL void qjs_base_set_immutable_prototype(JSContext *ctx,
                                                   JSValueConst obj);

QJS_INTERNAL JSValue qjs_base_function_apply(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
    int magic);
QJS_INTERNAL JSValue qjs_base_error_to_string(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
QJS_INTERNAL JSValue qjs_base_aggregate_error(JSContext *ctx,
                                              JSValueConst errors);
QJS_INTERNAL JSValue qjs_base_species_constructor(
    JSContext *ctx, JSValueConst obj, JSValueConst default_ctor);
QJS_INTERNAL JSValue qjs_base_function_constructor(
    JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv,
    int magic);
QJS_INTERNAL JSValue *qjs_base_build_arg_list(
    JSContext *ctx, uint32_t *length, JSValueConst array);
QJS_INTERNAL void qjs_base_free_arg_list(JSContext *ctx, JSValue *args,
                                         uint32_t length);

#endif /* QUICKJS_INTERNAL_BASE_H */
