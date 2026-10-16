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

int main(void)
{
    test_empty_dbuf();
    test_formatted_buffer();
    return 0;
}
