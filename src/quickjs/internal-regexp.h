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

QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);
QJS_INTERNAL JSValue JS_ThrowTypeErrorInvalidClass(JSContext *ctx, int class_id);
QJS_INTERNAL void JS_ThrowInterrupted(JSContext *ctx);

QJS_INTERNAL JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                   int class_id);
QJS_INTERNAL JSValue JS_NewObjectProtoList(JSContext *ctx, JSValueConst proto,
                                     const JSCFunctionListEntry *fields, int n_fields);
QJS_INTERNAL JSValue JS_NewCConstructor(JSContext *ctx, int class_id, const char *name,
                                  JSCFunction *func, int length, JSCFunctionEnum cproto, int magic,
                                  JSValueConst parent_ctor,
                                  const JSCFunctionListEntry *ctor_fields, int n_ctor_fields,
                                  const JSCFunctionListEntry *proto_fields, int n_proto_fields,
                                  int flags);
QJS_INTERNAL JSValue JS_SpeciesConstructor(JSContext *ctx, JSValueConst obj,
                                     JSValueConst defaultConstructor);
QJS_INTERNAL JSValue js_get_this(JSContext *ctx,
                           JSValueConst this_val);
QJS_INTERNAL void js_regexp_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_regexp_string_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_regexp_string_iterator_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue JS_NewRegexp(JSContext *ctx, JSValue pattern, JSValue bc);
QJS_INTERNAL int js_is_regexp(JSContext *ctx, JSValueConst obj);

#endif /* QUICKJS_INTERNAL_REGEXP_H */
