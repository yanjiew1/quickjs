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
#include <stdio.h>
#include "../../src/intl/relative/relative.h"

static void relative_text(const QJSIntlAllocator *allocator, QJSIntlRelativeFormatter *formatter,
                          double number, const char *expected)
{
    QJSIntlRelativeFormatted result;
    size_t i;

    assert(!qjs_intl_relative_format(formatter, number, QJS_INTL_RELATIVE_DAY, &result));
    assert(result.length == strlen(expected));
    for (i = 0; i < result.length; i++) {
        if (result.text[i] != (unsigned char)expected[i])
            fprintf(stderr, "Relative %.17g: offset %zu expected U+%04X, got U+%04X\n", number, i,
                    (unsigned char)expected[i], result.text[i]);
        assert(result.text[i] == (unsigned char)expected[i]);
    }
    qjs_intl_relative_result_clear(allocator, &result);
}

static void test_relative(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlRelativeService *service;
    QJSIntlRelativeFormatter *formatter;
    QJSIntlRelativeOptions options = {QJS_INTL_RELATIVE_LONG, QJS_INTL_RELATIVE_ALWAYS, 2048};
    QJSIntlRelativeFormatted result;
    const QJSIntlBytes *available;
    size_t count, en, calls, i, live;

    assert(!qjs_intl_relative_open(allocator, NULL, &service));
    assert(!qjs_intl_relative_available(service, &available, &count));
    en = find(available, count, "en");
    assert(!qjs_intl_relative_formatter_open(service, en, bytes("latn"), &options, &formatter));
    relative_text(allocator, formatter, -0.0, "0 days ago");
    relative_text(allocator, formatter, 0.0, "in 0 days");
    relative_text(allocator, formatter, 1.0005, "in 1 day");
    relative_text(allocator, formatter, 1000000000000000128.0, "in 1,000,000,000,000,000,128 days");
    calls = state->calls;
    live = state->live;
    assert(!qjs_intl_relative_format(formatter, 1.25, QJS_INTL_RELATIVE_DAY, &result));
    calls = state->calls - calls;
    qjs_intl_relative_result_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_relative_format(formatter, 1.25, QJS_INTL_RELATIVE_DAY, &result) ==
               QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.parts && !result.length && !result.part_count &&
               state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_relative_formatter_close(formatter);
    options.maximum_output_length = 50;
    assert(!qjs_intl_relative_formatter_open(service, en, bytes("latn"), &options, &formatter));
    relative_text(allocator, formatter, 0.1, "in 0.1 days");
    qjs_intl_relative_formatter_close(formatter);
    options.maximum_output_length = 2048;
    options.numeric = QJS_INTL_RELATIVE_AUTO;
    assert(!qjs_intl_relative_formatter_open(service, en, bytes("latn"), &options, &formatter));
    /* Root width aliases must not replace localized long-style fallback. */
    {
        QJSIntlRelativeFormatter *localized;
        QJSIntlRelativeOptions local_options = {QJS_INTL_RELATIVE_SHORT, QJS_INTL_RELATIVE_ALWAYS,
                                                2048};
        size_t af = find(available, count, "af"), gd = find(available, count, "gd");

        assert(!qjs_intl_relative_formatter_open(service, af, bytes("latn"), &local_options,
                                                 &localized));
        relative_text(allocator, localized, 4, "oor 4 dae");
        qjs_intl_relative_formatter_close(localized);
        local_options.numeric = QJS_INTL_RELATIVE_AUTO;
        assert(!qjs_intl_relative_formatter_open(service, af, bytes("latn"), &local_options,
                                                 &localized));
        assert(!qjs_intl_relative_format(localized, 1, QJS_INTL_RELATIVE_DAY, &result));
        assert(result.length == 4 && result.text[0] == 'm' && result.text[1] == 0xf4);
        qjs_intl_relative_result_clear(allocator, &result);
        qjs_intl_relative_formatter_close(localized);
        assert(!qjs_intl_relative_formatter_open(service, gd, bytes("latn"), &local_options,
                                                 &localized));
        /* CLDR's +3 literal is outside ICU's convenience API's -2..2 window. */
        relative_text(allocator, localized, 3, "an-eararais");
        qjs_intl_relative_formatter_close(localized);
    }
    qjs_intl_relative_close(service);
    relative_text(allocator, formatter, -0.0, "today");
    relative_text(allocator, formatter, 0.0, "today");
    relative_text(allocator, formatter, 1, "tomorrow");
    relative_text(allocator, formatter, 1.0001, "in 1 day");
    relative_text(allocator, formatter, 0.00001, "in 0 days");
    qjs_intl_relative_formatter_close(formatter);
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);

    test_relative(&allocator);
    assert(!state.live);
    return 0;
}
