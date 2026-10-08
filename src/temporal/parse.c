/*
 * Portable Temporal numeric ISO parsing primitives
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#include "parse.h"

static int qjs_temporal_ascii_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int qjs_temporal_parse_digits(uint32_t *result, size_t *position,
                                     const char *text, size_t length,
                                     size_t width)
{
    uint32_t value = 0;
    size_t i, start = *position;

    if (width > length - start)
        return -1;
    for (i = 0; i < width; i++) {
        if (!qjs_temporal_ascii_digit(text[start + i]))
            return -1;
        value = value * 10 + (text[start + i] - '0');
    }
    *position = start + width;
    *result = value;
    return 0;
}

static int qjs_temporal_parse_fraction(uint32_t *result, size_t *position,
                                       const char *text, size_t length)
{
    uint32_t value = 0;
    size_t p = *position + 1, count = 0;

    while (p < length && qjs_temporal_ascii_digit(text[p])) {
        if (count == 9)
            return -1;
        value = value * 10 + (text[p++] - '0');
        count++;
    }
    if (count == 0)
        return -1;
    while (count++ < 9)
        value *= 10;
    *position = p;
    *result = value;
    return 0;
}

/* TimeSpec and UTCOffset share numeric fields and separator consistency;
   only TimeSpec permits a leap second, normalized to 59 after parsing. */
static int qjs_temporal_parse_clock(QJSTemporalISOTime *result,
                                    size_t *consumed,
                                    const char *text, size_t length,
                                    int allow_leap_second, int allow_seconds,
                                    int *seconds_present)
{
    QJSTemporalISOTime time = { 0 };
    uint32_t hour, minute = 0, second = 0, fraction = 0;
    size_t p = 0;
    int extended = 0, has_minute = 0, has_second = 0;

    if (qjs_temporal_parse_digits(&hour, &p, text, length, 2) || hour > 23)
        return -1;
    if (p < length && text[p] == ':') {
        extended = 1;
        has_minute = 1;
        p++;
    } else if (p < length && qjs_temporal_ascii_digit(text[p])) {
        has_minute = 1;
    }
    if (has_minute) {
        if (qjs_temporal_parse_digits(&minute, &p, text, length, 2) || minute > 59)
            return -1;
        if (p < length && text[p] == ':') {
            if (!extended)
                return -1;
            has_second = 1;
            p++;
        } else if (p < length && qjs_temporal_ascii_digit(text[p])) {
            if (extended)
                return -1;
            has_second = 1;
        }
        if (has_second) {
            if (!allow_seconds ||
                qjs_temporal_parse_digits(&second, &p, text, length, 2) ||
                second > (allow_leap_second ? 60U : 59U))
                return -1;
            if (p < length && (text[p] == '.' || text[p] == ',')) {
                if (qjs_temporal_parse_fraction(&fraction, &p, text, length))
                    return -1;
            }
        }
    }
    if (p < length && (text[p] == ':' || text[p] == '.' || text[p] == ',' ||
                       qjs_temporal_ascii_digit(text[p])))
        return -1;
    time.hour = hour;
    time.minute = minute;
    time.second = second == 60 ? 59 : second;
    time.millisecond = fraction / 1000000;
    time.microsecond = fraction / 1000 % 1000;
    time.nanosecond = fraction % 1000;
    *result = time;
    *consumed = p;
    if (seconds_present)
        *seconds_present = has_second;
    return 0;
}

int qjs_temporal_parse_iso_date_prefix(QJSTemporalISODate *result,
                                       size_t *consumed,
                                       const char *text, size_t length)
{
    QJSTemporalISODate date;
    uint32_t year, month, day;
    size_t p = 0, year_width = 4;
    int negative = 0, extended = 0;

    if (length == 0)
        return -1;
    if (text[p] == '+' || text[p] == '-') {
        negative = text[p] == '-';
        p++;
        year_width = 6;
    }
    if (qjs_temporal_parse_digits(&year, &p, text, length, year_width) ||
        (negative && year == 0))
        return -1;
    if (p < length && text[p] == '-') {
        extended = 1;
        p++;
    }
    if (qjs_temporal_parse_digits(&month, &p, text, length, 2))
        return -1;
    if (extended) {
        if (p == length || text[p++] != '-')
            return -1;
    }
    if (qjs_temporal_parse_digits(&day, &p, text, length, 2))
        return -1;
    date.year = negative ? -(int32_t)year : (int32_t)year;
    date.month = month;
    date.day = day;
    if (!qjs_temporal_iso_date_is_valid(date))
        return -1;
    *result = date;
    *consumed = p;
    return 0;
}

