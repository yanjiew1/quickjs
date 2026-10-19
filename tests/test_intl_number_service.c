/* Native number service lifetime, exact formatting and allocation failures.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/number/number.h"
#include "../src/intl/number/currency-format.h"
#include "../src/intl/number/currency-pattern.h"
#include "../src/intl/unit/identifier.h"
#include "../src/cutils/cutils.h"
#include <assert.h>
#include <stdio.h>
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
            if (result->text[position] != c)
                fprintf(stderr, "Expected %s: UTF-16 offset %zu expected U+%04X, got U+%04X\n",
                        text, position, c, result->text[position]);
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

static void unit_part(const QJSIntlFormatted *result, const char *name)
{
    size_t i, count = 0;
    for (i = 0; i < result->part_count; i++) {
        const QJSIntlPart *part = &result->parts[i];
        if (part->type == QJS_INTL_PART_UNIT) {
            QJSIntlPart span = {0, part->end - part->start, QJS_INTL_PART_UNIT};
            QJSIntlFormatted view = {result->text + part->start, span.end, &span, 1};
            expected(&view, name);
            count++;
        }
    }
    assert(count == 1);
}

static void test_units(const QJSIntlAllocator *allocator, const QJSIntlNumberService *service,
                       const QJSIntlBytes *tags, size_t count)
{
    Allocation *state = allocator->opaque;
    QJSIntlNumberOptions settings = options();
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result, zero = {0};
    QJSIntlMathematicalValue input = value("-1200");
    size_t en = find(tags, count, "en"), i, calls, live;
    char name[] = "meter";
    settings.style = QJS_INTL_NUMBER_UNIT;
    settings.unit = bytes("meter");
    settings.unit_display = QJS_INTL_NUMBER_UNIT_LONG;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1"), "1 meter");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("2"), "2 meters");
    check(allocator, service, en, bytes("arab"), &settings, value("2"), "٢ meters");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-0.001"), "-0 meters");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NAN, {0}}, "NaN meters");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_INFINITY, {0}}, "-∞ meters");
    settings.digits.minimum_fraction_digits = 2;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1"), "1.00 meters");
    settings.digits.minimum_fraction_digits = 0;
    settings.unit_display = QJS_INTL_NUMBER_UNIT_SHORT;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1"), "1 m");
    settings.unit = bytes("percent");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("5"), "5%");
    settings.unit = bytes("meter");
    settings.unit_display = QJS_INTL_NUMBER_UNIT_NARROW;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1"), "1m");
    settings.unit_display = QJS_INTL_NUMBER_UNIT_LONG;
    settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1000"), "1E3 meter");
    settings.notation = QJS_INTL_NUMBER_ENGINEERING;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("12000"), "12E3 meters");
    settings.notation = QJS_INTL_NUMBER_COMPACT;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1000"), "1K meters");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-1200"), "-1.2K meters");
    settings.compact_long = 1;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1200000"), "1.2 million meters");
    settings.compact_long = 0;
    if (TEST_INTL_PROFILE) {
        size_t cs = find(tags, count, "cs");
        settings.notation = QJS_INTL_NUMBER_STANDARD;
        check(allocator, service, cs, (QJSIntlBytes){0}, &settings, value("1"), "1 metr");
        settings.digits.minimum_fraction_digits = 2;
        check(allocator, service, cs, (QJSIntlBytes){0}, &settings, value("1"), "1,00 metru");
        settings.notation = QJS_INTL_NUMBER_COMPACT;
        settings.digits.minimum_fraction_digits = settings.digits.maximum_fraction_digits = 5;
        assert(!qjs_intl_number_formatter_open(service, cs, (QJSIntlBytes){0}, &settings, &formatter));
        input = value("1200");
        assert(!qjs_intl_number_format(formatter, &input, &result));
        unit_part(&result, "metru"); /* 1.20000c3 -> 1200.00, retaining v=2. */
        qjs_intl_number_result_clear(allocator, &result);
        qjs_intl_number_formatter_close(formatter);
        settings.digits.minimum_fraction_digits = 0;
        settings.digits.maximum_fraction_digits = 2;
        assert(!qjs_intl_number_formatter_open(service, cs, (QJSIntlBytes){0}, &settings, &formatter));
        assert(!qjs_intl_number_format(formatter, &input, &result));
        unit_part(&result, "metrů");
        qjs_intl_number_result_clear(allocator, &result);
        qjs_intl_number_formatter_close(formatter);
        settings.notation = QJS_INTL_NUMBER_STANDARD;
        settings.unit = bytes("degree");
        check(allocator, service, find(tags, count, "he"), bytes("latn"), &settings,
              value("1"), "1 מעלות");
        settings.unit = bytes("second");
        check(allocator, service, find(tags, count, "ar"), bytes("latn"), &settings,
              value("1"), "1 ثانية");
    }
    settings.notation = QJS_INTL_NUMBER_COMPACT;
    settings.unit = bytes(name);
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    memset(name, '?', sizeof(name) - 1);
    input = value("-1200");
    assert(!qjs_intl_number_format(formatter, &input, &result));
    expected(&result, "-1.2K meters");
    qjs_intl_number_result_clear(allocator, &result);
    live = state->live;
    calls = state->calls;
    assert(!qjs_intl_number_format(formatter, &input, &result));
    calls = state->calls - calls;
    qjs_intl_number_result_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_format(formatter, &input, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_number_formatter_close(formatter);
    settings.unit = bytes("meter");
    for (i = 0; i < qjs_intl_unit_definition_count(); i++) {
        const QJSIntlUnitDefinition *definition = qjs_intl_unit_definition_at(i);
        settings.unit = bytes(definition->identifier);
        for (settings.unit_display = QJS_INTL_NUMBER_UNIT_SHORT;
             settings.unit_display <= QJS_INTL_NUMBER_UNIT_LONG; settings.unit_display++) {
            for (settings.notation = QJS_INTL_NUMBER_STANDARD;
                 settings.notation <= QJS_INTL_NUMBER_COMPACT; settings.notation++) {
                assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
                assert(!qjs_intl_number_format(formatter, &input, &result));
                assert(result.length && result.parts[result.part_count - 1].type == QJS_INTL_PART_UNIT);
                qjs_intl_number_result_clear(allocator, &result);
                qjs_intl_number_formatter_close(formatter);
            }
        }
    }
    settings.unit_display = QJS_INTL_NUMBER_UNIT_LONG;
    settings.notation = QJS_INTL_NUMBER_STANDARD;
    settings.unit = bytes("meter-per-second");
    assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter) ==
           QJS_INTL_UNSUPPORTED && !formatter);
    settings.unit = bytes("Meter");
    assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter) ==
           QJS_INTL_INVALID_ARGUMENT && !formatter);
    settings.unit = bytes("meter");
    settings.unit_display = -1;
    assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter) ==
           QJS_INTL_INVALID_ARGUMENT && !formatter);
    settings.unit_display = QJS_INTL_NUMBER_UNIT_LONG;
    settings.maximum_output_length = 4;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    assert(qjs_intl_number_format(formatter, &input, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)));
    qjs_intl_number_formatter_close(formatter);
    settings.maximum_output_length = 1000;
    live = state->live;
    calls = state->calls;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    calls = state->calls - calls;
    qjs_intl_number_formatter_close(formatter);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter) ==
               QJS_INTL_NO_MEMORY && !formatter && state->live == live);
    }
    state->fail_at = 0;
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

