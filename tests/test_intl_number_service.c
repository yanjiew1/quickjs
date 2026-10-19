/* Native number service lifetime, exact formatting and allocation failures.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/number/number.h"
#include "../src/cutils/cutils.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
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

static QJSIntlMathematicalValue value(const char *text)
{
    return (QJSIntlMathematicalValue){QJS_INTL_FINITE, bytes(text)};
}

static QJSIntlNumberOptions options(void)
{
    QJSIntlNumberOptions result = {0};
    result.grouping = QJS_INTL_NUMBER_GROUP_AUTO;
    result.digits = (QJSIntlDecimalOptions){1,
                                            0,
                                            2,
                                            1,
                                            21,
                                            1,
                                            QJS_INTL_ROUND_HALF_EXPAND,
                                            QJS_INTL_ROUND_FRACTION,
                                            QJS_INTL_TRAILING_ZERO_AUTO,
                                            1000};
    result.maximum_output_length = 1000;
    return result;
}

static size_t find(const QJSIntlBytes *tags, size_t count, const char *name)
{
    size_t i;
    for (i = 0; i < count; i++)
        if (tags[i].length == strlen(name) && !memcmp(tags[i].data, name, tags[i].length))
            return i;
    abort();
}

static void expected(const QJSIntlFormatted *result, const char *text)
{
    const uint8_t *at = (const uint8_t *)text, *end = at + strlen(text), *next;
    size_t position = 0, i, part_end = 0;
    while (at < end) {
        int c = unicode_from_utf8(at, (int)(end - at), &next);
        assert(c >= 0);
        if (c > 0xffff) {
            c -= 0x10000;
            assert(position + 1 < result->length);
            assert(result->text[position++] == 0xd800 + (c >> 10));
            assert(result->text[position++] == 0xdc00 + (c & 1023));
        } else {
            assert(position < result->length);
            assert(result->text[position++] == c);
        }
        at = next;
    }
    assert(position == result->length);
    for (i = 0; i < result->part_count; i++) {
        assert(result->parts[i].start == part_end);
        part_end = result->parts[i].end;
        assert(part_end <= result->length);
    }
    assert(part_end == result->length);
}

static void check(const QJSIntlAllocator *allocator, const QJSIntlNumberService *service,
                  size_t locale, QJSIntlBytes system, const QJSIntlNumberOptions *settings,
                  QJSIntlMathematicalValue input, const char *text)
{
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result;
    assert(!qjs_intl_number_formatter_open(service, locale, system, settings, &formatter));
    assert(!qjs_intl_number_format(formatter, &input, &result));
    /* Result storage is independent of the formatter, including affixes. */
    qjs_intl_number_formatter_close(formatter);
    expected(&result, text);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_result_clear(allocator, &result);
}

