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

#ifndef QUICKJS_JS_SHAPE_H
#define QUICKJS_JS_SHAPE_H

#include <stdio.h>
#include "quickjs.h"
#include "quickjs-internal.h"

int init_shape_hash(JSRuntime *rt);
uint32_t shape_hash(uint32_t h, uint32_t val);
uint32_t get_shape_hash(uint32_t h, int hash_bits);
uint32_t shape_initial_hash(JSObject *proto);
int resize_shape_hash(JSRuntime *rt, int new_shape_hash_bits);
void js_shape_hash_link(JSRuntime *rt, JSShape *sh);
void js_shape_hash_unlink(JSRuntime *rt, JSShape *sh);
JSShape *js_new_shape_nohash(JSContext *ctx, JSObject *proto,
                                   int hash_size, int prop_size);
JSShape *js_new_shape2(JSContext *ctx, JSObject *proto,
                       int hash_size, int prop_size);
JSShape *js_new_shape(JSContext *ctx, JSObject *proto);
JSShape *js_clone_shape(JSContext *ctx, JSShape *sh1);
void js_free_shape(JSRuntime *rt, JSShape *sh);
int resize_properties(JSContext *ctx, JSShape **psh,
                      JSObject *p, uint32_t count);
int compact_properties(JSContext *ctx, JSObject *p);
int add_shape_property(JSContext *ctx, JSShape **psh,
                       JSObject *p, JSAtom atom, int prop_flags);
JSShape *find_hashed_shape_proto(JSRuntime *rt, JSObject *proto);
JSShape *find_hashed_shape_prop(JSRuntime *rt, JSShape *sh,
                                JSAtom atom, int prop_flags);
void JS_DumpShapes(JSRuntime *rt);
int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                            JSShapeProperty **pprs);

#endif /* QUICKJS_JS_SHAPE_H */
