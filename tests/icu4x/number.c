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
#include "../../src/intl/number/number.h"

static QJSIntlMathematicalValue value(const char *text)
{
    return (QJSIntlMathematicalValue){QJS_INTL_FINITE, {text, strlen(text)}};
}
static void ascii(const QJSIntlNumberFormatter *formatter, const QJSIntlAllocator *allocator,
                  const char *input, const char *expected)
{
    QJSIntlFormatted result;
    QJSIntlMathematicalValue number = value(input);
    size_t i, end = 0;
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == strlen(expected));
    for (i = 0; i < result.length; i++)
        assert(result.text[i] == (unsigned char)expected[i]);
    for (i = 0; i < result.part_count; i++) {
        assert(result.parts[i].start == end);
        end = result.parts[i].end;
    }
    assert(end == result.length);
    qjs_intl_number_result_clear(allocator, &result);
}
/* Table 30 mandates these repertoires even though the selected public bake
   has no digit payload for them. Check actual glyphs, not just admission. */
static void missing_digits(const QJSIntlNumberService *service, size_t locale,
                           const QJSIntlNumberOptions *initial, const QJSIntlAllocator *allocator,
                           const char *system, uint32_t zero)
{
    QJSIntlNumberOptions options = *initial;
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result;
    QJSIntlMathematicalValue number = value("123456789");
    size_t i;
    options.digits.minimum_integer_digits = 10;
    assert(!qjs_intl_number_formatter_open(service, locale, bytes(system), &options, &formatter));
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == 20 && result.part_count == 1);
    for (i = 0; i < 10; i++) {
        uint32_t c = zero + i - 0x10000;
        assert(result.text[2 * i] == 0xd800 + (c >> 10));
        assert(result.text[2 * i + 1] == 0xdc00 + (c & 1023));
    }
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
}

static void missing_separators(const QJSIntlNumberService *service, size_t locale,
                               const QJSIntlNumberOptions *initial,
                               const QJSIntlAllocator *allocator, uint16_t decimal, uint16_t group)
{
    QJSIntlNumberOptions options = *initial;
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result;
    QJSIntlMathematicalValue number = value("1234.5");
    size_t i;
    unsigned found_decimal = 0, found_group = 0;
    options.grouping = QJS_INTL_NUMBER_GROUP_ALWAYS;
    assert(
        !qjs_intl_number_formatter_open(service, locale, bytes("mathbold"), &options, &formatter));
    assert(!qjs_intl_number_format(formatter, &number, &result));
    for (i = 0; i < result.part_count; i++) {
        const QJSIntlPart *part = &result.parts[i];
        if (part->type == QJS_INTL_PART_DECIMAL) {
            assert(part->end == part->start + 1 && result.text[part->start] == decimal);
            found_decimal++;
        }
        if (part->type == QJS_INTL_PART_GROUP) {
            assert(part->end == part->start + 1 && result.text[part->start] == group);
            found_group++;
        }
    }
    assert(found_decimal == 1 && found_group == 1);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
}

