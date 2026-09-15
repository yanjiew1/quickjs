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
#ifndef QUICKJS_INTERNAL_COLLECTION_H
#define QUICKJS_INTERNAL_COLLECTION_H

#include "internal-builtin.h"

QJS_INTERNAL int qjs_enqueue_job2(JSContext *ctx, JSJobFunc *job_func,
                                  int argc, JSValueConst *argv,
                                  BOOL no_exception);
QJS_INTERNAL void qjs_free_zero_refcount(JSRuntime *rt);
QJS_INTERNAL JSValue qjs_collection_throw_not_object(JSContext *ctx);
QJS_INTERNAL JSValue qjs_collection_create_array(JSContext *ctx, int len,
                                                 JSValueConst *values);
QJS_INTERNAL JSValue qjs_collection_create_from_ctor(JSContext *ctx,
                                                     JSValueConst ctor,
                                                     JSClassID class_id);

QJS_INTERNAL void qjs_map_finalizer(JSRuntime *rt, JSValue value);
QJS_INTERNAL void qjs_map_mark(JSRuntime *rt, JSValueConst value,
                               JS_MarkFunc *mark_func);
QJS_INTERNAL void qjs_map_iterator_finalizer(JSRuntime *rt, JSValue value);
QJS_INTERNAL void qjs_map_iterator_mark(JSRuntime *rt, JSValueConst value,
                                        JS_MarkFunc *mark_func);
QJS_INTERNAL void qjs_map_delete_weakrefs(JSRuntime *rt, JSWeakRefHeader *ref);
QJS_INTERNAL void qjs_weakref_delete(JSRuntime *rt, JSWeakRefHeader *ref);
QJS_INTERNAL void qjs_finrec_delete(JSRuntime *rt, JSWeakRefHeader *ref);
QJS_INTERNAL JSValue qjs_object_group_by(JSContext *ctx,
                                         JSValueConst this_val,
                                         int argc, JSValueConst *argv,
                                         int is_map);

#endif
