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

int main(void)
{
    test_empty_range();
    test_case_folding();
    return 0;
}