int qjs_temporal_parse_iso_time_prefix(QJSTemporalISOTime *result,
                                       size_t *consumed,
                                       const char *text, size_t length)
{
    return qjs_temporal_parse_clock(result, consumed, text, length, 1, 1, NULL);
}

int qjs_temporal_parse_utc_offset_prefix(QJSTemporalUTCOffset *result,
                                         size_t *consumed,
                                         const char *text, size_t length,
                                         unsigned flags)
{
    QJSTemporalUTCOffset offset = { 0 };
    QJSTemporalISOTime time;
    size_t p;
    int negative;

    if (length == 0 || (flags & ~(QJS_TEMPORAL_OFFSET_ALLOW_Z |
                                  QJS_TEMPORAL_OFFSET_ALLOW_SECONDS)))
        return -1;
    if (text[0] == 'Z' || text[0] == 'z') {
        if (!(flags & QJS_TEMPORAL_OFFSET_ALLOW_Z))
            return -1;
        offset.is_z = 1;
        p = 1;
    } else {
        if (text[0] != '+' && text[0] != '-')
            return -1;
        negative = text[0] == '-';
        if (qjs_temporal_parse_clock(&time, &p, text + 1, length - 1, 0,
                                     flags & QJS_TEMPORAL_OFFSET_ALLOW_SECONDS,
                                     &offset.has_seconds))
            return -1;
        offset.nanoseconds = (((int64_t)time.hour * 60 + time.minute) * 60 +
                              time.second) * INT64_C(1000000000) +
            (int64_t)time.millisecond * 1000000 +
            time.microsecond * 1000 + time.nanosecond;
        if (negative)
            offset.nanoseconds = -offset.nanoseconds;
        p++;
    }
    *result = offset;
    *consumed = p;
    return 0;
}

int qjs_temporal_parse_instant_prefix(QJSTemporalInstantPrefix *result,
                                      size_t *consumed,
                                      const char *text, size_t length)
{
    QJSTemporalInstantPrefix prefix;
    size_t p, used;

    if (qjs_temporal_parse_iso_date_prefix(&prefix.datetime.date, &p,
                                          text, length))
        return -1;
    if (p == length || (text[p] != 'T' && text[p] != 't' && text[p] != ' '))
        return -1;
    p++;
    if (qjs_temporal_parse_iso_time_prefix(&prefix.datetime.time, &used,
                                          text + p, length - p))
        return -1;
    p += used;
    if (qjs_temporal_parse_utc_offset_prefix(&prefix.offset, &used,
                                            text + p, length - p,
                                            QJS_TEMPORAL_OFFSET_ALLOW_Z |
                                            QJS_TEMPORAL_OFFSET_ALLOW_SECONDS))
        return -1;
    p += used;
    if (p < length && text[p] != '[')
        return -1;
    *result = prefix;
    *consumed = p;
    return 0;
}

static int qjs_temporal_ascii_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int qjs_temporal_annotation_key_start(char c)
{
    return (c >= 'a' && c <= 'z') || c == '_';
}

static int qjs_temporal_time_zone_name_start(char c)
{
    return qjs_temporal_ascii_alpha(c) || c == '.' || c == '_';
}

static int qjs_temporal_annotation_value_is_valid(const char *text,
                                                  size_t length)
{
    size_t i;
    int need_component = 1;

    for (i = 0; i < length; i++) {
        if (qjs_temporal_ascii_alpha(text[i]) ||
            qjs_temporal_ascii_digit(text[i])) {
            need_component = 0;
        } else if (text[i] == '-' && !need_component) {
            need_component = 1;
        } else {
            return 0;
        }
    }
    return !need_component;
}