static void currencies(const QJSIntlNumberService *service, size_t en, size_t pt,
                       const QJSIntlNumberOptions *initial, const QJSIntlAllocator *allocator,
                       Allocation *state)
{
    QJSIntlNumberOptions options = *initial;
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result;
    QJSIntlMathematicalValue number = value("1234.5");
    size_t calls, live, i;

    options.style = QJS_INTL_NUMBER_CURRENCY;
    options.currency = bytes("USD");
    options.digits.minimum_fraction_digits = options.digits.maximum_fraction_digits = 2;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "12.5", "$12.50");
    ascii(formatter, allocator, "-0", "-$0.00");
    qjs_intl_number_formatter_close(formatter);
    options.currency_sign = QJS_INTL_NUMBER_CURRENCY_ACCOUNTING;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "-12.5", "($12.50)");
    qjs_intl_number_formatter_close(formatter);
    options.currency_sign = QJS_INTL_NUMBER_CURRENCY_STANDARD;
    options.currency = bytes("CAD");
    options.currency_display = QJS_INTL_NUMBER_CURRENCY_NARROW;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "12.5", "$12.50");
    qjs_intl_number_formatter_close(formatter);
    options.currency_display = QJS_INTL_NUMBER_CURRENCY_CODE;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == 11 && result.text[3] == 0xa0);
    assert(result.parts[0].type == QJS_INTL_PART_CURRENCY && result.parts[0].end == 3);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);

    /* An explicit empty currency symbol is not a missing symbol. Its
       noCurrency pattern still uses the currency-specific decimal override. */
    options.currency = bytes("PTE");
    options.currency_display = QJS_INTL_NUMBER_CURRENCY_SYMBOL;
    assert(!qjs_intl_number_formatter_open(service, pt, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "12.5", "12$50");
    qjs_intl_number_formatter_close(formatter);
    assert(!qjs_intl_number_formatter_open(service, pt, bytes("mathbold"), &options, &formatter));
    assert(!qjs_intl_number_format(formatter, &number, &result));
    for (i = 0; i < result.part_count; i++) {
        if (result.parts[i].type == QJS_INTL_PART_DECIMAL)
            assert(result.text[result.parts[i].start] == '$');
        assert(result.parts[i].type != QJS_INTL_PART_CURRENCY);
    }
    assert(result.text[0] == 0xd835 && result.text[1] == 0xdfcf);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);

    options.currency = bytes("ZZZ");
    options.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    options.sign_display = QJS_INTL_NUMBER_SIGN_ALWAYS;
    calls = state->calls;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    calls = state->calls - calls;
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.text[0] == '+' && result.text[1] == 'Z' && result.text[4] == 0xa0);
    assert(result.parts[result.part_count - 1].type == QJS_INTL_PART_EXPONENT_INTEGER);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    live = state->live;
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter) ==
               QJS_INTL_NO_MEMORY);
        assert(!formatter && state->live == live);
    }
    state->fail_at = 0;
}

static void currency_names(const QJSIntlNumberService *service, size_t en, size_t ru,
                           const QJSIntlNumberOptions *initial, const QJSIntlAllocator *allocator,
                           Allocation *state)
{
    QJSIntlNumberOptions options = *initial;
    QJSIntlNumberFormatter *formatter;
    QJSIntlMathematicalValue number = value("100000000000000001");
    QJSIntlFormatted result;
    static const uint16_t russian_one[] = u"доллар США";
    size_t i, calls, format_calls, live;
    unsigned found;

    options.style = QJS_INTL_NUMBER_CURRENCY;
    options.currency = bytes("USD");
    options.currency_display = QJS_INTL_NUMBER_CURRENCY_NAME;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "1", "1 US dollar");
    ascii(formatter, allocator, "2", "2 US dollars");
    ascii(formatter, allocator, "-0", "-0 US dollars");
    number = (QJSIntlMathematicalValue){QJS_INTL_NAN, {0}};
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.parts[0].type == QJS_INTL_PART_NAN);
    assert(result.parts[result.part_count - 1].type == QJS_INTL_PART_CURRENCY);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    options.digits.minimum_fraction_digits = options.digits.maximum_fraction_digits = 2;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "1", "1.00 US dollars");
    qjs_intl_number_formatter_close(formatter);
    options.notation = QJS_INTL_NUMBER_ENGINEERING;
    options.digits.minimum_fraction_digits = 0;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "1000", "1E3 US dollar");
    qjs_intl_number_formatter_close(formatter);
    options.notation = QJS_INTL_NUMBER_STANDARD;
    options.currency = bytes("ZZZ");
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "1", "1 ZZZ");
    qjs_intl_number_formatter_close(formatter);

    options.currency = bytes("USD");
    number = value("100000000000000001");
    calls = state->calls;
    assert(!qjs_intl_number_formatter_open(service, ru, bytes("latn"), &options, &formatter));
    calls = state->calls - calls;
    /* Rounding through a double would erase the final digit and incorrectly
       select many. Verify the actual localized currency part, not admission. */
    format_calls = state->calls;
    assert(!qjs_intl_number_format(formatter, &number, &result));
    format_calls = state->calls - format_calls;
    found = 0;
    for (i = 0; i < result.part_count; i++) {
        if (result.parts[i].type == QJS_INTL_PART_CURRENCY) {
            assert(result.parts[i].end - result.parts[i].start == sizeof(russian_one) / 2 - 1);
            assert(
                !memcmp(result.text + result.parts[i].start, russian_one, sizeof(russian_one) - 2));
            found++;
        }
    }
    assert(found == 1);
    qjs_intl_number_result_clear(allocator, &result);
    live = state->live;
    for (i = 1; i <= format_calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_format(formatter, &number, &result) == QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.parts && state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_number_formatter_close(formatter);
    live = state->live;
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_formatter_open(service, ru, bytes("latn"), &options, &formatter) ==
               QJS_INTL_NO_MEMORY);
        assert(!formatter && state->live == live);
    }
    state->fail_at = 0;
    options.maximum_output_length = 5;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    number = value("1");
    live = state->live;
    assert(qjs_intl_number_format(formatter, &number, &result) == QJS_INTL_OVERFLOW);
    assert(!result.text && !result.parts && state->live == live);
    qjs_intl_number_formatter_close(formatter);
}

