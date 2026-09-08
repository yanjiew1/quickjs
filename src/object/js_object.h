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

#ifndef QUICKJS_JS_OBJECT_H
#define QUICKJS_JS_OBJECT_H

#include <stdio.h>
#include "quickjs.h"
#include "quickjs-internal.h"

JSProperty *add_property(JSContext *ctx, JSObject *p, JSAtom prop, int prop_flags);
void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
void set_cycle_flag(JSContext *ctx, JSValueConst obj);
void js_autoinit_free(JSRuntime *rt, JSProperty *pr);

void js_method_set_home_object(JSContext *ctx, JSValueConst func_obj, JSValueConst home_obj);
JSValue js_get_function_name(JSContext *ctx, JSAtom name);
int js_method_set_properties(JSContext *ctx, JSValueConst func_obj,
                             JSAtom name, int flags, JSValueConst home_obj);

JSValueConst JS_GetPrototypePrimitive(JSContext *ctx, JSValueConst val);
int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj,
                            JSValueConst proto_val, BOOL throw_flag);

#endif /* QUICKJS_JS_OBJECT_H */
