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
#ifndef QUICKJS_INTERNAL_DATE_H
#define QUICKJS_INTERNAL_DATE_H

#include "internal-builtin.h"

QJS_INTERNAL JSValue get_date_string(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue js_new_string8(JSContext *ctx, const char *buf);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);
QJS_INTERNAL JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);
QJS_INTERNAL JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                   int class_id);
QJS_INTERNAL JSValue JS_NewCConstructor(JSContext *ctx, int class_id, const char *name,
                                  JSCFunction *func, int length, JSCFunctionEnum cproto, int magic,
                                  JSValueConst parent_ctor,
                                  const JSCFunctionListEntry *ctor_fields, int n_ctor_fields,
                                  const JSCFunctionListEntry *proto_fields, int n_proto_fields,
                                  int flags);

#endif /* QUICKJS_INTERNAL_DATE_H */