static void test_currency_names(const QJSIntlAllocator *allocator, const QJSIntlNumberService *service,
                                 const QJSIntlBytes *tags, size_t count)
{
    Allocation *state = allocator->opaque;
    QJSIntlNumberOptions settings = options();
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result, zero = {0};
    QJSIntlMathematicalValue input = value("1");
    uint32_t digits;
    size_t en = find(tags, count, "en"), i, calls, live, currencies = 0;
    char code[] = "USD";
    assert(!qjs_intl_number_currency_digits(service, bytes("JPY"), &digits) && digits == 0);
    assert(!qjs_intl_number_currency_digits(service, bytes("BHD"), &digits) && digits == 3);
    assert(!qjs_intl_number_currency_digits(service, bytes("CHF"), &digits) && digits == 2);
    assert(!qjs_intl_number_currency_digits(service, bytes("ZZZ"), &digits) && digits == 2);
    assert(qjs_intl_number_currency_digits(service, bytes("usd"), &digits) == QJS_INTL_INVALID_ARGUMENT);
    assert(!digits);
    assert(qjs_intl_number_currency_digits(NULL, bytes("USD"), &digits) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_currency_digits(service, bytes("USD"), NULL) == QJS_INTL_INVALID_ARGUMENT);
    settings.style = QJS_INTL_NUMBER_CURRENCY;
    settings.currency_display = QJS_INTL_NUMBER_CURRENCY_NAME;
    settings.currency = bytes(code);
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1"), "1 US dollar");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("2"), "2 US dollars");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1.2"), "1.2 US dollars");
    check(allocator, service, en, bytes("arab"), &settings, value("1.2"), "١٫٢ US dollars");
    settings.currency_sign = QJS_INTL_NUMBER_CURRENCY_ACCOUNTING;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-1"), "-1 US dollar");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NAN, {0}}, "NaN US dollars");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_INFINITY, {0}}, "-∞ US dollars");
    settings.digits.minimum_fraction_digits = 2;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1"), "1.00 US dollars");
    settings.digits = options().digits;
    settings.digits.maximum_fraction_digits = 0;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("0.99"), "1 US dollar");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-0.001"), "-0 US dollars");
    settings.currency = bytes("ZZZ");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("2"), "2 ZZZ");
    settings.currency = bytes(code);
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    memcpy(code, "JPY", 3);
    assert(!qjs_intl_number_format(formatter, &input, &result));
    expected(&result, "1 US dollar");
    for (i = 0; i < result.part_count; i++)
        currencies += result.parts[i].type == QJS_INTL_PART_CURRENCY;
    assert(currencies == 1);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    memcpy(code, "USD", 3);
    settings.maximum_output_length = 5;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    assert(qjs_intl_number_format(formatter, &input, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)));
    qjs_intl_number_formatter_close(formatter);
    settings.maximum_output_length = 1000;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    live = state->live;
    calls = state->calls;
    assert(!qjs_intl_number_format(formatter, &input, &result));
    calls = state->calls - calls;
    qjs_intl_number_result_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_format(formatter, &input, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_number_formatter_close(formatter);
    if (TEST_INTL_PROFILE) {
        settings.digits = options().digits;
        check(allocator, service, find(tags, count, "ja"), bytes("arab"), &settings, value("1.2"),
              "١٫٢米ドル");
        settings.currency = bytes("CVE");
        check(allocator, service, find(tags, count, "pt-CV"), (QJSIntlBytes){0}, &settings,
              value("1.2"), "1$2 escudo cabo-verdiano");
        check(allocator, service, find(tags, count, "pt-CV"), (QJSIntlBytes){0}, &settings,
              value("2.2"), "2$2 escudos cabo-verdianos");
    }
}

