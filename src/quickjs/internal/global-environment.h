/*
 * QuickJS global binding interfaces
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
#ifndef QUICKJS_GLOBAL_ENVIRONMENT_H
#define QUICKJS_GLOBAL_ENVIRONMENT_H

#include "runtime.h"

typedef struct JSShapeProperty JSShapeProperty;
typedef struct JSProperty JSProperty;

typedef struct JSGlobalObject {
    JSValue uninitialized_vars; /* hidden object containing the list of uninitialized variables */
} JSGlobalObject;

#define DEFINE_GLOBAL_LEX_VAR (1 << 7)
#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)

int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom prop, int flags);
int JS_GetGlobalVarRef(JSContext *ctx, JSAtom prop, JSValue *sp);
int JS_DeleteGlobalVar(JSContext *ctx, JSAtom prop);
JSVarRef *js_global_object_get_uninitialized_var(JSContext *ctx, JSObject *p,
                                                 JSAtom atom);
JSVarRef *js_global_object_find_uninitialized_var(JSContext *ctx, JSObject *p,
                                                  JSAtom atom, BOOL is_lexical);
void js_global_object_finalizer(JSRuntime *rt, JSValue obj);
void js_global_object_mark(JSRuntime *rt, JSValueConst val,
                           JS_MarkFunc *mark_func);
int remove_global_object_property(JSContext *ctx, JSObject *p,
                                  JSShapeProperty *prs, JSProperty *pr);

#endif