static void compact_numbers(const QJSIntlNumberService *service, size_t en, size_t fr, size_t ja,
                            const QJSIntlNumberOptions *initial, const QJSIntlAllocator *allocator,
                            Allocation *state)
{
    QJSIntlNumberOptions options = *initial;
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result;
    QJSIntlMathematicalValue number = value("1234");
    size_t calls, format_calls, live, i;

    options.notation = QJS_INTL_NUMBER_COMPACT;
    options.digits.maximum_fraction_digits = 2;
    calls = state->calls;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    calls = state->calls - calls;
    ascii(formatter, allocator, "1234", "1.23K");
    ascii(formatter, allocator, "-1234", "-1.23K");
    ascii(formatter, allocator, "12", "12");
    for (i = 0; i < 2; i++) {
        number = (QJSIntlMathematicalValue){i ? QJS_INTL_POSITIVE_INFINITY : QJS_INTL_NAN, {0}};
        assert(!qjs_intl_number_format(formatter, &number, &result));
        assert(result.part_count == 1);
        assert(result.parts[0].type == (i ? QJS_INTL_PART_INFINITY : QJS_INTL_PART_NAN));
        qjs_intl_number_result_clear(allocator, &result);
    }
    number = value("1234");
    format_calls = state->calls;
    assert(!qjs_intl_number_format(formatter, &number, &result));
    format_calls = state->calls - format_calls;
    assert(result.parts[result.part_count - 1].type == QJS_INTL_PART_COMPACT);
    qjs_intl_number_result_clear(allocator, &result);
    live = state->live;
    for (i = 1; i <= format_calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_format(formatter, &number, &result) == QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.parts && state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_number_formatter_close(formatter);
    live = state->live;
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter) ==
               QJS_INTL_NO_MEMORY);
        assert(!formatter && state->live == live);
    }
    state->fail_at = 0;
    options.compact_long = 1;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "1234", "1.23 thousand");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.parts[result.part_count - 2].type == QJS_INTL_PART_LITERAL);
    assert(result.parts[result.part_count - 1].type == QJS_INTL_PART_COMPACT);
    assert(result.parts[result.part_count - 1].end - result.parts[result.part_count - 1].start ==
           8);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    options.compact_long = 0;
    options.digits.maximum_fraction_digits = 0;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "999.5", "1K");
    qjs_intl_number_formatter_close(formatter);
    options.digits.maximum_fraction_digits = 2;
    options.style = QJS_INTL_NUMBER_PERCENT;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "12.34", "1.23K%");
    qjs_intl_number_formatter_close(formatter);
    options.style = QJS_INTL_NUMBER_CURRENCY;
    options.currency = bytes("USD");
    options.currency_display = QJS_INTL_NUMBER_CURRENCY_NAME;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "1000", "1K US dollars");
    qjs_intl_number_formatter_close(formatter);
    options.style = QJS_INTL_NUMBER_DECIMAL;
    options.compact_long = 1;
    options.digits.minimum_fraction_digits = 1;
    assert(!qjs_intl_number_formatter_open(service, fr, bytes("latn"), &options, &formatter));
    /* Explicit mathematical one wins even with a visible fraction digit. A
       literal-only compact pattern deliberately omits every numeric part. */
    ascii(formatter, allocator, "1000", "mille");
    number = value("1000");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.part_count == 1 && result.parts[0].type == QJS_INTL_PART_COMPACT);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    options.compact_long = 0;
    options.digits.minimum_fraction_digits = 0;
    assert(!qjs_intl_number_formatter_open(service, ja, bytes("latn"), &options, &formatter));
    number = value("10000");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == 2 && result.text[0] == '1' && result.text[1] == 0x4e07);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    assert(!qjs_intl_number_formatter_open(service, en, bytes("mathbold"), &options, &formatter));
    number = value("1000");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == 3 && result.text[0] == 0xd835 && result.text[1] == 0xdfcf &&
           result.text[2] == 'K');
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
}

