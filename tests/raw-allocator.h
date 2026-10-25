/* Public-API allocator support for native test fixtures.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#ifndef TESTS_RAW_ALLOCATOR_H
#define TESTS_RAW_ALLOCATOR_H

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include "quickjs.h"

/* Record the requested size without using private or platform allocator APIs.
   The union keeps returned storage suitably aligned for any engine object. */
typedef union TestAllocationHeader {
    size_t size;
    max_align_t alignment;
} TestAllocationHeader;

static inline void *test_raw_malloc(JSMallocState *s, size_t size)
{
    TestAllocationHeader *header;
    size_t total;

    if (!size || size > SIZE_MAX - sizeof(*header))
        return NULL;
    total = size + sizeof(*header);
    if (s->malloc_size > s->malloc_limit ||
        total > s->malloc_limit - s->malloc_size)
        return NULL;
    header = malloc(total);
    if (!header)
        return NULL;
    header->size = size;
    s->malloc_count++;
    s->malloc_size += total;
    return header + 1;
}

static inline void test_raw_free(JSMallocState *s, void *ptr)
{
    if (ptr) {
        TestAllocationHeader *header = (TestAllocationHeader *)ptr - 1;
        size_t total = header->size + sizeof(*header);

        assert(s->malloc_count && s->malloc_size >= total);
        s->malloc_count--;
        s->malloc_size -= total;
        free(header);
    }
}

static inline void *test_raw_realloc(JSMallocState *s, void *ptr, size_t size)
{
    TestAllocationHeader *header;
    size_t retained;

    if (!ptr)
        return test_raw_malloc(s, size);
    if (!size) {
        test_raw_free(s, ptr);
        return NULL;
    }
    header = (TestAllocationHeader *)ptr - 1;
    retained = s->malloc_size - header->size;
    if (size > SIZE_MAX - sizeof(*header) || retained > s->malloc_limit ||
        size > s->malloc_limit - retained)
        return NULL;
    header = realloc(header, size + sizeof(*header));
    if (!header)
        return NULL;
    header->size = size;
    s->malloc_size = retained + size;
    return header + 1;
}

static inline size_t test_raw_usable_size(const void *ptr)
{
    return ptr ? ((const TestAllocationHeader *)ptr)[-1].size : 0;
}

#endif
