/* DateTimeFormat owner: selection, independent handles and input-kind records.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/datetime.h"
#include "../src/intl/datetime/interval-select.h"
#include "../src/calendar/calendar.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;

extern const uint8_t qjs_intl_interval_selector_fixture_bytes[];
extern const size_t qjs_intl_interval_selector_fixture_length;

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

static void *resize(void *opaque, void *ptr, size_t size)
{
    Allocation *state = opaque;
    void *result;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    result = realloc(ptr, size);
    if (result && !ptr)
        state->live++;
    return result;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    if (ptr) {
        assert(state->live);
        state->live--;
        free(ptr);
    }
}

static QJSIntlBytes bytes(const char *text) { return (QJSIntlBytes){text, strlen(text)}; }

static void selector_authored(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlData pack;
    QJSIntlDateIntervalData data;
    QJSIntlDatePattern ordinary;
    QJSIntlDateTextData names = {0}; /* Numeric sources need no localized names. */
    QJSIntlDateNameAdmission admission = {0};
    QJSIntlDateIntervalSet set, zero = {0};
    const QJSIntlDateIntervalProgram *program;
    uint32_t locale, calendar, parent;
    size_t calls, i, baseline;
    QJSIntlBytes tag;
    assert(!qjs_intl_data_open(qjs_intl_interval_selector_fixture_bytes,
                              qjs_intl_interval_selector_fixture_length, allocator, &pack));
    assert(!qjs_intl_date_interval_data_open(&pack, allocator, &data));
    for (locale = 0; locale < pack.locales.count; locale++) {
        assert(!qjs_intl_data_locale(&pack, locale, &tag, &parent));
        if (tag.length == 5 && !memcmp(tag.data, "en-GB", 5))
            break;
    }
    for (calendar = 0; calendar < data.calendar_count; calendar++) {
        assert(!qjs_intl_date_interval_calendar_at(&data, calendar, &tag));
        if (tag.length == 7 && !memcmp(tag.data, "generic", 7))
            break;
    }
    assert(locale < pack.locales.count && calendar < data.calendar_count);
    assert(!qjs_intl_date_pattern_compile(allocator, bytes("M/d"), &ordinary));
    baseline = state->live;
    state->calls = 0;
    assert(!qjs_intl_date_interval_set_open(&data, locale, calendar, &ordinary, QJS_INTL_DATE_H23,
                                           QJS_CAL_GREGORY, &names, &admission, allocator, &set));
    calls = state->calls;
    program = qjs_intl_date_interval_set_get(&set, QJS_INTL_DATE_RANGE_MONTH);
    assert(set.count == 1 && program && program->sources[0] == QJS_INTL_RANGE_END);
    /* The generic alias restarts at the requesting locale's Gregorian data.
       Its blocked Md day leaf must not resurrect the root's usable program. */
    assert(!qjs_intl_date_interval_set_get(&set, QJS_INTL_DATE_RANGE_DAY));
    assert(!qjs_intl_date_interval_set_get(&set, QJS_INTL_DATE_RANGE_YEAR));
    qjs_intl_date_interval_set_clear(&set);
    assert(state->live == baseline && calls);
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        memset(&set, 0xff, sizeof(set));
        assert(qjs_intl_date_interval_set_open(&data, locale, calendar, &ordinary, QJS_INTL_DATE_H23,
                   QJS_CAL_GREGORY, &names, &admission, allocator, &set) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&set, &zero, sizeof(set)) && state->live == baseline);
    }
    state->calls = state->fail_at = 0;
    qjs_intl_date_pattern_clear(allocator, &ordinary);

    /* The calendar-date workaround applies to the source skeleton before
       scoring as well as to the selected interval before endpoint splitting. */
    for (calendar = 0; calendar < data.calendar_count; calendar++) {
        assert(!qjs_intl_date_interval_calendar_at(&data, calendar, &tag));
        if (tag.length == 7 && !memcmp(tag.data, "islamic", 7))
            break;
    }
    assert(calendar < data.calendar_count);
    assert(!qjs_intl_date_pattern_compile(allocator, bytes("y/M/d"), &ordinary));
    assert(!qjs_intl_date_interval_set_open(&data, locale, calendar, &ordinary, QJS_INTL_DATE_H23,
                                           QJS_CAL_GREGORY, &names, &admission, allocator, &set));
    program = qjs_intl_date_interval_set_get(&set, QJS_INTL_DATE_RANGE_DAY);
    assert(program);
    for (i = 0; i < program->pattern.count; i++)
        assert(program->pattern.tokens[i].symbol != 'Y');
    qjs_intl_date_interval_set_clear(&set);
    qjs_intl_date_pattern_clear(allocator, &ordinary);

    /* A better-scored source repeats U but shares r. Use real cyclic names
       so rejection cannot accidentally be explained by unavailable name data. */
    {
        QJSIntlData name_pack;
        QJSIntlCalendarNameData name_data;
        assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, allocator, &name_pack));
        assert(!qjs_intl_calendar_name_data_open(&name_pack, allocator, &name_data));
        names.names = &name_data;
        for (names.locale = 0; names.locale < name_pack.locales.count; names.locale++) {
            assert(!qjs_intl_data_locale(&name_pack, names.locale, &tag, &parent));
            if (tag.length == 2 && !memcmp(tag.data, "en", 2))
                break;
        }
        for (names.calendar = 0; names.calendar < name_data.calendar_count; names.calendar++) {
            assert(!qjs_intl_calendar_name_calendar_at(&name_data, names.calendar, &tag));
            if (tag.length == 7 && !memcmp(tag.data, "chinese", 7))
                break;
        }
        for (calendar = 0; calendar < data.calendar_count; calendar++) {
            assert(!qjs_intl_date_interval_calendar_at(&data, calendar, &tag));
            if (tag.length == 7 && !memcmp(tag.data, "chinese", 7))
                break;
        }
        assert(names.locale < name_pack.locales.count && names.calendar < name_data.calendar_count &&
               calendar < data.calendar_count);
        memset(&admission, 0, sizeof(admission));
        assert(!qjs_intl_date_pattern_compile(allocator, bytes("r U"), &ordinary));
        assert(!qjs_intl_date_interval_set_open(&data, locale, calendar, &ordinary, QJS_INTL_DATE_H23,
                                               QJS_CAL_CHINESE, &names, &admission, allocator, &set));
        program = qjs_intl_date_interval_set_get(&set, QJS_INTL_DATE_RANGE_YEAR);
        assert(set.count == 1 && program && program->pattern.count == 7);
        assert(program->sources[0] == QJS_INTL_RANGE_START && program->sources[4] == QJS_INTL_RANGE_END);
        qjs_intl_date_interval_set_clear(&set);
        qjs_intl_date_pattern_clear(allocator, &ordinary);
    }
}

