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

QJS_INTERNAL JSValueConst JS_GetActiveFunction(JSContext *ctx);
QJS_INTERNAL JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                   int class_id);
QJS_INTERNAL int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj,
                                   JSValueConst proto_val,
                                   BOOL throw_flag);
QJS_INTERNAL int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                                     JSObject *p, JSAtom prop);
QJS_INTERNAL int js_obj_to_desc(JSContext *ctx, JSPropertyDescriptor *d,
                          JSValueConst desc);
QJS_INTERNAL void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);
QJS_INTERNAL int JS_CreateDataPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                       int64_t idx, JSValue val, int flags);
int JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj,
                                JSValue prop, JSValue val, int flags);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx,
                                                JSValueConst func_obj);
QJS_INTERNAL JSValue JS_GetPrototypeFree(JSContext *ctx, JSValue obj);
QJS_INTERNAL void JS_SetImmutablePrototype(JSContext *ctx, JSValueConst obj);

QJS_INTERNAL JSValue js_function_apply(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue js_error_toString(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_aggregate_error_constructor(JSContext *ctx,
                                              JSValueConst errors);
QJS_INTERNAL JSValue JS_SpeciesConstructor(JSContext *ctx, JSValueConst obj,
                                     JSValueConst defaultConstructor);
QJS_INTERNAL JSValue js_function_constructor(JSContext *ctx, JSValueConst new_target,
                                       int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue *build_arg_list(JSContext *ctx, uint32_t *plen,
                               JSValueConst array_arg);
QJS_INTERNAL void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);

#endif /* QUICKJS_INTERNAL_BASE_H */