static void test_currency_name_notation(const QJSIntlAllocator *allocator,
                                       const QJSIntlNumberService *service,
                                       const QJSIntlBytes *tags, size_t count)
{
    QJSIntlNumberOptions settings = options();
    size_t en = find(tags, count, "en");
    settings.style = QJS_INTL_NUMBER_CURRENCY;
    settings.currency_display = QJS_INTL_NUMBER_CURRENCY_NAME;
    settings.currency = bytes("USD");
    settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    settings.digits.maximum_fraction_digits = 0;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1000"), "1E3 US dollar");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("2000"), "2E3 US dollars");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("0.001"), "1E-3 US dollar");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1e1000000000"),
          "1E1000000000 US dollar");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1e-1000000000"),
          "1E-1000000000 US dollar");
    settings.currency_sign = QJS_INTL_NUMBER_CURRENCY_ACCOUNTING;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-1000"), "-1E3 US dollar");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_ZERO, {0}}, "-0E0 US dollars");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_INFINITY, {0}}, "-∞ US dollars");
    settings.digits.minimum_fraction_digits = settings.digits.maximum_fraction_digits = 2;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1000"), "1.00E3 US dollars");
    settings.digits.trailing_zero_display = QJS_INTL_TRAILING_ZERO_STRIP_IF_INTEGER;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1000"), "1E3 US dollar");
    settings.digits = options().digits;
    settings.notation = QJS_INTL_NUMBER_ENGINEERING;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("12345"), "12.35E3 US dollars");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1000"), "1E3 US dollar");
    if (TEST_INTL_PROFILE) {
        settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
        settings.digits.maximum_fraction_digits = 0;
        check(allocator, service, find(tags, count, "fr"), (QJSIntlBytes){0}, &settings,
              value("1000000"), "1E6 dollar des États-Unis");
    }
}