static void part(const QJSIntlFormatted *result, QJSIntlPartType type, const char *value)
{
    size_t i, j, found = 0, end = 0;
    for (i = 0; i < result->part_count; i++) {
        const QJSIntlPart *p = &result->parts[i];
        assert(p->start == end && p->end > end && p->end <= result->length);
        end = p->end;
        if (p->type != type)
            continue;
        found++;
        assert(p->end - p->start == strlen(value));
        for (j = 0; j < strlen(value); j++)
            assert(result->text[p->start + j] == (unsigned char)value[j]);
    }
    assert(end == result->length && found == 1);
}

/* Keep the same July wall-clock witness with either named-zone provider. */
static const char *test_zone(void)
{
    return qjs_timezone_count() > 1 ? "US/Eastern" : "-04:00";
}

static QJSIntlDateTimeFormatter *open_formatter(const QJSIntlDateTimeService *service, size_t locale,
                                                const QJSIntlDateTimeOptions *options)
{
    QJSIntlDateTimeFormatter *formatter;
    QJSTimeZoneIdentifier zone;
    QJSIntlStatus status;
    assert(!qjs_timezone_parse_identifier(&zone, test_zone(), strlen(test_zone())));
    status = qjs_intl_datetime_formatter_open(service, locale, bytes("gregory"), bytes("latn"),
                                              &zone, options, &formatter);
    if (status)
        fprintf(stderr, "formatter open status %d\n", status);
    assert(!status);
    return formatter;
}

