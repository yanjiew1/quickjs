/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "helpers.h"

static void *allocate(void *opaque, size_t length)
{
    Allocation *state = opaque;
    void *ptr;

    assert(length);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(length);
    state->live += ptr != NULL;
    return ptr;
}

static void *resize(void *opaque, void *ptr, size_t length)
{
    Allocation *state = opaque;
    int had_ptr = ptr != NULL;
    void *next;

    assert(length);
    if (++state->calls == state->fail_at)
        return NULL;
    next = realloc(ptr, length);
    if (next)
        state->live += !had_ptr;
    return next;
}

void icu4c_test_free(void *opaque, void *ptr)
{
    Allocation *state = opaque;

    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

QJSIntlAllocator icu4c_test_allocator(Allocation *state)
{
    return (QJSIntlAllocator){state, allocate, resize, icu4c_test_free};
}
