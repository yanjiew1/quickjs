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
#ifndef QUICKJS_INTERNAL_PROXY_H
#define QUICKJS_INTERNAL_PROXY_H

#include "internal-builtin.h"

QJS_INTERNAL int qjs_proxy_register_class(JSRuntime *rt,
                                           JSClassFinalizer *finalizer,
                                           JSClassGCMark *gc_mark,
                                           const JSClassExoticMethods *exotic,
                                           JSClassCall *call);
QJS_INTERNAL BOOL qjs_proxy_is_strict_mode(JSContext *ctx);
QJS_INTERNAL JSValue qjs_proxy_new_c_function3(
    JSContext *ctx, JSCFunction *func, const char *name, int length,
    JSCFunctionEnum cproto, int magic, JSValueConst proto, int prop_count);
QJS_INTERNAL JSValue qjs_proxy_throw_stack_overflow(JSContext *ctx);
QJS_INTERNAL JSValue qjs_proxy_throw_type_error_not_object(JSContext *ctx);
QJS_INTERNAL JSValue qjs_proxy_throw_type_error_not_constructor(
    JSContext *ctx, JSValueConst value);
QJS_INTERNAL int qjs_proxy_set_prototype_internal(JSContext *ctx,
                                                  JSValueConst obj,
                                                  JSValueConst proto,
                                                  BOOL throw_flag);
QJS_INTERNAL int qjs_proxy_get_own_property_internal(
    JSContext *ctx, JSPropertyDescriptor *desc, JSObject *obj, JSAtom atom);
QJS_INTERNAL JSValue qjs_proxy_create_array(JSContext *ctx, int len,
                                            JSValueConst *values);
QJS_INTERNAL void qjs_proxy_free_desc(JSContext *ctx,
                                      JSPropertyDescriptor *desc);
QJS_INTERNAL BOOL qjs_proxy_check_define_prop_flags(int prop_flags, int flags);
QJS_INTERNAL int qjs_proxy_to_bool_free(JSContext *ctx, JSValue value);
QJS_INTERNAL BOOL qjs_proxy_same_value(JSContext *ctx, JSValueConst left,
                                       JSValueConst right);
QJS_INTERNAL int qjs_proxy_obj_to_desc(JSContext *ctx,
                                       JSPropertyDescriptor *desc,
                                       JSValueConst value);

QJS_INTERNAL JSValue qjs_proxy_throw_revoked(JSContext *ctx);
QJS_INTERNAL int qjs_resolve_proxy(JSContext *ctx, JSValueConst *value,
                                   BOOL throw_exception);

#endif /* QUICKJS_INTERNAL_PROXY_H */
