/*
 * QuickJS sanitizer allocator policy tests
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#include <assert.h>
#include <stdio.h>

#include "../src/quickjs/internal/allocator.h"
/* Inspect the actual allocation owner without an engine test API. */
#include "../src/quickjs/allocator.c"

/* Determine the expected policy independently of the engine policy macro. */
#if defined(__SANITIZE_ADDRESS__)
#define EXPECT_HOST_PER_BLOCK 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define EXPECT_HOST_PER_BLOCK 1
#else
#define EXPECT_HOST_PER_BLOCK 0
#endif
#else
#define EXPECT_HOST_PER_BLOCK 0
#endif

typedef struct AllocationCounts {
    size_t allocations;
    size_t frees;
    size_t live;
} AllocationCounts;

static void *count_malloc(JSMallocState *s, size_t size)
{
    AllocationCounts *counts = s->opaque;
    void *ptr = def_malloc_funcs.js_malloc(s, size);

    if (ptr) {
        counts->allocations++;
        counts->live++;
    }
    return ptr;
}

static void count_free(JSMallocState *s, void *ptr)
{
    AllocationCounts *counts = s->opaque;

    if (ptr) {
        assert(counts->live > 0);
        counts->frees++;
        counts->live--;
    }
    def_malloc_funcs.js_free(s, ptr);
}

static void *count_realloc(JSMallocState *s, void *ptr, size_t size)
{
    AllocationCounts *counts = s->opaque;
    int had_ptr = ptr != NULL;
    void *result = def_malloc_funcs.js_realloc(s, ptr, size);

    if (!had_ptr && result) {
        counts->allocations++;
        counts->live++;
    } else if (had_ptr && size == 0) {
        assert(counts->live > 0);
        counts->frees++;
        counts->live--;
    }
    return result;
}

static size_t count_usable_size(const void *ptr)
{
    return def_malloc_funcs.js_malloc_usable_size(ptr);
}

int main(void)
{
    static const JSMallocFunctions funcs = {
        count_malloc, count_free, count_realloc, count_usable_size
    };
    enum { BLOCK_COUNT = 32, BLOCK_SIZE = 24 };
    AllocationCounts counts = { 0 };
    JSRuntime *rt = JS_NewRuntime2(&funcs, &counts);
    void *blocks[BLOCK_COUNT];
    size_t allocations_before, frees_before, live_before, allocation_delta;
    int i;

    assert(rt);
    assert(JS_MALLOC_LARGE_BLOCKS_ONLY == EXPECT_HOST_PER_BLOCK);
    allocations_before = counts.allocations;
    frees_before = counts.frees;
    live_before = counts.live;
    /* Hold all small blocks simultaneously so reuse cannot hide allocations. */
    for (i = 0; i < BLOCK_COUNT; i++) {
        blocks[i] = js_malloc_rt(rt, BLOCK_SIZE);
        assert(blocks[i]);
    }
    allocation_delta = counts.allocations - allocations_before;
    assert(counts.allocations >= counts.frees);
    assert(counts.live == counts.allocations - counts.frees);
    if (EXPECT_HOST_PER_BLOCK) {
        assert(allocation_delta == BLOCK_COUNT);
        assert(counts.frees == frees_before);
        assert(counts.live == live_before + BLOCK_COUNT);
    }
    for (i = 0; i < BLOCK_COUNT; i++)
        js_free_rt(rt, blocks[i]);
    if (EXPECT_HOST_PER_BLOCK) {
        assert(counts.frees - frees_before == BLOCK_COUNT);
        assert(counts.live == live_before);
    }
    /* Pool lifetime and arena counts can vary; require complete cleanup. */
    JS_FreeRuntime(rt);
    assert(counts.live == 0);
    assert(counts.allocations == counts.frees);
    printf("allocator policy: host_per_block=%d small_blocks=%d host_allocations=%zu cleanup_live=%zu\n",
           EXPECT_HOST_PER_BLOCK, BLOCK_COUNT, allocation_delta, counts.live);
    return 0;
}
