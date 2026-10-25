/* ASCII source evidence must not normalize unrelated or quoted range text.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../src/intl/datetime/period-spacing.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void *allocate(void *opaque, size_t size)
{
    (void)opaque;
    return malloc(size);
}

static void release(void *opaque, void *ptr)
{
    (void)opaque;
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static void interval(const QJSIntlAllocator *allocator, const char *source, unsigned int proof,
                     unsigned int expected)
{
    QJSIntlDatePattern pattern;
    const char *dash = "\xe2\x80\x89\xe2\x80\x93\xe2\x80\x89";
    unsigned int spaces = 0;
    int dash_seen = 0;
    size_t i;
    assert(!qjs_intl_date_interval_compile(allocator, bytes(source), &pattern));
    qjs_intl_date_period_ascii_interval(&pattern, bytes(source), proof);
    for (i = 1; i < pattern.count; i++) {
        const QJSIntlDatePatternToken *gap = &pattern.tokens[i - 1];
        if (pattern.tokens[i].symbol == 'a' && gap->literal.length == 1 && gap->literal.data[0] == ' ')
            spaces++;
        if (!gap->symbol && gap->literal.length == strlen(dash) &&
            !memcmp(gap->literal.data, dash, strlen(dash)))
            dash_seen = 1;
    }
    assert(!strstr(source, dash) || dash_seen);
    assert(spaces == expected);
    qjs_intl_date_pattern_clear(allocator, &pattern);
}

int main(void)
{
    QJSIntlAllocator allocator = {NULL, allocate, NULL, release};
    QJSIntlDatePattern actual;
    unsigned int proof;
    const char *two = "h:mm\xe2\x80\xaf" "a\xe2\x80\x89\xe2\x80\x93\xe2\x80\x89h:mm\xe2\x80\xaf" "a";
    proof = qjs_intl_date_period_ascii_source(bytes("h:mm a"), bytes("ascii"));
    assert(proof == 2);
    assert(!qjs_intl_date_period_ascii_source(bytes("h:mm a"), bytes("")));
    assert(!qjs_intl_date_period_ascii_source(bytes("h:mm' 'a"), bytes("ascii")));
    assert(!qjs_intl_date_period_ascii_source(bytes("h:mm 'x' a"), bytes("ascii")));
    assert(!qjs_intl_date_period_ascii_source(bytes("h:mm'' a"), bytes("ascii")));
    assert(!qjs_intl_date_period_ascii_source(bytes("a h:mm"), bytes("ascii")));
    assert(!qjs_intl_date_period_ascii_source(bytes("h:mm B"), bytes("ascii")));
    assert(!qjs_intl_date_period_ascii_source(bytes("h:mm  a"), bytes("ascii")));
    assert(!qjs_intl_date_period_ascii_source(bytes("h:mm\xe2\x80\xaf" "a"), bytes("ascii")));
    assert(!qjs_intl_date_period_ascii_source(bytes("h:mm a'"), bytes("ascii")));
    assert(!qjs_intl_date_period_ascii_source((QJSIntlBytes){0}, bytes("ascii")));

    assert(!qjs_intl_date_pattern_compile(&allocator, bytes("h:mm:ss.SSS a"), &actual));
    assert(!qjs_intl_date_period_ascii_mask(&actual, 4));
    qjs_intl_date_pattern_clear(&allocator, &actual);
    assert(!qjs_intl_date_pattern_compile(&allocator, bytes("h:mm a"), &actual));
    assert(qjs_intl_date_period_ascii_mask(&actual, proof) == proof);
    assert(!qjs_intl_date_period_ascii_mask(&actual, 1));
    qjs_intl_date_pattern_clear(&allocator, &actual);

    interval(&allocator, two, proof, 2);
    interval(&allocator, two, 0, 0);
    interval(&allocator, two, 4, 0);
    interval(&allocator, "latestFirst:h:mm-h:mm\xe2\x80\xaf" "a", proof, 1);
    interval(&allocator, "h:mm-h:mm\xe2\x80\x89" "a", proof, 1);
    interval(&allocator, "h:mm-h:mm'\xe2\x80\xaf'a", proof, 0);
    interval(&allocator, "h:mm-h:mm' ' '\xe2\x80\xaf'a", proof, 0);
    interval(&allocator, "h:mm-h:mm'quoted a'\xe2\x80\xaf" "a", proof, 0);
    interval(&allocator, "h:mm-h:mm\xe2\x80\xaf\xe2\x80\xaf" "a", proof, 0);
    return 0;
}
