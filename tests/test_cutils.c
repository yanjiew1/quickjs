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

int main(void)
{
    test_empty_dbuf();
    return 0;
}