static void compact_currencies(const QJSIntlNumberService *service, size_t en, size_t fr, size_t pt,
                               size_t zh, const QJSIntlNumberOptions *initial,
                               const QJSIntlAllocator *allocator, Allocation *state)
{
    QJSIntlNumberOptions options = *initial;
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result;
    QJSIntlMathematicalValue number = value("1234");
    static const uint16_t french[] = u"1,23\u00a0k\u00a0€";
    size_t i, calls, format_calls, live;
    unsigned currencies;

    options.style = QJS_INTL_NUMBER_CURRENCY;
    options.currency = bytes("USD");
    options.notation = QJS_INTL_NUMBER_COMPACT;
    options.digits.maximum_fraction_digits = 2;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "1234", "$1.23K");
    ascii(formatter, allocator, "12", "$12");
    qjs_intl_number_formatter_close(formatter);
    options.currency_sign = QJS_INTL_NUMBER_CURRENCY_ACCOUNTING;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "-1234", "($1.23K)");
    qjs_intl_number_formatter_close(formatter);
    options.currency_sign = QJS_INTL_NUMBER_CURRENCY_STANDARD;
    options.currency_display = QJS_INTL_NUMBER_CURRENCY_CODE;
    calls = state->calls;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    calls = state->calls - calls;
    format_calls = state->calls;
    assert(!qjs_intl_number_format(formatter, &number, &result));
    format_calls = state->calls - format_calls;
    assert(result.length == 9 && result.text[3] == 0xa0 && result.text[8] == 'K');
    currencies = 0;
    for (i = 0; i < result.part_count; i++)
        currencies += result.parts[i].type == QJS_INTL_PART_CURRENCY;
    assert(currencies == 1);
    qjs_intl_number_result_clear(allocator, &result);
    live = state->live;
    for (i = 1; i <= format_calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_format(formatter, &number, &result) == QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.parts && state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_number_formatter_close(formatter);
    live = state->live;
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter) ==
               QJS_INTL_NO_MEMORY);
        assert(!formatter && state->live == live);
    }
    state->fail_at = 0;
    /* A raw-zero baseline wins over an inherited alpha K pattern at 10^3.
       At 10^4 the real compact family must still execute normally. */
    assert(!qjs_intl_number_formatter_open(service, zh, bytes("latn"), &options, &formatter));
    number = value("1000");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    for (i = 0; i < result.part_count; i++)
        assert(result.parts[i].type != QJS_INTL_PART_COMPACT);
    qjs_intl_number_result_clear(allocator, &result);
    number = value("10000");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.text[result.length - 1] == 0x4e07);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    options.currency_display = QJS_INTL_NUMBER_CURRENCY_SYMBOL;
    options.currency = bytes("EUR");
    number = value("1234");
    assert(!qjs_intl_number_formatter_open(service, fr, bytes("latn"), &options, &formatter));
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == sizeof(french) / 2 - 1 &&
           !memcmp(result.text, french, sizeof(french) - 2));
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    options.currency = bytes("PTE");
    assert(!qjs_intl_number_formatter_open(service, pt, bytes("latn"), &options, &formatter));
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == 8 && result.text[1] == '$' && result.text[4] == 0xa0 &&
           result.text[7] == 'l');
    for (i = 0; i < result.part_count; i++)
        assert(result.parts[i].type != QJS_INTL_PART_CURRENCY);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    /* Removed empty-symbol spacing must not count against the final bound. */
    options.maximum_output_length = 5;
    assert(!qjs_intl_number_formatter_open(service, pt, bytes("latn"), &options, &formatter));
    number = value("1000");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == 5 && result.text[1] == 0xa0 && result.text[4] == 'l');
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
}