static void test_formats(const QJSIntlAllocator *allocator, QJSIntlNumberService *service,
                         const QJSIntlBytes *tags, size_t count)
{
    QJSIntlNumberOptions settings = options();
    size_t en = find(tags, count, "en");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("12345.678"), "12,345.68");
    check(allocator, service, en, bytes("arab"), &settings, value("12345.678"), "١٢٬٣٤٥٫٦٨");
    check(allocator, service, en, bytes("mathbold"), &settings, value("1234.5"), "𝟏,𝟐𝟑𝟒.𝟓");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("123456789012345678901.125"),
          "123,456,789,012,345,678,901.13");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-0.001"), "-0");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_ZERO, {0}}, "-0");
    settings.sign_display = QJS_INTL_NUMBER_SIGN_NEGATIVE;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-0.001"), "0");
    settings.sign_display = QJS_INTL_NUMBER_SIGN_ALWAYS;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("0.001"), "+0");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NAN, {0}}, "+NaN");
    settings.sign_display = QJS_INTL_NUMBER_SIGN_EXCEPT_ZERO;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NAN, {0}}, "NaN");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_POSITIVE_INFINITY, {0}}, "+∞");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_INFINITY, {0}}, "-∞");
    settings = options();
    settings.grouping = QJS_INTL_NUMBER_GROUP_MIN2;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1234"), "1234");
    settings.grouping = QJS_INTL_NUMBER_GROUP_OFF;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("12345"), "12345");
    settings = options();
    settings.style = QJS_INTL_NUMBER_PERCENT;
    settings.digits.maximum_fraction_digits = 0;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("12.345"), "1,235%");
    settings = options();
    settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("12345.678"), "1.23E4");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("0.00012345"), "1.23E-4");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_ZERO, {0}}, "-0E0");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_POSITIVE_INFINITY, {0}}, "∞");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NAN, {0}}, "NaN");
    check(allocator, service, en, bytes("arab"), &settings, value("1234"), "١٫٢٣اس3");
    check(allocator, service, en, bytes("mathbold"), &settings, value("1234"), "𝟏.𝟐𝟑E3");
    check(allocator, service, en, bytes("arab"), &settings, value("0"), "٠اس0");
    check(allocator, service, en, bytes("mathbold"), &settings, value("0"), "𝟎E0");
    check(allocator, service, en, bytes("arab"), &settings, value("1e-25"), "١اس؜-25");
    settings.digits.minimum_integer_digits = 4;
    settings.grouping = QJS_INTL_NUMBER_GROUP_ALWAYS;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1234"), "0001.23E3");
    settings.digits.minimum_integer_digits = 1;
    settings.grouping = QJS_INTL_NUMBER_GROUP_AUTO;
    settings.digits.maximum_fraction_digits = 1;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("999.5"), "1E3");
    settings.notation = QJS_INTL_NUMBER_ENGINEERING;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("12345.678"), "12.3E3");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("0.00012345"), "123.5E-6");
    settings.style = QJS_INTL_NUMBER_PERCENT;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("12.345"), "1.2E3%");
    settings = options();
    if (TEST_INTL_PROFILE) {
        check(allocator, service, find(tags, count, "fr"), (QJSIntlBytes){0}, &settings,
              value("12345.678"), "12 345,68");
        check(allocator, service, find(tags, count, "hi"), (QJSIntlBytes){0}, &settings,
              value("1234567.89"), "12,34,567.89");
        check(allocator, service, find(tags, count, "pl"), (QJSIntlBytes){0}, &settings,
              value("1234"), "1234");
        settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
        check(allocator, service, find(tags, count, "hi"), (QJSIntlBytes){0}, &settings,
              value("-1234"), "-[1.23E3]");
        check(allocator, service, find(tags, count, "hi"), (QJSIntlBytes){0}, &settings,
              (QJSIntlMathematicalValue){QJS_INTL_POSITIVE_INFINITY, {0}}, "∞");
        check(allocator, service, find(tags, count, "lo"), (QJSIntlBytes){0}, &settings,
              value("1234"), "1,23E3");
        check(allocator, service, find(tags, count, "en-US-posix"), (QJSIntlBytes){0}, &settings,
              value("1234"), "1.23E3");
        settings = options();
        settings.grouping = QJS_INTL_NUMBER_GROUP_ALWAYS;
        check(allocator, service, find(tags, count, "pl"), (QJSIntlBytes){0}, &settings,
              value("1234"), "1 234");
    }
}

static void test_compact(const QJSIntlAllocator *allocator, const QJSIntlNumberService *service,
                         const QJSIntlBytes *tags, size_t count)
{
    QJSIntlNumberOptions settings = options();
    size_t en = find(tags, count, "en");
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result;
    QJSIntlMathematicalValue input = value("9876");
    settings.notation = QJS_INTL_NUMBER_COMPACT;
    settings.grouping = QJS_INTL_NUMBER_GROUP_MIN2;
    settings.digits.rounding_type = QJS_INTL_ROUND_MORE_PRECISION;
    settings.digits.minimum_significant_digits = 1;
    settings.digits.maximum_significant_digits = 2;
    settings.digits.maximum_fraction_digits = 0;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("987654321"), "988M");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("98765432"), "99M");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("98765"), "99K");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("9876"), "9.9K");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("15.9"), "16");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("0.00159"), "0.0016");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("999500"), "1M");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NAN, {0}}, "NaN");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_INFINITY, {0}}, "-∞");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_ZERO, {0}}, "-0");
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    assert(!qjs_intl_number_format(formatter, &input, &result));
    expected(&result, "9.9K");
    assert(result.part_count == 4 && result.parts[3].type == QJS_INTL_PART_COMPACT);
    assert(result.parts[3].start == 3 && result.parts[3].end == 4);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    settings.compact_long = 1;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    assert(!qjs_intl_number_format(formatter, &input, &result));
    expected(&result, "9.9 thousand");
    assert(result.part_count == 5 && result.parts[3].type == QJS_INTL_PART_LITERAL);
    assert(result.parts[4].type == QJS_INTL_PART_COMPACT);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    settings.digits = options().digits;
    settings.digits.maximum_fraction_digits = 0;
    settings.digits.rounding_mode = QJS_INTL_ROUND_CEIL;
    settings.compact_long = 0;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-999.9"), "-0K");
    settings.digits.rounding_mode = QJS_INTL_ROUND_FLOOR;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-999.9"), "-1000");
    settings.digits = options().digits;
    if (TEST_INTL_PROFILE) {
        size_t fr = find(tags, count, "fr"), vec = find(tags, count, "vec");
        settings.compact_long = 1;
        check(allocator, service, fr, (QJSIntlBytes){0}, &settings, value("1000"), "mille");
        check(allocator, service, fr, (QJSIntlBytes){0}, &settings, value("1000000"), "1 million");
        check(allocator, service, fr, (QJSIntlBytes){0}, &settings, value("2000000"), "2 millions");
        settings.digits.minimum_fraction_digits = 2;
        check(allocator, service, fr, (QJSIntlBytes){0}, &settings, value("1000"), "mille");
        check(allocator, service, fr, (QJSIntlBytes){0}, &settings, value("1200000"),
              "1,20 million");
        settings.digits.minimum_integer_digits = 21;
        settings.maximum_output_length = 5;
        check(allocator, service, fr, bytes("mathbold"), &settings, value("1000"), "mille");
        assert(!qjs_intl_number_formatter_open(service, fr, bytes("mathbold"), &settings,
                                               &formatter));
        input = value("-1000");
        assert(qjs_intl_number_format(formatter, &input, &result) == QJS_INTL_OVERFLOW);
        assert(!result.text && !result.parts);
        qjs_intl_number_formatter_close(formatter);
        settings.digits = options().digits;
        settings.maximum_output_length = 1000;
        check(allocator, service, find(tags, count, "yrl"), (QJSIntlBytes){0}, &settings,
              value("1000000"), "1 miliãu-ita");
        check(allocator, service, find(tags, count, "yrl"), bytes("arab"), &settings,
              value("1000000"), "١ miliãu؜-ita");
        assert(!qjs_intl_number_formatter_open(service, find(tags, count, "yrl"), bytes("arab"),
                                               &settings, &formatter));
        input = value("1000000");
        assert(!qjs_intl_number_format(formatter, &input, &result));
        assert(result.part_count == 3 && result.parts[2].type == QJS_INTL_PART_COMPACT);
        qjs_intl_number_result_clear(allocator, &result);
        qjs_intl_number_formatter_close(formatter);
        settings.compact_long = 0;
        settings.digits.minimum_fraction_digits = 0;
        check(allocator, service, vec, (QJSIntlBytes){0}, &settings, value("1000"), "1 mila");
        check(allocator, service, find(tags, count, "zh"), (QJSIntlBytes){0}, &settings,
              value("12345"), "1.23万");
    }
}