/* Instant ignores the annotation's zone identity, but the complete zone
   identifier grammar still applies, even when the annotation is critical. */
static int qjs_temporal_time_zone_annotation_is_valid(const char *text,
                                                      size_t length)
{
    QJSTemporalUTCOffset offset;
    size_t consumed, i;
    int need_component = 1;

    if (length == 0)
        return 0;
    if (text[0] == '+' || text[0] == '-') {
        return qjs_temporal_parse_utc_offset_prefix(&offset, &consumed,
                                                    text, length, 0) == 0 &&
            consumed == length;
    }
    for (i = 0; i < length; i++) {
        if (need_component) {
            if (!qjs_temporal_time_zone_name_start(text[i]))
                return 0;
            need_component = 0;
        } else if (text[i] == '/') {
            need_component = 1;
        } else if (!qjs_temporal_time_zone_name_start(text[i]) &&
                   !qjs_temporal_ascii_digit(text[i]) &&
                   text[i] != '-' && text[i] != '+') {
            return 0;
        }
    }
    return !need_component;
}

/* Temporal ISO strings have at most one zone annotation, before all key /
   value annotations. Unknown noncritical keys are ignored. The first
   calendar wins unless either duplicate calendar carries a critical flag. */
static int qjs_temporal_annotations_are_valid(const char *text, size_t length,
                                             QJSTemporalParsedISO *result)
{
    size_t p = 0, start, end, equals, i;
    int critical, have_zone = 0, have_annotations = 0;
    int have_calendar = 0, calendar_critical = 0;

    while (p < length) {
        if (text[p++] != '[')
            return 0;
        critical = p < length && text[p] == '!';
        if (critical)
            p++;
        start = p;
        while (p < length && text[p] != ']')
            p++;
        if (p == length || p == start)
            return 0;
        end = p++;
        equals = start;
        while (equals < end && text[equals] != '=')
            equals++;
        if (equals == end) {
            if (have_zone || have_annotations ||
                !qjs_temporal_time_zone_annotation_is_valid(text + start,
                                                            end - start))
                return 0;
            have_zone = 1;
            if (result) {
                result->time_zone = text + start;
                result->time_zone_length = end - start;
            }
            continue;
        }
        have_annotations = 1;
        if (equals == start ||
            !qjs_temporal_annotation_key_start(text[start]))
            return 0;
        for (i = start + 1; i < equals; i++) {
            if (!qjs_temporal_annotation_key_start(text[i]) &&
                !qjs_temporal_ascii_digit(text[i]) && text[i] != '-')
                return 0;
        }
        if (!qjs_temporal_annotation_value_is_valid(text + equals + 1,
                                                    end - equals - 1))
            return 0;
        if (equals - start == 4 && text[start] == 'u' &&
            text[start + 1] == '-' && text[start + 2] == 'c' &&
            text[start + 3] == 'a') {
            if (have_calendar && (critical || calendar_critical))
                return 0;
            if (!have_calendar) {
                calendar_critical = critical;
                if (result) {
                    result->calendar = text + equals + 1;
                    result->calendar_length = end - equals - 1;
                }
            }
            have_calendar = 1;
        } else if (critical) {
            return 0;
        }
    }
    return 1;
}

int qjs_temporal_parse_instant(QJSTemporalEpochNs *result,
                               const char *text, size_t length)
{
    QJSTemporalInstantPrefix prefix;
    QJSTemporalEpochNs local, offset, epoch_ns;
    size_t consumed;

    if (qjs_temporal_parse_instant_prefix(&prefix, &consumed, text, length) ||
        !qjs_temporal_annotations_are_valid(text + consumed,
                                            length - consumed, NULL) ||
        qjs_temporal_iso_datetime_to_epoch_ns(&local, prefix.datetime))
        return -1;
    offset = qjs_temporal_epoch_ns_from_int64(prefix.offset.nanoseconds);
    if (qjs_temporal_epoch_ns_subtract(&epoch_ns, local, offset) ||
        !qjs_temporal_epoch_ns_is_valid(epoch_ns))
        return -1;
    *result = epoch_ns;
    return 0;
}