static void currency_style_metadata(const QJSIntlNumberService *service,
                                    const QJSIntlBytes *available, size_t count,
                                    const QJSIntlNumberOptions *initial,
                                    const QJSIntlAllocator *allocator)
{
    QJSIntlNumberOptions options = *initial;
    QJSIntlNumberFormatter *formatter;
    QJSIntlMathematicalValue number = value("1234.5");
    QJSIntlFormatted result;
    const char *tags[] = {"fr-CH", "de-AT"};
    const QJSIntlPartType types[] = {QJS_INTL_PART_DECIMAL, QJS_INTL_PART_GROUP};
    size_t i, j;
    unsigned found;

    options.style = QJS_INTL_NUMBER_CURRENCY;
    options.currency = bytes("EUR");
    options.grouping = QJS_INTL_NUMBER_GROUP_ALWAYS;
    options.digits.minimum_fraction_digits = options.digits.maximum_fraction_digits = 2;
    for (i = 0; i < 2; i++) {
        assert(!qjs_intl_number_formatter_open(service, find(available, count, tags[i]),
                                               bytes("latn"), &options, &formatter));
        assert(!qjs_intl_number_format(formatter, &number, &result));
        found = 0;
        for (j = 0; j < result.part_count; j++) {
            if (result.parts[j].type == types[i]) {
                assert(result.parts[j].end == result.parts[j].start + 1);
                assert(result.text[result.parts[j].start] == '.');
                found++;
            }
        }
        assert(found == 1);
        qjs_intl_number_result_clear(allocator, &result);
        qjs_intl_number_formatter_close(formatter);
    }
    /* Accounting and ordinary decimal styles can have different group widths
       in the same locale. A public decimal-symbol payload alone is not enough. */
    options.currency = bytes("USD");
    options.currency_sign = QJS_INTL_NUMBER_CURRENCY_ACCOUNTING;
    assert(!qjs_intl_number_formatter_open(service, find(available, count, "en-IN"), bytes("latn"),
                                           &options, &formatter));
    ascii(formatter, allocator, "-1234567.89", "($1,234,567.89)");
    qjs_intl_number_formatter_close(formatter);
    options.currency_sign = QJS_INTL_NUMBER_CURRENCY_STANDARD;
    options.currency_display = QJS_INTL_NUMBER_CURRENCY_CODE;
    assert(!qjs_intl_number_formatter_open(service, find(available, count, "tyv"), bytes("latn"),
                                           &options, &formatter));
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.parts[0].type == QJS_INTL_PART_CURRENCY && result.text[0] == 'U');
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    options.style = QJS_INTL_NUMBER_PERCENT;
    assert(!qjs_intl_number_formatter_open(service, find(available, count, "blo"), bytes("latn"),
                                           &options, &formatter));
    number = value("123.45");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    found = 0;
    for (i = 0; i < result.part_count; i++) {
        if (result.parts[i].type == QJS_INTL_PART_INTEGER && found) {
            assert(result.parts[i].end - result.parts[i].start == 2);
        }
        if (result.parts[i].type == QJS_INTL_PART_GROUP)
            found++;
    }
    assert(found == 2);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
}

