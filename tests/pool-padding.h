/* Reach real host allocation failures even when engine pools have spare blocks.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT. */
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
