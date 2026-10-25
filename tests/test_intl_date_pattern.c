/* LDML source syntax, owned literals and bounded allocation failure coverage.
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
#include "../src/intl/datetime/pattern.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, live, fail_at, size;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    state->calls++;
    state->size = size;
    if (state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        state->live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) && (!text.length || !memcmp(text.data, expected, text.length));
}

static int empty(const QJSIntlDatePattern *pattern)
{
    return !pattern->storage && !pattern->tokens && !pattern->count && pattern->order == QJS_INTL_DATE_INTERVAL_DEFAULT;
}

static void rejected(const QJSIntlAllocator *allocator, QJSIntlBytes text)
{
    QJSIntlDatePattern pattern;
    Allocation *state = allocator->opaque;
    size_t calls = state->calls;
    memset(&pattern, 0xff, sizeof(pattern));
    assert(qjs_intl_date_pattern_compile(allocator, text, &pattern) == QJS_INTL_DATA_ERROR);
    assert(empty(&pattern) && !state->live && state->calls == calls);
}

int main(void)
{
    static const char symbols[] = "GyYuUrQqMLlwWdDFgEecabBhHKkmsSAzZOvVXx";
    static const char *const invalid[] = {"'", "yyyy 'oops", "j", "J", "C", "P", "i",
        "\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "\x80", "\xff"};
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlDatePattern pattern;
    size_t i;
    char source[] = "hh 'o''clock' a";
    assert(!qjs_intl_date_pattern_compile(&allocator, bytes(source), &pattern));
    assert(pattern.count == 3 && state.calls == 1 && state.live == 1);
    assert(pattern.tokens[0].symbol == 'h' && pattern.tokens[0].width == 2 && !pattern.tokens[0].literal.length);
    assert(!pattern.tokens[1].symbol && !pattern.tokens[1].width && equal(pattern.tokens[1].literal, " o'clock "));
    assert(pattern.tokens[2].symbol == 'a' && pattern.tokens[2].width == 1);
    memset(source, 'x', sizeof(source) - 1);
    assert(equal(pattern.tokens[1].literal, " o'clock ")); /* Owned independently of source. */
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    assert(empty(&pattern) && !state.live);
    for (i = 0; i < sizeof(symbols) - 1; i++) {
        char field[18];
        memset(field, symbols[i], sizeof(field));
        assert(!qjs_intl_date_pattern_compile(&allocator, (QJSIntlBytes){field, sizeof(field)}, &pattern));
        assert(pattern.count == 1 && pattern.tokens[0].symbol == (unsigned char)symbols[i]);
        assert(pattern.tokens[0].width == sizeof(field) && !pattern.tokens[0].literal.data);
        qjs_intl_date_pattern_clear(&allocator, &pattern);
    }
    assert(!qjs_intl_date_pattern_compile(&allocator, bytes("rU hHkK a b B M L EEEEEE"), &pattern));
    assert(pattern.tokens[0].symbol == 'r' && pattern.tokens[1].symbol == 'U');
    assert(pattern.tokens[pattern.count - 1].symbol == 'E' && pattern.tokens[pattern.count - 1].width == 6);
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    assert(!qjs_intl_date_pattern_compile(&allocator, bytes("'' 'x''y' '{0}' \xf0\x9f\x98\x80\xe2\x80\xaf\xe2\x80\x8f"), &pattern));
    assert(pattern.count == 1 && equal(pattern.tokens[0].literal, "' x'y {0} \xf0\x9f\x98\x80\xe2\x80\xaf\xe2\x80\x8f"));
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    assert(!qjs_intl_date_pattern_compile(&allocator, bytes("{0} 'X' yyyy"), &pattern));
    assert(pattern.count == 2 && equal(pattern.tokens[0].literal, "{0} X ") && pattern.tokens[1].symbol == 'y');
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    assert(!qjs_intl_date_combination_compile(&allocator, bytes("{1} 'at' {0} '{0}'"), &pattern));
    assert(pattern.count == 4 && pattern.tokens[0].symbol == '1' && !pattern.tokens[0].width);
    assert(equal(pattern.tokens[1].literal, " at ") && pattern.tokens[2].symbol == '0');
    assert(equal(pattern.tokens[3].literal, " {0}"));
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    assert(!qjs_intl_date_combination_compile(&allocator, bytes("{2}{0}{1}"), &pattern));
    assert(pattern.count == 3 && equal(pattern.tokens[0].literal, "{2}"));
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    assert(qjs_intl_date_combination_compile(&allocator, bytes("{0}{1}{0}"), &pattern) == QJS_INTL_DATA_ERROR && empty(&pattern));
    assert(qjs_intl_date_combination_compile(&allocator, bytes("{0}'{1}'"), &pattern) == QJS_INTL_DATA_ERROR && empty(&pattern));
    assert(!qjs_intl_date_interval_compile(&allocator, bytes("latestFirst:M/d \xe2\x80\x93 M/d"), &pattern));
    assert(pattern.order == QJS_INTL_DATE_INTERVAL_LATEST_FIRST && pattern.count == 7);
    assert(pattern.tokens[0].symbol == 'M' && pattern.tokens[4].symbol == 'M');
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    assert(!qjs_intl_date_interval_compile(&allocator, bytes("earliestFirst:d\xe2\x80\x93" "d"), &pattern));
    assert(pattern.order == QJS_INTL_DATE_INTERVAL_EARLIEST_FIRST);
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    assert(!qjs_intl_date_interval_compile(&allocator, bytes("d\xe2\x80\x93" "d"), &pattern));
    assert(pattern.order == QJS_INTL_DATE_INTERVAL_DEFAULT);
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    assert(qjs_intl_date_interval_compile(&allocator, bytes("earliestFirst:"), &pattern) == QJS_INTL_DATA_ERROR && empty(&pattern));
    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
        rejected(&allocator, bytes(invalid[i]));
    rejected(&allocator, (QJSIntlBytes){"y\0M", 3});
    state.calls = 0;
    state.fail_at = 1;
    memset(&pattern, 0xff, sizeof(pattern));
    assert(qjs_intl_date_pattern_compile(&allocator, bytes("yyyy-MM-dd"), &pattern) == QJS_INTL_NO_MEMORY);
    assert(empty(&pattern) && state.calls == 1 && !state.live);
    state.fail_at = 0;
    assert(!qjs_intl_date_pattern_compile(&allocator, bytes("yyyy-MM-dd"), &pattern));
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    {
        char literal[4096];
        memset(literal, '_', sizeof(literal));
        assert(!qjs_intl_date_pattern_compile(&allocator, (QJSIntlBytes){literal, sizeof(literal)}, &pattern));
        assert(pattern.count == 1 && pattern.tokens[0].literal.length == sizeof(literal));
        assert(state.size == sizeof(literal) + sizeof(*pattern.tokens));
        qjs_intl_date_pattern_clear(&allocator, &pattern);
    }
    assert(qjs_intl_date_pattern_compile(NULL, bytes("y"), &pattern) == QJS_INTL_INVALID_ARGUMENT && empty(&pattern));
    assert(qjs_intl_date_pattern_compile(&allocator, (QJSIntlBytes){NULL, 1}, &pattern) == QJS_INTL_INVALID_ARGUMENT && empty(&pattern));
    assert(qjs_intl_date_pattern_compile(&allocator, bytes(""), &pattern) == QJS_INTL_INVALID_ARGUMENT && empty(&pattern));
    assert(qjs_intl_date_pattern_compile(&allocator, bytes("y"), NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!state.live);
    return 0;
}