static void units(const QJSIntlNumberService *service, size_t en,
                  const QJSIntlNumberOptions *initial, const QJSIntlAllocator *allocator,
                  Allocation *state)
{
    QJSIntlNumberOptions options = *initial;
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result;
    QJSIntlMathematicalValue number = value("2");
    size_t calls, i, live = state->live;

    options.style = QJS_INTL_NUMBER_UNIT;
    options.unit = bytes("meter");
    options.unit_display = QJS_INTL_NUMBER_UNIT_LONG;
    calls = state->calls;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    calls = state->calls - calls;
    ascii(formatter, allocator, "1", "1 meter");
    ascii(formatter, allocator, "2", "2 meters");
    ascii(formatter, allocator, "-2", "-2 meters");
    ascii(formatter, allocator, "-0", "-0 meters");
    {
        QJSIntlMathematicalValue infinity = {QJS_INTL_POSITIVE_INFINITY, {0}};
        QJSIntlFormattedRange range;
        QJSIntlMathematicalValue end = value("3");
        unsigned unit_parts = 0;
        assert(!qjs_intl_number_format(formatter, &infinity, &result));
        assert(result.text[0] == 0x221e && result.parts[0].type == QJS_INTL_PART_INFINITY);
        for (i = 0; i < result.part_count; i++)
            unit_parts += result.parts[i].type == QJS_INTL_PART_UNIT;
        assert(unit_parts == 1);
        qjs_intl_number_result_clear(allocator, &result);
        infinity.kind = QJS_INTL_NAN;
        assert(!qjs_intl_number_format(formatter, &infinity, &result));
        assert(result.parts[0].type == QJS_INTL_PART_NAN);
        qjs_intl_number_result_clear(allocator, &result);
        assert(!qjs_intl_number_format_range(formatter, &number, &end, &range));
        unit_parts = 0;
        for (i = 0; i < range.part_count; i++)
            unit_parts += range.parts[i].type == QJS_INTL_PART_UNIT;
        assert(unit_parts == 2);
        qjs_intl_number_range_result_clear(allocator, &range);
    }
    qjs_intl_number_formatter_close(formatter);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter) ==
               QJS_INTL_NO_MEMORY);
        assert(!formatter && state->live == live);
    }
    state->fail_at = 0;
    options.digits.minimum_fraction_digits = options.digits.maximum_fraction_digits = 1;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "1", "1.0 meters");
    qjs_intl_number_formatter_close(formatter);
    options.digits = initial->digits;
    options.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "1000", "1E3 meter");
    qjs_intl_number_formatter_close(formatter);
    options.notation = QJS_INTL_NUMBER_COMPACT;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "1000", "1K meters");
    qjs_intl_number_formatter_close(formatter);
    options.notation = QJS_INTL_NUMBER_STANDARD;
    options.unit = bytes("percent");
    options.unit_display = QJS_INTL_NUMBER_UNIT_SHORT;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    /* Unit percent is a measurement label, not the percentage style's x100. */
    ascii(formatter, allocator, "0.5", "0.5%");
    qjs_intl_number_formatter_close(formatter);
    options.unit = bytes("meter-per-second");
    options.unit_display = QJS_INTL_NUMBER_UNIT_LONG;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, allocator, "2", "2 meters per second");
    calls = state->calls;
    assert(!qjs_intl_number_format(formatter, &number, &result));
    calls = state->calls - calls;
    qjs_intl_number_result_clear(allocator, &result);
    live = state->live;
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_format(formatter, &number, &result) == QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.parts && state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_number_formatter_close(formatter);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    QJSIntlNumberService *service;
    QJSIntlNumberFormatter *formatter;
    QJSIntlNumberOptions options = {0};
    const QJSIntlBytes *available;
    QJSIntlFormatted result;
    QJSIntlFormattedRange range;
    QJSIntlMathematicalValue number = value("12345");
    size_t count, en, es, i, calls, live;
    uint32_t cursor = 0;
    QJSIntlBytes system;
    int supported;

    calls = state.calls;
    assert(!qjs_intl_number_open(&allocator, NULL, &service));
    calls = state.calls - calls;
    qjs_intl_number_close(service);
    for (i = 1; i <= calls; i++) {
        state.fail_at = state.calls + i;
        assert(qjs_intl_number_open(&allocator, NULL, &service) == QJS_INTL_NO_MEMORY);
        assert(!service && !state.live);
    }
    state.fail_at = 0;
    assert(!qjs_intl_number_open(&allocator, NULL, &service));
    assert(!qjs_intl_number_available(service, &available, &count));
    en = find(available, count, "en");
    es = find(available, count, "es");
    assert(!qjs_intl_number_default_system(service, en, &system));
    assert(system.length == 4 && !memcmp(system.data, "latn", 4));
    while (!qjs_intl_number_system_next(service, &cursor, &system)) {
        assert(!qjs_intl_number_system_supported(service, en, system, &supported) && supported);
    }
    assert(cursor >= 3);
    assert(!qjs_intl_number_system_available(service, bytes("roman"), &supported) && !supported);
    options.maximum_output_length = 1024;
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
    units(service, en, &options, &allocator, &state);
    currencies(service, en, find(available, count, "pt-PT"), &options, &allocator, &state);
    currency_style_metadata(service, available, count, &options, &allocator);
    currency_names(service, en, find(available, count, "ru"), &options, &allocator, &state);
    compact_numbers(service, en, find(available, count, "fr"), find(available, count, "ja"),
                    &options, &allocator, &state);
    compact_currencies(service, en, find(available, count, "fr"), find(available, count, "pt-PT"),
                       find(available, count, "zh"), &options, &allocator, &state);
    missing_digits(service, en, &options, &allocator, "ahom", 0x11730);
    missing_digits(service, en, &options, &allocator, "mathbold", 0x1d7ce);
    missing_digits(service, en, &options, &allocator, "gara", 0x10d40);
    missing_digits(service, en, &options, &allocator, "outlined", 0x1ccf0);
    missing_separators(service, find(available, count, "ar"), &options, &allocator, '.', ',');
    missing_separators(service, find(available, count, "ar-EG"), &options, &allocator, '.', ',');
    missing_separators(service, find(available, count, "fa"), &options, &allocator, '.', ',');
    missing_separators(service, find(available, count, "fr"), &options, &allocator, ',', 0x202f);
    cursor = 0;
    while (!qjs_intl_number_system_next(service, &cursor, &system)) {
        assert(!qjs_intl_number_formatter_open(service, en, system, &options, &formatter));
        number = value("1234.5");
        assert(!qjs_intl_number_format(formatter, &number, &result));
        qjs_intl_number_result_clear(&allocator, &result);
        qjs_intl_number_formatter_close(formatter);
    }
    assert(cursor >= 78);
    calls = state.calls;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    calls = state.calls - calls;
    qjs_intl_number_formatter_close(formatter);
    live = state.live;
    for (i = 1; i <= calls; i++) {
        state.fail_at = state.calls + i;
        assert(qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter) ==
               QJS_INTL_NO_MEMORY);
        assert(!formatter && state.live == live);
    }
    state.fail_at = 0;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, &allocator, "100000000000000003", "100000000000000003");
    calls = state.calls;
    assert(!qjs_intl_number_format(formatter, &number, &result));
    calls = state.calls - calls;
    qjs_intl_number_result_clear(&allocator, &result);
    live = state.live;
    for (i = 1; i <= calls; i++) {
        state.fail_at = state.calls + i;
        assert(qjs_intl_number_format(formatter, &number, &result) == QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.parts && state.live == live);
    }
    state.fail_at = 0;
    assert(!qjs_intl_number_format_range(formatter, &number, &number, &range));
    assert(range.part_count && range.parts[0].type == QJS_INTL_PART_APPROXIMATELY_SIGN);
    for (i = 0; i < range.part_count; i++)
        assert(range.parts[i].source == QJS_INTL_RANGE_SHARED);
    qjs_intl_number_range_result_clear(&allocator, &range);
    {
        QJSIntlMathematicalValue first = value("2"), last = value("1");
        assert(!qjs_intl_number_format_range(formatter, &first, &last, &range));
        assert(range.length == 3 && range.text[0] == '2' && range.text[1] == 0x2013 &&
               range.text[2] == '1');
        assert(range.parts[0].source == QJS_INTL_RANGE_START);
        assert(range.parts[1].source == QJS_INTL_RANGE_SHARED);
        assert(range.parts[2].source == QJS_INTL_RANGE_END);
        qjs_intl_number_range_result_clear(&allocator, &range);
        calls = state.calls;
        assert(!qjs_intl_number_format_range(formatter, &first, &last, &range));
        calls = state.calls - calls;
        qjs_intl_number_range_result_clear(&allocator, &range);
        for (i = 1; i <= calls; i++) {
            state.fail_at = state.calls + i;
            assert(qjs_intl_number_format_range(formatter, &first, &last, &range) ==
                   QJS_INTL_NO_MEMORY);
            assert(!range.text && !range.parts && state.live == live);
        }
        state.fail_at = 0;
        first = (QJSIntlMathematicalValue){QJS_INTL_NAN, {0}};
        calls = state.calls;
        assert(qjs_intl_number_format_range(formatter, &first, &last, &range) ==
               QJS_INTL_INVALID_ARGUMENT);
        assert(!range.text && !range.parts && state.calls == calls);
        first = last = (QJSIntlMathematicalValue){QJS_INTL_POSITIVE_INFINITY, {0}};
        assert(!qjs_intl_number_format_range(formatter, &first, &last, &range));
        assert(range.parts[range.part_count - 1].type == QJS_INTL_PART_INFINITY);
        for (i = 0; i < range.part_count; i++)
            assert(range.parts[i].source == QJS_INTL_RANGE_SHARED);
        qjs_intl_number_range_result_clear(&allocator, &range);
    }
    qjs_intl_number_formatter_close(formatter);
    /* Every admitted default must execute each implemented style. A
       successful constructor alone is not enough to establish capability. */
    for (i = 0; i < count; i++) {
        unsigned style;
        for (style = QJS_INTL_NUMBER_DECIMAL; style <= QJS_INTL_NUMBER_UNIT; style++) {
            QJSIntlNumberOptions current = options;
            current.style = style;
            current.currency = bytes("USD");
            current.unit = bytes("meter");
            assert(!qjs_intl_number_formatter_open(service, i, (QJSIntlBytes){0}, &current,
                                                   &formatter));
            number = value("1234.5");
            assert(!qjs_intl_number_format(formatter, &number, &result));
            qjs_intl_number_result_clear(&allocator, &result);
            qjs_intl_number_formatter_close(formatter);
            if (style == QJS_INTL_NUMBER_CURRENCY) {
                current.currency_display = QJS_INTL_NUMBER_CURRENCY_NAME;
                assert(!qjs_intl_number_formatter_open(service, i, (QJSIntlBytes){0}, &current,
                                                       &formatter));
                assert(!qjs_intl_number_format(formatter, &number, &result));
                qjs_intl_number_result_clear(&allocator, &result);
                qjs_intl_number_formatter_close(formatter);
            }
        }
    }
    for (i = 0; i < count; i++) {
        QJSIntlNumberOptions current = options;
        current.notation = QJS_INTL_NUMBER_COMPACT;
        assert(
            !qjs_intl_number_formatter_open(service, i, (QJSIntlBytes){0}, &current, &formatter));
        number = value("1234.5");
        assert(!qjs_intl_number_format(formatter, &number, &result));
        qjs_intl_number_result_clear(&allocator, &result);
        qjs_intl_number_formatter_close(formatter);
        current.style = QJS_INTL_NUMBER_CURRENCY;
        current.currency = bytes("USD");
        current.currency_display = QJS_INTL_NUMBER_CURRENCY_CODE;
        assert(
            !qjs_intl_number_formatter_open(service, i, (QJSIntlBytes){0}, &current, &formatter));
        assert(!qjs_intl_number_format(formatter, &number, &result));
        qjs_intl_number_result_clear(&allocator, &result);
        qjs_intl_number_formatter_close(formatter);
    }
    options.grouping = QJS_INTL_NUMBER_GROUP_AUTO;
    assert(!qjs_intl_number_formatter_open(service, es, bytes("latn"), &options, &formatter));
    ascii(formatter, &allocator, "1234", "1234");
    qjs_intl_number_formatter_close(formatter);
    options.grouping = QJS_INTL_NUMBER_GROUP_ALWAYS;
    assert(!qjs_intl_number_formatter_open(service, es, bytes("latn"), &options, &formatter));
    ascii(formatter, &allocator, "1234", "1.234");
    qjs_intl_number_formatter_close(formatter);
    options.grouping = QJS_INTL_NUMBER_GROUP_OFF;
    options.style = QJS_INTL_NUMBER_PERCENT;
    options.digits.maximum_fraction_digits = 2;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, &allocator, "0.12345", "12.35%");
    qjs_intl_number_formatter_close(formatter);
    options.style = QJS_INTL_NUMBER_DECIMAL;
    options.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    ascii(formatter, &allocator, "12345", "1.23E4");
    /* Nonfinite values keep their numeric part kinds; scientific notation
       does not add an exponent to them. */
    number = (QJSIntlMathematicalValue){QJS_INTL_POSITIVE_INFINITY, {0}};
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.length == 1 && result.text[0] == 0x221e && result.part_count == 1);
    assert(result.parts[0].type == QJS_INTL_PART_INFINITY);
    qjs_intl_number_result_clear(&allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    /* The selected numbering repertoire also applies to exponent digits. */
    assert(!qjs_intl_number_formatter_open(service, en, bytes("arab"), &options, &formatter));
    number = value("12345");
    assert(!qjs_intl_number_format(formatter, &number, &result));
    assert(result.parts[result.part_count - 1].type == QJS_INTL_PART_EXPONENT_INTEGER);
    assert(result.text[result.length - 1] == 0x0664);
    qjs_intl_number_result_clear(&allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    options.notation = QJS_INTL_NUMBER_COMPACT;
    options.style = QJS_INTL_NUMBER_CURRENCY;
    options.currency = bytes("USD");
    options.currency_display = QJS_INTL_NUMBER_CURRENCY_NAME;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &options, &formatter));
    /* A unit formatter retains its source rules and pattern payloads too. */
    {
        QJSIntlNumberFormatter *unit_formatter;
        QJSIntlNumberOptions unit_options = options;
        unit_options.style = QJS_INTL_NUMBER_UNIT;
        unit_options.notation = QJS_INTL_NUMBER_STANDARD;
        unit_options.unit = bytes("meter");
        unit_options.unit_display = QJS_INTL_NUMBER_UNIT_LONG;
        assert(!qjs_intl_number_formatter_open(service, en, bytes("latn"), &unit_options,
                                               &unit_formatter));
        qjs_intl_number_close(service);
        ascii(unit_formatter, &allocator, "2", "2 meters");
        qjs_intl_number_formatter_close(unit_formatter);
    }
    ascii(formatter, &allocator, "-1234", "-1.23K US dollars");
    qjs_intl_number_formatter_close(formatter);
    assert(!state.live);
    return 0;
}
