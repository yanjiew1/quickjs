/* Combine decoded tokens without reparsing inserted fields or quoted slots.
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
    size_t calls, live;
    int fail;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    state->calls++;
    if (state->fail)
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

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static void empty(const QJSIntlDatePattern *pattern)
{
    assert(!pattern->storage && !pattern->tokens && !pattern->count && !pattern->order);
}

int main(void)
{
    Allocation input = {0}, output = {0};
    QJSIntlAllocator a = {&input, allocate, NULL, release};
    QJSIntlAllocator b = {&output, allocate, NULL, release};
    QJSIntlDatePattern date, time, connector, pattern, copy;
    size_t calls;
    assert(!qjs_intl_date_pattern_compile(&a, bytes("yyyy 'd''ate' "), &date));
    assert(!qjs_intl_date_pattern_compile(&a, bytes("'t' HH:mm"), &time));
    assert(!qjs_intl_date_combination_compile(&a, bytes("{1}' / '{0}"), &connector));
    assert(!qjs_intl_date_pattern_combine(&b, &connector, &time, &date, &pattern));
    assert(output.calls == 1 && output.live == 1 && input.live == 3);
    assert(pattern.count == 5 && pattern.tokens[0].symbol == 'y' && pattern.tokens[0].width == 4);
    assert(!pattern.tokens[1].symbol && equal(pattern.tokens[1].literal, " d'ate  / t "));
    assert(pattern.tokens[2].symbol == 'H' && pattern.tokens[2].width == 2);
    assert(equal(pattern.tokens[3].literal, ":") && pattern.tokens[4].symbol == 'm');
    assert(!qjs_intl_date_pattern_copy(&b, &pattern, &copy));
    assert(copy.storage != pattern.storage && copy.tokens[1].literal.data != pattern.tokens[1].literal.data);
    qjs_intl_date_pattern_clear(&b, &pattern);
    qjs_intl_date_pattern_clear(&a, &date);
    qjs_intl_date_pattern_clear(&a, &time);
    qjs_intl_date_pattern_clear(&a, &connector);
    assert(!input.live && equal(copy.tokens[1].literal, " d'ate  / t "));
    qjs_intl_date_pattern_clear(&b, &copy);

    assert(!qjs_intl_date_pattern_compile(&a, bytes("'date'"), &date));
    assert(!qjs_intl_date_pattern_compile(&a, bytes("'time'"), &time));
    assert(!qjs_intl_date_combination_compile(&a, bytes("'{0}'{1}{0}'{1}'"), &connector));
    assert(!qjs_intl_date_pattern_combine(&b, &connector, &time, &date, &pattern));
    assert(pattern.count == 1 && equal(pattern.tokens[0].literal, "{0}datetime{1}"));
    qjs_intl_date_pattern_clear(&b, &pattern);
    assert(!qjs_intl_date_pattern_copy(&b, &connector, &copy));
    assert(copy.count == 4 && copy.tokens[1].symbol == '1' && copy.tokens[2].symbol == '0');
    qjs_intl_date_pattern_clear(&b, &copy);
    output.fail = 1;
    calls = output.calls;
    memset(&pattern, 0xff, sizeof(pattern));
    assert(qjs_intl_date_pattern_combine(&b, &connector, &time, &date, &pattern) == QJS_INTL_NO_MEMORY);
    empty(&pattern);
    assert(output.calls == calls + 1 && !output.live && input.live == 3);
    assert(qjs_intl_date_pattern_copy(&b, &date, &pattern) == QJS_INTL_NO_MEMORY);
    empty(&pattern);
    output.fail = 0;
    assert(!qjs_intl_date_pattern_combine(&b, &connector, &time, &date, &pattern));
    qjs_intl_date_pattern_clear(&b, &pattern);
    calls = output.calls;
    /* Reject an ordinary pattern as a connector, or a connector as a child,
       before allocating. Neither changes the input pattern owners. */
    assert(qjs_intl_date_pattern_combine(&b, &date, &time, &date, &pattern) == QJS_INTL_INVALID_ARGUMENT);
    empty(&pattern);
    assert(qjs_intl_date_pattern_combine(&b, &connector, &connector, &date, &pattern) == QJS_INTL_INVALID_ARGUMENT);
    empty(&pattern);
    assert(qjs_intl_date_pattern_combine(&b, &connector, NULL, &date, &pattern) == QJS_INTL_INVALID_ARGUMENT);
    empty(&pattern);
    assert(qjs_intl_date_pattern_combine(NULL, &connector, &time, &date, &pattern) == QJS_INTL_INVALID_ARGUMENT);
    empty(&pattern);
    assert(qjs_intl_date_pattern_combine(&b, &connector, &time, &date, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_pattern_copy(&b, NULL, &pattern) == QJS_INTL_INVALID_ARGUMENT);
    empty(&pattern);
    assert(output.calls == calls && input.live == 3 && !output.live);
    qjs_intl_date_pattern_clear(&a, &time);
    assert(!qjs_intl_date_interval_compile(&a, bytes("latestFirst:H-H"), &time));
    assert(qjs_intl_date_pattern_combine(&b, &connector, &time, &date, &pattern) == QJS_INTL_INVALID_ARGUMENT);
    empty(&pattern);
    assert(!qjs_intl_date_pattern_copy(&b, &time, &copy));
    assert(copy.order == QJS_INTL_DATE_INTERVAL_LATEST_FIRST && copy.count == 3);
    qjs_intl_date_pattern_clear(&b, &copy);
    qjs_intl_date_pattern_clear(&a, &time);
    qjs_intl_date_pattern_clear(&a, &date);
    qjs_intl_date_pattern_clear(&a, &connector);

    assert(!qjs_intl_date_pattern_compile(&a, bytes("r(U) LLLL EEEEEE"), &date));
    assert(!qjs_intl_date_pattern_compile(&a, bytes("K B"), &time));
    assert(!qjs_intl_date_combination_compile(&a, bytes("{0}\xe2\x80\xaf{1}"), &connector));
    assert(!qjs_intl_date_pattern_combine(&b, &connector, &time, &date, &pattern));
    assert(pattern.tokens[0].symbol == 'K' && pattern.tokens[2].symbol == 'B');
    assert(equal(pattern.tokens[3].literal, "\xe2\x80\xaf") && pattern.tokens[4].symbol == 'r');
    assert(pattern.tokens[6].symbol == 'U' && pattern.tokens[8].symbol == 'L');
    assert(pattern.tokens[10].symbol == 'E' && pattern.tokens[10].width == 6);
    qjs_intl_date_pattern_clear(&b, &pattern);
    qjs_intl_date_pattern_clear(&a, &date);
    qjs_intl_date_pattern_clear(&a, &time);
    qjs_intl_date_pattern_clear(&a, &connector);
    assert(!input.live && !output.live);
    return 0;
}
