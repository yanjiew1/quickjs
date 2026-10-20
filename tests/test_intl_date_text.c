/* Executable numeric date text and date-specific UTF-16 parts.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/text.h"
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

extern const uint8_t qjs_intl_date_text_fixture_bytes[];
extern const size_t qjs_intl_date_text_fixture_length;

static uint32_t calendar(const QJSIntlCalendarNameData *data, const char *name)
{
    uint32_t i;
    for (i = 0; i < data->calendar_count; i++) {
        QJSIntlBytes value;
        assert(!qjs_intl_calendar_name_calendar_at(data, i, &value));
        if (value.length == strlen(name) && !memcmp(value.data, name, value.length))
            return i;
    }
    abort();
}

static uint32_t locale(const QJSIntlData *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes value;
        assert(!qjs_intl_data_locale(data, i, &value, &parent));
        if (value.length == strlen(name) && !memcmp(value.data, name, value.length))
            return i;
    }
    abort();
}

static QJSIntlFormatted render(const QJSIntlAllocator *allocator,
                                const QJSIntlDateNumberFormats *numbers,
                                const QJSIntlDateTextData *data, const QJSIntlDateCalendarFields *local,
                                unsigned int subsecond, const char *source)
{
    QJSIntlDatePattern pattern;
    QJSIntlFormatted result;
    assert(!qjs_intl_date_pattern_compile(allocator, bytes(source), &pattern));
    assert(!qjs_intl_date_text_format(allocator, &pattern, local, subsecond, QJS_INTL_DATE_H12,
                                     numbers, data, 1000, &result));
    qjs_intl_date_pattern_clear(allocator, &pattern);
    return result;
}

static void check(const QJSIntlAllocator *allocator, const QJSIntlDateNumberFormats *numbers,
                   const QJSIntlDateTextData *data, const QJSIntlDateCalendarFields *local,
                   unsigned int subsecond, const char *source, const char *text)
{
    QJSIntlFormatted result = render(allocator, numbers, data, local, subsecond, source);
    expected(&result, text);
    qjs_intl_formatted_clear(allocator, &result);
}

static void failure(const QJSIntlAllocator *allocator, const QJSIntlDateNumberFormats *numbers,
                     const QJSIntlDateTextData *data, const QJSIntlDateCalendarFields *local,
                     const char *source, QJSIntlStatus error)
{
    QJSIntlDatePattern pattern;
    QJSIntlFormatted result, zero = {0};
    Allocation *state = allocator->opaque;
    size_t live;
    assert(!qjs_intl_date_pattern_compile(allocator, bytes(source), &pattern));
    live = state->live;
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_text_format(allocator, &pattern, local, 0, QJS_INTL_DATE_H12,
                                    numbers, data, 1000, &result) == error);
    assert(!memcmp(&result, &zero, sizeof(result)) && live == state->live);
    qjs_intl_date_pattern_clear(allocator, &pattern);
}

static void names(const QJSIntlAllocator *allocator, const QJSIntlDateNumberFormats *numbers,
                   QJSIntlDateTextData data)
{
    QJSIntlDateCalendarFields local = {0};
    QJSIntlFormatted result;
    local.numeric = (QJSIntlDateNumericFields){2021, 2021, 1, 3, 12, 2, 3, 0};
    local.has_era = 1;
    local.era_calendar = QJS_CAL_GREGORY;
    strcpy(local.era_code, "ce");
    strcpy(local.month_key, "1");
    data.locale = locale(&data.names->data, "root");
    data.calendar = calendar(data.names, "gregorian");
    result = render(allocator, numbers, &data, &local, 0, "EEEE, MMMM d y G 'at' h:mm b");
    expected(&result, "Sunday, January 3 2021 CE at 12:02 PM");
    assert(result.parts[0].type == QJS_INTL_PART_WEEKDAY && result.parts[2].type == QJS_INTL_PART_MONTH &&
           result.parts[8].type == QJS_INTL_PART_ERA && result.parts[14].type == QJS_INTL_PART_DAY_PERIOD);
    qjs_intl_formatted_clear(allocator, &result);
    check(allocator, numbers, &data, &local, 0, "EEEEEE", "Sun");
    local.numeric.hour = 9;
    check(allocator, numbers, &data, &local, 0, "B", "morning");
    local.numeric.hour = 12;
    local.numeric.minute = 0;
    check(allocator, numbers, &data, &local, 0, "b", "PM");
    local.numeric.second = 0;
    check(allocator, numbers, &data, &local, 0, "b", "NOON");
    check(allocator, numbers, &data, &local, 1, "b", "PM"); /* Noon plus one nanosecond. */
    data.locale = locale(&data.names->data, "en");
    check(allocator, numbers, &data, &local, 0, "b", "PM"); /* Explicit empty noon, allowed AM/PM substitution. */
    strcpy(local.month_key, "2");
    failure(allocator, numbers, &data, &local, "MMMM", QJS_INTL_UNSUPPORTED); /* Block. */
    strcpy(local.month_key, "3");
    failure(allocator, numbers, &data, &local, "MMMM", QJS_INTL_UNSUPPORTED); /* Empty. */
    strcpy(local.month_key, "4");
    failure(allocator, numbers, &data, &local, "MMMM", QJS_INTL_UNSUPPORTED); /* Missing. */
    failure(allocator, numbers, &data, &local, "z", QJS_INTL_UNSUPPORTED);
    failure(allocator, numbers, &data, &local, "Y", QJS_INTL_UNSUPPORTED);
    failure(allocator, numbers, &data, &local, "U", QJS_INTL_UNSUPPORTED);
    data.calendar = calendar(data.names, "iso8601");
    check(allocator, numbers, &data, &local, 0, "G", "ISO-CE");
    data.calendar = calendar(data.names, "japanese");
    local.numeric.year = 3;
    local.era_calendar = QJS_CAL_JAPANESE;
    strcpy(local.era_code, "reiwa");
    check(allocator, numbers, &data, &local, 0, "y G", "3 Reiwa");
    strcpy(local.era_code, "next-era");
    check(allocator, numbers, &data, &local, 0, "y G", "3 Future");
    local.numeric.year = 0;
    local.era_calendar = QJS_CAL_GREGORY;
    strcpy(local.era_code, "bce");
    check(allocator, numbers, &data, &local, 0, "y G", "1 BCE");
    data.calendar = calendar(data.names, "hebrew");
    local.in_leap_year = local.leap_month = 1;
    strcpy(local.month_key, "6");
    check(allocator, numbers, &data, &local, 0, "MMMM", "Adar I");
    local.leap_month = 0;
    strcpy(local.month_key, "7");
    check(allocator, numbers, &data, &local, 0, "MMMM", "Adar II");
    local.in_leap_year = 0;
    check(allocator, numbers, &data, &local, 0, "MMMM", "Adar");
    local.has_era = 0;
    local.has_cyclic_year = local.in_leap_year = local.leap_month = 1;
    strcpy(local.month_key, "2");
    strcpy(local.cyclic_key, "40");
    local.numeric.related_year = 2023;
    local.numeric.month = 2;
    data.calendar = calendar(data.names, "chinese");
    check(allocator, numbers, &data, &local, 0, "r U MMMM", "2023 cycle四十 閏'second月'!");
    check(allocator, numbers, &data, &local, 0, "LLLL", "(standalone月)閏");
    check(allocator, numbers, &data, &local, 0, "M", "2");
    check(allocator, numbers, &data, &local, 0, "MM", "02");
    failure(allocator, numbers, &data, &local, "G", QJS_INTL_UNSUPPORTED);
    failure(allocator, numbers, &data, &local, "MMM", QJS_INTL_DATA_ERROR);
    failure(allocator, numbers, &data, &local, "MMMMM", QJS_INTL_UNSUPPORTED);
    data.calendar = calendar(data.names, "dangi");
    check(allocator, numbers, &data, &local, 0, "U MMMM", "cycle四十 閏'second月'!");
    local.leap_month = 0;
    check(allocator, numbers, &data, &local, 0, "MMMM", "second月");
}