static void defaults(const QJSIntlAllocator *allocator, const QJSIntlDateTimeService *service, size_t locale)
{
    QJSIntlDateTimeOptions options = {0};
    QJSIntlDateTimeFormatter *formatter;
    QJSIntlFormatted result;
    QJSIntlDateTimeComponents components;
    QJSIntlBytes calendar, system;
    const char *zone;
    options.maximum_output_length = 1000;
    options.hour_cycle = QJS_INTL_DATE_H23;
    formatter = open_formatter(service, locale, &options);
    assert(!qjs_intl_datetime_formatter_info(formatter, &calendar, &system, &zone, &components));
    assert(calendar.length == 7 && !memcmp(calendar.data, "gregory", 7));
    assert(system.length == 4 && !memcmp(system.data, "latn", 4));
    assert(!strcmp(zone, test_zone()));
    assert(components.values[QJS_INTL_DATETIME_YEAR] && components.values[QJS_INTL_DATETIME_MONTH] &&
           components.values[QJS_INTL_DATETIME_DAY] && !components.values[QJS_INTL_DATETIME_HOUR]);
    assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_DATE, 1719835200, 0, &result));
    part(&result, QJS_INTL_PART_YEAR, "2024");
    qjs_intl_formatted_clear(allocator, &result);
    assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_INSTANT, 1719835200, 0, &result));
    part(&result, QJS_INTL_PART_HOUR, "08");
    qjs_intl_formatted_clear(allocator, &result);
    assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_PLAIN_TIME, 1719835200, 0, &result));
    part(&result, QJS_INTL_PART_HOUR, "12");
    qjs_intl_formatted_clear(allocator, &result);
    qjs_intl_datetime_formatter_close(formatter);
}

static void incompatible_kind(const QJSIntlAllocator *allocator,
                              const QJSIntlDateTimeService *service, size_t locale)
{
    QJSIntlDateTimeOptions options = {0};
    QJSIntlDateTimeFormatter *formatter;
    QJSIntlFormatted result, zero = {0};
    options.maximum_output_length = 1000;
    options.hour_cycle = QJS_INTL_DATE_H12;
    options.components.values[QJS_INTL_DATETIME_YEAR] = QJS_INTL_DATETIME_NUMERIC;
    formatter = open_formatter(service, locale, &options);
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_PLAIN_TIME, 0, 0, &result) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_PLAIN_YEAR_MONTH, 1719835200, 0, &result));
    part(&result, QJS_INTL_PART_YEAR, "2024");
    qjs_intl_formatted_clear(allocator, &result);
    qjs_intl_datetime_formatter_close(formatter);
    memset(&options.components, 0, sizeof(options.components));
    options.time_style = QJS_INTL_DATE_STYLE_SHORT + 1;
    formatter = open_formatter(service, locale, &options);
    assert(qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_PLAIN_DATE, 0, 0, &result) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_DATE, 1719835200, 0, &result));
    part(&result, QJS_INTL_PART_HOUR, "8");
    qjs_intl_formatted_clear(allocator, &result);
    qjs_intl_datetime_formatter_close(formatter);
}

static void adjusted_date_style(const QJSIntlAllocator *allocator,
                                const QJSIntlDateTimeService *service, size_t locale)
{
    QJSIntlDateTimeOptions options = {0};
    QJSIntlDateTimeFormatter *formatter;
    QJSIntlFormatted result;
    options.maximum_output_length = 1000;
    options.date_style = QJS_INTL_DATE_STYLE_FULL + 1;
    options.time_style = QJS_INTL_DATE_STYLE_SHORT + 1;
    formatter = open_formatter(service, locale, &options);
    /* Adjustment chooses a whole date record, retaining the full style's
       wide names when a PlainDate removes the clock from a mixed style. */
    assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_PLAIN_DATE, 1719835200, 0, &result));
    part(&result, QJS_INTL_PART_WEEKDAY, "Monday");
    part(&result, QJS_INTL_PART_MONTH, "July");
    qjs_intl_formatted_clear(allocator, &result);
    qjs_intl_datetime_formatter_close(formatter);
}

static void failures(const QJSIntlAllocator *allocator, const QJSIntlDateTimeService *service, size_t locale)
{
    Allocation *state = allocator->opaque;
    QJSIntlDateTimeOptions options = {0};
    QJSIntlDateTimeFormatter *formatter;
    QJSTimeZoneIdentifier zone;
    size_t baseline = state->live, calls, i;
    options.maximum_output_length = 1000;
    options.date_style = options.time_style = QJS_INTL_DATE_STYLE_FULL + 1;
    assert(!qjs_timezone_parse_identifier(&zone, test_zone(), strlen(test_zone())));
    state->calls = 0;
    assert(!qjs_intl_datetime_formatter_open(service, locale, bytes("gregory"), bytes("latn"), &zone, &options, &formatter));
    calls = state->calls;
    qjs_intl_datetime_formatter_close(formatter);
    assert(state->live == baseline);
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        formatter = (void *)1;
        assert(qjs_intl_datetime_formatter_open(service, locale, bytes("gregory"), bytes("latn"),
                                                &zone, &options, &formatter) == QJS_INTL_NO_MEMORY);
        assert(!formatter && state->live == baseline);
    }
    state->calls = state->fail_at = 0;
}

