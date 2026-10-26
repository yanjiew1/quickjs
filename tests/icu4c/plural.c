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
#include "../../src/intl/plural/plural.h"

static QJSIntlMathematicalValue number(const char *text)
{
    return (QJSIntlMathematicalValue){QJS_INTL_FINITE, {text, strlen(text)}};
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    QJSIntlPluralService *service;
    QJSIntlPluralFormatter *formatter;
    QJSIntlPluralOptions options = {0};
    QJSIntlPluralCategory category;
    QJSIntlMathematicalValue one = number("1"), negative = number("-1");
    QJSIntlMathematicalValue fraction = number("1.25"), nan = {QJS_INTL_NAN, {0}};
    const QJSIntlBytes *locales;
    size_t count, en, calls, live, i;
    unsigned categories;

    options.digits.minimum_integer_digits = 1;
    options.digits.maximum_fraction_digits = 3;
    options.digits.minimum_significant_digits = 1;
    options.digits.maximum_significant_digits = 21;
    options.digits.rounding_increment = 1;
    options.digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
    options.digits.maximum_output_length = 4096;
    assert(!qjs_intl_plural_open(&allocator, NULL, &service));
    assert(!qjs_intl_plural_available(service, &locales, &count));
    en = find(locales, count, "en");

    /* Resource copies are formatter-owned, including partial construction.
       Sweep each callback allocation without replacing ICU's global heap. */
    live = state.live;
    calls = state.calls;
    assert(!qjs_intl_plural_formatter_open(service, en, &options, &formatter));
    calls = state.calls - calls;
    qjs_intl_plural_formatter_close(formatter);
    for (i = 1; i <= calls; i++) {
        state.fail_at = state.calls + i;
        assert(qjs_intl_plural_formatter_open(service, en, &options, &formatter) ==
               QJS_INTL_NO_MEMORY);
        assert(!formatter && state.live == live);
    }
    state.fail_at = 0;
    {
        static const char *const tags[] = {"en-US", "pt", "pt-PT"};
        static const QJSIntlPluralCategory expected[] = {QJS_INTL_PLURAL_OTHER, QJS_INTL_PLURAL_ONE,
                                                         QJS_INTL_PLURAL_OTHER};
        QJSIntlMathematicalValue zero = number("0");

        /* en-US inherits; pt-PT has its own cardinal rule. Resource fallback
           must not flatten the latter to the language or host-default set. */
        for (i = 0; i < sizeof(tags) / sizeof(*tags); i++) {
            assert(!qjs_intl_plural_formatter_open(service, find(locales, count, tags[i]), &options,
                                                   &formatter));
            assert(!qjs_intl_plural_select(formatter, &zero, &category));
            assert(category == expected[i]);
            qjs_intl_plural_formatter_close(formatter);
        }
    }

    {
        QJSIntlMathematicalValue huge = number("100000000000000003");
        QJSIntlMathematicalValue late = number("1.0000000000000000001");

        // Do not lose a large integer residue or the nonzero nineteenth
        // fractional digit through the foreign library's bounded operands.
        options.digits.maximum_fraction_digits = 100;
        assert(!qjs_intl_plural_formatter_open(service, find(locales, count, "ar"), &options,
                                               &formatter));
        assert(!qjs_intl_plural_select(formatter, &huge, &category));
        assert(category == QJS_INTL_PLURAL_FEW);
        assert(!qjs_intl_plural_select(formatter, &late, &category));
        assert(category == QJS_INTL_PLURAL_OTHER);
        qjs_intl_plural_formatter_close(formatter);
        options.digits.maximum_fraction_digits = 3;
    }

    for (options.notation = QJS_INTL_PLURAL_STANDARD; options.notation <= QJS_INTL_PLURAL_COMPACT;
         options.notation++) {
        assert(!qjs_intl_plural_formatter_open(service, en, &options, &formatter));
        assert(!qjs_intl_plural_categories(formatter, &categories));
        assert(categories == ((1U << QJS_INTL_PLURAL_ONE) | (1U << QJS_INTL_PLURAL_OTHER)));
        assert(!qjs_intl_plural_select(formatter, &one, &category));
        assert(category == QJS_INTL_PLURAL_ONE);
        /* Range identity is unsigned and uses the common rounded text. */
        assert(!qjs_intl_plural_select_range(formatter, &negative, &one, &category));
        assert(category == QJS_INTL_PLURAL_ONE);
        qjs_intl_plural_formatter_close(formatter);
    }

    options.notation = QJS_INTL_PLURAL_STANDARD;
    assert(!qjs_intl_plural_formatter_open(service, en, &options, &formatter));
    qjs_intl_plural_close(service);
    assert(!qjs_intl_plural_select(formatter, &nan, &category));
    assert(category == QJS_INTL_PLURAL_OTHER);
    category = QJS_INTL_PLURAL_ONE;
    assert(qjs_intl_plural_select_range(formatter, &nan, &one, &category) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(category == QJS_INTL_PLURAL_OTHER);

    /* The formatter owns its locale/rules after factory teardown. Sweep only
       its engine-owned rounding allocations, not ICU's process-wide heap. */
    live = state.live;
    state.calls = 0;
    assert(!qjs_intl_plural_select(formatter, &fraction, &category));
    assert(category == QJS_INTL_PLURAL_OTHER);
    calls = state.calls;
    assert(calls && state.live == live);
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        category = QJS_INTL_PLURAL_ONE;
        assert(qjs_intl_plural_select(formatter, &fraction, &category) == QJS_INTL_NO_MEMORY);
        assert(category == QJS_INTL_PLURAL_OTHER && state.live == live);
    }
    state.fail_at = 0;
    qjs_intl_plural_formatter_close(formatter);
    assert(!state.live);
    return 0;
}
