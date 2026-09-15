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
#ifndef QUICKJS_INTERNAL_BUILTIN_H
#define QUICKJS_INTERNAL_BUILTIN_H

#include "internal-frontend.h"

#define JS_NEW_CTOR_NO_GLOBAL   (1 << 0)
#define JS_NEW_CTOR_PROTO_CLASS (1 << 1)
#define JS_NEW_CTOR_PROTO_EXIST (1 << 2)
#define JS_NEW_CTOR_READONLY    (1 << 3)

typedef struct JSClassShortDef {
    JSAtom class_name;
    JSClassFinalizer *finalizer;
    JSClassGCMark *gc_mark;
} JSClassShortDef;

QJS_INTERNAL int JS_AddIntrinsicBasicObjects(JSContext *ctx);
QJS_INTERNAL int qjs_add_intrinsic_math(JSContext *ctx);
QJS_INTERNAL int qjs_add_intrinsics(JSContext *ctx);
QJS_INTERNAL int qjs_add_intrinsic_number_boolean_string(JSContext *ctx);
QJS_INTERNAL int qjs_add_intrinsic_symbol(JSContext *ctx);
QJS_INTERNAL int JS_AddIntrinsicBigInt(JSContext *ctx);
QJS_INTERNAL void qjs_primitive_init_classes(JSRuntime *rt);

QJS_INTERNAL double js_pow(double left, double right);
QJS_INTERNAL int check_function(JSContext *ctx, JSValueConst value);
QJS_INTERNAL int check_exception_free(JSContext *ctx, JSValue value);
QJS_INTERNAL JSValue JS_NewObjectProtoList(
    JSContext *ctx, JSValueConst proto, const JSCFunctionListEntry *fields,
    int field_count);
QJS_INTERNAL JSValue JS_InstantiateFunctionListItem2(
    JSContext *ctx, JSObject *obj, JSAtom atom, void *opaque);
QJS_INTERNAL int JS_SetConstructor2(JSContext *ctx,
                                      JSValueConst constructor,
                                      JSValueConst prototype,
                                      int prototype_flags,
                                      int constructor_flags);
QJS_INTERNAL JSValue JS_NewCConstructor(
    JSContext *ctx, int class_id, const char *name, JSCFunction *func,
    int length, JSCFunctionEnum cproto, int magic, JSValueConst parent_ctor,
    const JSCFunctionListEntry *ctor_fields, int ctor_field_count,
    const JSCFunctionListEntry *proto_fields, int proto_field_count, int flags);
QJS_INTERNAL JSValue JS_NewCFunction3(
    JSContext *ctx, JSCFunction *func, const char *name, int length,
    JSCFunctionEnum cproto, int magic, JSValueConst proto, int prop_count);
QJS_INTERNAL int init_class_range(JSRuntime *rt,
                                      const JSClassShortDef *classes,
                                      int first_class, int class_count);

#endif /* QUICKJS_INTERNAL_BUILTIN_H */