static void test_exponent_parts(const QJSIntlAllocator *allocator,
                                const QJSIntlNumberService *service, size_t en)
{
    QJSIntlNumberOptions settings = options();
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result;
    QJSIntlMathematicalValue input = value("0.00012345");
    static const QJSIntlPartType types[] = {
        QJS_INTL_PART_INTEGER,
        QJS_INTL_PART_DECIMAL,
        QJS_INTL_PART_FRACTION,
        QJS_INTL_PART_EXPONENT_SEPARATOR,
        QJS_INTL_PART_EXPONENT_MINUS_SIGN,
        QJS_INTL_PART_EXPONENT_INTEGER,
    };
    size_t i;
    settings.notation = QJS_INTL_NUMBER_ENGINEERING;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    assert(!qjs_intl_number_format(formatter, &input, &result));
    expected(&result, "123.45E-6");
    assert(result.part_count == sizeof(types) / sizeof(types[0]));
    for (i = 0; i < result.part_count; i++)
        assert(result.parts[i].type == types[i]);
    assert(result.parts[3].start == 6 && result.parts[3].end == 7);
    assert(result.parts[4].start == 7 && result.parts[4].end == 8);
    assert(result.parts[5].start == 8 && result.parts[5].end == 9);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    assert(!qjs_intl_number_formatter_open(service, en, bytes("arab"), &settings, &formatter));
    assert(!qjs_intl_number_format(formatter, &input, &result));
    expected(&result, "١٫٢٣اس؜-4");
    assert(result.part_count == sizeof(types) / sizeof(types[0]));
    for (i = 0; i < result.part_count; i++)
        assert(result.parts[i].type == types[i]);
    assert(result.parts[3].end - result.parts[3].start == 2);
    assert(result.parts[4].end - result.parts[4].start == 2);
    assert(result.text[result.parts[4].start] == 0x061c);
    assert(result.parts[5].end - result.parts[5].start == 1);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
}