static void ownership(const QJSIntlAllocator *allocator, const QJSIntlDateNumberFormats *numbers,
                       QJSIntlDateTextData data)
{
    QJSIntlDateCalendarFields local = {0};
    QJSIntlDatePattern pattern;
    QJSIntlFormatted result, zero = {0};
    Allocation *state = allocator->opaque;
    size_t baseline, calls, i, limit;
    local.numeric = (QJSIntlDateNumericFields){2021, 2021, 1, 3, 12, 2, 3, 987};
    local.has_era = 1;
    local.era_calendar = QJS_CAL_GREGORY;
    strcpy(local.era_code, "ce");
    strcpy(local.month_key, "1");
    data.calendar = calendar(data.names, "gregorian");
    data.locale = locale(&data.names->data, "en");
    assert(!qjs_intl_date_pattern_compile(allocator, bytes("EEEE MMMM d y G HH:mm:ss.SSS a"), &pattern));
    baseline = state->live;
    state->calls = 0;
    assert(!qjs_intl_date_text_format(allocator, &pattern, &local, 1, QJS_INTL_DATE_H23,
                                     numbers, &data, 1000, &result));
    calls = state->calls;
    limit = result.length;
    expected(&result, "Sunday January 3 2021 CE 12:02:03.987 PM");
    qjs_intl_formatted_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_text_format(allocator, &pattern, &local, 1, QJS_INTL_DATE_H23,
                                        numbers, &data, 1000, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    }
    state->calls = state->fail_at = 0;
    assert(qjs_intl_date_text_format(allocator, &pattern, &local, 1, QJS_INTL_DATE_H23,
                                    numbers, &data, limit - 1, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    assert(!qjs_intl_date_text_format(allocator, &pattern, &local, 1, QJS_INTL_DATE_H23,
                                     numbers, &data, limit, &result));
    qjs_intl_formatted_clear(allocator, &result);
    assert(qjs_intl_date_text_format(allocator, &pattern, &local, 0, QJS_INTL_DATE_H23,
                                    numbers, &data, 1000, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    qjs_intl_date_pattern_clear(allocator, &pattern);
}

static void prepared_names(const QJSIntlAllocator *allocator, const QJSIntlDateNumberFormats *numbers,
                            const QJSIntlData *prepared, const QJSIntlLocaleMetadata *metadata)
{
    QJSIntlCalendarNameData names;
    QJSIntlCalendarEraData eras;
    QJSIntlDateTextData data = {0};
    QJSIntlDateCalendarFields local = {0};
    assert(!qjs_intl_calendar_name_data_open(prepared, allocator, &names));
    assert(!qjs_intl_calendar_era_data_open(prepared, allocator, &eras));
    data.names = &names;
    data.eras = &eras;
    data.metadata = metadata;
    data.locale = locale(prepared, "en");
    data.calendar = calendar(&names, "gregorian");
    local.numeric = (QJSIntlDateNumericFields){2021, 2021, 1, 3, 12, 2, 3, 0};
    local.has_era = 1;
    local.era_calendar = QJS_CAL_GREGORY;
    strcpy(local.era_code, "ce");
    strcpy(local.month_key, "1");
    check(allocator, numbers, &data, &local, 0, "EEEE, MMMM d y G", "Sunday, January 3 2021 AD");
    data.calendar = calendar(&names, "japanese");
    local.numeric.year = 3;
    local.era_calendar = QJS_CAL_JAPANESE;
    strcpy(local.era_code, "reiwa");
    check(allocator, numbers, &data, &local, 0, "y G", "3 Reiwa");
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlNumberService *service;
    const QJSIntlBytes *tags;
    QJSIntlDateNumberFormats numbers;
    QJSIntlData prepared, authored;
    QJSIntlCalendarNameData name_data;
    QJSIntlCalendarEraData eras;
    QJSIntlDayPeriodData periods;
    QJSIntlDayPeriodRuleList rules;
    QJSIntlLocaleMetadata metadata;
    QJSIntlDateTextData data = {0};
    QJSIntlDateCalendarFields local = {0};
    QJSIntlFormatted result;
    size_t en, count;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &prepared));
    assert(!qjs_intl_locale_metadata_open(&prepared, &allocator, &metadata));
    assert(!qjs_intl_dayperiod_data_open(&prepared, &allocator, &periods));
    assert(!qjs_intl_dayperiod_resolve(&periods, QJS_INTL_DAYPERIOD_FORMAT, bytes("en"), &rules));
    assert(!qjs_intl_data_open(qjs_intl_date_text_fixture_bytes, qjs_intl_date_text_fixture_length,
                              &allocator, &authored));
    assert(!qjs_intl_calendar_name_data_open(&authored, &allocator, &name_data));
    assert(!qjs_intl_calendar_era_data_open(&authored, &allocator, &eras));
    data.names = &name_data;
    data.metadata = &metadata;
    data.eras = &eras;
    data.periods = &periods;
    data.period_rules = &rules;
    assert(!qjs_intl_number_open(&allocator, &service));
    assert(!qjs_intl_number_available(service, &tags, &count));
    for (en = 0; en < count; en++)
        if (tags[en].length == 2 && !memcmp(tags[en].data, "en", 2))
            break;
    assert(en < count);
    numbers = open_numbers(service, en, bytes("latn"));
    names(&allocator, &numbers, data);
    ownership(&allocator, &numbers, data);
    prepared_names(&allocator, &numbers, &prepared, &metadata);
    close_numbers(&numbers);
    data.calendar = calendar(data.names, "gregorian");
    data.locale = locale(&authored, "en");
    local.numeric = (QJSIntlDateNumericFields){2021, 2021, 1, 3, 0, 2, 3, 0};
    strcpy(local.month_key, "1");
    numbers = open_numbers(service, en, bytes("arab"));
    check(&allocator, &numbers, &data, &local, 0, "MMMM yy", "January ٢١");
    close_numbers(&numbers);
    numbers = open_numbers(service, en, bytes("mathbold"));
    result = render(&allocator, &numbers, &data, &local, 0, "MMMM yy");
    close_numbers(&numbers);
    qjs_intl_number_close(service);
    memset(&name_data, 0, sizeof(name_data));
    memset(&metadata, 0, sizeof(metadata));
    expected(&result, "January 𝟐𝟏");
    assert(result.parts[2].end - result.parts[2].start == 4);
    qjs_intl_formatted_clear(&allocator, &result);
    assert(!state.live);
    return 0;
}
