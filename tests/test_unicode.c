/*
 * Unicode utility tests
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
#include <stdlib.h>

#include "cutils.h"
#include "libunicode.h"

static void *test_realloc(void *opaque, void *ptr, size_t size)
{
    (void)opaque;
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

typedef struct RangeAllocation {
    size_t requested;
    int calls;
} RangeAllocation;

static void *reject_range_allocation(void *opaque, void *ptr, size_t size)
{
    RangeAllocation *allocation = opaque;
    assert(!ptr);
    allocation->requested = size;
    allocation->calls++;
    return NULL;
}

static void test_range_capacity_limits(void)
{
    RangeAllocation allocation = { 0 };
    CharRange range;
    int capacity = INT_MAX / 2;

    /* Capacity-only fixtures: the rejecting callback never accesses points. */
    cr_init(&range, &allocation, reject_range_allocation);
    range.size = capacity;
    assert(cr_realloc(&range, capacity + 1) < 0);
    assert(!range.points && range.size == capacity && range.len == 0);
    if (SIZE_MAX / sizeof(uint32_t) >= (size_t)capacity + 1) {
        assert(allocation.calls == 1);
        assert(allocation.requested == ((uint64_t)capacity + capacity / 2) * sizeof(uint32_t));
    } else {
        assert(allocation.calls == 0);
    }
    allocation.calls = 0;
    range.len = range.size = INT_MAX;
    assert(cr_add_point(&range, 1) < 0);
    range.len = INT_MAX - 1;
    assert(cr_add_interval(&range, 1, 2) < 0);
    assert(cr_invert(&range) < 0);
    assert(allocation.calls == 0 && !range.points && range.len == INT_MAX - 1);
    assert(cr_realloc(&range, -1) < 0);
}

static void test_default_range_allocator(void)
{
    CharRange range;

    cr_init(&range, NULL, NULL);
    /* Empty cleanup must not create an ignored zero-size allocation. */
    assert(range.realloc_func(NULL, NULL, 0) == NULL);
    cr_free(&range);
    assert(cr_add_interval(&range, 'a', 'z' + 1) == 0);
    cr_free(&range);
}

static void test_empty_range(void)
{
    CharRange src, dst;
    cr_init(&src, NULL, test_realloc);
    cr_init(&dst, NULL, test_realloc);
    assert(cr_copy(&dst, &src) == 0);
    assert(dst.len == 0);
    assert(cr_add_interval(&dst, 'a', 'z' + 1) == 0);
    assert(dst.len == 2);
    assert(cr_copy(&dst, &src) == 0);
    assert(dst.len == 0);
    cr_free(&dst);
    cr_free(&src);
}

static void test_case_folding(void)
{
    static const struct {
        uint32_t code;
        uint32_t simple;
        int len;
        uint32_t full[LRE_CC_RES_LEN_MAX];
    } cases[] = {
        { 0x1fd3, 0x0390, 3, { 0x03b9, 0x0308, 0x0301 } },
        { 0x1fe3, 0x03b0, 3, { 0x03c5, 0x0308, 0x0301 } },
        { 0xfb05, 0xfb06, 2, { 0x0073, 0x0074 } },
    };
    uint32_t res[LRE_CC_RES_LEN_MAX];
    int i, j;

    for (i = 0; i < countof(cases); i++) {
        assert(lre_case_conv(res, cases[i].code, 2) == cases[i].len);
        for (j = 0; j < cases[i].len; j++)
            assert(res[j] == cases[i].full[j]);
        assert(lre_canonicalize(cases[i].code, TRUE) ==
               lre_canonicalize(cases[i].simple, TRUE));
    }
}

static int category_contains(const CharRange *range, uint32_t code)
{
    int i;
    for (i = 0; i < range->len; i += 2)
        if (code >= range->points[i] && code < range->points[i + 1])
            return 1;
    return 0;
}

static void test_category_points(void)
{
    const char *names[] = {"L", "M"};
    int (*predicates[])(uint32_t) = {lre_is_letter, lre_is_mark};
    int kind, i, delta;
    for (kind = 0; kind < 2; kind++) {
        CharRange range;
        cr_init(&range, NULL, test_realloc);
        assert(!unicode_general_category(&range, names[kind]));
        for (i = 0; i < range.len; i++) {
            /* Every category boundary and its immediate neighbours, including
               supplementary ranges and alternating Lu/Ll table runs. */
            for (delta = -1; delta <= 1; delta++) {
                int64_t code = (int64_t)range.points[i] + delta;
                if (code >= 0 && code <= 0x10ffff)
                    assert(predicates[kind](code) == category_contains(&range, code));
            }
        }
        for (i = 0; i < 128; i++)
            assert(predicates[kind](i) == category_contains(&range, i));
        assert(!predicates[kind](0xd800) && !predicates[kind](0x10ffff));
        assert(!predicates[kind](0x110000) && !predicates[kind](UINT32_MAX));
        cr_free(&range);
    }
    assert(lre_is_letter(0x02b0)); /* Modifier letter. */
    assert(lre_is_letter(0x10400)); /* Supplementary capital letter. */
    assert(!lre_is_letter(0x2160)); /* Letter number is not General_Category L. */
    assert(lre_is_mark(0x0345) && !lre_is_letter(0x0345));
    assert(lre_is_mark(0x0903) && lre_is_mark(0x20dd));
}

int main(void)
{
    test_range_capacity_limits();
    test_default_range_allocator();
    test_empty_range();
    test_case_folding();
    test_category_points();
    return 0;
}
