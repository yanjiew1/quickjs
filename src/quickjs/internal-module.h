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
#ifndef QUICKJS_INTERNAL_MODULE_H
#define QUICKJS_INTERNAL_MODULE_H

#include "internal-iterator.h"

QJS_INTERNAL JSModuleDef *qjs_new_module_def(JSContext *ctx, JSAtom name);
QJS_INTERNAL JSValue qjs_new_module_value(JSContext *ctx, JSModuleDef *module);
QJS_INTERNAL void qjs_module_free_all(JSContext *ctx);
QJS_INTERNAL void qjs_free_module_def(JSRuntime *rt, JSModuleDef *module);
QJS_INTERNAL void qjs_mark_module_def(JSRuntime *rt, JSModuleDef *module,
                                      JS_MarkFunc *mark_func);
QJS_INTERNAL void qjs_module_init_class(JSRuntime *rt);
QJS_INTERNAL JSValue qjs_module_ns_autoinit(JSContext *ctx, JSObject *obj,
                                            JSAtom atom, void *opaque);
QJS_INTERNAL int qjs_module_add_request(JSContext *ctx, JSModuleDef *module,
                                        JSAtom module_name);
QJS_INTERNAL JSExportEntry *qjs_module_find_export(JSModuleDef *module,
                                                   JSAtom export_name);
QJS_INTERNAL JSExportEntry *qjs_module_add_export_unchecked(
    JSContext *ctx, JSModuleDef *module, JSAtom local_name,
    JSAtom export_name, JSExportTypeEnum export_type);
QJS_INTERNAL int qjs_module_add_star_export(JSContext *ctx,
                                            JSModuleDef *module,
                                            int req_module_idx);
QJS_INTERNAL int qjs_resolve_module(JSContext *ctx, JSModuleDef *module);
QJS_INTERNAL JSValue qjs_module_link_and_evaluate(JSContext *ctx,
                                                  JSModuleDef *module);
QJS_INTERNAL JSValue qjs_import_meta(JSContext *ctx);
QJS_INTERNAL JSValue qjs_dynamic_import(JSContext *ctx,
                                        JSValueConst specifier,
                                        JSValueConst options);

#endif /* QUICKJS_INTERNAL_MODULE_H */