static void test_currency_formats(const QJSIntlAllocator *allocator, const QJSIntlNumberService *service,
                                  const QJSIntlBytes *tags, size_t count)
{
    QJSIntlNumberOptions settings = options();
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result, zero = {0};
    QJSIntlMathematicalValue input = value("-1.2");
    Allocation *state = allocator->opaque;
    size_t en = find(tags, count, "en"), i, calls, live;
    char code[] = "ZZZ";
    settings.style = QJS_INTL_NUMBER_CURRENCY;
    settings.currency = bytes("USD");
    settings.digits.minimum_fraction_digits = settings.digits.maximum_fraction_digits = 2;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1.2"), "$1.20");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "-$1.20");
    settings.currency_display = QJS_INTL_NUMBER_CURRENCY_CODE;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1.2"), "USD 1.20");
    settings.currency_sign = QJS_INTL_NUMBER_CURRENCY_ACCOUNTING;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "(USD 1.20)");
    settings.sign_display = QJS_INTL_NUMBER_SIGN_ALWAYS;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1.2"), "+USD 1.20");
    settings.sign_display = QJS_INTL_NUMBER_SIGN_NEVER;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "USD 1.20");
    settings.sign_display = QJS_INTL_NUMBER_SIGN_AUTO;
    settings.currency_sign = QJS_INTL_NUMBER_CURRENCY_STANDARD;
    settings.currency_display = QJS_INTL_NUMBER_CURRENCY_SYMBOL;
    settings.currency = bytes("CAD");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1.2"), "CA$1.20");
    settings.currency_display = QJS_INTL_NUMBER_CURRENCY_NARROW;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1.2"), "$1.20");
    settings.currency = bytes(code);
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    memcpy(code, "USD", 3);
    assert(!qjs_intl_number_format(formatter, &input, &result));
    expected(&result, "-ZZZ 1.20");
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    settings.currency = bytes("USD");
    settings.currency_display = QJS_INTL_NUMBER_CURRENCY_CODE;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NAN, {0}}, "USD NaN");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_INFINITY, {0}}, "-USD ∞");
    settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1234"), "USD 1.23E3");
    settings.notation = QJS_INTL_NUMBER_ENGINEERING;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("12345"), "USD 12.35E3");
    settings.notation = QJS_INTL_NUMBER_STANDARD;
    live = state->live;
    calls = state->calls;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    calls = state->calls - calls;
    qjs_intl_number_formatter_close(formatter);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter) ==
               QJS_INTL_NO_MEMORY);
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
        assert(qjs_intl_number_format(formatter, &input, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_number_formatter_close(formatter);
    if (TEST_INTL_PROFILE) {
        settings.currency_display = QJS_INTL_NUMBER_CURRENCY_SYMBOL;
        check(allocator, service, find(tags, count, "fr"), (QJSIntlBytes){0}, &settings,
              value("1.2"), "1,20 $US");
        settings.currency = bytes("CVE");
        check(allocator, service, find(tags, count, "pt-CV"), (QJSIntlBytes){0}, &settings,
              value("12345.5"), "12 345$50 \u200b");
        settings.currency = bytes("TRY");
        check(allocator, service, find(tags, count, "tr"), (QJSIntlBytes){0}, &settings,
              value("1.2"), "₺1,20");
        settings.currency_display = QJS_INTL_NUMBER_CURRENCY_CODE;
        check(allocator, service, find(tags, count, "tr"), (QJSIntlBytes){0}, &settings,
              value("1.2"), "TRY 1,20");
        check(allocator, service, find(tags, count, "tr"), bytes("mathdbl"), &settings,
              value("1.2"), "TRY 𝟙,𝟚𝟘");
        check(allocator, service, find(tags, count, "tr"), bytes("hanidec"), &settings,
              value("1.2"), "TRY一,二〇");
        check(allocator, service, find(tags, count, "tr"), (QJSIntlBytes){0}, &settings,
              (QJSIntlMathematicalValue){QJS_INTL_NAN, {0}}, "TRYNaN");
        settings.currency_sign = QJS_INTL_NUMBER_CURRENCY_ACCOUNTING;
        settings.currency_display = QJS_INTL_NUMBER_CURRENCY_SYMBOL;
        check(allocator, service, find(tags, count, "tr"), (QJSIntlBytes){0}, &settings,
              value("1.2"), "₺1,20");
        check(allocator, service, find(tags, count, "tr"), (QJSIntlBytes){0}, &settings,
              value("-1.2"), "(₺1,20)");
    }
}

static void test_compact_currency_names(const QJSIntlAllocator *allocator,
                                        const QJSIntlNumberService *service,
                                        const QJSIntlBytes *tags, size_t count)
{
    QJSIntlNumberOptions settings = options();
    QJSIntlNumberFormatter *formatter;
    QJSIntlFormatted result, zero = {0};
    QJSIntlMathematicalValue input = value("1000");
    Allocation *state = allocator->opaque;
    size_t en = find(tags, count, "en"), i, calls, live;
    settings.style = QJS_INTL_NUMBER_CURRENCY;
    settings.currency = bytes("USD");
    settings.currency_display = QJS_INTL_NUMBER_CURRENCY_NAME;
    settings.notation = QJS_INTL_NUMBER_COMPACT;
    settings.digits.maximum_fraction_digits = 0;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "1K US dollars");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("999999"), "1M US dollars");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1"), "1 US dollar");
    settings.currency_sign = QJS_INTL_NUMBER_CURRENCY_ACCOUNTING;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-1000"), "-1K US dollars");
    settings.sign_display = QJS_INTL_NUMBER_SIGN_ALWAYS;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "+1K US dollars");
    settings.sign_display = QJS_INTL_NUMBER_SIGN_AUTO;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_ZERO, {0}}, "-0 US dollars");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NAN, {0}}, "NaN US dollars");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_INFINITY, {0}}, "-∞ US dollars");
    settings.digits.rounding_mode = QJS_INTL_ROUND_CEIL;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-999.9"), "-0K US dollars");
    settings.digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
    settings.digits.minimum_fraction_digits = settings.digits.maximum_fraction_digits = 2;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1200"), "1.20K US dollars");
    settings.digits.trailing_zero_display = QJS_INTL_TRAILING_ZERO_STRIP_IF_INTEGER;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "1K US dollars");
    settings.digits = options().digits;
    settings.digits.maximum_fraction_digits = 0;
    settings.compact_long = 1;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "1 thousand US dollars");
    settings.compact_long = 0;
    settings.currency = bytes("ZZZ");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "1K ZZZ");
    settings.currency = bytes("USD");
    settings.maximum_output_length = 13;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    assert(!qjs_intl_number_format(formatter, &input, &result));
    expected(&result, "1K US dollars");
    assert(result.part_count == 4 && result.parts[0].type == QJS_INTL_PART_INTEGER &&
           result.parts[1].type == QJS_INTL_PART_COMPACT && result.parts[2].type == QJS_INTL_PART_LITERAL &&
           result.parts[3].type == QJS_INTL_PART_CURRENCY);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    settings.maximum_output_length = 12;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    assert(qjs_intl_number_format(formatter, &input, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)));
    qjs_intl_number_formatter_close(formatter);
    settings.maximum_output_length = 1000;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    live = state->live;
    calls = state->calls;
    assert(!qjs_intl_number_format(formatter, &input, &result));
    calls = state->calls - calls;
    qjs_intl_number_result_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_format(formatter, &input, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_number_formatter_close(formatter);
    if (TEST_INTL_PROFILE) {
        /* Czech names distinguish expanded 1200 (v=0) from 1200.00 (v=2),
           even though both originate in visible fractional mantissas. */
        size_t cs = find(tags, count, "cs");
        settings.digits.minimum_fraction_digits = settings.digits.maximum_fraction_digits = 2;
        check(allocator, service, cs, (QJSIntlBytes){0}, &settings,
              value("1200"), "1,20 tis. amerických dolarů");
        settings.digits.minimum_fraction_digits = settings.digits.maximum_fraction_digits = 5;
        check(allocator, service, cs, (QJSIntlBytes){0}, &settings,
              value("1200"), "1,20000 tis. amerického dolaru");
        settings.digits = options().digits;
        settings.digits.maximum_fraction_digits = 0;
        settings.compact_long = 1;
        check(allocator, service, find(tags, count, "fr"), (QJSIntlBytes){0}, &settings,
              input, "mille dollars des États-Unis");
        check(allocator, service, find(tags, count, "fr"), (QJSIntlBytes){0}, &settings,
              value("1000000"), "1 million dollars des États-Unis");
    }
}

