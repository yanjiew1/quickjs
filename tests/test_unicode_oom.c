/* Unicode allocation-failure regression tests.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdlib.h>
#include "cutils.h"
#include "libunicode.h"

typedef struct { int calls, fail_at, live; } Allocator;
static void *alloc(void *opaque, void *ptr, size_t size)
{
    Allocator *a = opaque;
    int had_ptr = ptr != NULL;
    void *result;
    if (!size) {
        if (ptr)
            a->live--;
        free(ptr);
        return NULL;
    }
    if (++a->calls == a->fail_at)
        return NULL;
    result = realloc(ptr, size);
    if (result && !had_ptr)
        a->live++;
    return result;
}

static void test_normalization_oom(void)
{
    static const uint32_t source[] = { 0xfb03 };
    Allocator a = { 0, 0, 0 };
    uint32_t *dst = NULL;
    int calls, fail;
    assert(unicode_normalize(&dst, source, 1, UNICODE_NFKD, &a, alloc) == 3);
    assert(dst[0] == 'f' && dst[1] == 'f' && dst[2] == 'i');
    calls = a.calls;
    assert(calls > 0);
    alloc(&a, dst, 0);
    assert(!a.live);
    for (fail = 1; fail <= calls; fail++) {
        a.calls = 0;
        a.fail_at = fail;
        dst = (uint32_t *)(uintptr_t)1;
        assert(unicode_normalize(&dst, source, 1, UNICODE_NFKD, &a, alloc) == -1);
        assert(!dst && !a.live);
    }
}

static void test_canonicalization_oom(void)
{
    Allocator a = { 0, 0, 0 };
    CharRange cr;
    int calls, fail;
    a.calls = a.fail_at = 0;
    cr_init(&cr, &a, alloc);
    assert(cr_add_interval(&cr, 'A', 'A' + 1) == 0);
    assert(cr_add_interval(&cr, 'C', 'C' + 1) == 0);
    a.calls = 0;
    assert(cr_regexp_canonicalize(&cr, 1) == 0);
    calls = a.calls;
    assert(calls > 0);
    cr_free(&cr);
    assert(!a.live);
    for (fail = 1; fail <= calls; fail++) {
        a.calls = a.fail_at = 0;
        cr_init(&cr, &a, alloc);
        assert(cr_add_interval(&cr, 'A', 'A' + 1) == 0);
        assert(cr_add_interval(&cr, 'C', 'C' + 1) == 0);
        a.calls = 0;
        a.fail_at = fail;
        assert(cr_regexp_canonicalize(&cr, 1) == -1);
        cr_free(&cr);
        assert(!a.live);
    }
}

int main(void)
{
    test_normalization_oom();
    test_canonicalization_oom();
    return 0;
}
