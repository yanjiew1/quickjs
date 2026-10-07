/*
 * QuickJS memory allocator
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
#ifndef QUICKJS_ALLOCATOR_INLINES_H
#define QUICKJS_ALLOCATOR_INLINES_H

#include "runtime.h"

#ifdef JS_ALLOCATOR_FORCE_INLINE
#define JS_ALLOCATOR_INLINE force_inline
#else
#define JS_ALLOCATOR_INLINE inline
#endif

#define JS_MALLOC_ARENA_SIZE 4096
#define JS_MALLOC_MIN_SMALL_SIZE 16
#define JS_MALLOC_MAX_SMALL_SIZE 512
#if defined(__SANITIZE_ADDRESS__)
/* use the host malloc() for all allocations */
#define JS_MALLOC_LARGE_BLOCKS_ONLY 1
#else
#define JS_MALLOC_LARGE_BLOCKS_ONLY 0
#endif

typedef struct {
    struct list_head free_link;
    struct list_head link;
    uint8_t block_size_idx;
    uint16_t n_used_blocks; /* number of allocated blocks */
    uint16_t n_blocks; /* total number of blocks */
    uint16_t first_free_block; /* FREE_NIL if none */
#ifdef JS_MALLOC_USE_ITER
    /* bit set to 1 for allocated block */
    uint32_t bitmap[((JS_MALLOC_ARENA_SIZE / JS_MALLOC_MIN_SMALL_SIZE) + 31) / 32];
#endif
    /* n_blocks memory blocks of identical size */
    __attribute__((aligned(JS_MALLOC_ALIGN))) uint8_t blocks[];
} JSMallocArena;

no_inline JSMallocArena *js_malloc_new_arena(JSMallocContext *s, int block_size_idx);
no_inline void *js_malloc_large(JSMallocContext *s, size_t size);

static JS_ALLOCATOR_INLINE int get_block_size_index(size_t size)
{
    if (size <= 16) {
        return 0;
    } else if (size <= 128) {
        return (size + 7) / 8 - 2;
    } else if (size <= 256) {
        return (size + 15) / 16 + 6;
    } else if (size <= 512) {
        return (size + 31) / 32 + 14;
    } else {
        return JS_MALLOC_BLOCK_SIZE_COUNT;
    }
}

static JS_ALLOCATOR_INLINE JSMallocBlockHeader *get_zero_size_block(JSMallocContext *s)
{
    return (JSMallocBlockHeader *)s->zero_size_block;
}

static JS_ALLOCATOR_INLINE void *get_arena_block(JSMallocArena *ar,
                                                 unsigned int idx,
                                                 unsigned int block_size)
{
    return ar->blocks + idx * block_size;
}

/* size includes the block header and is at most JS_MALLOC_MAX_SMALL_SIZE. */
static JS_ALLOCATOR_INLINE unsigned int js_malloc_small_block_size(size_t size)
{
    if (size <= 16)
        return 16;
    else if (size <= 128)
        return (size + 7) & ~7;
    else if (size <= 256)
        return (size + 15) & ~15;
    else
        return (size + 31) & ~31;
}

static JS_ALLOCATOR_INLINE void *__js_malloc(JSMallocContext *s, size_t size)
{
    size_t total_size;
    if (unlikely(size == 0)) {
        JSMallocBlockHeader *b = get_zero_size_block(s);
        return b->user_data;
    } else {
        if (!JS_MALLOC_LARGE_BLOCKS_ONLY &&
            size <= JS_MALLOC_MAX_SMALL_SIZE - sizeof(JSMallocBlockHeader)) {
            int block_size_idx;
            unsigned int block_idx, block_size;
            JSMallocBlockHeader *b;
            JSMallocArena *ar;
            struct list_head *el, *head;

            total_size = ((size + JS_MALLOC_ALIGN - 1) & ~(JS_MALLOC_ALIGN - 1)) +
                sizeof(JSMallocBlockHeader);
            block_size_idx = get_block_size_index(total_size);
            block_size = js_malloc_small_block_size(total_size);
            head = &s->free_arena_list[block_size_idx];
            el = head->next;
            if (unlikely(el == head)) {
                ar = js_malloc_new_arena(s, block_size_idx);
                if (!ar)
                    return NULL;
            } else {
                ar = list_entry(el, JSMallocArena, free_link);
            }
            block_idx = ar->first_free_block;
            b = get_arena_block(ar, ar->first_free_block, block_size);
            ar->first_free_block = b->u.free_next;
            b->u.block_idx = block_idx;
            ar->n_used_blocks++;
            if (unlikely(ar->n_used_blocks == ar->n_blocks)) {
                list_del(&ar->free_link);
            }
#ifdef JS_MALLOC_USE_ITER
            ar->bitmap[block_idx / 32] |= 1 << (block_idx % 32);
#endif
            return b->user_data;
        } else {
            return js_malloc_large(s, size);
        }
    }
}

static JS_ALLOCATOR_INLINE void *js_malloc_inline(JSContext *ctx, size_t size)
{
    void *ptr;
    ptr = __js_malloc(&ctx->rt->malloc_ctx, size);
    if (unlikely(!ptr)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return ptr;
}

/* Throw out of memory in case of error */
static JS_ALLOCATOR_INLINE void *js_mallocz_inline(JSContext *ctx, size_t size)
{
    void *ptr;
    ptr = js_mallocz_rt(ctx->rt, size);
    if (unlikely(!ptr)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return ptr;
}

static JS_ALLOCATOR_INLINE void js_free_inline(JSContext *ctx, void *ptr)
{
    js_free_rt(ctx->rt, ptr);
}

/* Throw out of memory in case of error */
static JS_ALLOCATOR_INLINE void *js_realloc_inline(JSContext *ctx,
                                                   void *ptr,
                                                   size_t size)
{
    void *ret;
    ret = js_realloc_rt(ctx->rt, ptr, size);
    if (unlikely(!ret && size != 0)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return ret;
}

/* store extra allocated size in *pslack if successful */
static JS_ALLOCATOR_INLINE void *js_realloc2_inline(JSContext *ctx,
                                                    void *ptr,
                                                    size_t size,
                                                    size_t *pslack)
{
    void *ret;
    ret = js_realloc_rt(ctx->rt, ptr, size);
    if (unlikely(!ret && size != 0)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    if (pslack) {
        size_t new_size = js_malloc_usable_size_rt(ctx->rt, ret);
        *pslack = (new_size > size) ? new_size - size : 0;
    }
    return ret;
}

static JS_ALLOCATOR_INLINE size_t js_malloc_usable_size_inline(JSContext *ctx,
                                                               const void *ptr)
{
    return js_malloc_usable_size_rt(ctx->rt, ptr);
}

#undef JS_ALLOCATOR_INLINE
#endif
