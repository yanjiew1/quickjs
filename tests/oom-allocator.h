/* Allocation-failure test support.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#ifndef TESTS_OOM_ALLOCATOR_H
#define TESTS_OOM_ALLOCATOR_H

#include <assert.h>
#include <string.h>
#include "../src/quickjs/internal/runtime.h"
#include "../src/quickjs/internal/allocator.h"

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
    ptr = def_malloc_funcs.js_malloc(ms, size);
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
    def_malloc_funcs.js_free(ms, ptr);
}

static void *oom_test_realloc(JSMallocState *ms, void *ptr, size_t size)
{
    OOMTestAllocator *s = ms->opaque;
    int had_ptr = ptr != NULL;
    void *result;

    if (size && oom_test_fail(s))
        return NULL;
    result = def_malloc_funcs.js_realloc(ms, ptr, size);
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
    JSMallocFunctions mf = def_malloc_funcs;
    JSRuntime *rt;

    memset(s, 0, sizeof(*s));
    mf.js_malloc = oom_test_malloc;
    mf.js_free = oom_test_free;
    mf.js_realloc = oom_test_realloc;
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
