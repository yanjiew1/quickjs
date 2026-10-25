/* Reach real host allocation failures even when engine pools have spare blocks.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT.
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
#ifndef TESTS_POOL_PADDING_H
#define TESTS_POOL_PADDING_H
#include <string.h>
#include "quickjs.h"

/* Use only with a fresh test runtime whose previous memory limit is unlimited.
   The zero limit prevents new arenas while consuming existing free blocks. */
static void *test_pool_padding(JSRuntime *rt)
{
    void *padding = NULL, *ptr;
    size_t size;

    JS_SetMemoryLimit(rt, 0);
    for (size = sizeof(void *); size <= 512; size += sizeof(void *)) {
        while ((ptr = js_malloc_rt(rt, size)) != NULL) {
            memcpy(ptr, &padding, sizeof(padding));
            padding = ptr;
        }
    }
    JS_SetMemoryLimit(rt, (size_t)-1);
    return padding;
}

static void test_pool_padding_free(JSRuntime *rt, void *padding)
{
    while (padding) {
        void *next;

        memcpy(&next, padding, sizeof(next));
        js_free_rt(rt, padding);
        padding = next;
    }
}
#endif
