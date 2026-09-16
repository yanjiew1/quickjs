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
#ifndef QUICKJS_INTERNAL_ITERATOR_H
#define QUICKJS_INTERNAL_ITERATOR_H

#include "internal-function.h"

QJS_INTERNAL JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj,
                               JSValueConst method);
QJS_INTERNAL JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async);
QJS_INTERNAL JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj,
                                JSValueConst method,
                                int argc, JSValueConst *argv, int *pdone);
/* Note: always return JS_UNDEFINED when *pdone = TRUE. */
QJS_INTERNAL JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
                               JSValueConst method,
                               int argc, JSValueConst *argv, BOOL *pdone);
QJS_INTERNAL int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj,
                            BOOL is_exception_pending);
QJS_INTERNAL JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj,
                                           BOOL *pdone);
QJS_INTERNAL JSValue js_create_iterator_result(JSContext *ctx,
                                         JSValue val,
                                         BOOL done);
QJS_INTERNAL JSValue js_iterator_proto_iterator(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv);

QJS_INTERNAL BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj,
                              JSValue **arrpp, uint32_t *countp);
QJS_INTERNAL __exception int JS_CopyDataProperties(JSContext *ctx,
                                             JSValueConst target,
                                             JSValueConst source,
                                             JSValueConst excluded,
                                             BOOL setprop);


/* Cross-TU declarations owned by this subsystem. */
QJS_INTERNAL JSValue build_for_in_iterator(JSContext *ctx, JSValue obj);

QJS_INTERNAL void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val);

QJS_INTERNAL void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);

QJS_INTERNAL __exception int js_for_in_prepare_prototype_chain_enum(JSContext *ctx,
                                                              JSValueConst enum_obj);

QJS_INTERNAL __exception int js_iterator_get_value_done(JSContext *ctx, JSValue *sp);

#endif /* QUICKJS_INTERNAL_ITERATOR_H */
