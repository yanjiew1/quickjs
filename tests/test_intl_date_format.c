/* Single-instant composition: one chronology for fields and zone names.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/format.h"
#include "../src/cutils/cutils.h"
#include <assert.h>
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
    }
    assert(part_end == result->length);
}

static QJSIntlDateNumberFormats open_numbers(const QJSIntlNumberService *service,
                                              size_t locale_index, const char *system)
{
    QJSIntlDateNumberFormats result = {0};
    QJSIntlNumberOptions options = {0};
    QJSIntlNumberFormatter *formatter;
    unsigned int i;
    options.digits = (QJSIntlDecimalOptions){1, 0, 0, 1, 21, 1, QJS_INTL_ROUND_HALF_EXPAND,
                                              QJS_INTL_ROUND_FRACTION, QJS_INTL_TRAILING_ZERO_AUTO, 1000};
    options.maximum_output_length = 1000;
    for (i = 0; i < 3; i++) {
        options.digits.minimum_integer_digits = i + 1;
        assert(!qjs_intl_number_formatter_open(service, locale_index, bytes(system), &options, &formatter));
        result.fraction[i] = formatter;
    }
    result.numeric = result.fraction[0];
    result.two_digit = result.fraction[1];
    return result;
}

static void close_numbers(QJSIntlDateNumberFormats *numbers)
{
    unsigned int i;
    for (i = 0; i < 3; i++)
        qjs_intl_number_formatter_close((QJSIntlNumberFormatter *)numbers->fraction[i]);
    memset(numbers, 0, sizeof(*numbers));
}

static QJSIntlFormatted render(const QJSIntlAllocator *allocator,
                                const QJSIntlDateFormatData *data,
                                const QJSIntlDateNumberFormats *numbers, const char *source,
                                const char *zone_name, int64_t seconds, uint32_t nanosecond,
                                QJSIntlDateHourCycle cycle, const char *text)
{
    QJSIntlDatePattern pattern;
    QJSTimeZoneIdentifier zone;
    QJSIntlFormatted result;
    assert(!qjs_intl_date_pattern_compile(allocator, bytes(source), &pattern));
    assert(!qjs_timezone_parse_identifier(&zone, zone_name, strlen(zone_name)));
    assert(!qjs_intl_date_format(allocator, &pattern, QJS_CAL_GREGORY, seconds, nanosecond,
                                cycle, numbers, data, &zone, 1000, &result));
    qjs_intl_date_pattern_clear(allocator, &pattern);
    expected(&result, text);
    return result;
}

static void ownership(const QJSIntlAllocator *allocator, const QJSIntlDateFormatData *data,
                       const QJSIntlDateNumberFormats *numbers)
{
    static const char source[] = "EEEE, MMMM d, y 'at' h:mm:ss.SSS a zzzz 'end'";
    static const QJSIntlPartType parts[] = {
        QJS_INTL_PART_WEEKDAY, QJS_INTL_PART_LITERAL,
        QJS_INTL_PART_MONTH, QJS_INTL_PART_LITERAL,
        QJS_INTL_PART_DAY, QJS_INTL_PART_LITERAL,
        QJS_INTL_PART_YEAR, QJS_INTL_PART_LITERAL,
        QJS_INTL_PART_HOUR, QJS_INTL_PART_LITERAL,
        QJS_INTL_PART_MINUTE, QJS_INTL_PART_LITERAL,
        QJS_INTL_PART_SECOND, QJS_INTL_PART_LITERAL,
        QJS_INTL_PART_FRACTIONAL_SECOND, QJS_INTL_PART_LITERAL,
        QJS_INTL_PART_DAY_PERIOD, QJS_INTL_PART_LITERAL,
        QJS_INTL_PART_TIME_ZONE_NAME, QJS_INTL_PART_LITERAL,
    };
    QJSIntlDatePattern pattern;
    QJSTimeZoneIdentifier zone;
    QJSIntlFormatted result, zero = {0};
    Allocation *state = allocator->opaque;
    size_t baseline, calls, limit, i;
    const char *zone_name = qjs_timezone_count() > 1 ? "America/New_York" : "-04:00";

    assert(!qjs_intl_date_pattern_compile(allocator, bytes(source), &pattern));
    assert(!qjs_timezone_parse_identifier(&zone, zone_name, strlen(zone_name)));
    baseline = state->live;
    state->calls = 0;
    assert(!qjs_intl_date_format(allocator, &pattern, QJS_CAL_GREGORY, 1719835200, 123000000,
                                QJS_INTL_DATE_H12, numbers, data, &zone, 1000, &result));
    calls = state->calls;
    limit = result.length;
    expected(&result, qjs_timezone_count() > 1 ?
             "Monday, July 1, 2024 at 8:00:00.123 AM Eastern Daylight Time end" :
             "Monday, July 1, 2024 at 8:00:00.123 AM GMT-04:00 end");
    assert(result.part_count == sizeof(parts) / sizeof(*parts));
    for (i = 0; i < result.part_count; i++)
        assert(result.parts[i].type == parts[i]);
    qjs_intl_formatted_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_format(allocator, &pattern, QJS_CAL_GREGORY, 1719835200, 123000000,
                                    QJS_INTL_DATE_H12, numbers, data, &zone, 1000, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    }
    state->calls = state->fail_at = 0;
    assert(qjs_intl_date_format(allocator, &pattern, QJS_CAL_GREGORY, 1719835200, 123000000,
                                QJS_INTL_DATE_H12, numbers, data, &zone, limit - 1, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    assert(!qjs_intl_date_format(allocator, &pattern, QJS_CAL_GREGORY, 1719835200, 123000000,
                                QJS_INTL_DATE_H12, numbers, data, &zone, limit, &result));
    qjs_intl_formatted_clear(allocator, &result);
    zone.offset_nanoseconds = 1;
    zone.is_offset = 1;
    assert(qjs_intl_date_format(allocator, &pattern, QJS_CAL_GREGORY, 0, 0,
                                QJS_INTL_DATE_H12, numbers, data, &zone, 1000, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    qjs_intl_date_pattern_clear(allocator, &pattern);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData pack;
    QJSIntlLocaleMetadata metadata;
    QJSIntlCalendarNameData names;
    QJSIntlCalendarEraData eras;
    QJSIntlDayPeriodData periods;
    QJSIntlDayPeriodRuleList rules;
    QJSIntlTimeZoneNameData zones;
    QJSIntlTimeZonePresentationData presentation;
    QJSIntlNameData regions;
    QJSIntlDateOffset offset;
    QJSIntlDateFormatData data = {0};
    QJSIntlNumberService *service;
    QJSIntlDateNumberFormats numbers;
    QJSIntlFormatted result;
    const QJSIntlBytes *tags;
    size_t en, count;
    uint32_t calendar;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &pack));
    assert(!qjs_intl_locale_metadata_open(&pack, &allocator, &metadata));
    assert(!qjs_intl_calendar_name_data_open(&pack, &allocator, &names));
    assert(!qjs_intl_calendar_era_data_open(&pack, &allocator, &eras));
    assert(!qjs_intl_dayperiod_data_open(&pack, &allocator, &periods));
    assert(!qjs_intl_dayperiod_resolve(&periods, QJS_INTL_DAYPERIOD_FORMAT, bytes("en"), &rules));
    assert(!qjs_intl_timezone_name_data_open(&pack, &allocator, &zones));
    assert(!qjs_intl_timezone_presentation_data_open(&pack, &allocator, &presentation));
    assert(!qjs_intl_name_data_open(&pack, &allocator, &regions));
    data.text.names = &names;
    data.text.metadata = &metadata;
    data.text.eras = &eras;
    data.text.periods = &periods;
    data.text.period_rules = &rules;
    data.text.locale = locale(&names.data, "en");
    for (calendar = 0; calendar < names.calendar_count; calendar++) {
        QJSIntlBytes name;
        assert(!qjs_intl_calendar_name_calendar_at(&names, calendar, &name));
        if (name.length == 9 && !memcmp(name.data, "gregorian", 9))
            break;
    }
    assert(calendar < names.calendar_count);
    data.text.calendar = calendar;
    data.zones = (QJSIntlDateZoneData){&zones, &presentation, &metadata, &regions,
                                      locale(&zones.data, "en"), locale(&regions.data, "en"), {"US", 2}};
    assert(!qjs_intl_date_offset_from_data(&zones, data.zones.names_locale, &allocator, &offset));
    data.offset = &offset;
    assert(!qjs_intl_number_open(&allocator, NULL, &service));
    assert(!qjs_intl_number_available(service, &tags, &count));
    for (en = 0; en < count; en++)
        if (tags[en].length == 2 && !memcmp(tags[en].data, "en", 2))
            break;
    assert(en < count);
    numbers = open_numbers(service, en, "latn");
    ownership(&allocator, &data, &numbers);
    if (qjs_timezone_count() > 1) {
        result = render(&allocator, &data, &numbers, "y-MM-dd HH:mm:ss.SSS OOOO", "Africa/Monrovia",
                        0, 0, QJS_INTL_DATE_H23, "1969-12-31 23:15:30.000 GMT-00:44:30");
        qjs_intl_formatted_clear(&allocator, &result);
    }
    result = render(&allocator, &data, &numbers, "OOOO y-MM-dd HH:mm:ss.SSS", "+00:30",
                    -1, 999999999, QJS_INTL_DATE_H23, "GMT+00:30 1970-01-01 00:29:59.999");
    qjs_intl_formatted_clear(&allocator, &result);
    result = render(&allocator, &data, &numbers, "y-MM-dd HH:mm:ss.SSS", "UTC",
                    -1, 999999999, QJS_INTL_DATE_H23, "1969-12-31 23:59:59.999");
    qjs_intl_formatted_clear(&allocator, &result);
    if (qjs_timezone_count() > 1) {
        result = render(&allocator, &data, &numbers, "zzzz", "America/New_York",
                        1719835200, 0, QJS_INTL_DATE_H23, "Eastern Daylight Time");
        qjs_intl_formatted_clear(&allocator, &result);
    }
    close_numbers(&numbers);
    numbers = open_numbers(service, en, "mathbold");
    result = render(&allocator, &data, &numbers, "yy O HH", "+01:00", 0, 0,
                    QJS_INTL_DATE_H23, "𝟕𝟎 GMT+𝟏 𝟎𝟏");
    close_numbers(&numbers);
    qjs_intl_number_close(service);
    qjs_intl_date_offset_clear(&offset);
    memset(&data, 0, sizeof(data));
    memset(&names, 0, sizeof(names));
    memset(&zones, 0, sizeof(zones));
    expected(&result, "𝟕𝟎 GMT+𝟏 𝟎𝟏");
    qjs_intl_formatted_clear(&allocator, &result);
    assert(!state.live);
    return 0;
}