static void prepared_ranges(const QJSIntlAllocator *allocator,
                            const QJSIntlDateTimeService *service, size_t locale)
{
    Allocation *state = allocator->opaque;
    QJSIntlDateTimeOptions options = {0};
    QJSIntlDateTimeFormatter *formatter;
    QJSIntlFormattedRange result, zero = {0};
    size_t calls, baseline, before = state->live, i;
    unsigned int months = 0, years = 0, days = 0;
    options.hour_cycle = QJS_INTL_DATE_H23;
    options.maximum_output_length = 1000;
    options.components.values[QJS_INTL_DATETIME_YEAR] = QJS_INTL_DATETIME_NUMERIC;
    options.components.values[QJS_INTL_DATETIME_MONTH] = QJS_INTL_DATETIME_SHORT;
    options.components.values[QJS_INTL_DATETIME_DAY] = QJS_INTL_DATETIME_NUMERIC;
    formatter = open_formatter(service, locale, &options);
    state->calls = 0;
    assert(!qjs_intl_datetime_formatter_prepare_ranges(formatter));
    calls = state->calls;
    qjs_intl_datetime_formatter_close(formatter);
    assert(calls && state->live == before);
    formatter = open_formatter(service, locale, &options);
    baseline = state->live;
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        assert(qjs_intl_datetime_formatter_prepare_ranges(formatter) == QJS_INTL_NO_MEMORY);
        assert(state->live == baseline);
    }
    /* Failed preparation must remain retryable; a successful second call is
       idempotent and performs no allocation or interval ownership transfer. */
    state->calls = state->fail_at = 0;
    assert(!qjs_intl_datetime_formatter_prepare_ranges(formatter));
    calls = state->calls;
    assert(!qjs_intl_datetime_formatter_prepare_ranges(formatter) && state->calls == calls);
    state->calls = 0;
    assert(!qjs_intl_datetime_format_range(formatter, QJS_INTL_DATE_VALUE_DATE,
                                           1719835200, 0, 1720008000, 0, &result));
    calls = state->calls;
    for (i = 0; i < result.part_count; i++) {
        const QJSIntlRangePart *part = &result.parts[i];
        if (part->type == QJS_INTL_PART_MONTH) {
            months++;
            assert(part->source == QJS_INTL_RANGE_SHARED);
        } else if (part->type == QJS_INTL_PART_YEAR) {
            years++;
            assert(part->source == QJS_INTL_RANGE_SHARED);
        } else if (part->type == QJS_INTL_PART_DAY) {
            days++;
            assert(part->source != QJS_INTL_RANGE_SHARED);
        }
    }
    assert(months == 1 && years == 1 && days == 2 && result.text[0]);
    qjs_intl_formatted_range_clear(allocator, &result);
    /* This confirmed collapsed program has several source runs; fail every
       fragment/composition allocation, not only the full-endpoint fallback. */
    baseline = state->live;
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_datetime_format_range(formatter, QJS_INTL_DATE_VALUE_DATE,
                   1719835200, 0, 1720008000, 0, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    }
    state->calls = state->fail_at = 0;
    assert(!qjs_intl_datetime_format_range(formatter, QJS_INTL_DATE_VALUE_DATE,
                                           1719835200, 0, 1720008000, 0, &result));
    qjs_intl_datetime_formatter_close(formatter);
    assert(result.text[0] && result.part_count);
    qjs_intl_formatted_range_clear(allocator, &result);
    assert(state->live == before);
}

