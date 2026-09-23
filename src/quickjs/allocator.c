/*
 * QuickJS Allocator
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__) || defined(__GLIBC__)
#include <malloc.h>
#elif defined(__FreeBSD__)
#include <malloc_np.h>
#endif

#include "cutils.h"
#include "internal/runtime.h"
#include "internal/allocator.h"
#include "internal/atom-string.h"
#include "internal/error.h"


/* JS malloc */

/* max overhead for size >= 64: 12.5% */
static const uint16_t js_malloc_block_sizes[JS_MALLOC_BLOCK_SIZE_COUNT] = {
    16,
    24,
    32,
    40,
    48,
    56,
    64,
    72,
    80,
    88,
    96,
    104,
    112,
    120,
    128,
    144,
    160,
    176,
    192,
    208,
    224,
    240,
    256,
    288,
    320,
    352,
    384,
    416,
    448,
    480,
    512,
};

static int get_block_size_index(size_t size)
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

static JSMallocBlockHeader *get_zero_size_block(JSMallocContext *s)
{
    return (JSMallocBlockHeader *)s->zero_size_block;
}

QJS_INTERNAL void js_malloc_init(JSMallocContext *s)
{
    int i;
    memset(s, 0, sizeof(*s));
    get_zero_size_block(s)->u.block_idx = FREE_NIL;
    for(i = 0; i < JS_MALLOC_BLOCK_SIZE_COUNT; i++) {
        init_list_head(&s->arena_list[i]);
        init_list_head(&s->free_arena_list[i]);
    }
#ifdef JS_MALLOC_USE_ITER
    init_list_head(&s->large_block_list);
#endif
}

static void *get_arena_block(JSMallocArena *ar, unsigned int idx, unsigned int block_size)
{
    return ar->blocks + idx * block_size;
}


static no_inline JSMallocArena *js_malloc_new_arena(JSMallocContext *s, int block_size_idx)
{
    JSMallocBlockHeader *b;
    JSMallocArena *ar;
    int n_blocks, block_size, i;

    block_size = js_malloc_block_sizes[block_size_idx];
    n_blocks = (JS_MALLOC_ARENA_SIZE - sizeof(JSMallocArena)) / block_size;
    ar = s->mf.js_malloc(&s->malloc_state, sizeof(JSMallocArena) + n_blocks * block_size);
    if (!ar)
        return NULL;

    ar->block_size_idx = block_size_idx;
    ar->n_blocks = n_blocks;
    ar->n_used_blocks = 0;
    ar->first_free_block = 0;
#ifdef JS_MALLOC_USE_ITER
    {
        int n_bitmap_words = (n_blocks + 31) / 32;
        for(i = 0; i < n_bitmap_words; i++)
            ar->bitmap[i] = 0;
    }
#endif
    for(i = 0; i < n_blocks - 1; i++) {
        b = get_arena_block(ar, i, block_size);
        b->u.free_next = i + 1;
        b->block_size_idx = block_size_idx;
    }
    b = get_arena_block(ar, n_blocks - 1, block_size);
    b->u.free_next = FREE_NIL;
    b->block_size_idx = block_size_idx;
    
    /* add to the head */
    list_add(&ar->link, &s->arena_list[block_size_idx]);
    list_add(&ar->free_link, &s->free_arena_list[block_size_idx]);
    return ar;
}

static no_inline void *js_malloc_large(JSMallocContext *s, size_t size)
{
    JSMallocLargeBlockHeader *b;
    b = s->mf.js_malloc(&s->malloc_state, sizeof(JSMallocLargeBlockHeader) + size);
    if (!b)
        return NULL;
    b->header.u.block_idx = FREE_NIL;
    b->header.block_size_idx = 0xff; /* fail safe */
#ifdef JS_MALLOC_USE_ITER
    list_add_tail(&b->link, &s->large_block_list);
#endif
    return b->header.user_data;
}

