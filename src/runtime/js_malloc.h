/*
 * QuickJS Javascript Engine: Memory Allocator Definitions
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

#ifndef QUICKJS_RUNTIME_JS_MALLOC_H
#define QUICKJS_RUNTIME_JS_MALLOC_H

#include "quickjs-internal.h"

/* Lifecycle and GC trigger */
void js_malloc_init(JSMallocContext *s);
void js_trigger_gc(JSRuntime *rt, size_t size);

/* Runtime Allocations */
void *js_malloc_rt(JSRuntime *rt, size_t size);
void js_free_rt(JSRuntime *rt, void *ptr);
void *js_realloc_rt(JSRuntime *rt, void *ptr, size_t size);
size_t js_malloc_usable_size_rt(JSRuntime *rt, const void *ptr);
void *js_mallocz_rt(JSRuntime *rt, size_t size);

/* Context Allocations */
void *js_malloc(JSContext *ctx, size_t size);
void *js_mallocz(JSContext *ctx, size_t size);
void js_free(JSContext *ctx, void *ptr);
void *js_realloc(JSContext *ctx, void *ptr, size_t size);
void *js_realloc2(JSContext *ctx, void *ptr, size_t size, size_t *pslack);
size_t js_malloc_usable_size(JSContext *ctx, const void *ptr);

/* Utilities */
char *js_strndup(JSContext *ctx, const char *s, size_t n);
char *js_strdup(JSContext *ctx, const char *str);
int js_realloc_array(JSContext *ctx, void **parray, int elem_size, int *psize, int req_size);
void js_dbuf_init(JSContext *ctx, DynBuf *s);
void *js_realloc_bytecode_rt(void *opaque, void *ptr, size_t size);

#endif /* QUICKJS_RUNTIME_JS_MALLOC_H */
