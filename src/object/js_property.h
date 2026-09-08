/*
 * QuickJS Javascript Engine - Property Access, Descriptors & Operators
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

#ifndef QUICKJS_JS_PROPERTY_H
#define QUICKJS_JS_PROPERTY_H

#include "quickjs.h"
#include "quickjs-internal.h"

/* Property access */
int JS_AutoInitProperty(JSContext *ctx, JSObject *p, JSAtom prop,
                        JSProperty *pr, JSShapeProperty *prs);
int JS_DefinePrivateField(JSContext *ctx, JSValueConst obj,
                          JSValueConst name, JSValue val);
JSValue JS_GetPrivateField(JSContext *ctx, JSValueConst obj,
                           JSValueConst name);
int JS_SetPrivateField(JSContext *ctx, JSValueConst obj,
                       JSValueConst name, JSValue val);
int JS_AddBrand(JSContext *ctx, JSValueConst obj, JSValueConst home_obj);
int JS_CheckBrand(JSContext *ctx, JSValueConst obj, JSValueConst func);

int convert_fast_array_to_array(JSContext *ctx, JSObject *p);
int delete_property(JSContext *ctx, JSObject *p, JSAtom atom);
JSValue js_create_array_free(JSContext *ctx, int len, JSValue *tab);

int JS_CreateProperty(JSContext *ctx, JSObject *p,
                      JSAtom prop, JSValueConst val,
                      JSValueConst getter, JSValueConst setter,
                      int flags);
int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj,
                              JSAtom prop, JSAutoInitIDEnum id,
                              void *opaque, int flags);

/* Object naming */
int JS_DefineObjectName(JSContext *ctx, JSValueConst obj,
                        JSAtom name, int flags);
int JS_DefineObjectNameComputed(JSContext *ctx, JSValueConst obj,
                                JSValueConst str, int flags);

/* Global variable support */
#define DEFINE_GLOBAL_LEX_VAR (1 << 7)
#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)

JSValue JS_ThrowSyntaxErrorVarRedeclaration(JSContext *ctx, JSAtom prop);
int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom prop, int flags);
int JS_GetGlobalVarRef(JSContext *ctx, JSAtom prop, JSValue *sp);
int JS_DeleteGlobalVar(JSContext *ctx, JSAtom prop);

/* Operators */
__exception int js_operator_in(JSContext *ctx, JSValue *sp);
__exception int js_operator_private_in(JSContext *ctx, JSValue *sp);
__exception int js_has_unscopable(JSContext *ctx, JSValueConst obj, JSAtom atom);
__exception int js_operator_instanceof(JSContext *ctx, JSValue *sp);
__exception int js_operator_typeof(JSContext *ctx, JSValueConst op1);
__exception int js_operator_delete(JSContext *ctx, JSValue *sp);

#endif /* QUICKJS_JS_PROPERTY_H */
