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
#ifndef QUICKJS_INTERNAL_SHAPE_H
#define QUICKJS_INTERNAL_SHAPE_H

#include "internal-types.h"

/* Cross-TU declarations owned by this subsystem. */
QJS_INTERNAL __maybe_unused void JS_DumpShapes(JSRuntime *rt);

QJS_INTERNAL int compact_properties(JSContext *ctx, JSObject *p);

QJS_INTERNAL JSShape *find_hashed_shape_prop(JSRuntime *rt, JSShape *sh,
                                       JSAtom atom, int prop_flags);

QJS_INTERNAL JSShape *find_hashed_shape_proto(JSRuntime *rt, JSObject *proto);

QJS_INTERNAL size_t get_shape_size(size_t hash_size, size_t prop_size);

QJS_INTERNAL int init_shape_hash(JSRuntime *rt);

QJS_INTERNAL JSShape *js_clone_shape(JSContext *ctx, JSShape *sh1);

QJS_INTERNAL void js_free_shape(JSRuntime *rt, JSShape *sh);

QJS_INTERNAL void js_free_shape_null(JSRuntime *rt, JSShape *sh);

QJS_INTERNAL JSShape *js_new_shape(JSContext *ctx, JSObject *proto);

QJS_INTERNAL JSShape *js_new_shape_nohash(JSContext *ctx, JSObject *proto,
                                           int hash_size, int prop_size);

QJS_INTERNAL void js_shape_hash_link(JSRuntime *rt, JSShape *sh);

QJS_INTERNAL int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                                   JSShapeProperty **pprs);

QJS_INTERNAL no_inline int resize_properties(JSContext *ctx, JSShape **psh,
                                       JSObject *p, uint32_t count);

#endif
