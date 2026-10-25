/* Allocation-failure test support.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT
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
#ifndef TESTS_OOM_ALLOCATOR_H
#define TESTS_OOM_ALLOCATOR_H

#include <assert.h>
#include <string.h>
#include "raw-allocator.h"

typedef struct OOMTestAllocator {
    size_t calls, fail_at, live;
    int enabled, failures;
} OOMTestAllocator;

static int oom_test_fail(OOMTestAllocator *s)
{
    if (s->enabled && ++s->calls == s->fail_at) {
        s->failures++;
        return 1;
    }
    return 0;
}

static void *oom_test_malloc(JSMallocState *ms, size_t size)
{
    OOMTestAllocator *s = ms->opaque;
    void *ptr;

    if (oom_test_fail(s))
        return NULL;
    ptr = test_raw_malloc(ms, size);
    if (ptr)
        s->live++;
    return ptr;
}

static void oom_test_free(JSMallocState *ms, void *ptr)
{
    OOMTestAllocator *s = ms->opaque;

    if (ptr) {
        assert(s->live > 0);
        s->live--;
    }
    test_raw_free(ms, ptr);
}

static void *oom_test_realloc(JSMallocState *ms, void *ptr, size_t size)
{
    OOMTestAllocator *s = ms->opaque;
    int had_ptr = ptr != NULL;
    void *result;

    if (size && oom_test_fail(s))
        return NULL;
    result = test_raw_realloc(ms, ptr, size);
    if (!had_ptr && result)
        s->live++;
    else if (had_ptr && !size) {
        assert(s->live > 0);
        s->live--;
    }
    return result;
}

static inline JSRuntime *oom_test_runtime(OOMTestAllocator *s)
{
    const JSMallocFunctions mf = {
        oom_test_malloc, oom_test_free, oom_test_realloc, test_raw_usable_size,
    };
    JSRuntime *rt;

    memset(s, 0, sizeof(*s));
    rt = JS_NewRuntime2(&mf, s);
    assert(rt);
    return rt;
}

static inline void oom_test_begin(OOMTestAllocator *s, size_t fail_at)
{
    s->calls = s->failures = 0;
    s->fail_at = fail_at;
    s->enabled = 1;
}

static inline void oom_test_end(OOMTestAllocator *s)
{
    s->enabled = 0;
}

static inline void oom_test_check_exception(JSContext *ctx)
{
    JSValue error;

    assert(JS_HasException(ctx));
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
}

#endif
