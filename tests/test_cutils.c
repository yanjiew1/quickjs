/*
 * Dynamic buffer tests
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
#include <string.h>

#include "cutils.h"

static void test_empty_dbuf(void)
{
    DynBuf b;
    dbuf_init(&b);
    assert(dbuf_put(&b, NULL, 0) == 0);
    assert(b.buf == NULL && b.size == 0 && b.allocated_size == 0);
    assert(!dbuf_error(&b));
    assert(dbuf_putstr(&b, "abc") == 0);
    assert(dbuf_put(&b, NULL, 0) == 0);
    assert(b.size == 3 && memcmp(b.buf, "abc", 3) == 0);
    dbuf_free(&b);
}

static void *reject_printf_allocation(void *opaque, void *ptr, size_t size)
{
    size_t *requested = opaque;
    assert(!ptr);
    *requested = size;
    return NULL;
}

static void test_formatted_buffer(void)
{
    DynBuf b;
    char text[257];
    size_t requested = 0;

    memset(text, 'x', 256);
    text[256] = '\0';
    dbuf_init(&b);
    assert(dbuf_printf(&b, "%s:%d", text, 7) == 0);
    assert(b.size == 258 && !memcmp(b.buf, text, 256));
    assert(b.buf[256] == ':' && b.buf[257] == '7');
    dbuf_free(&b);

    dbuf_init2(&b, &requested, reject_printf_allocation);
    assert(dbuf_printf(&b, "%s", text) < 0);
    assert(dbuf_error(&b) && !b.buf && b.size == 0);
    assert(requested >= 257);
    dbuf_free(&b);
}

static void test_bounded_strings(void)
{
    char buffer[5] = "old";
    const char *tail = "untouched";
    const char *source = "prefix-value";

    pstrcpy(buffer, 0, "ignored");
    pstrcpy(buffer, -1, "ignored");
    assert(!strcmp(buffer, "old"));
    pstrcpy(buffer, sizeof(buffer), "abcdef");
    assert(!strcmp(buffer, "abcd"));
    pstrcpy(buffer, sizeof(buffer), "a");
    assert(pstrcat(buffer, sizeof(buffer), "bcdef") == buffer);
    assert(!strcmp(buffer, "abcd"));
    assert(strstart(source, "prefix-", &tail) && !strcmp(tail, "value"));
    tail = source;
    assert(!strstart(source, "wrong", &tail) && tail == source);
    assert(strstart(source, "", NULL));
    assert(has_suffix(source, "value") && has_suffix(source, ""));
    assert(!has_suffix(source, "longer-than-the-input"));
}

typedef struct MovingAllocator {
    size_t size;
    int fail_next, moves, frees;
} MovingAllocator;

static void *moving_realloc(void *opaque, void *ptr, size_t size)
{
    MovingAllocator *a = opaque;
    void *next;

    if (!size) {
        a->frees++;
        free(ptr);
        a->size = 0;
        return NULL;
    }
    if (a->fail_next) {
        a->fail_next = 0;
        return NULL;
    }
    next = malloc(size);
    assert(next);
    if (ptr) {
        memcpy(next, ptr, a->size < size ? a->size : size);
        free(ptr);
        a->moves++;
    }
    a->size = size;
    return next;
}

static void test_typed_buffer_growth(void)
{
    MovingAllocator a = { 0 };
    DynBuf b;
    size_t saved_size;
    uint8_t *saved_buf;

    dbuf_init2(&b, &a, moving_realloc);
    assert(dbuf_putc(&b, 0x55) == 0);
    assert(dbuf_put_u16(&b, 0x1234) == 0);
    assert(dbuf_put_u32(&b, 0x89abcdef) == 0);
    assert(__dbuf_put_u64(&b, UINT64_C(0x123456789abcdef0)) == 0);
    assert(b.size == 15 && b.buf[0] == 0x55);
    assert(get_u16(b.buf + 1) == 0x1234);
    assert(get_u32(b.buf + 3) == 0x89abcdef);
    assert(get_u64(b.buf + 7) == UINT64_C(0x123456789abcdef0));
    assert(dbuf_put_self(&b, 1, 6) == 0);
    assert(b.size == 21 && !memcmp(b.buf + 15, b.buf + 1, 6));
    assert(a.moves > 0);
    saved_size = b.size;
    saved_buf = b.buf;
    a.fail_next = 1;
    assert(dbuf_claim(&b, b.allocated_size + 1) < 0);
    assert(dbuf_error(&b) && b.size == saved_size && b.buf == saved_buf);
    dbuf_free(&b);
    assert(a.frees == 1 && !a.size);
    assert(!b.buf && !b.size && !b.allocated_size && !dbuf_error(&b));
    dbuf_free(&b);
    assert(a.frees == 1);
}

int main(void)
{
    test_empty_dbuf();
    test_formatted_buffer();
    test_bounded_strings();
    test_typed_buffer_growth();
    return 0;
}
