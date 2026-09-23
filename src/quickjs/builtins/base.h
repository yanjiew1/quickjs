/*
 * QuickJS Builtins Private Interface
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

#ifndef QJS_INTERNAL_BUILTINS_API_H
#define QJS_INTERNAL_BUILTINS_API_H

#include "internal/object.h"
#include "internal/vm.h"
#include "builtins/math.h"

QJS_INTERNAL int JS_AddIntrinsicBasicObjects(JSContext *ctx);
QJS_INTERNAL JSValue JS_CreateAsyncFromSyncIterator(JSContext *ctx,
                                              JSValueConst sync_iter);
QJS_INTERNAL JSValue JS_InstantiateFunctionListItem2(JSContext *ctx, JSObject *p,
                                               JSAtom atom, void *opaque);
QJS_INTERNAL JSValue JS_NewRegexp(JSContext *ctx, JSValue pattern, JSValue bc);
QJS_INTERNAL JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);
QJS_INTERNAL JSValue JS_ThrowTypeErrorRevokedProxy(JSContext *ctx);
QJS_INTERNAL JSValue JS_ToObject(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);
QJS_INTERNAL BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf);
QJS_INTERNAL JSValue *build_arg_list(JSContext *ctx, uint32_t *plen,
                               JSValueConst array_arg);
QJS_INTERNAL void finrec_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
QJS_INTERNAL void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);
QJS_INTERNAL JSValue get_date_string(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue js_array_buffer_constructor3(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, uint64_t *max_len,
                                            JSClassID class_id,
                                            uint8_t *buf,
                                            JSFreeArrayBufferDataFunc *free_func,
                                            void *opaque, BOOL alloc_flag);
QJS_INTERNAL void js_array_buffer_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);
QJS_INTERNAL void js_array_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_array_iterator_mark(JSRuntime *rt, JSValueConst val,
                                   JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_array_iterator_next(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv,
                                      BOOL *pdone, int magic);
QJS_INTERNAL JSValue js_create_array_iterator(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue js_function_apply(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL __exception int js_get_length32(JSContext *ctx, uint32_t *pres,
                                       JSValueConst obj);
QJS_INTERNAL void js_iterator_concat_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_concat_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
QJS_INTERNAL void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_helper_mark(JSRuntime *rt, JSValueConst val,
                                   JS_MarkFunc *mark_func);
QJS_INTERNAL void js_iterator_wrap_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_wrap_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func);
QJS_INTERNAL void js_map_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_map_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_map_iterator_mark(JSRuntime *rt, JSValueConst val,
                                 JS_MarkFunc *mark_func);
QJS_INTERNAL void js_map_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_promise_resolve(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue js_promise_then(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv);
QJS_INTERNAL void js_regexp_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_regexp_string_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_regexp_string_iterator_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func);
QJS_INTERNAL int js_resolve_proxy(JSContext *ctx, JSValueConst *pval, BOOL throw_exception);
QJS_INTERNAL extern const JSClassExoticMethods js_string_exotic_methods;
QJS_INTERNAL int js_string_find_invalid_codepoint(JSString *p);
QJS_INTERNAL JSValue js_typed_array_constructor(JSContext *ctx,
                                          JSValueConst new_target,
                                          int argc, JSValueConst *argv,
                                          int classid);
QJS_INTERNAL void js_typed_array_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_typed_array_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL void map_delete_weakrefs(JSRuntime *rt, JSWeakRefHeader *wh);
QJS_INTERNAL __exception int perform_promise_then(JSContext *ctx,
                                            JSValueConst promise,
                                            JSValueConst *resolve_reject,
                                            JSValueConst *cap_resolving_funcs);
QJS_INTERNAL void weakref_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);



#endif /* QJS_INTERNAL_BUILTINS_API_H */