static void test_failures(const QJSIntlAllocator *allocator, QJSIntlNumberService *service,
                          size_t en, size_t count, QJSIntlNumberNotation notation)
{
    Allocation *state = allocator->opaque;
    QJSIntlNumberOptions settings = options(), invalid;
    QJSIntlNumberFormatter *formatter = NULL;
    QJSIntlNumberService *retry = NULL;
    QJSIntlFormatted result, zero = {0};
    QJSIntlMathematicalValue input = value("-12345.678");
    QJSIntlBytes system;
    const QJSIntlBytes *tags;
    size_t calls, i, live, available;
    settings.notation = notation;
    assert(qjs_intl_number_available(NULL, &tags, &available) == QJS_INTL_INVALID_ARGUMENT);
    assert(!tags && !available);
    assert(qjs_intl_number_default_system(service, count, &system) == QJS_INTL_INVALID_ARGUMENT);
    assert(!system.data && !system.length);
    assert(qjs_intl_number_formatter_open(service, count, (QJSIntlBytes){0}, &settings,
                                          &formatter) == QJS_INTL_INVALID_ARGUMENT &&
           !formatter);
    assert(qjs_intl_number_formatter_open(service, en, bytes("roman"), &settings, &formatter) ==
               QJS_INTL_UNSUPPORTED &&
           !formatter);
    assert(qjs_intl_number_formatter_open(service, en, bytes("unknown"), &settings, &formatter) ==
               QJS_INTL_UNSUPPORTED &&
           !formatter);
    invalid = settings;
    invalid.notation = -1;
    assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &invalid, &formatter) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !formatter);
    invalid = settings;
    invalid.style = -1;
    assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &invalid, &formatter) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !formatter);
    invalid = settings;
    invalid.sign_display = -1;
    assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &invalid, &formatter) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !formatter);
    invalid = settings;
    invalid.grouping = -1;
    assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &invalid, &formatter) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !formatter);
    invalid = settings;
    invalid.digits.rounding_increment = 3;
    assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &invalid, &formatter) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !formatter);
    invalid = settings;
    invalid.maximum_output_length = 0;
    assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &invalid, &formatter) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !formatter);
    live = state->live;
    calls = state->calls;
    assert(!qjs_intl_number_open(allocator, &retry));
    calls = state->calls - calls;
    qjs_intl_number_close(retry);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_open(allocator, &retry) == QJS_INTL_NO_MEMORY);
        assert(!retry && state->live == live);
    }
    state->fail_at = 0;
    calls = state->calls;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    calls = state->calls - calls;
    qjs_intl_number_formatter_close(formatter);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings,
                                              &formatter) == QJS_INTL_NO_MEMORY);
        assert(!formatter && state->live == live);
    }
    state->fail_at = 0;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    live = state->live;
    calls = state->calls;
    assert(!qjs_intl_number_format(formatter, &input, &result));
    calls = state->calls - calls;
    qjs_intl_number_result_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_number_format(formatter, &input, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == live);
    }
    state->fail_at = 0;
    assert(!qjs_intl_number_format(formatter, &input, &result));
    qjs_intl_number_result_clear(allocator, &result);
    assert(qjs_intl_number_format(NULL, &input, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_format(formatter, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_format(formatter, &input, NULL) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_number_formatter_close(formatter);
    settings.maximum_output_length = 4;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    assert(qjs_intl_number_format(formatter, &input, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)));
    qjs_intl_number_formatter_close(formatter);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlNumberService *service;
    QJSIntlNumberFormatter *formatter;
    QJSIntlNumberOptions settings = options();
    QJSIntlMathematicalValue input = value("1234.5");
    QJSIntlFormatted result;
    QJSIntlBytes system;
    const QJSIntlBytes *tags;
    size_t count, i, en;
    assert(!qjs_intl_number_open(&allocator, &service));
    assert(!qjs_intl_number_available(service, &tags, &count));
    assert(count == (TEST_INTL_PROFILE ? 1211 : 2));
    en = find(tags, count, "en");
    test_formats(&allocator, service, tags, count);
    test_exponent_parts(&allocator, service, en);
    test_compact(&allocator, service, tags, count);
    test_failures(&allocator, service, en, count, QJS_INTL_NUMBER_STANDARD);
    test_failures(&allocator, service, en, count, QJS_INTL_NUMBER_SCIENTIFIC);
    test_failures(&allocator, service, en, count, QJS_INTL_NUMBER_COMPACT);
    for (i = 0; i < count; i++) {
        assert(!qjs_intl_number_default_system(service, i, &system) && system.length);
        assert(!qjs_intl_number_formatter_open(service, i, system, &settings, &formatter));
        assert(!qjs_intl_number_format(formatter, &input, &result));
        assert(result.length && result.part_count);
        qjs_intl_number_result_clear(&allocator, &result);
        qjs_intl_number_formatter_close(formatter);
    }
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    qjs_intl_number_close(service);
    assert(!qjs_intl_number_format(formatter, &input, &result));
    qjs_intl_number_formatter_close(formatter);
    expected(&result, "1,234.5");
    qjs_intl_number_result_clear(&allocator, &result);
    qjs_intl_number_close(NULL);
    qjs_intl_number_formatter_close(NULL);
    qjs_intl_number_result_clear(&allocator, NULL);
    assert(!state.live);
    return 0;
}