static void *__js_malloc(JSMallocContext *s, size_t size)
{
    size_t total_size;
    if (unlikely(size == 0)) {
        JSMallocBlockHeader *b = get_zero_size_block(s);
        return b->user_data;
    } else {
        total_size = ((size + JS_MALLOC_ALIGN - 1) & ~(JS_MALLOC_ALIGN - 1)) +
            sizeof(JSMallocBlockHeader);
        if (!JS_MALLOC_LARGE_BLOCKS_ONLY &&
            total_size <= JS_MALLOC_MAX_SMALL_SIZE) {
            int block_size_idx;
            unsigned int block_idx, block_size;
            JSMallocBlockHeader *b;
            JSMallocArena *ar;
            struct list_head *el, *head;
            
            block_size_idx = get_block_size_index(total_size);
            block_size = js_malloc_block_sizes[block_size_idx];
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

static void __js_free(JSMallocContext *s, void *ptr)
{
    JSMallocBlockHeader *b;

    if (!ptr)
        return;
    b = container_of(ptr, JSMallocBlockHeader, user_data);
    if (unlikely(b->u.block_idx == FREE_NIL)) {
        /* large or zero size block */
        if (b == get_zero_size_block(s)) {
            /* nothing to do */
        } else {
            JSMallocLargeBlockHeader *lb = container_of(ptr, JSMallocLargeBlockHeader, header.user_data);
#ifdef JS_MALLOC_USE_ITER
            list_del(&lb->link);
#endif
            s->mf.js_free(&s->malloc_state, lb);
        }
    } else {
        unsigned int block_idx = b->u.block_idx;
        unsigned int block_size_idx = b->block_size_idx;
        unsigned int block_size = js_malloc_block_sizes[block_size_idx];
        JSMallocArena *ar = (JSMallocArena *)((uint8_t *)b - block_size * block_idx - sizeof(JSMallocArena));
        b->u.free_next = ar->first_free_block;
        ar->first_free_block = block_idx;
#ifdef JS_MALLOC_USE_ITER
        ar->bitmap[block_idx / 32] &= ~(1 << (block_idx % 32));
#endif
        /* add back to the free list if needed */
        if (unlikely(ar->n_used_blocks == ar->n_blocks)) {
            list_add(&ar->free_link, &s->free_arena_list[block_size_idx]);
        }
        ar->n_used_blocks--;
        if (unlikely(ar->n_used_blocks == 0)) {
            list_del(&ar->link);
            list_del(&ar->free_link);
            s->mf.js_free(&s->malloc_state, ar);
        }
    }
}

static void *__js_realloc(JSMallocContext *s, void *ptr, size_t size)
{
    JSMallocBlockHeader *b;
    if (ptr == NULL) {
        return __js_malloc(s, size);
    } else if (size == 0) {
        __js_free(s, ptr);
        return NULL;
    }
    b = container_of(ptr, JSMallocBlockHeader, user_data);
    if (b->u.block_idx == FREE_NIL) {
        if (b == get_zero_size_block(s)) {
            return __js_malloc(s, size);
        } else {
            JSMallocLargeBlockHeader *lb, *new_lb;
            lb = container_of(ptr, JSMallocLargeBlockHeader, header.user_data);
#ifdef JS_MALLOC_USE_ITER
            list_del(&lb->link);
#endif
            new_lb = s->mf.js_realloc(&s->malloc_state, lb, sizeof(JSMallocLargeBlockHeader) + size);
            if (!new_lb) {
#ifdef JS_MALLOC_USE_ITER
                /* add again in the list */
                list_add_tail(&lb->link, &s->large_block_list);
#endif
                return NULL;
            }
            new_lb->header.u.block_idx = FREE_NIL;
            new_lb->header.block_size_idx = 0xff; /* fail safe */
#ifdef JS_MALLOC_USE_ITER
            list_add_tail(&new_lb->link, &s->large_block_list);
#endif
            return new_lb->header.user_data;
        }
    } else {
        unsigned int block_size_idx = b->block_size_idx;
        size_t block_size = js_malloc_block_sizes[block_size_idx];
        size_t total_size, old_size;
        void *new_ptr;
        JSMallocBlockHeader *new_b;

        total_size = ((size + JS_MALLOC_ALIGN - 1) & ~(JS_MALLOC_ALIGN - 1)) +
            sizeof(JSMallocBlockHeader);
        if (total_size <= block_size)
            return ptr;
        new_ptr = __js_malloc(s, size);
        if (!new_ptr)
            return NULL;
        new_b = container_of(new_ptr, JSMallocBlockHeader, user_data);
        /* copy the GC data */
        new_b->gc_obj_type = b->gc_obj_type;
        new_b->mark = b->mark;
        new_b->ref_count = b->ref_count;
        /* copy the data */
        old_size = block_size - sizeof(JSMallocBlockHeader);
        if (size > old_size)
            size = old_size;
        memcpy(new_ptr, ptr, size);
        __js_free(s, ptr);
        return new_ptr;
    }
}

static size_t __js_malloc_usable_size(JSMallocContext *s, const char *ptr)
{
    JSMallocBlockHeader *b;
    if (!ptr)
        return 0;
    b = container_of(ptr, JSMallocBlockHeader, user_data);
    if (b->u.block_idx == FREE_NIL) {
        if (b == get_zero_size_block(s)) {
            return 0;
        } else {
            JSMallocLargeBlockHeader *lb;
            size_t size;
            lb = container_of(ptr, JSMallocLargeBlockHeader, header.user_data);
            if (s->mf.js_malloc_usable_size) {
                size = s->mf.js_malloc_usable_size(lb);
                if (size != 0)
                    size -= sizeof(JSMallocLargeBlockHeader);
                return size;
            } else {
                return 0;
            }
        }
    } else {
        size_t block_size = js_malloc_block_sizes[b->block_size_idx];
        return block_size - sizeof(*b);
    }
}

static __maybe_unused void js_malloc_dump_arenas(JSMallocContext *s)
{
    struct list_head *el;
    int block_size_idx;

    printf("%20s %10s %10s\n", "PTR", "BLK_SIZE", "ALLOC");
    for(block_size_idx = 0; block_size_idx < JS_MALLOC_BLOCK_SIZE_COUNT; block_size_idx++) {
        int block_size = js_malloc_block_sizes[block_size_idx];
        list_for_each(el, &s->arena_list[block_size_idx]) {
            JSMallocArena *ar = list_entry(el, JSMallocArena, link);
            printf("%20p %10u %9.1f%%\n",
                   ar, block_size,
                   (double)ar->n_used_blocks / ar->n_blocks * 100);
        }
    }
}

#ifdef JS_MALLOC_USE_ITER
typedef void JSMallocIterFunc(void *opaque, void *ptr);

/* iterate thru allocated blocks. The allocated block list should not
   be modified while iterating. */
static __maybe_unused void js_malloc_iter(JSMallocContext *s, JSMallocIterFunc *iter_func, void *iter_opaque)
{
    struct list_head *el;
    int block_size_idx;
    int i, j, n_words;
    uint32_t bmp;
    
    for(block_size_idx = 0; block_size_idx < JS_MALLOC_BLOCK_SIZE_COUNT; block_size_idx++) {
        unsigned int block_size = js_malloc_block_sizes[block_size_idx];
        list_for_each(el, &s->arena_list[block_size_idx]) {
            JSMallocArena *ar = list_entry(el, JSMallocArena, link);
            n_words = (ar->n_blocks + 31) / 32;
            for(i = 0; i < n_words; i++) {
                bmp = ar->bitmap[i];
                while (bmp != 0) {
                    j = ctz32(bmp);
                    bmp &= ~(1 << j);
                    iter_func(iter_opaque, get_arena_block(ar, i * 32+ j, block_size));
                }
            }
        }
    }
    list_for_each(el, &s->large_block_list) {
        JSMallocLargeBlockHeader *lb = list_entry(el, JSMallocLargeBlockHeader, link);
        iter_func(iter_opaque, lb->header.user_data);
    }
}
#endif

/* end JS malloc */

QJS_INTERNAL void js_trigger_gc(JSRuntime *rt, size_t size)
{
    BOOL force_gc;
#ifdef FORCE_GC_AT_MALLOC
    force_gc = TRUE;
#else
    force_gc = ((rt->malloc_ctx.malloc_state.malloc_size + size) >
                rt->malloc_gc_threshold);
#endif
    if (force_gc) {
#ifdef DUMP_GC
        printf("GC: size=%" PRIu64 "\n",
               (uint64_t)rt->malloc_ctx.malloc_state.malloc_size);
#endif
        JS_RunGC(rt);
        rt->malloc_gc_threshold = rt->malloc_ctx.malloc_state.malloc_size +
            (rt->malloc_ctx.malloc_state.malloc_size >> 1);
    }
}

void *js_malloc_rt(JSRuntime *rt, size_t size)
{
    return __js_malloc(&rt->malloc_ctx, size);
}

void js_free_rt(JSRuntime *rt, void *ptr)
{
    __js_free(&rt->malloc_ctx, ptr);
}

void *js_realloc_rt(JSRuntime *rt, void *ptr, size_t size)
{
    return __js_realloc(&rt->malloc_ctx, ptr, size);
}

size_t js_malloc_usable_size_rt(JSRuntime *rt, const void *ptr)
{
    return __js_malloc_usable_size(&rt->malloc_ctx, ptr);
}

void *js_mallocz_rt(JSRuntime *rt, size_t size)
{
    void *ptr;
    ptr = js_malloc_rt(rt, size);
    if (unlikely(!ptr))
        return NULL;
    return memset(ptr, 0, size);
}

/* Throw out of memory in case of error */
void *js_malloc(JSContext *ctx, size_t size)
{
    void *ptr;
    ptr = js_malloc_rt(ctx->rt, size);
    if (unlikely(!ptr)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return ptr;
}

/* Throw out of memory in case of error */
void *js_mallocz(JSContext *ctx, size_t size)
{
    void *ptr;
    ptr = js_mallocz_rt(ctx->rt, size);
    if (unlikely(!ptr)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return ptr;
}

void js_free(JSContext *ctx, void *ptr)
{
    js_free_rt(ctx->rt, ptr);
}

/* Throw out of memory in case of error */
void *js_realloc(JSContext *ctx, void *ptr, size_t size)
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
void *js_realloc2(JSContext *ctx, void *ptr, size_t size, size_t *pslack)
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

size_t js_malloc_usable_size(JSContext *ctx, const void *ptr)
{
    return js_malloc_usable_size_rt(ctx->rt, ptr);
}

/* Throw out of memory exception in case of error */
char *js_strndup(JSContext *ctx, const char *s, size_t n)
{
    char *ptr;
    ptr = js_malloc(ctx, n + 1);
    if (ptr) {
        memcpy(ptr, s, n);
        ptr[n] = '\0';
    }
    return ptr;
}

char *js_strdup(JSContext *ctx, const char *str)
{
    return js_strndup(ctx, str, strlen(str));
}

QJS_INTERNAL no_inline int js_realloc_array(JSContext *ctx, void **parray,
                                      int elem_size, int *psize, int req_size)
{
    int new_size;
    size_t slack;
    void *new_array;
    /* XXX: potential arithmetic overflow */
    new_size = max_int(req_size, *psize * 3 / 2);
    new_array = js_realloc2(ctx, *parray, new_size * elem_size, &slack);
    if (!new_array)
        return -1;
    new_size += slack / elem_size;
    *psize = new_size;
    *parray = new_array;
    return 0;
}

/* default memory allocation functions with memory limitation */
static size_t js_def_malloc_usable_size(const void *ptr)
{
#if defined(__APPLE__)
    return malloc_size(ptr);
#elif defined(_WIN32)
    return _msize((void *)ptr);
#elif defined(__EMSCRIPTEN__)
    return 0;
#elif defined(__linux__) || defined(__GLIBC__)
    return malloc_usable_size((void *)ptr);
#else
    /* change this to `return 0;` if compilation fails */
    return malloc_usable_size((void *)ptr);
#endif
}

static void *js_def_malloc(JSMallocState *s, size_t size)
{
    void *ptr;

    /* Do not allocate zero bytes: behavior is platform dependent */
    assert(size != 0);

    if (unlikely(s->malloc_size + size > s->malloc_limit))
        return NULL;

    ptr = malloc(size);
    if (!ptr)
        return NULL;

    s->malloc_count++;
    s->malloc_size += js_def_malloc_usable_size(ptr) + MALLOC_OVERHEAD;
    return ptr;
}

static void js_def_free(JSMallocState *s, void *ptr)
{
    if (!ptr)
        return;

    s->malloc_count--;
    s->malloc_size -= js_def_malloc_usable_size(ptr) + MALLOC_OVERHEAD;
    free(ptr);
}

static void *js_def_realloc(JSMallocState *s, void *ptr, size_t size)
{
    size_t old_size;

    if (!ptr) {
        if (size == 0)
            return NULL;
        return js_def_malloc(s, size);
    }
    old_size = js_def_malloc_usable_size(ptr);
    if (size == 0) {
        s->malloc_count--;
        s->malloc_size -= old_size + MALLOC_OVERHEAD;
        free(ptr);
        return NULL;
    }
    if (s->malloc_size + size - old_size > s->malloc_limit)
        return NULL;

    ptr = realloc(ptr, size);
    if (!ptr)
        return NULL;

    s->malloc_size += js_def_malloc_usable_size(ptr) - old_size;
    return ptr;
}

static const JSMallocFunctions def_malloc_funcs = {
    js_def_malloc,
    js_def_free,
    js_def_realloc,
    js_def_malloc_usable_size,
};

JSRuntime *JS_NewRuntime(void)
{
    return JS_NewRuntime2(&def_malloc_funcs, NULL);
}