static int qjs_temporal_parse_year_month(QJSTemporalISODate *result,
                                        size_t *consumed,
                                        const char *text, size_t length)
{
    QJSTemporalISODate date;
    uint32_t year, month;
    size_t p = 0, width = 4;
    int negative = 0;

    if (!length)
        return -1;
    if (text[p] == '+' || text[p] == '-') {
        negative = text[p] == '-';
        p++;
        width = 6;
    }
    if (qjs_temporal_parse_digits(&year, &p, text, length, width) ||
        (negative && !year))
        return -1;
    if (p < length && text[p] == '-')
        p++;
    if (qjs_temporal_parse_digits(&month, &p, text, length, 2) ||
        month < 1 || month > 12)
        return -1;
    date.year = negative ? -(int32_t)year : (int32_t)year;
    date.month = month;
    date.day = 1;
    *result = date;
    *consumed = p;
    return 0;
}

static int qjs_temporal_parse_month_day(QJSTemporalISODate *result,
                                       size_t *consumed,
                                       const char *text, size_t length)
{
    QJSTemporalISODate date = { 1972, 0, 0 };
    uint32_t month, day;
    size_t p = 0;

    if (length >= 2 && text[0] == '-' && text[1] == '-')
        p = 2;
    if (qjs_temporal_parse_digits(&month, &p, text, length, 2))
        return -1;
    if (p < length && text[p] == '-')
        p++;
    if (qjs_temporal_parse_digits(&day, &p, text, length, 2))
        return -1;
    date.month = month;
    date.day = day;
    if (!qjs_temporal_iso_date_is_valid(date))
        return -1;
    *result = date;
    *consumed = p;
    return 0;
}

static int qjs_temporal_short_form_calendar_is_iso(
    const QJSTemporalParsedISO *parsed)
{
    static const char iso[] = "iso8601";
    size_t i;

    if (!parsed->calendar)
        return 1;
    if (parsed->calendar_length != sizeof(iso) - 1)
        return 0;
    for (i = 0; i < sizeof(iso) - 1; i++) {
        char c = parsed->calendar[i];
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        if (c != iso[i])
            return 0;
    }
    return 1;
}

