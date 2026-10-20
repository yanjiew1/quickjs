/* Executable numeric date text and date-specific UTF-16 parts.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/numeric.h"
#include "../src/cutils/cutils.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;

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

static QJSIntlBytes bytes(const char *text) { return (QJSIntlBytes){text, strlen(text)}; }

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
            assert(position < result->length && result->text[position++] == c);
        }
        at = next;
    }
    assert(position == result->length);
    for (i = 0; i < result->part_count; i++) {
        assert(result->parts[i].start == part_end);
        part_end = result->parts[i].end;
        assert(part_end > result->parts[i].start && part_end <= result->length);
        assert(result->parts[i].type == QJS_INTL_PART_LITERAL ||
               ((unsigned int)result->parts[i].type >= QJS_INTL_PART_NUMBER_COUNT &&
                (unsigned int)result->parts[i].type < QJS_INTL_PART_COUNT));
    }
    assert(part_end == result->length);
}

static QJSIntlDateNumberFormats open_numbers(const QJSIntlNumberService *service, size_t locale,
                                              QJSIntlBytes system)
{
    QJSIntlDateNumberFormats result = {0};
    QJSIntlNumberOptions options = {0};
    QJSIntlNumberFormatter *formatter;
    unsigned int i;
    options.digits = (QJSIntlDecimalOptions){1, 0, 0, 1, 21, 1, QJS_INTL_ROUND_HALF_EXPAND,
                                              QJS_INTL_ROUND_FRACTION, QJS_INTL_TRAILING_ZERO_AUTO, 1000};
    options.maximum_output_length = 1000;
    assert(!qjs_intl_number_formatter_open(service, locale, system, &options, &formatter));
    result.numeric = formatter;
    options.digits.minimum_integer_digits = 2;
    assert(!qjs_intl_number_formatter_open(service, locale, system, &options, &formatter));
    result.two_digit = formatter;
    for (i = 0; i < 3; i++) {
        options.digits.minimum_integer_digits = i + 1;
        assert(!qjs_intl_number_formatter_open(service, locale, system, &options, &formatter));
        result.fraction[i] = formatter;
    }
    return result;
}

static void close_numbers(QJSIntlDateNumberFormats *numbers)
{
    unsigned int i;
    qjs_intl_number_formatter_close((QJSIntlNumberFormatter *)numbers->numeric);
    qjs_intl_number_formatter_close((QJSIntlNumberFormatter *)numbers->two_digit);
    for (i = 0; i < 3; i++)
        qjs_intl_number_formatter_close((QJSIntlNumberFormatter *)numbers->fraction[i]);
    memset(numbers, 0, sizeof(*numbers));
}

static void check(const QJSIntlAllocator *allocator, const QJSIntlDateNumberFormats *numbers,
                   const QJSIntlDateNumericFields *local, const char *source,
                   QJSIntlDateHourCycle cycle, const char *text)
{
    QJSIntlDatePattern pattern;
    QJSIntlFormatted result;
    assert(!qjs_intl_date_pattern_compile(allocator, bytes(source), &pattern));
    assert(!qjs_intl_date_numeric_format(allocator, &pattern, local, cycle, numbers, 1000, &result));
    qjs_intl_date_pattern_clear(allocator, &pattern);
    expected(&result, text);
    qjs_intl_formatted_clear(allocator, &result);
}

static void test_latn(const QJSIntlAllocator *allocator, const QJSIntlDateNumberFormats *numbers)
{
    QJSIntlDateNumericFields local = {2026, 2026, 10, 20, 0, 2, 3, 987};
    QJSIntlDatePattern pattern;
    QJSIntlFormatted result, zero = {0};
    Allocation *state = allocator->opaque;
    size_t calls, baseline, i;
    check(allocator, numbers, &local, "yyyy-MM-dd HH:mm:ss.SSS", QJS_INTL_DATE_H23,
          "2026-10-20 00:02:03.987");
    check(allocator, numbers, &local, "H:m:s.S", QJS_INTL_DATE_H11, "0:2:3.9");
    check(allocator, numbers, &local, "HH:mm:ss.SS", QJS_INTL_DATE_H12, "12:02:03.98");
    check(allocator, numbers, &local, "HH:mm", QJS_INTL_DATE_H24, "24:02");
    local.hour = 13;
    check(allocator, numbers, &local, "HH:mm", QJS_INTL_DATE_H11, "01:02");
    check(allocator, numbers, &local, "hh:mm", QJS_INTL_DATE_H23, "13:02");
    local.year = 7;
    check(allocator, numbers, &local, "yyyy 'year' 😀", QJS_INTL_DATE_H23, "7 year 😀");
    local.year = -1;
    local.related_year = -1;
    check(allocator, numbers, &local, "yy", QJS_INTL_DATE_H23, "02");
    check(allocator, numbers, &local, "rr", QJS_INTL_DATE_H23, "-1");
    local.year = INT32_MIN;
    local.related_year = INT32_MIN;
    check(allocator, numbers, &local, "y", QJS_INTL_DATE_H23, "2147483649");
    check(allocator, numbers, &local, "r", QJS_INTL_DATE_H23, "-2147483648");
    local.millisecond = 9;
    check(allocator, numbers, &local, "ss.SSS", QJS_INTL_DATE_H23, "03.009");
    check(allocator, numbers, &local, "ss.SS", QJS_INTL_DATE_H23, "03.00");
    assert(!qjs_intl_date_pattern_compile(allocator, bytes("yy-MM-dd HH:mm:ss.SSS"), &pattern));
    baseline = state->live;
    state->calls = 0;
    assert(!qjs_intl_date_numeric_format(allocator, &pattern, &local, QJS_INTL_DATE_H23, numbers, 1000, &result));
    calls = state->calls;
    assert(result.part_count == 13 && result.parts[0].type == QJS_INTL_PART_YEAR &&
           result.parts[2].type == QJS_INTL_PART_MONTH && result.parts[4].type == QJS_INTL_PART_DAY &&
           result.parts[6].type == QJS_INTL_PART_HOUR && result.parts[8].type == QJS_INTL_PART_MINUTE &&
           result.parts[10].type == QJS_INTL_PART_SECOND && result.parts[12].type == QJS_INTL_PART_FRACTIONAL_SECOND);
    qjs_intl_formatted_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_numeric_format(allocator, &pattern, &local, QJS_INTL_DATE_H23,
                                           numbers, 1000, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
        state->fail_at = 0;
    }
    assert(qjs_intl_date_numeric_format(allocator, &pattern, &local, QJS_INTL_DATE_H23,
                                       numbers, 1, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    local.second = 60;
    assert(qjs_intl_date_numeric_format(allocator, &pattern, &local, QJS_INTL_DATE_H23,
                                       numbers, 1000, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    local.second = 3;
    {
        QJSIntlDateNumberFormats missing = *numbers;
        missing.two_digit = NULL;
        assert(qjs_intl_date_numeric_format(allocator, &pattern, &local, QJS_INTL_DATE_H23,
                                           &missing, 1000, &result) == QJS_INTL_INVALID_ARGUMENT);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    }
    qjs_intl_date_pattern_clear(allocator, &pattern);
    {
        QJSIntlDateNumberFormats empty = {0};
        check(allocator, &empty, &local, "'literal 😀'", QJS_INTL_DATE_H23, "literal 😀");
    }
    {
        const char *unsupported[] = {"MMM", "EEEE", "h a", "r(U)", "y G", "z", "Y", "y r"};
        for (i = 0; i < sizeof(unsupported) / sizeof(*unsupported); i++) {
            assert(!qjs_intl_date_pattern_compile(allocator, bytes(unsupported[i]), &pattern));
            baseline = state->live;
            assert(qjs_intl_date_numeric_format(allocator, &pattern, &local, QJS_INTL_DATE_H23,
                                               numbers, 1000, &result) == QJS_INTL_UNSUPPORTED);
            assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
            qjs_intl_date_pattern_clear(allocator, &pattern);
        }
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlNumberService *service;
    const QJSIntlBytes *tags;
    QJSIntlDateNumberFormats numbers;
    QJSIntlDateNumericFields local = {2026, 2026, 10, 20, 0, 2, 3, 987};
    QJSIntlDatePattern pattern;
    QJSIntlFormatted result;
    size_t locale, count;
    assert(!qjs_intl_number_open(&allocator, &service));
    assert(!qjs_intl_number_available(service, &tags, &count));
    for (locale = 0; locale < count; locale++)
        if (tags[locale].length == 2 && !memcmp(tags[locale].data, "en", 2))
            break;
    assert(locale < count);
    numbers = open_numbers(service, locale, bytes("latn"));
    test_latn(&allocator, &numbers);
    close_numbers(&numbers);
    numbers = open_numbers(service, locale, bytes("arab"));
    check(&allocator, &numbers, &local, "yy-MM-dd", QJS_INTL_DATE_H23, "٢٦-١٠-٢٠");
    close_numbers(&numbers);
    numbers = open_numbers(service, locale, bytes("mathbold"));
    assert(!qjs_intl_date_pattern_compile(&allocator, bytes("yy-MM-dd"), &pattern));
    {
        QJSIntlFormatted zero = {0};
        size_t baseline = state.live;
        assert(qjs_intl_date_numeric_format(&allocator, &pattern, &local, QJS_INTL_DATE_H23,
                                           &numbers, 13, &result) == QJS_INTL_OVERFLOW);
        assert(!memcmp(&result, &zero, sizeof(result)) && state.live == baseline);
    }
    assert(!qjs_intl_date_numeric_format(&allocator, &pattern, &local, QJS_INTL_DATE_H23,
                                       &numbers, 14, &result));
    assert(result.parts[0].end == 4); /* Two supplementary code points. */
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    close_numbers(&numbers);
    qjs_intl_number_close(service);
    expected(&result, "𝟐𝟔-𝟏𝟎-𝟐𝟎");
    qjs_intl_formatted_clear(&allocator, &result);
    assert(!state.live);
    return 0;
}