static void ranges(const QJSIntlAllocator *allocator, const QJSIntlDateTimeService *service, size_t locale)
{
    Allocation *state = allocator->opaque;
    QJSIntlDateTimeOptions options = {0};
    QJSIntlDateTimeFormatter *formatter;
    QJSIntlFormattedRange result, zero = {0};
    size_t baseline, calls, i, at;
    options.hour_cycle = QJS_INTL_DATE_H23;
    options.maximum_output_length = 1000;
    formatter = open_formatter(service, locale, &options);
    baseline = state->live;
    assert(!qjs_intl_datetime_check_value_format(formatter, QJS_INTL_DATE_VALUE_DATE));
    /* Sweep distinct endpoints and the independently allocated shared-result
       path without repeating expensive JS realm setup at every failure. */
    for (unsigned int shared = 0; shared < 2; shared++) {
        int64_t last = shared ? 1719835200 : 1719921600;
        state->calls = 0;
        assert(!qjs_intl_datetime_format_range(formatter, QJS_INTL_DATE_VALUE_DATE,
                                               1719835200, 0, last, 0, &result));
        calls = state->calls;
        qjs_intl_formatted_range_clear(allocator, &result);
        assert(state->live == baseline && calls);
        for (i = 1; i <= calls; i++) {
            state->calls = 0;
            state->fail_at = i;
            memset(&result, 0xff, sizeof(result));
            assert(qjs_intl_datetime_format_range(formatter, QJS_INTL_DATE_VALUE_DATE,
                       1719835200, 0, last, 0, &result) == QJS_INTL_NO_MEMORY);
            assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
        }
        state->calls = state->fail_at = 0;
    }
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_datetime_format_range(formatter, QJS_INTL_DATE_VALUE_DATE,
               0, 1000000000, 0, 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
    assert(!qjs_intl_datetime_format_range(formatter, QJS_INTL_DATE_VALUE_DATE,
                                           1719835200, 0, 1719921600, 0, &result));
    qjs_intl_datetime_formatter_close(formatter);
    at = 0;
    for (i = 0; i < result.part_count; i++) {
        const QJSIntlRangePart *part = &result.parts[i];
        assert(part->start == at && part->end > at && part->end <= result.length &&
               (unsigned int)part->source <= QJS_INTL_RANGE_END);
        at = part->end;
    }
    assert(at == result.length && result.text[0]);
    qjs_intl_formatted_range_clear(allocator, &result);
}

/* The full prepared corpus is an explicit audit mode; ordinary repository
   tests keep the same proof over all calendars of their English locale. */
static size_t styles(const QJSIntlAllocator *allocator, const QJSIntlDateTimeService *service,
                     size_t locale, QJSIntlBytes tag, QJSIntlBytes calendar, int intervals)
{
    QJSIntlDateTimeOptions options = {0};
    QJSIntlBytes system;
    QJSTimeZoneIdentifier zone;
    size_t checked = 0;
    unsigned int date, time;
    assert(!qjs_timezone_parse_identifier(&zone, "UTC", 3));
    assert(!qjs_intl_datetime_default_system(service, locale, &system));
    options.hour_cycle = QJS_INTL_DATE_H12;
    options.maximum_output_length = 1000;
    for (date = 0; date <= QJS_INTL_DATE_STYLE_WIDTH_COUNT; date++) {
        for (time = 0; time <= QJS_INTL_DATE_STYLE_WIDTH_COUNT; time++) {
            QJSIntlDateTimeFormatter *formatter;
            QJSIntlFormatted result;
            QJSIntlStatus status;
            if (!date && !time)
                continue;
            options.date_style = date;
            options.time_style = time;
            status = qjs_intl_datetime_formatter_open(service, locale, calendar, system, &zone, &options, &formatter);
            if (!status && intervals)
                status = qjs_intl_datetime_formatter_prepare_ranges(formatter);
            if (status)
                fprintf(stderr, "style %.*s/%.*s %u/%u open status %d\n", (int)tag.length, tag.data,
                        (int)calendar.length, calendar.data, date, time, status);
            assert(!status);
            assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_DATE, 1719835200, 0, &result));
            qjs_intl_formatted_clear(allocator, &result);
            qjs_intl_datetime_formatter_close(formatter);
            checked++;
        }
    }
    return checked;
}

static size_t capabilities(const QJSIntlAllocator *allocator, const QJSIntlDateTimeService *service,
                           size_t locale, QJSIntlBytes tag)
{
    QJSIntlDateTimeOptions options = {0};
    QJSTimeZoneIdentifier zone;
    QJSIntlBytes system;
    unsigned int calendar;
    size_t checked = 0;
    assert(!qjs_timezone_parse_identifier(&zone, "UTC", 3));
    assert(!qjs_intl_datetime_default_system(service, locale, &system));
    options.hour_cycle = QJS_INTL_DATE_H23;
    options.maximum_output_length = 1000;
    for (calendar = 0; calendar < QJS_CAL_COUNT; calendar++) {
        const char *name = qjs_calendar_identifier(calendar);
        QJSIntlDateTimeFormatter *formatter;
        QJSIntlFormatted result;
        QJSIntlStatus status;
        int supported;
        assert(!qjs_intl_datetime_calendar_supported(service, locale, bytes(name), &supported));
        if (!supported)
            continue;
        status = qjs_intl_datetime_formatter_open(service, locale, bytes(name), system, &zone, &options, &formatter);
        if (status)
            fprintf(stderr, "capability %.*s/%s open status %d\n", (int)tag.length, tag.data, name, status);
        assert(!status);
        assert(!qjs_intl_datetime_formatter_prepare_ranges(formatter));
        assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_INSTANT, 1719835200, 0, &result));
        qjs_intl_formatted_clear(allocator, &result);
        qjs_intl_datetime_formatter_close(formatter);
        checked++;
    }
    return checked;
}

