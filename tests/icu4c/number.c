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
#include "../../src/intl/number/number.h"

static QJSIntlMathematicalValue decimal_value(const char *text)
{
    return (QJSIntlMathematicalValue){QJS_INTL_FINITE, {text, strlen(text)}};
}

static void test_number(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlNumberService *service;
    QJSIntlNumberFormatter *formatter;
    QJSIntlNumberOptions options = {0};
    QJSIntlFormatted result;
    QJSIntlFormattedRange range;
    QJSIntlMathematicalValue number = decimal_value("12345678901234567890");
    QJSIntlMathematicalValue negative = decimal_value("-1"), positive = decimal_value("1");
    const QJSIntlBytes *available;
    size_t count, en, i, j, calls, live;
    uint32_t digits;
    int found;

    assert(!qjs_intl_number_open(allocator, NULL, &service));
    assert(!qjs_intl_number_available(service, &available, &count));
    en = find(available, count, "en");
    assert(!qjs_intl_number_currency_digits(service, bytes("JPY"), &digits) && digits == 0);
    assert(!qjs_intl_number_currency_digits(service, bytes("ZZZ"), &digits) && digits == 2);
    options.digits = (QJSIntlDecimalOptions){1,
                                             0,
                                             3,
                                             1,
                                             21,
                                             1,
                                             QJS_INTL_ROUND_HALF_EXPAND,
                                             QJS_INTL_ROUND_FRACTION,
                                             QJS_INTL_TRAILING_ZERO_AUTO,
                                             1024};
    options.maximum_output_length = 1024;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    live = state->live;
    calls = state->calls;
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == 20 && result.part_count == 1 &&
           result.parts[0].type == QJS_INTL_PART_INTEGER);
    for (i = 0; i < result.length; i++)
        assert(result.text[i] == (unsigned char)number.decimal.data[i]);
    calls = state->calls - calls;
    qjs_intl_number_result_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_format(formatter, &number, &result) == QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.parts && !result.length && !result.part_count &&
               state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_number_formatter_close(formatter);

    options.grouping = QJS_INTL_NUMBER_GROUP_ALWAYS;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    number = decimal_value("1234.5");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    found = 0;
    for (i = 0; i < result.part_count; i++)
        found += result.parts[i].type == QJS_INTL_PART_GROUP;
    assert(found == 1);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);

    options.sign_display = QJS_INTL_NUMBER_SIGN_NEVER;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    assert(!qjs_intl_number_format_range(formatter, &negative, &positive, &range));
    found = 0;
    for (i = 0; i < range.part_count; i++) {
        assert(range.parts[i].source == QJS_INTL_RANGE_SHARED);
        found += range.parts[i].type == QJS_INTL_PART_APPROXIMATELY_SIGN;
    }
    assert(found == 1);
    qjs_intl_number_range_result_clear(allocator, &range);
    qjs_intl_number_formatter_close(formatter);

    options.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("arab"), &options, &formatter));
    number = decimal_value("1200");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    found = 0;
    for (i = 0; i < result.part_count; i++) {
        if (result.parts[i].type == QJS_INTL_PART_EXPONENT_INTEGER) {
            for (j = result.parts[i].start; j < result.parts[i].end; j++)
                assert(result.text[j] >= 0x660 && result.text[j] <= 0x669);
            found++;
        }
    }
    assert(found == 1);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);

    assert(!qjs_intl_number_formatter_open(service, en, bytes("mathbold"), &options, &formatter));
    number = decimal_value("1e125");
    calls = state->calls;
    live = state->live;
    assert(!qjs_intl_number_format(formatter, &number, &result));
    calls = state->calls - calls;
    found = 0;
    for (i = 0; i < result.part_count; i++) {
        if (result.parts[i].type == QJS_INTL_PART_EXPONENT_INTEGER) {
            static const uint16_t expected[] = {0xd835, 0xdfcf, 0xd835, 0xdfd0, 0xd835, 0xdfd3};

            assert(result.parts[i].end - result.parts[i].start ==
                   sizeof(expected) / sizeof(*expected));
            assert(!memcmp(result.text + result.parts[i].start, expected, sizeof(expected)));
            found++;
        }
    }
    assert(found == 1);
    qjs_intl_number_result_clear(allocator, &result);
    /* Includes the optional surrogate-repair copy allocation; corrected ICU
       releases naturally need one fewer callback without changing coverage. */
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_format(formatter, &number, &result) == QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.parts && state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_number_formatter_close(formatter);

    options.notation = QJS_INTL_NUMBER_STANDARD;
    options.digits.maximum_output_length = 4;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    number = decimal_value("1.23456789");
    assert(qjs_intl_number_format(formatter, &number, &result) == QJS_INTL_OVERFLOW);
    number = decimal_value("123.4");
    assert(qjs_intl_number_format(formatter, &number, &result) == QJS_INTL_OVERFLOW);
    number = decimal_value("1e1000000");
    assert(qjs_intl_number_format(formatter, &number, &result) == QJS_INTL_OVERFLOW);
    number = decimal_value("0e1000000000");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == 1 && result.text[0] == '0');
    qjs_intl_number_result_clear(allocator, &result);
    number = decimal_value("1234");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == 5); /* Groups do not consume ASCII numeric budget. */
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    options.digits.maximum_output_length = 5;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    number = decimal_value("1.23456789");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == 5); /* Bound the rounded result, not input digits. */
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    options.notation = QJS_INTL_NUMBER_STANDARD;
    options.style = QJS_INTL_NUMBER_UNIT;
    options.unit = bytes("meter-per-second");
    options.unit_display = QJS_INTL_NUMBER_UNIT_LONG;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    qjs_intl_number_close(service);
    assert(!qjs_intl_number_format(formatter, &positive, &result));
    found = 0;
    for (i = 0; i < result.part_count; i++)
        found += result.parts[i].type == QJS_INTL_PART_UNIT;
    assert(found);
    qjs_intl_number_formatter_close(formatter);
    qjs_intl_number_result_clear(allocator, &result);
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);

    test_number(&allocator);
    assert(!state.live);
    return 0;
}
