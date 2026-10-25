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
#include "../../src/intl/list/list.h"

static void test_list(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlListService *service;
    QJSIntlListFormatter *formatter;
    QJSIntlFormatted result;
    const QJSIntlBytes *available;
    size_t count, locale, length, i, j, calls, live;
    static const char *const tags[] = {"en", "es", "he"};
    static const uint16_t a[] = {'A'}, nul[] = {0, 0xd800}, vowel[] = {'i'};
    QJSIntlUTF16 items[] = {{a, 1}, {NULL, 0}, {nul, 2}, {NULL, 0}, {vowel, 1}};

    assert(!qjs_intl_list_open(allocator, NULL, &service));
    assert(!qjs_intl_list_available(service, &available, &count));
    for (i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
        locale = find(available, count, tags[i]);
        assert(!qjs_intl_list_formatter_open(service, locale, QJS_INTL_LIST_CONJUNCTION,
                                             QJS_INTL_LIST_LONG, &formatter));
        for (length = 0; length <= 5; length++) {
            size_t elements = 0;

            assert(!qjs_intl_list_format(formatter, items, length, &result));
            for (j = 0; j < result.part_count; j++) {
                const QJSIntlPart *part = &result.parts[j];

                if (part->type == QJS_INTL_PART_ELEMENT) {
                    assert(elements < length && part->end - part->start == items[elements].length);
                    if (items[elements].length)
                        assert(!memcmp(result.text + part->start, items[elements].data,
                                       items[elements].length * sizeof(uint16_t)));
                    elements++;
                }
            }
            assert(elements == length);
            qjs_intl_list_result_clear(allocator, &result);
        }
        /* Sweep formatting allocations with empty parts and counted lone
           surrogates present; failed calls publish no partial result. */
        live = state->live;
        calls = state->calls;
        assert(!qjs_intl_list_format(formatter, items, 5, &result));
        calls = state->calls - calls;
        qjs_intl_list_result_clear(allocator, &result);
        for (j = 1; j <= calls; j++) {
            state->fail_at = state->calls + j;
            assert(qjs_intl_list_format(formatter, items, 5, &result) == QJS_INTL_NO_MEMORY);
            assert(!result.text && !result.parts && !result.length && !result.part_count);
            assert(state->live == live);
        }
        state->fail_at = 0;
        qjs_intl_list_formatter_close(formatter);
    }
    locale = find(available, count, "en");
    assert(!qjs_intl_list_formatter_open(service, locale, QJS_INTL_LIST_CONJUNCTION,
                                         QJS_INTL_LIST_LONG, &formatter));
    qjs_intl_list_close(service);
    assert(!qjs_intl_list_format(formatter, items, 1, &result));
    qjs_intl_list_formatter_close(formatter);
    assert(result.length == 1 && result.text[0] == 'A');
    qjs_intl_list_result_clear(allocator, &result);
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);

    test_list(&allocator);
    assert(!state.live);
    return 0;
}
