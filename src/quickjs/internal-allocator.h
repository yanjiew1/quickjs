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
#ifndef QUICKJS_INTERNAL_ALLOCATOR_H
#define QUICKJS_INTERNAL_ALLOCATOR_H

#include "internal-types.h"

QJS_INTERNAL void js_malloc_init(JSMallocContext *ctx);
QJS_INTERNAL extern const JSMallocFunctions def_malloc_funcs;
QJS_INTERNAL void *__js_malloc(JSMallocContext *ctx, size_t size);
QJS_INTERNAL void __js_free(JSMallocContext *ctx, void *ptr);
QJS_INTERNAL void *__js_realloc(JSMallocContext *ctx, void *ptr,
                                   size_t size);

static inline void *js_malloc_rt_inline(JSRuntime *rt, size_t size);
static inline void js_free_rt_inline(JSRuntime *rt, void *ptr);
static inline void *js_realloc_rt_inline(JSRuntime *rt, void *ptr,
                                         size_t size);
static inline void *js_malloc_inline(JSContext *ctx, size_t size);
static inline void js_free_inline(JSContext *ctx, void *ptr);
static inline void *js_realloc_inline(JSContext *ctx, void *ptr, size_t size);
static inline void *js_realloc2_inline(JSContext *ctx, void *ptr, size_t size,
                                       size_t *slack);

#define js_malloc_rt(rt, size) js_malloc_rt_inline((rt), (size))
#define js_free_rt(rt, ptr) js_free_rt_inline((rt), (ptr))
#define js_realloc_rt(rt, ptr, size) js_realloc_rt_inline((rt), (ptr), (size))
#define js_malloc(ctx, size) js_malloc_inline((ctx), (size))
#define js_free(ctx, ptr) js_free_inline((ctx), (ptr))
#define js_realloc(ctx, ptr, size) js_realloc_inline((ctx), (ptr), (size))
#define js_realloc2(ctx, ptr, size, slack) \
    js_realloc2_inline((ctx), (ptr), (size), (slack))

static inline void *js_malloc_rt_inline(JSRuntime *rt, size_t size)
{
    return __js_malloc(&rt->malloc_ctx, size);
}

static inline void js_free_rt_inline(JSRuntime *rt, void *ptr)
{
    __js_free(&rt->malloc_ctx, ptr);
}

static inline void *js_realloc_rt_inline(JSRuntime *rt, void *ptr,
                                            size_t size)
{
    return __js_realloc(&rt->malloc_ctx, ptr, size);
}

static inline void *js_malloc_inline(JSContext *ctx, size_t size)
{
    void *ptr = __js_malloc(&ctx->rt->malloc_ctx, size);

    if (unlikely(!ptr)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return ptr;
}

static inline void js_free_inline(JSContext *ctx, void *ptr)
{
    __js_free(&ctx->rt->malloc_ctx, ptr);
}

static inline void *js_realloc_inline(JSContext *ctx, void *ptr,
                                         size_t size)
{
    void *ret = __js_realloc(&ctx->rt->malloc_ctx, ptr, size);

    if (unlikely(!ret && size != 0)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return ret;
}

/* js_realloc2 was inlined into the monolithic array-growth path.  Preserve
   that wrapper shape without duplicating the allocator arena algorithm. */
static inline void *js_realloc2_inline(JSContext *ctx, void *ptr,
                                          size_t size, size_t *slack)
{
    void *ret = __js_realloc(&ctx->rt->malloc_ctx, ptr, size);

    if (unlikely(!ret && size != 0)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    if (slack) {
        size_t new_size = js_malloc_usable_size_rt(ctx->rt, ret);
        *slack = new_size > size ? new_size - size : 0;
    }
    return ret;
}

#endif /* QUICKJS_INTERNAL_ALLOCATOR_H */