static int qjs_temporal_parse_datetime_goal(QJSTemporalParsedISO *result,
                                           const char *text, size_t length,
                                           unsigned grammar)
{
    QJSTemporalParsedISO parsed = { 0 };
    QJSTemporalISODate ambiguity_date;
    size_t p = 0, used, clock_end;
    int standalone_time = 0, time_designator = 0;

    parsed.datetime.date.year = 1972;
    parsed.datetime.date.month = 1;
    parsed.datetime.date.day = 1;
    if (grammar == QJS_TEMPORAL_PARSE_YEAR_MONTH &&
        !qjs_temporal_parse_year_month(&parsed.datetime.date, &p,
                                       text, length) &&
        qjs_temporal_annotations_are_valid(text + p, length - p, &parsed)) {
        if (!qjs_temporal_short_form_calendar_is_iso(&parsed))
            return -1;
        parsed.short_year_month = 1;
        *result = parsed;
        return 0;
    }
    if (grammar == QJS_TEMPORAL_PARSE_MONTH_DAY &&
        !qjs_temporal_parse_month_day(&parsed.datetime.date, &p,
                                      text, length) &&
        qjs_temporal_annotations_are_valid(text + p, length - p, &parsed)) {
        if (!qjs_temporal_short_form_calendar_is_iso(&parsed))
            return -1;
        parsed.year_absent = 1;
        *result = parsed;
        return 0;
    }
    /* Reset slices from a failed short-form annotation parse. */
    parsed.time_zone = parsed.calendar = NULL;
    parsed.time_zone_length = parsed.calendar_length = 0;
    if (!qjs_temporal_parse_iso_date_prefix(&parsed.datetime.date, &p,
                                            text, length)) {
        if (p < length && (text[p] == 'T' || text[p] == 't' || text[p] == ' ')) {
            p++;
            if (qjs_temporal_parse_iso_time_prefix(&parsed.datetime.time,
                                                   &used, text + p,
                                                   length - p))
                return -1;
            p += used;
            parsed.has_time = 1;
        } else if (grammar == QJS_TEMPORAL_PARSE_INSTANT ||
                   grammar == QJS_TEMPORAL_PARSE_TIME) {
            return -1;
        }
    } else if (grammar == QJS_TEMPORAL_PARSE_TIME) {
        standalone_time = 1;
        parsed.datetime.date.year = 0;
        p = 0;
        if (length && (text[0] == 'T' || text[0] == 't')) {
            p++;
            time_designator = 1;
        }
        if (qjs_temporal_parse_iso_time_prefix(&parsed.datetime.time, &used,
                                               text + p, length - p))
            return -1;
        p += used;
        parsed.has_time = 1;
    } else {
        return -1;
    }
    if (parsed.has_time && p < length &&
        (text[p] == '+' || text[p] == '-' || text[p] == 'Z' || text[p] == 'z')) {
        unsigned flags = QJS_TEMPORAL_OFFSET_ALLOW_SECONDS;
        if (grammar == QJS_TEMPORAL_PARSE_INSTANT ||
            grammar == QJS_TEMPORAL_PARSE_ZONED)
            flags |= QJS_TEMPORAL_OFFSET_ALLOW_Z;
        parsed.offset_text = text + p;
        if (qjs_temporal_parse_utc_offset_prefix(&parsed.offset, &used,
                                                 text + p, length - p, flags))
            return -1;
        parsed.offset_length = used;
        parsed.offset_present = 1;
        p += used;
    }
    clock_end = p;
    if (grammar == QJS_TEMPORAL_PARSE_INSTANT && !parsed.offset_present)
        return -1;
    if (!qjs_temporal_annotations_are_valid(text + p, length - p, &parsed))
        return -1;
    if (grammar == QJS_TEMPORAL_PARSE_ZONED && !parsed.time_zone)
        return -1;
    if (standalone_time && !time_designator) {
        if ((!qjs_temporal_parse_year_month(&ambiguity_date, &used,
                                            text, clock_end) && used == clock_end) ||
            (!qjs_temporal_parse_month_day(&ambiguity_date, &used,
                                           text, clock_end) && used == clock_end))
            return -1;
    }
    *result = parsed;
    return 0;
}

int qjs_temporal_parse_iso_datetime(QJSTemporalParsedISO *result,
                                    const char *text, size_t length,
                                    unsigned allowed_grammars)
{
    static const unsigned grammars[] = {
        QJS_TEMPORAL_PARSE_ZONED, QJS_TEMPORAL_PARSE_DATE_TIME,
        QJS_TEMPORAL_PARSE_INSTANT, QJS_TEMPORAL_PARSE_TIME,
        QJS_TEMPORAL_PARSE_MONTH_DAY, QJS_TEMPORAL_PARSE_YEAR_MONTH,
    };
    QJSTemporalParsedISO parsed;
    size_t i;

    if (!length || !allowed_grammars ||
        (allowed_grammars & ~(QJS_TEMPORAL_PARSE_DATE_TIME |
                             QJS_TEMPORAL_PARSE_ZONED |
                             QJS_TEMPORAL_PARSE_INSTANT |
                             QJS_TEMPORAL_PARSE_TIME |
                             QJS_TEMPORAL_PARSE_YEAR_MONTH |
                             QJS_TEMPORAL_PARSE_MONTH_DAY)))
        return -1;
    for (i = 0; i < sizeof(grammars) / sizeof(grammars[0]); i++) {
        if ((allowed_grammars & grammars[i]) &&
            !qjs_temporal_parse_datetime_goal(&parsed, text, length, grammars[i])) {
            *result = parsed;
            return 0;
        }
    }
    return -1;
}
