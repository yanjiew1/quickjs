/* Checked QJCA version 1 reader; no native-struct serialization.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "data.h"
#include "arithmetic.h"
#include <limits.h>
#include <string.h>

#define HEADER_SIZE 32
#define INDEX_SIZE 32
#define YEAR_SIZE 12

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | p[3];
}

static int32_t read_i32(const uint8_t *p)
{
    uint32_t value = read_u32(p);

    return value <= INT32_MAX ? (int32_t)value : -1 - (int32_t)~value;
}

static unsigned calendar_kind(QJSCalendarId calendar)
{
    switch (calendar) {
    case QJS_CAL_PERSIAN: return 10;
    case QJS_CAL_ISLAMIC_UMALQURA: return 20;
    case QJS_CAL_CHINESE: return 30;
    case QJS_CAL_DANGI: return 40;
    default: return 0;
    }
}

static int year_length(unsigned kind, const uint8_t *row)
{
    uint32_t bits = read_u32(row + 4);
    unsigned months = row[8], leap = row[9], i;
    int total = 0;

    if ((months != 12 && months != 13) || bits >> (2 * months) ||
        row[10] || row[11])
        return 0;
    if (kind == 10 || kind == 20) {
        if (months != 12 || leap)
            return 0;
    } else if ((months == 12 && leap) ||
               (months == 13 && (!leap || leap > 12))) {
        return 0;
    }
    for (i = 0; i < months; i++) {
        int days = 29 + ((bits >> (2 * i)) & 3);

        if (kind == 10) {
            if (i < 11 ? days != (i < 6 ? 31 : 30) : days > 30)
                return 0;
        } else if (days > 30) {
            return 0;
        }
        total += days;
    }
    if (kind == 20 && total != 354 && total != 355)
        return 0;
    if (kind >= 30 && (months == 12 ? total < 353 || total > 355 :
                                     total < 383 || total > 385))
        return 0;
    return total;
}

int qjs_calendar_data_open(QJSCalendarData *result, const uint8_t *bytes,
                           size_t size)
{
    uint32_t count, strings, payload, previous = 0, i;
    size_t next_string, next_payload;
    QJSCalendarData value;

    if (size < HEADER_SIZE || size > 1024 * 1024 ||
        memcmp(bytes, "QJCA", 4) || read_u32(bytes + 4) != 1 ||
        read_u32(bytes + 8) != size || read_u32(bytes + 28) != 1)
        return QJS_CAL_BACKEND;
    count = read_u32(bytes + 12);
    strings = read_u32(bytes + 20);
    payload = read_u32(bytes + 24);
    if (!count || count > 4 || read_u32(bytes + 16) != HEADER_SIZE ||
        strings != HEADER_SIZE + count * INDEX_SIZE ||
        strings > payload || payload > size || payload % 4)
        return QJS_CAL_BACKEND;
    next_string = strings;
    next_payload = payload;
    for (i = 0; i < count; i++) {
        const uint8_t *entry = bytes + HEADER_SIZE + i * INDEX_SIZE;
        uint32_t kind = read_u32(entry), years = read_u32(entry + 8);
        uint32_t offset = read_u32(entry + 16), length = read_u32(entry + 20);
        int32_t first = read_i32(entry + 4), end = read_i32(entry + 12);
        size_t start = next_string;
        int64_t expected = 0;
        uint32_t n;

        if ((kind != 10 && kind != 20 && kind != 30 && kind != 40) ||
            kind <= previous || read_u32(entry + 28) || !years || years > 10000 ||
            first < QJS_CAL_MIN_YEAR || (int64_t)first + years - 1 > QJS_CAL_MAX_YEAR ||
            length != years * YEAR_SIZE || offset != next_payload ||
            offset > size || length > size - offset ||
            read_u32(entry + 24) != next_string)
            return QJS_CAL_BACKEND;
        while (next_string < payload && bytes[next_string]) {
            if (bytes[next_string] < 33 || bytes[next_string] > 126)
                return QJS_CAL_BACKEND;
            next_string++;
        }
        if (next_string == payload || next_string == start || next_string - start > 95)
            return QJS_CAL_BACKEND;
        next_string++;
        for (n = 0; n < years; n++) {
            const uint8_t *row = bytes + offset + n * YEAR_SIZE;
            int days = year_length(kind, row);
            int32_t day = read_i32(row);

            if (!days || (n && day != expected))
                return QJS_CAL_BACKEND;
            expected = (int64_t)day + days;
        }
        if (expected != end)
            return QJS_CAL_BACKEND;
        previous = kind;
        next_payload += length;
    }
    if (next_payload != size || payload - next_string > 3)
        return QJS_CAL_BACKEND;
    while (next_string < payload) {
        if (bytes[next_string++])
            return QJS_CAL_BACKEND;
    }
    value = (QJSCalendarData){bytes, size, count};
    *result = value;
    return QJS_CAL_OK;
}

static const uint8_t *find_calendar(const QJSCalendarData *data,
                                    QJSCalendarId calendar)
{
    unsigned kind = calendar_kind(calendar);
    uint32_t i;

    if (!kind)
        return NULL;
    for (i = 0; i < data->count; i++) {
        const uint8_t *entry = data->bytes + HEADER_SIZE + i * INDEX_SIZE;

        if (read_u32(entry) == kind)
            return entry;
    }
    return NULL;
}

int qjs_calendar_data_year(const QJSCalendarData *data, QJSCalendarId calendar,
                           int32_t year, QJSCalendarYear *result)
{
    const uint8_t *entry = find_calendar(data, calendar), *row;
    int64_t index;
    uint32_t count;
    QJSCalendarYear value;

    if (!entry)
        return QJS_CAL_UNSUPPORTED;
    index = (int64_t)year - read_i32(entry + 4);
    count = read_u32(entry + 8);
    if (index < 0 || index >= count)
        return QJS_CAL_RANGE;
    row = data->bytes + read_u32(entry + 16) + (size_t)index * YEAR_SIZE;
    value.first_day = read_i32(row);
    value.end_day = index + 1 < count ? read_i32(row + YEAR_SIZE) : read_i32(entry + 12);
    value.month_lengths = read_u32(row + 4);
    value.months = row[8];
    value.leap_month = row[9];
    *result = value;
    return QJS_CAL_OK;
}

const char *qjs_calendar_data_version(const QJSCalendarData *data,
                                      QJSCalendarId calendar)
{
    const uint8_t *entry = find_calendar(data, calendar);

    return entry ? (const char *)data->bytes + read_u32(entry + 24) : NULL;
}
