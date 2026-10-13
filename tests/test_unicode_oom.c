/* Unicode allocation-failure regression tests.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "libregexp.h"
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

void *lre_realloc(void *opaque, void *ptr, size_t size)
{
    return alloc(opaque, ptr, size);
}

int lre_check_stack_overflow(void *opaque, size_t size)
{
    return 0;
}

int lre_check_timeout(void *opaque)
{
    return 0;
}

static void test_compile_failures(const char *pattern, int flags)
{
    Allocator a = { 0, 0, 0 };
    uint8_t *bytecode;
    char error[128];
    int allocations, length, fail;

    bytecode = lre_compile(&length, error, sizeof(error), pattern,
                           strlen(pattern), flags, &a);
    assert(bytecode && length > 0);
    allocations = a.calls;
    alloc(&a, bytecode, 0);
    assert(a.live == 0);
    for (fail = 1; fail <= allocations; fail++) {
        a.calls = 0;
        a.fail_at = fail;
        bytecode = lre_compile(&length, error, sizeof(error), pattern,
                               strlen(pattern), flags, &a);
        assert(bytecode == NULL && length == 0);
        assert(strcmp(error, "out of memory") == 0);
        assert(a.live == 0);
    }
}

static int failing_sequence(void *opaque, const uint32_t *sequence, int length)
{
    int *calls = opaque;

    assert(length > 0);
    (*calls)++;
    return -1;
}

static void test_sequence_failures(void)
{
    static const char pattern[] = "\\p{Emoji_Keycap_Sequence}";
    Allocator a = { 0, 0, 0 };
    CharRange cr;
    uint8_t *bytecode;
    char error[128];
    int calls = 0, allocations, length, fail;

    cr_init(&cr, &a, alloc);
    assert(unicode_sequence_prop("unknown", failing_sequence, &calls, &cr) == -2);
    assert(unicode_sequence_prop("RGI_Emoji", NULL, &calls, &cr) == -1);
    assert(calls == 0 && a.live == 0);
    assert(unicode_sequence_prop("RGI_Emoji", failing_sequence,
                                 &calls, &cr) == -1);
    assert(calls == 1);
    cr_free(&cr);
    assert(a.live == 0);

    a.calls = 0;
    bytecode = lre_compile(&length, error, sizeof(error), pattern,
                           strlen(pattern), LRE_FLAG_UNICODE_SETS, &a);
    assert(bytecode);
    allocations = a.calls;
    alloc(&a, bytecode, 0);
    assert(a.live == 0);
    for (fail = 1; fail <= allocations; fail++) {
        a.calls = 0;
        a.fail_at = fail;
        bytecode = lre_compile(&length, error, sizeof(error), pattern,
                               strlen(pattern), LRE_FLAG_UNICODE_SETS, &a);
        assert(bytecode == NULL);
        assert(strcmp(error, "out of memory") == 0);
        assert(a.live == 0);
    }
}

int main(void)
{
    test_normalization_oom();
    test_canonicalization_oom();
    test_compile_failures("a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p", 0);
    test_compile_failures("(?<=ab|cd)(?:ef|gh){2,4}", 0);
    test_compile_failures(
        "(?<abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_first>a)|"
        "(?<abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_second>b)", 0);
    test_compile_failures("[\\q{abcdefghijklmnop|abcdefghijklmnoq|abcdefghijklmnor}]", LRE_FLAG_UNICODE_SETS);
    test_sequence_failures();
    return 0;
}
