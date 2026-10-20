/* Localized GMT offsets, copied sources and one typed timezone field.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/numeric.h"
#include "../src/intl/datetime/zone-offset.h"
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

static QJSIntlFormatted render(const QJSIntlDateOffset *format, const QJSIntlDateNumberFormats *numbers,
                                int32_t offset, unsigned int long_format, const char *text)
{
    QJSIntlFormatted result;
    assert(!qjs_intl_date_offset_format(format, offset, long_format, numbers->numeric,
                                       numbers->two_digit, 1000, &result));
    expected(&result, text);
    assert(result.part_count == 1 && result.parts[0].type == QJS_INTL_PART_TIME_ZONE_NAME &&
           result.parts[0].start == 0 && result.parts[0].end == result.length);
    return result;
}

static void check(const QJSIntlDateOffset *format, const QJSIntlDateNumberFormats *numbers,
                   int32_t offset, unsigned int long_format, const char *text)
{
    QJSIntlFormatted result = render(format, numbers, offset, long_format, text);
    qjs_intl_formatted_clear(&format->allocator, &result);
}

static void authored(const QJSIntlAllocator *allocator, const QJSIntlDateNumberFormats *numbers)
{
    QJSIntlDateOffset format, zero = {0};
    QJSIntlFormatted result, empty = {0};
    Allocation *state = allocator->opaque;
    size_t baseline, calls, i;
    char source[] = "'+'HH'.'mm'!';'−'H'.'mm'?'", wrapper[] = "'UTC'{0}😀", utc[] = "zero";
    static const char *const invalid[] = {
        "HH:mm", ";-HH:mm", "+HH:mm;", "+HH:mm;-HH:mm;HH:mm", "'HH:mm;-HH:mm",
        "+HHH:mm;-HH:mm", "+HH:m;-HH:mm", "+hh:mm;-HH:mm", "+mm:HH;-HH:mm",
        "+HH:mm:ss;-HH:mm", "+HH:mm;-HH:mm y", "+HH:mm;bad", "\xffHH:mm;-HH:mm",
    };
    baseline = state->live;
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        assert(qjs_intl_date_offset_open(allocator, bytes(invalid[i]), bytes("GMT{0}"), bytes("GMT"),
                                         &format) == QJS_INTL_DATA_ERROR);
        assert(!memcmp(&format, &zero, sizeof(format)) && state->live == baseline);
    }
    assert(qjs_intl_date_offset_open(allocator, bytes("+HH:mm;-HH:mm"), bytes("{1}"), bytes("GMT"),
                                     &format) == QJS_INTL_DATA_ERROR);
    assert(qjs_intl_date_offset_open(allocator, bytes("+HH:mm;-HH:mm"), bytes("{0}{0}"), bytes("GMT"),
                                     &format) == QJS_INTL_DATA_ERROR);
    assert(qjs_intl_date_offset_open(allocator, bytes("+HH:mm;-HH:mm"), bytes("\xff{0}"), bytes("GMT"),
                                     &format) == QJS_INTL_DATA_ERROR);
    assert(qjs_intl_date_offset_open(allocator, bytes(""), bytes("{0}"), bytes("GMT"),
                                     &format) == QJS_INTL_UNSUPPORTED);
    state->calls = 0;
    assert(!qjs_intl_date_offset_open(allocator, bytes(source), bytes(wrapper), bytes(utc), &format));
    calls = state->calls;
    qjs_intl_date_offset_clear(&format);
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        assert(qjs_intl_date_offset_open(allocator, bytes(source), bytes(wrapper), bytes(utc),
                                         &format) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&format, &zero, sizeof(format)) && state->live == baseline);
    }
    state->calls = state->fail_at = 0;
    assert(!qjs_intl_date_offset_open(allocator, bytes(source), bytes(wrapper), bytes(utc), &format));
    memset(source, 'X', sizeof(source));
    memset(wrapper, 'X', sizeof(wrapper));
    memset(utc, 'X', sizeof(utc));
    check(&format, numbers, 19800, 1, "'UTC'+05.30!😀");
    check(&format, numbers, -19800, 0, "'UTC'−5.30?😀");
    check(&format, numbers, 18000, 0, "'UTC'+5!😀");
    check(&format, numbers, -18000, 1, "'UTC'−05.00?😀");
    check(&format, numbers, 18001, 0, "'UTC'+5.00.01!😀");
    check(&format, numbers, -59, 1, "'UTC'−00.00.59?😀");
    check(&format, numbers, 0, 1, "zero");
    check(&format, numbers, 86399, 0, "'UTC'+23.59.59!😀");
    baseline = state->live;
    state->calls = 0;
    result = render(&format, numbers, -19801, 1, "'UTC'−05.30.01?😀");
    calls = state->calls;
    qjs_intl_formatted_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        assert(qjs_intl_date_offset_format(&format, -19801, 1, numbers->numeric, numbers->two_digit,
                                           1000, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &empty, sizeof(result)) && state->live == baseline);
    }
    state->calls = state->fail_at = 0;
    assert(qjs_intl_date_offset_format(&format, -19801, 1, numbers->numeric, numbers->two_digit,
                                       16, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &empty, sizeof(result)) && state->live == baseline);
    assert(!qjs_intl_date_offset_format(&format, -19801, 1, numbers->numeric, numbers->two_digit,
                                       17, &result));
    qjs_intl_formatted_clear(allocator, &result);
    assert(qjs_intl_date_offset_format(&format, INT32_MIN, 1, numbers->numeric, numbers->two_digit,
                                       1000, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_offset_format(&format, 86400, 1, numbers->numeric, numbers->two_digit,
                                       1000, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_offset_format(&format, 0, 2, numbers->numeric, numbers->two_digit,
                                       1000, &result) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_date_offset_clear(&format);
    assert(!memcmp(&format, &zero, sizeof(format)));
    assert(!qjs_intl_date_offset_open(allocator, bytes("'+;'HH:mm;'-;'HH:mm"), bytes("GMT{0}"), bytes("GMT"),
                                     &format));
    check(&format, numbers, 3600, 0, "GMT+;1");
    check(&format, numbers, -3600, 1, "GMT-;01:00");
    qjs_intl_date_offset_clear(&format);
    assert(!qjs_intl_date_offset_open(allocator, bytes("‎+HH:mm;‎−HH:mm"), bytes("GMT{0}"), bytes("GMT"), &format));
    check(&format, numbers, -19800, 0, "GMT‎−5:30");
    qjs_intl_date_offset_clear(&format);
    assert(!qjs_intl_date_offset_open(allocator, bytes("+HHmm;-HHmm"), bytes("{0}"), bytes("0"), &format));
    check(&format, numbers, 19801, 1, "+053001");
    check(&format, numbers, 18000, 0, "+5");
    qjs_intl_date_offset_clear(&format);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlNumberService *service;
    const QJSIntlBytes *tags;
    QJSIntlDateNumberFormats numbers;
    QJSIntlData pack;
    QJSIntlTimeZoneNameData names;
    QJSIntlDateOffset format;
    QJSIntlFormatted result;
    QJSIntlBytes tag;
    size_t en, count;
    uint32_t locale, parent;
    assert(!qjs_intl_number_open(&allocator, &service));
    assert(!qjs_intl_number_available(service, &tags, &count));
    for (en = 0; en < count; en++)
        if (tags[en].length == 2 && !memcmp(tags[en].data, "en", 2))
            break;
    assert(en < count);
    numbers = open_numbers(service, en, bytes("latn"));
    authored(&allocator, &numbers);
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &pack));
    assert(!qjs_intl_timezone_name_data_open(&pack, &allocator, &names));
    for (locale = 0; locale < pack.locales.count; locale++) {
        assert(!qjs_intl_data_locale(&pack, locale, &tag, &parent));
        if (tag.length == 2 && !memcmp(tag.data, "en", 2))
            break;
    }
    assert(locale < pack.locales.count);
    {
        uint32_t index;
        for (index = 0; index < pack.locales.count; index++) {
            QJSIntlDateOffset candidate;
            QJSIntlFormatted value;
            assert(!qjs_intl_date_offset_from_data(&names, index, &allocator, &candidate));
            assert(!qjs_intl_date_offset_format(&candidate, -28378, 1, numbers.numeric, numbers.two_digit,
                                                1000, &value));
            assert(value.part_count == 1 && value.length && value.parts[0].type == QJS_INTL_PART_TIME_ZONE_NAME);
            qjs_intl_formatted_clear(&allocator, &value);
            qjs_intl_date_offset_clear(&candidate);
        }
    }
    assert(!qjs_intl_date_offset_from_data(&names, locale, &allocator, &format));
    check(&format, &numbers, 19800, 1, "GMT+05:30");
    check(&format, &numbers, 19800, 0, "GMT+5:30");
    check(&format, &numbers, 3600, 0, "GMT+1");
    check(&format, &numbers, 0, 0, "GMT+0");
    check(&format, &numbers, 0, 1, "GMT+00:00");
    check(&format, &numbers, -28378, 1, "GMT-07:52:58");
    close_numbers(&numbers);
    numbers = open_numbers(service, en, bytes("arab"));
    check(&format, &numbers, 19800, 1, "GMT+٠٥:٣٠");
    close_numbers(&numbers);
    numbers = open_numbers(service, en, bytes("mathbold"));
    result = render(&format, &numbers, -19801, 1, "GMT-𝟎𝟓:𝟑𝟎:𝟎𝟏");
    qjs_intl_date_offset_clear(&format);
    close_numbers(&numbers);
    qjs_intl_number_close(service);
    expected(&result, "GMT-𝟎𝟓:𝟑𝟎:𝟎𝟏");
    qjs_intl_formatted_clear(&allocator, &result);
    assert(!state.live);
    return 0;
}
