/*
 * QuickJS native generated-context allocation ownership fixture
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
#ifndef QUICKJS_TEST_QJSC_ALLOCATION_PROBE_H
#define QUICKJS_TEST_QJSC_ALLOCATION_PROBE_H
#include "../src/quickjs/internal/allocator.h"
typedef struct AllocationProbe {
    size_t live;
    size_t attempts;
    size_t failure_at;
    int failed;
} AllocationProbe;

static int should_fail(AllocationProbe *probe)
{
    if (!probe->failure_at)
        return 0;
    if (++probe->attempts == probe->failure_at) {
        probe->failed = 1;
        return 1;
    }
    return 0;
}

static void *probe_malloc(JSMallocState *state, size_t size)
{
    AllocationProbe *probe = state->opaque;
    void *ptr;
    if (should_fail(probe))
        return NULL;
    ptr = def_malloc_funcs.js_malloc(state, size);
    if (ptr)
        probe->live++;
    return ptr;
}

static void probe_free(JSMallocState *state, void *ptr)
{
    AllocationProbe *probe = state->opaque;
    if (ptr) {
        assert(probe->live);
        probe->live--;
    }
    def_malloc_funcs.js_free(state, ptr);
}

static void *probe_realloc(JSMallocState *state, void *ptr, size_t size)
{
    AllocationProbe *probe = state->opaque;
    void *result;
    if (!ptr)
        return probe_malloc(state, size);
    if (!size) {
        probe_free(state, ptr);
        return NULL;
    }
    if (should_fail(probe))
        return NULL;
    result = def_malloc_funcs.js_realloc(state, ptr, size);
    return result;
}

static const JSMallocFunctions probe_functions = {
    probe_malloc, probe_free, probe_realloc, NULL,
};

#endif
