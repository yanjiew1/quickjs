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
#include "../icu4c/helpers.h"
#include "../../src/intl/relative/relative.h"
#include <math.h>

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    QJSIntlRelativeService *service;
    QJSIntlRelativeFormatter *formatter;
    QJSIntlRelativeOptions options = {QJS_INTL_RELATIVE_LONG, QJS_INTL_RELATIVE_ALWAYS, 2048};
    QJSIntlRelativeFormatted result;
    const QJSIntlBytes *available;
    QJSIntlBytes system;
    size_t count, en, ar, locale, i;
    int supported;

    assert(!qjs_intl_relative_open(&allocator, NULL, &service));
    assert(!qjs_intl_relative_available(service, &available, &count) && count > 2);
    en = find(available, count, "en");
    ar = find(available, count, "ar");
    for (locale = 0; locale < count; locale++) {
        assert(!qjs_intl_relative_default_system(service, locale, &system));
        assert(!qjs_intl_relative_system_supported(service, locale, system, &supported) &&
               supported);
    }
    assert(!qjs_intl_relative_system_available(service, bytes("mathbold"), &supported) &&
           supported);
    assert(!qjs_intl_relative_system_available(service, bytes("native"), &supported) && !supported);
    assert(!qjs_intl_relative_formatter_open(service, en, bytes("mathbold"), &options, &formatter));
    assert(!qjs_intl_relative_format(formatter, 2, QJS_INTL_RELATIVE_DAY, &result));
    assert(result.length == 10 && result.text[3] == 0xd835 && result.text[4] == 0xdfd0);
    assert(result.part_count == 3 && result.parts[1].start == 3 && result.parts[1].end == 5 &&
           result.parts[1].unit == QJS_INTL_RELATIVE_DAY);
    qjs_intl_relative_result_clear(&allocator, &result);
    assert(qjs_intl_relative_format(formatter, INFINITY, QJS_INTL_RELATIVE_DAY, &result) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !result.text && !result.parts);
    qjs_intl_relative_formatter_close(formatter);
    assert(!qjs_intl_relative_formatter_open(service, ar, bytes("arab"), &options, &formatter));
    assert(!qjs_intl_relative_format(formatter, 2, QJS_INTL_RELATIVE_DAY, &result));
    assert(result.part_count == 1 && result.parts[0].type == QJS_INTL_PART_LITERAL &&
           result.parts[0].unit == QJS_INTL_RELATIVE_UNIT_COUNT);
    qjs_intl_relative_result_clear(&allocator, &result);
    qjs_intl_relative_formatter_close(formatter);
    formatter = (QJSIntlRelativeFormatter *)(uintptr_t)1;
    assert(qjs_intl_relative_formatter_open(service, en, bytes("unknown"), &options, &formatter) ==
               QJS_INTL_UNSUPPORTED &&
           !formatter);
    /* Sample owner construction failures without allocating a full catalogue
       at every step. The shared relative contract tests every format buffer. */
    for (i = 1; i <= 4; i++) {
        size_t live = state.live;
        state.fail_at = state.calls + i;
        assert(qjs_intl_relative_formatter_open(service, en, bytes("latn"), &options, &formatter) ==
                   QJS_INTL_NO_MEMORY &&
               !formatter && state.live == live);
    }
    state.fail_at = 0;
    qjs_intl_relative_close(service);
    assert(!state.live);
    qjs_intl_relative_close(NULL);
    qjs_intl_relative_formatter_close(NULL);
    return 0;
}
