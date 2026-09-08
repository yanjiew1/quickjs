/*
 * QuickJS Javascript Engine - ECMAScript Module Runtime
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

#ifndef QUICKJS_JS_MODULE_H
#define QUICKJS_JS_MODULE_H

#include "quickjs.h"
#include "quickjs-internal.h"

JSModuleDef *js_new_module_def(JSContext *ctx, JSAtom name);
void js_mark_module_def(JSRuntime *rt, JSModuleDef *m,
                        JS_MarkFunc *mark_func);
void js_free_module_def(JSRuntime *rt, JSModuleDef *m);

int add_req_module_entry(JSContext *ctx, JSModuleDef *m, JSAtom module_name);
int add_star_export_entry(JSContext *ctx, JSModuleDef *m, int req_module_idx);
int js_resolve_module(JSContext *ctx, JSModuleDef *m);
int js_create_module_function(JSContext *ctx, JSModuleDef *m);
int js_link_module(JSContext *ctx, JSModuleDef *m);
JSValue js_evaluate_module(JSContext *ctx, JSModuleDef *m);

JSValue js_import_meta(JSContext *ctx);
JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier,
                          JSValueConst options);
JSValue js_module_ns_autoinit(JSContext *ctx, JSObject *p, JSAtom atom,
                              void *opaque);
extern const JSClassExoticMethods js_module_ns_exotic_methods;

#endif /* QUICKJS_JS_MODULE_H */
