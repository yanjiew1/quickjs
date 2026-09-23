/*
 * QuickJS Module Private Interface
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

#ifndef QJS_INTERNAL_MODULE_API_H
#define QJS_INTERNAL_MODULE_API_H

#include "internal/compiler-types.h"

QJS_INTERNAL JSValue js_evaluate_module(JSContext *ctx, JSModuleDef *m);

QJS_INTERNAL JSValue JS_NewModuleValue(JSContext *ctx, JSModuleDef *m);
QJS_INTERNAL JSExportEntry *add_export_entry(JSParseState *s, JSModuleDef *m,
                                       JSAtom local_name, JSAtom export_name,
                                       JSExportTypeEnum export_type);
QJS_INTERNAL int add_req_module_entry(JSContext *ctx, JSModuleDef *m,
                                JSAtom module_name);
QJS_INTERNAL int add_star_export_entry(JSContext *ctx, JSModuleDef *m,
                                 int req_module_idx);
QJS_INTERNAL int js_create_module_function(JSContext *ctx, JSModuleDef *m);
QJS_INTERNAL JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier, JSValueConst options);
QJS_INTERNAL void js_free_module_def(JSRuntime *rt, JSModuleDef *m);
QJS_INTERNAL JSValue js_import_meta(JSContext *ctx);
QJS_INTERNAL int js_inner_module_evaluation(JSContext *ctx, JSModuleDef *m,
                                      int index, JSModuleDef **pstack_top,
                                      JSValue *pvalue);
QJS_INTERNAL int js_link_module(JSContext *ctx, JSModuleDef *m);
QJS_INTERNAL void js_mark_module_def(JSRuntime *rt, JSModuleDef *m,
                               JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_module_ns_autoinit(JSContext *ctx, JSObject *p, JSAtom atom,
                                     void *opaque);
QJS_INTERNAL extern const JSClassExoticMethods js_module_ns_exotic_methods;
QJS_INTERNAL JSModuleDef *js_new_module_def(JSContext *ctx, JSAtom name);
QJS_INTERNAL int js_resolve_module(JSContext *ctx, JSModuleDef *m);

#endif /* QJS_INTERNAL_MODULE_API_H */