static void test_compact_currency(const QJSIntlAllocator *allocator,
                                  const QJSIntlNumberService *service,
                                  const QJSIntlBytes *tags, size_t count)
{
    QJSIntlNumberOptions settings = options();
    QJSIntlNumberFormatter *formatter;
    QJSIntlNumberService *owned_service;
    QJSIntlFormatted result, zero = {0};
    QJSIntlMathematicalValue input = value("-1234");
    Allocation *state = allocator->opaque;
    size_t en = find(tags, count, "en"), i, calls, live;
    char code[] = "ZZZ";
    settings.style = QJS_INTL_NUMBER_CURRENCY;
    settings.currency = bytes("USD");
    settings.notation = QJS_INTL_NUMBER_COMPACT;
    settings.digits.maximum_fraction_digits = 0;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1234"), "$1K");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "-$1K");
    settings.compact_long = 1;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1234"), "$1K");
    settings.currency_sign = QJS_INTL_NUMBER_CURRENCY_ACCOUNTING;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "($1K)");
    settings.sign_display = QJS_INTL_NUMBER_SIGN_ALWAYS;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1234"), "+$1K");
    settings.sign_display = QJS_INTL_NUMBER_SIGN_NEVER;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "$1K");
    settings.sign_display = QJS_INTL_NUMBER_SIGN_AUTO;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_INFINITY, {0}}, "($∞)");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings,
          (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_ZERO, {0}}, "($0)");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("-123"), "($123)");
    settings.currency_display = QJS_INTL_NUMBER_CURRENCY_CODE;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, input, "(USD 1K)");
    settings.currency_sign = QJS_INTL_NUMBER_CURRENCY_STANDARD;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("999999"), "USD 1M");
    settings.currency_display = QJS_INTL_NUMBER_CURRENCY_SYMBOL;
    settings.currency = bytes("CAD");
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1234"), "CA$1K");
    settings.currency_display = QJS_INTL_NUMBER_CURRENCY_NARROW;
    check(allocator, service, en, (QJSIntlBytes){0}, &settings, value("1234"), "$1K");
    settings.currency = bytes(code);
    assert(!qjs_intl_number_open(allocator, &owned_service));
    assert(!qjs_intl_number_formatter_open(owned_service, en, (QJSIntlBytes){0}, &settings, &formatter));
    memcpy(code, "USD", 3);
    qjs_intl_number_close(owned_service);
    assert(!qjs_intl_number_format(formatter, &input, &result));
    expected(&result, "-ZZZ 1K");
    calls = 0;
    for (i = 0; i < result.part_count; i++)
        calls += result.parts[i].type == QJS_INTL_PART_CURRENCY;
    assert(calls == 1);
    qjs_intl_number_result_clear(allocator, &result);
    qjs_intl_number_formatter_close(formatter);
    settings.currency = bytes("USD");
    live = state->live;
    calls = state->calls;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    calls = state->calls - calls;
    qjs_intl_number_formatter_close(formatter);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter) ==
               QJS_INTL_NO_MEMORY);
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
        assert(qjs_intl_number_format(formatter, &input, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == live);
    }
    state->fail_at = 0;
    qjs_intl_number_formatter_close(formatter);
    if (TEST_INTL_PROFILE) {
        settings.currency_display = QJS_INTL_NUMBER_CURRENCY_CODE;
        check(allocator, service, find(tags, count, "zh"), (QJSIntlBytes){0}, &settings,
              value("1234"), "USD 1,234");
        check(allocator, service, find(tags, count, "zh"), bytes("arab"), &settings,
              value("12345"), "USD ١٢٬٣٤٥");
        check(allocator, service, find(tags, count, "he"), bytes("latn"), &settings,
              value("1234"), "‏1 א׳‏ ‏USD");
    }
}

