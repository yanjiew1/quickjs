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
#include "internal-array.h"

#define HINT_NUMBER 1

QJS_INTERNAL int qjs_add_intrinsic_number_boolean_string(JSContext *ctx);
QJS_INTERNAL int qjs_add_intrinsic_symbol(JSContext *ctx);
QJS_INTERNAL int JS_AddIntrinsicBigInt(JSContext *ctx);

QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAConstructor(
    JSContext *ctx, JSValueConst value);
QJS_INTERNAL uint32_t js_string_obj_get_length(JSContext *ctx,
                                               JSValueConst value);
QJS_INTERNAL JSValue JS_GetPropertyValue(JSContext *ctx,
                                                      JSValueConst obj,
                                                      JSValue property);
QJS_INTERNAL JSValue JS_GetPropertyInt64(JSContext *ctx,
                                                      JSValueConst obj,
                                                      int64_t index);
QJS_INTERNAL BOOL check_define_prop_flags(int prop_flags, int flags);
QJS_INTERNAL int JS_CreateDataPropertyUint32(
    JSContext *ctx, JSValueConst obj, uint32_t index, JSValue value,
    int flags);
QJS_INTERNAL JSValue JS_ToPrimitiveFree(JSContext *ctx,
                                                    JSValue value, int hint);
QJS_INTERNAL JSValue JS_ToStringCheckObject(
    JSContext *ctx, JSValueConst value);
QJS_INTERNAL JSValue js_create_from_ctor(JSContext *ctx,
                                                    JSValueConst ctor,
                                                    int class_id);
QJS_INTERNAL JSValue JS_InvokeFree(JSContext *ctx, JSValue value,
                                               JSAtom atom, int argc,
                                               JSValueConst *argv);
QJS_INTERNAL JSValue JS_ToObjectFree(JSContext *ctx,
                                                  JSValue value);
QJS_INTERNAL JSValue JS_NewObjectProtoList(
    JSContext *ctx, JSValueConst proto, const JSCFunctionListEntry *fields,
    int field_count);
QJS_INTERNAL JSValue JS_NewCConstructor(
    JSContext *ctx, int class_id, const char *name, JSCFunction *func,
    int length, JSCFunctionEnum cproto, int magic, JSValueConst parent_ctor,
    const JSCFunctionListEntry *ctor_fields, int ctor_field_count,
    const JSCFunctionListEntry *proto_fields, int proto_field_count, int flags);

#endif /* QUICKJS_INTERNAL_PRIMITIVE_H */
