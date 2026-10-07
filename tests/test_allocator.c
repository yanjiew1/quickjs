/*
 * QuickJS default allocator accounting tests
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
#include <string.h>

#include "../src/quickjs/internal/allocator.h"
#include "../src/quickjs/internal/runtime.h"

static void test_malloc_limit_overflow(void)
{
    JSMallocState state = { 0 };
    size_t used = SIZE_MAX - 128;

    state.malloc_size = used;
    state.malloc_limit = SIZE_MAX;
    assert(def_malloc_funcs.js_malloc(&state, 256) == NULL);
    assert(state.malloc_size == used && state.malloc_count == 0);
    state.malloc_limit = 0;
    assert(def_malloc_funcs.js_malloc(&state, 1) == NULL);
    assert(state.malloc_size == used && state.malloc_count == 0);
}

static void test_realloc_limit_overflow(void)
{
    JSMallocState state = { 0 };
    uint8_t *ptr;
    size_t allocated, i;

    state.malloc_limit = SIZE_MAX;
    ptr = def_malloc_funcs.js_malloc(&state, 32);
    assert(ptr);
    memset(ptr, 0xa5, 32);
    allocated = state.malloc_size;
    state.malloc_size = SIZE_MAX - 128;
    assert(def_malloc_funcs.js_realloc(&state, ptr, 512) == NULL);
    assert(state.malloc_size == SIZE_MAX - 128 && state.malloc_count == 1);
    for (i = 0; i < 32; i++)
        assert(ptr[i] == 0xa5);
    state.malloc_size = allocated;
    state.malloc_limit = 0;
    assert(def_malloc_funcs.js_realloc(&state, ptr, 16) == NULL);
    assert(state.malloc_size == allocated && state.malloc_count == 1);
    for (i = 0; i < 32; i++)
        assert(ptr[i] == 0xa5);
    assert(def_malloc_funcs.js_realloc(&state, ptr, 0) == NULL);
    assert(state.malloc_size == 0 && state.malloc_count == 0);
}

static void test_gc_accounting_overflow(void)
{
    JSRuntime *rt = JS_NewRuntime();
    size_t allocated;

    assert(rt);
    allocated = rt->malloc_ctx.malloc_state.malloc_size;
#ifndef FORCE_GC_AT_MALLOC
    rt->malloc_gc_threshold = SIZE_MAX;
    js_trigger_gc(rt, 1);
    assert(rt->malloc_gc_threshold == SIZE_MAX);
    rt->malloc_gc_threshold = allocated;
    js_trigger_gc(rt, 0);
    assert(rt->malloc_gc_threshold == allocated);
#endif
    rt->malloc_ctx.malloc_state.malloc_size = SIZE_MAX - 128;
    rt->malloc_gc_threshold = SIZE_MAX - 64;
    js_trigger_gc(rt, 256);
    assert(rt->malloc_gc_threshold == SIZE_MAX);
    rt->malloc_ctx.malloc_state.malloc_size = allocated;
    rt->malloc_gc_threshold = 0;
    js_trigger_gc(rt, 1);
    assert(rt->malloc_gc_threshold == allocated + (allocated >> 1));
    JS_FreeRuntime(rt);
}

int main(void)
{
    test_malloc_limit_overflow();
    test_realloc_limit_overflow();
    test_gc_accounting_overflow();
    return 0;
}