static void test_currency_patterns(const QJSIntlAllocator *allocator)
{
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlNumberPatternData patterns;
    QJSIntlCurrencyData currencies;
    QJSIntlNumberLayoutData layouts;
    QJSIntlCurrencyPattern result, zero = {0};
    QJSIntlNumberPatternToken token;
    QJSIntlBytes tag;
    uint32_t en = UINT32_MAX, parent, system, i;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, allocator, &data));
    assert(!qjs_intl_numbering_data_open(&data, allocator, &numbering));
    assert(!qjs_intl_number_pattern_data_open(&numbering, allocator, &patterns));
    assert(!qjs_intl_currency_data_open(&data, allocator, &currencies));
    assert(!qjs_intl_number_layout_data_open(&numbering, allocator, &layouts));
    assert(!qjs_intl_numbering_system_find(&numbering, bytes("latn"), &system));
    for (i = 0; i < data.locales.count; i++) {
        assert(!qjs_intl_data_locale(&data, i, &tag, &parent));
        if (tag.length == 2 && !memcmp(tag.data, "en", 2))
            en = i;
    }
    assert(en != UINT32_MAX);
    /* Standard alpha tests the closest scalar, not the compact L M* rule.
       An alpha-capable family suppresses legacy spacing for ordinary Fr. */
    assert(!qjs_intl_number_currency_pattern(&patterns, &currencies, &layouts, en, system,
                                             bytes("ZZZ"), bytes("Fr."), 0, &result));
    assert(result.pattern.forms[0].count == 2);
    assert(!result.before_currency.length && !result.after_currency.length);
    assert(!qjs_intl_number_currency_pattern(&patterns, &currencies, &layouts, en, system,
                                             bytes("ZZZ"), bytes("Á"), 0, &result));
    assert(result.pattern.forms[0].count == 2 && !result.after_currency.length);
    assert(!qjs_intl_number_currency_pattern(&patterns, &currencies, &layouts, en, system,
                                             bytes("ZZZ"), bytes("USD"), 0, &result));
    assert(result.pattern.forms[0].count == 3);
    assert(!qjs_intl_number_pattern_token_at(&patterns, &result.pattern.forms[0], 1, &token));
    assert(token.kind == QJS_INTL_NUMBER_TOKEN_LITERAL && token.text.length == 2 &&
           !memcmp(token.text.data, " ", 2));
    assert(!result.before_currency.length && !result.after_currency.length);
    assert(!qjs_intl_number_currency_pattern(&patterns, &currencies, &layouts, en, system,
                                             bytes("ZZZ"), bytes(""), 0, &result));
    assert(result.pattern.forms[0].count == 2 && !result.after_currency.length);
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_number_currency_pattern(&patterns, &currencies, &layouts, en, system,
                                            bytes("usd"), bytes("USD"), 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_number_currency_pattern(&patterns, &currencies, &layouts, en, system,
                                            bytes("USD"), bytes("USD"), 2, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_currency_pattern(NULL, &currencies, &layouts, en, system,
                                            bytes("USD"), bytes("USD"), 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    if (TEST_INTL_PROFILE) {
        QJSIntlNumberPattern generic;
        uint32_t tr = UINT32_MAX;
        for (i = 0; i < data.locales.count; i++) {
            assert(!qjs_intl_data_locale(&data, i, &tag, &parent));
            if (tag.length == 2 && !memcmp(tag.data, "tr", 2))
                tr = i;
        }
        assert(tr != UINT32_MAX);
        assert(!qjs_intl_number_currency_pattern(&patterns, &currencies, &layouts, tr, system,
                                                 bytes("TRY"), bytes("₺"), 1, &result));
        assert(!qjs_intl_number_pattern_at(&patterns, tr, system,
                                            QJS_INTL_PATTERN_CURRENCY_ACCOUNTING, &generic));
        for (i = 0; i < QJS_INTL_NUMBER_PATTERN_FORM_COUNT; i++) {
            assert(result.pattern.forms[i].tokens == generic.forms[i].tokens);
            assert(result.pattern.forms[i].count == generic.forms[i].count);
        }
        assert(!result.before_currency.length && !result.after_currency.length);
    }
}

static void test_name_templates(const QJSIntlAllocator *allocator)
{
    uint16_t digits[] = {'-', '2'};
    QJSIntlPart parts[] = {{0, 1, QJS_INTL_PART_MINUS_SIGN}, {1, 2, QJS_INTL_PART_INTEGER}};
    QJSIntlFormatted number = {digits, 2, parts, 2}, result, zero = {0};
    size_t i;
    const char *invalid[] = {"", "{0}", "{1}", "{0}{0}", "{0}{1}{1}", "{2}{1}",
                             "{0}{{1}", "{0}{1}}", "{0}{1}\xed\xa0\x80"};
    assert(!qjs_intl_number_currency_name_format(allocator, bytes("'{1}' {0}"), bytes("currency"),
                                                  &number, 100, &result));
    expected(&result, "'currency' -2");
    assert(result.parts[1].type == QJS_INTL_PART_CURRENCY);
    qjs_intl_number_result_clear(allocator, &result);
    assert(!qjs_intl_number_currency_name_format(allocator, bytes("{0} {1}"), bytes(""),
                                                  &number, 3, &result));
    expected(&result, "-2 ");
    assert(result.parts[result.part_count - 1].type == QJS_INTL_PART_CURRENCY);
    assert(result.parts[result.part_count - 1].start == result.length);
    qjs_intl_number_result_clear(allocator, &result);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        assert(qjs_intl_number_currency_name_format(allocator, bytes(invalid[i]), bytes("name"),
                                                     &number, 100, &result) == QJS_INTL_DATA_ERROR);
        assert(!memcmp(&result, &zero, sizeof(result)));
    }
    assert(qjs_intl_number_currency_name_format(allocator, (QJSIntlBytes){NULL, 3}, bytes("name"),
                                                 &number, 100, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_currency_name_format(NULL, bytes("{0}{1}"), bytes("name"),
                                                 &number, 100, &result) == QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlNumberService *service;
    QJSIntlNumberFormatter *formatter, *currency_formatter, *unit_formatter;
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
    test_units(&allocator, service, tags, count);
    test_currency_names(&allocator, service, tags, count);
    test_currency_formats(&allocator, service, tags, count);
    test_currency_name_notation(&allocator, service, tags, count);
    test_compact_currency(&allocator, service, tags, count);
    test_compact_currency_names(&allocator, service, tags, count);
    test_currency_patterns(&allocator);
    test_name_templates(&allocator);
    test_failures(&allocator, service, en, count, QJS_INTL_NUMBER_STANDARD);
    test_failures(&allocator, service, en, count, QJS_INTL_NUMBER_SCIENTIFIC);
    test_failures(&allocator, service, en, count, QJS_INTL_NUMBER_COMPACT);
    for (i = 0; i < count; i++) {
        QJSIntlNumberOptions currency_settings = settings;
        assert(!qjs_intl_number_default_system(service, i, &system) && system.length);
        assert(!qjs_intl_number_formatter_open(service, i, system, &settings, &formatter));
        assert(!qjs_intl_number_format(formatter, &input, &result));
        assert(result.length && result.part_count);
        qjs_intl_number_result_clear(&allocator, &result);
        qjs_intl_number_formatter_close(formatter);
        currency_settings.style = QJS_INTL_NUMBER_CURRENCY;
        currency_settings.currency = bytes("USD");
        currency_settings.currency_display = QJS_INTL_NUMBER_CURRENCY_NAME;
        assert(!qjs_intl_number_formatter_open(service, i, system, &currency_settings, &formatter));
        assert(!qjs_intl_number_format(formatter, &input, &result));
        assert(result.length && result.part_count);
        qjs_intl_number_result_clear(&allocator, &result);
        qjs_intl_number_formatter_close(formatter);
        for (currency_settings.compact_long = 0; currency_settings.compact_long < 2;
             currency_settings.compact_long++) {
            currency_settings.notation = QJS_INTL_NUMBER_COMPACT;
            assert(!qjs_intl_number_formatter_open(service, i, system, &currency_settings, &formatter));
            assert(!qjs_intl_number_format(formatter, &input, &result));
            assert(result.length && result.part_count);
            qjs_intl_number_result_clear(&allocator, &result);
            qjs_intl_number_formatter_close(formatter);
        }
        currency_settings.compact_long = 0;
        currency_settings.notation = QJS_INTL_NUMBER_STANDARD;
        for (currency_settings.currency_display = QJS_INTL_NUMBER_CURRENCY_SYMBOL;
             currency_settings.currency_display < QJS_INTL_NUMBER_CURRENCY_NAME;
             currency_settings.currency_display++) {
            assert(!qjs_intl_number_formatter_open(service, i, system, &currency_settings, &formatter));
            assert(!qjs_intl_number_format(formatter, &input, &result));
            assert(result.length && result.part_count);
            qjs_intl_number_result_clear(&allocator, &result);
            qjs_intl_number_formatter_close(formatter);
            currency_settings.notation = QJS_INTL_NUMBER_COMPACT;
            assert(!qjs_intl_number_formatter_open(service, i, system, &currency_settings, &formatter));
            assert(!qjs_intl_number_format(formatter, &input, &result));
            assert(result.length && result.part_count);
            qjs_intl_number_result_clear(&allocator, &result);
            qjs_intl_number_formatter_close(formatter);
            currency_settings.currency_sign = QJS_INTL_NUMBER_CURRENCY_ACCOUNTING;
            currency_settings.sign_display = QJS_INTL_NUMBER_SIGN_ALWAYS;
            assert(!qjs_intl_number_formatter_open(service, i, system, &currency_settings, &formatter));
            assert(!qjs_intl_number_format(formatter, &input, &result));
            assert(result.length && result.part_count);
            qjs_intl_number_result_clear(&allocator, &result);
            {
                QJSIntlMathematicalValue negative = value("-1234.5");
                assert(!qjs_intl_number_format(formatter, &negative, &result));
                assert(result.length && result.part_count);
                qjs_intl_number_result_clear(&allocator, &result);
            }
            qjs_intl_number_formatter_close(formatter);
            currency_settings.currency_sign = QJS_INTL_NUMBER_CURRENCY_STANDARD;
            currency_settings.sign_display = QJS_INTL_NUMBER_SIGN_AUTO;
            currency_settings.notation = QJS_INTL_NUMBER_STANDARD;
        }
        {
            QJSIntlNumberOptions unit_settings = settings;
            unit_settings.style = QJS_INTL_NUMBER_UNIT;
            unit_settings.unit = bytes("meter");
            unit_settings.unit_display = QJS_INTL_NUMBER_UNIT_LONG;
            for (unit_settings.notation = QJS_INTL_NUMBER_STANDARD;
                 unit_settings.notation <= QJS_INTL_NUMBER_COMPACT;
                 unit_settings.notation += QJS_INTL_NUMBER_COMPACT) {
                assert(!qjs_intl_number_formatter_open(service, i, system, &unit_settings, &formatter));
                assert(!qjs_intl_number_format(formatter, &input, &result));
                assert(result.length && result.part_count);
                qjs_intl_number_result_clear(&allocator, &result);
                qjs_intl_number_formatter_close(formatter);
            }
        }
    }
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &formatter));
    settings.style = QJS_INTL_NUMBER_CURRENCY;
    settings.currency_display = QJS_INTL_NUMBER_CURRENCY_NAME;
    settings.currency = bytes("USD");
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &currency_formatter));
    settings.style = QJS_INTL_NUMBER_UNIT;
    settings.unit = bytes("meter");
    settings.unit_display = QJS_INTL_NUMBER_UNIT_LONG;
    assert(!qjs_intl_number_formatter_open(service, en, (QJSIntlBytes){0}, &settings, &unit_formatter));
    qjs_intl_number_close(service);
    assert(!qjs_intl_number_format(formatter, &input, &result));
    qjs_intl_number_formatter_close(formatter);
    expected(&result, "1,234.5");
    qjs_intl_number_result_clear(&allocator, &result);
    assert(!qjs_intl_number_format(currency_formatter, &input, &result));
    qjs_intl_number_formatter_close(currency_formatter);
    expected(&result, "1,234.5 US dollars");
    qjs_intl_number_result_clear(&allocator, &result);
    assert(!qjs_intl_number_format(unit_formatter, &input, &result));
    qjs_intl_number_formatter_close(unit_formatter);
    expected(&result, "1,234.5 meters");
    qjs_intl_number_result_clear(&allocator, &result);
    qjs_intl_number_close(NULL);
    qjs_intl_number_formatter_close(NULL);
    qjs_intl_number_result_clear(&allocator, NULL);
    assert(!state.live);
    return 0;
}
