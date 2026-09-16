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

QJS_INTERNAL int JS_AddIntrinsicBigInt(JSContext *ctx);

QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx,
                                                JSValueConst func_obj);
QJS_INTERNAL uint32_t js_string_obj_get_length(JSContext *ctx,
                                         JSValueConst obj);
QJS_INTERNAL JSValue JS_GetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                                   JSValue prop);
QJS_INTERNAL JSValue JS_GetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx);
QJS_INTERNAL BOOL check_define_prop_flags(int prop_flags, int flags);
QJS_INTERNAL int JS_CreateDataPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                       int64_t idx, JSValue val, int flags);
QJS_INTERNAL JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint);
QJS_INTERNAL JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                   int class_id);
QJS_INTERNAL JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                             int argc, JSValueConst *argv);
QJS_INTERNAL JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_NewObjectProtoList(JSContext *ctx, JSValueConst proto,
                                     const JSCFunctionListEntry *fields, int n_fields);
QJS_INTERNAL JSValue JS_NewCConstructor(JSContext *ctx, int class_id, const char *name,
                                  JSCFunction *func, int length, JSCFunctionEnum cproto, int magic,
                                  JSValueConst parent_ctor,
                                  const JSCFunctionListEntry *ctor_fields, int n_ctor_fields,
                                  const JSCFunctionListEntry *proto_fields, int n_proto_fields,
                                  int flags);


/* Cross-TU declarations owned by this subsystem. */
extern QJS_INTERNAL const JSCFunctionListEntry js_number_funcs[14];

extern QJS_INTERNAL const JSCFunctionListEntry js_number_proto_funcs[6];

extern QJS_INTERNAL const JSCFunctionListEntry js_boolean_proto_funcs[2];

extern QJS_INTERNAL const JSClassExoticMethods js_string_exotic_methods;

extern QJS_INTERNAL const JSCFunctionListEntry js_string_funcs[3];

extern QJS_INTERNAL const JSCFunctionListEntry js_string_proto_funcs[50];

extern QJS_INTERNAL const JSCFunctionListEntry js_string_iterator_proto_funcs[2];

extern QJS_INTERNAL const JSCFunctionListEntry js_symbol_proto_funcs[5];

extern QJS_INTERNAL const JSCFunctionListEntry js_symbol_funcs[15];

QJS_INTERNAL JSValue js_boolean_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);

QJS_INTERNAL JSValue js_number_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);

QJS_INTERNAL JSValue js_string_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);

QJS_INTERNAL JSValue js_symbol_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);

#endif /* QUICKJS_INTERNAL_PRIMITIVE_H */
