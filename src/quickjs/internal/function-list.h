/*
 * QuickJS function-list interface
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
#ifndef QUICKJS_FUNCTION_LIST_H
#define QUICKJS_FUNCTION_LIST_H

#include "base.h"

JSValue JS_NewCConstructor(JSContext *ctx, int class_id, const char *name,
                           JSCFunction *func, int length, JSCFunctionEnum cproto, int magic,
                           JSValueConst parent_ctor,
                           const JSCFunctionListEntry *ctor_fields, int n_ctor_fields,
                           const JSCFunctionListEntry *proto_fields, int n_proto_fields,
                           int flags);
JSValue JS_InstantiateFunctionListItem2(JSContext *ctx, JSObject *p,
                                        JSAtom atom, void *opaque);

JSValue JS_NewObjectProtoList(JSContext *ctx, JSValueConst proto,
                              const JSCFunctionListEntry *fields, int n_fields);
int JS_SetConstructor2(JSContext *ctx,
                       JSValueConst func_obj,
                       JSValueConst proto,
                       int proto_flags, int ctor_flags);

#define JS_NEW_CTOR_NO_GLOBAL   (1 << 0) /* don't create a global binding */
#define JS_NEW_CTOR_READONLY    (1 << 3) /* read-only constructor field */

#define JS_NEW_CTOR_PROTO_CLASS (1 << 1) /* the prototype class is 'class_id' instead of JS_CLASS_OBJECT */

#define JS_NEW_CTOR_PROTO_EXIST (1 << 2) /* the prototype is already defined */

#endif