int main(int argc, char **argv)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlDateTimeService *service;
    QJSIntlDateTimeFormatter *formatter;
    QJSIntlDateTimeOptions options = {0};
    QJSIntlDateHourPreferences hours;
    QJSIntlFormatted result;
    const QJSIntlBytes *tags;
    QJSIntlBytes calendar, system;
    QJSIntlStatus status;
    size_t count, en;
    int supported;
    status = qjs_intl_datetime_open(&allocator, &service);
    if (status)
        fprintf(stderr, "service open status %d\n", status);
    assert(!status);
    assert(!qjs_intl_datetime_available(service, &tags, &count));
    for (en = 0; en < count; en++)
        if (tags[en].length == 2 && !memcmp(tags[en].data, "en", 2))
            break;
    assert(en < count);
    if (argc == 4 && !strcmp(argv[1], "--audit")) {
        size_t index, checked = 0, styled = 0;
        unsigned long shard = strtoul(argv[2], NULL, 10), shards = strtoul(argv[3], NULL, 10);
        assert(shards && shard < shards);
        for (index = shard; index < count; index += shards) {
            QJSIntlBytes preferred;
            checked += capabilities(&allocator, service, index, tags[index]);
            assert(!qjs_intl_datetime_default_calendar(service, index, &preferred));
            styled += styles(&allocator, service, index, tags[index], preferred, 1);
        }
        qjs_intl_datetime_close(service);
        assert(!state.live);
        printf("admission shard %lu/%lu: %zu locale/calendar combinations; %zu preferred-calendar styles\n",
               shard, shards, checked, styled);
        return 0;
    }
    assert(argc == 1);
    assert(!qjs_intl_datetime_default_calendar(service, en, &calendar));
    assert(calendar.length == 7 && !memcmp(calendar.data, "gregory", 7));
    assert(!qjs_intl_datetime_calendar_supported(service, en, bytes("gregory"), &supported) && supported);
    assert(!qjs_intl_datetime_calendar_supported(service, en, bytes("not-calendar"), &supported) && !supported);
    assert(!qjs_intl_datetime_default_system(service, en, &system));
    assert(!qjs_intl_datetime_system_supported(service, en, system, &supported) && supported);
    assert(!qjs_intl_datetime_hour_preferences(service, en, &hours));
    assert(hours.preferred == QJS_INTL_DATE_H12 && hours.twelve == QJS_INTL_DATE_H12 &&
           hours.twenty_four == QJS_INTL_DATE_H23);
    defaults(&allocator, service, en);
    selector_authored(&allocator);
    incompatible_kind(&allocator, service, en);
    adjusted_date_style(&allocator, service, en);
    ranges(&allocator, service, en);
    prepared_ranges(&allocator, service, en);
    capabilities(&allocator, service, en, tags[en]);
    for (unsigned int c = 0; c < QJS_CAL_COUNT; c++) {
        QJSIntlBytes identifier = bytes(qjs_calendar_identifier(c));
        assert(!qjs_intl_datetime_calendar_supported(service, identifier, &supported));
        if (supported)
            styles(&allocator, service, en, tags[en], identifier, 0);
    }
    failures(&allocator, service, en);
    options.maximum_output_length = 1000;
    options.date_style = QJS_INTL_DATE_STYLE_FULL + 1;
    formatter = open_formatter(service, en, &options);
    /* Both lifetime boundaries matter: formatter survives service close,
       then returned parts survive formatter close. */
    qjs_intl_datetime_close(service);
    assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_DATE, 1719835200, 0, &result));
    part(&result, QJS_INTL_PART_WEEKDAY, "Monday");
    part(&result, QJS_INTL_PART_MONTH, "July");
    qjs_intl_datetime_formatter_close(formatter);
    part(&result, QJS_INTL_PART_YEAR, "2024");
    qjs_intl_formatted_clear(&allocator, &result);
    assert(!state.live);
    return 0;
}
