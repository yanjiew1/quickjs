/*
 * QuickJS Compiler Private Interface
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

#ifndef QJS_INTERNAL_COMPILER_API_H
#define QJS_INTERNAL_COMPILER_API_H

#include "internal/object.h"
#include "internal/vm.h"
#include "internal/compiler-types.h"

QJS_INTERNAL JSValue JS_EvalObject(JSContext *ctx, JSValueConst this_obj,
                             JSValueConst val, int flags, int scope_idx);
QJS_INTERNAL JSValue JS_NewModuleValue(JSContext *ctx, JSModuleDef *m);
QJS_INTERNAL JSValue __JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                                 const char *input, size_t input_len,
                                 const char *filename, int flags, int scope_idx);
QJS_INTERNAL void free_function_bytecode(JSRuntime *rt, JSFunctionBytecode *b);
QJS_INTERNAL void free_token(JSParseState *s, JSToken *token);
QJS_INTERNAL JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier, JSValueConst options);
QJS_INTERNAL void js_free_module_def(JSRuntime *rt, JSModuleDef *m);
QJS_INTERNAL JSValue js_import_meta(JSContext *ctx);
QJS_INTERNAL void js_mark_module_def(JSRuntime *rt, JSModuleDef *m,
                               JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_module_ns_autoinit(JSContext *ctx, JSObject *p, JSAtom atom,
                                     void *opaque);
QJS_INTERNAL extern const JSClassExoticMethods js_module_ns_exotic_methods;
QJS_INTERNAL JSModuleDef *js_new_module_def(JSContext *ctx, JSAtom name);
QJS_INTERNAL __attribute__((format(printf, 2, 3))) int js_parse_error(JSParseState *s, const char *fmt, ...);
QJS_INTERNAL void js_parse_init(JSContext *ctx, JSParseState *s,
                          const char *input, size_t input_len,
                          const char *filename);
QJS_INTERNAL __exception int json_next_token(JSParseState *s);
QJS_INTERNAL extern const JSOpCode opcode_info[263];



#endif /* QJS_INTERNAL_COMPILER_API_H */
