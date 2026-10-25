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
#include "dayperiod-data.h"
#include "../common/source-locale.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 3
#define ROW_WIDTH 3
#define MAP_WIDTH 2
#define RULE_WIDTH 4
#define DAY_SECONDS 86400

typedef struct Segment {
    uint32_t start, end;
} Segment;

static uint32_t word(const uint8_t *bytes, size_t index)
{
    return qjs_intl_read_u32(bytes + index * 4);
}

static int take(const QJSIntlSection *section, size_t *at, uint32_t count, uint32_t width,
                const uint8_t **out)
{
    if (*at > section->length / 4 || count > (section->length / 4 - *at) / width)
        return 0;
    *out = section->bytes + *at * 4;
    *at += (size_t)count * width;
    return 1;
}

static int valid_rules(const uint8_t *rules, uint32_t count, int format)
{
    Segment segments[QJS_INTL_DAYPERIOD_CODE_COUNT * 2];
    size_t segment_count = 0, i;
    uint32_t at = 0, previous = 0;
    if (count > QJS_INTL_DAYPERIOD_CODE_COUNT)
        return 0;
    for (i = 0; i < count; i++) {
        uint32_t code = word(rules, i * RULE_WIDTH), form = word(rules, i * RULE_WIDTH + 1);
        uint32_t start = word(rules, i * RULE_WIDTH + 2), end = word(rules, i * RULE_WIDTH + 3);
        if (code >= QJS_INTL_DAYPERIOD_CODE_COUNT || form > QJS_INTL_DAYPERIOD_RANGE ||
            start >= DAY_SECONDS || end > DAY_SECONDS || start % 60 || end % 60 ||
            (i && previous >= code))
            return 0;
        previous = code;
        switch (code) {
        case QJS_INTL_DAYPERIOD_AM:
            if (form != QJS_INTL_DAYPERIOD_RANGE || start || end != 43200)
                return 0;
            break;
        case QJS_INTL_DAYPERIOD_PM:
            if (form != QJS_INTL_DAYPERIOD_RANGE || start != 43200 || end != DAY_SECONDS)
                return 0;
            break;
        case QJS_INTL_DAYPERIOD_MIDNIGHT:
            if (form != QJS_INTL_DAYPERIOD_AT || start || end)
                return 0;
            break;
        case QJS_INTL_DAYPERIOD_NOON:
            if (form != QJS_INTL_DAYPERIOD_AT || start != 43200 || end != start)
                return 0;
            break;
        default: {
            uint32_t unwrapped_end = end > start ? end : end + DAY_SECONDS;
            if (form != QJS_INTL_DAYPERIOD_RANGE || start == end ||
                (format && (unwrapped_end - 1) / 3600 - start / 3600 + 1 > 12))
                return 0;
            if (start < end) {
                segments[segment_count++] = (Segment){start, end};
            } else {
                segments[segment_count++] = (Segment){start, DAY_SECONDS};
                if (end)
                    segments[segment_count++] = (Segment){0, end};
            }
            break;
        }
        }
    }
    /* At most twelve types produce at most twenty-four segments. Sorting this
       fixed local bound needs no allocator or locale-sized expansion. */
    for (i = 1; i < segment_count; i++) {
        Segment current = segments[i];
        size_t j = i;
        while (j && (segments[j - 1].start > current.start ||
                     (segments[j - 1].start == current.start && segments[j - 1].end > current.end))) {
            segments[j] = segments[j - 1];
            j--;
        }
        segments[j] = current;
    }
    for (i = 0; i < segment_count; i++) {
        if (segments[i].start != at)
            return 0;
        at = segments[i].end;
    }
    return !segment_count || at == DAY_SECONDS;
}

static int compare_lists(const uint8_t *a, uint32_t ac, const uint8_t *b, uint32_t bc)
{
    size_t i, common = (size_t)(ac < bc ? ac : bc) * RULE_WIDTH;
    for (i = 0; i < common; i++) {
        uint32_t left = word(a, i), right = word(b, i);
        if (left != right)
            return left < right ? -1 : 1;
    }
    return ac < bc ? -1 : ac != bc;
}

QJSIntlStatus qjs_intl_dayperiod_data_open(const QJSIntlData *data,
                                           const QJSIntlAllocator *allocator,
                                           QJSIntlDayPeriodData *out)
{
    QJSIntlDayPeriodData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    uint8_t *classes = NULL;
    size_t at = HEADER_WORDS, i, next = 0;
    uint32_t previous_start = 0, previous_count = 0;
    QJSIntlBytes text;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !data->strings.bytes || !data->locales.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_DAYPERIOD_RULES, &result.section) ||
        result.section.length < HEADER_WORDS * 4 || result.section.length % 4 || word(result.section.bytes, 0) != 1 ||
        qjs_intl_data_string(data, 0, &text) || text.length)
        return status;
    result.row_count = result.section.count;
    result.map_count = word(result.section.bytes, 1);
    result.rule_count = word(result.section.bytes, 2);
    if (result.map_count > result.row_count || (uint64_t)result.rule_count > (uint64_t)result.map_count * QJS_INTL_DAYPERIOD_CODE_COUNT ||
        !take(&result.section, &at, result.row_count, ROW_WIDTH, &result.rows) ||
        !take(&result.section, &at, result.map_count, MAP_WIDTH, &result.maps) ||
        !take(&result.section, &at, result.rule_count, RULE_WIDTH, &result.rules) || at != result.section.length / 4)
        return status;
    if (result.map_count) {
        classes = allocator->malloc(allocator->opaque, result.map_count);
        if (!classes)
            return QJS_INTL_NO_MEMORY;
        memset(classes, 0, result.map_count);
    }
    for (i = 0; i < result.row_count; i++) {
        uint32_t kind = word(result.rows, i * ROW_WIDTH), locale = word(result.rows, i * ROW_WIDTH + 1);
        uint32_t map = word(result.rows, i * ROW_WIDTH + 2);
        QJSIntlStatus check;
        if (kind >= QJS_INTL_DAYPERIOD_KIND_COUNT || map >= result.map_count ||
            (i && (word(result.rows, (i - 1) * ROW_WIDTH) > kind ||
                   (word(result.rows, (i - 1) * ROW_WIDTH) == kind &&
                    word(result.rows, (i - 1) * ROW_WIDTH + 1) >= locale))) ||
            qjs_intl_data_string(data, locale, &text))
            goto done;
        check = qjs_intl_source_locale_validate(allocator, text);
        if (check) {
            status = check;
            goto done;
        }
        classes[map] |= 1U << kind;
    }
    for (i = 0; i < result.map_count; i++) {
        uint32_t start = word(result.maps, i * MAP_WIDTH), count = word(result.maps, i * MAP_WIDTH + 1);
        const uint8_t *rules;
        if (!classes[i] || start != next || count > result.rule_count - next)
            goto done;
        rules = result.rules + next * RULE_WIDTH * 4;
        if (!valid_rules(rules, count, classes[i] & (1U << QJS_INTL_DAYPERIOD_FORMAT)) ||
            (i && compare_lists(result.rules + (size_t)previous_start * RULE_WIDTH * 4, previous_count, rules, count) >= 0))
            goto done;
        previous_start = start;
        previous_count = count;
        next += count;
    }
    if (next != result.rule_count)
        goto done;
    *out = result;
    status = QJS_INTL_OK;
done:
    if (classes)
        allocator->free(allocator->opaque, classes);
    return status;
}

QJSIntlStatus qjs_intl_dayperiod_row(const QJSIntlDayPeriodData *data, uint32_t index,
                                     QJSIntlDayPeriodRuleSetKind *kind, QJSIntlBytes *origin,
                                     uint32_t *map)
{
    if (kind)
        *kind = QJS_INTL_DAYPERIOD_FORMAT;
    if (origin)
        *origin = (QJSIntlBytes){0};
    if (map)
        *map = 0;
    if (!data || !kind || !origin || !map || index >= data->row_count)
        return QJS_INTL_INVALID_ARGUMENT;
    *kind = (QJSIntlDayPeriodRuleSetKind)word(data->rows, (size_t)index * ROW_WIDTH);
    qjs_intl_data_string(&data->data, word(data->rows, (size_t)index * ROW_WIDTH + 1), origin);
    *map = word(data->rows, (size_t)index * ROW_WIDTH + 2);
    return QJS_INTL_OK;
}

static int compare_bytes(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t length = a.length < b.length ? a.length : b.length;
    int order = length ? memcmp(a.data, b.data, length) : 0;
    return order ? order : a.length < b.length ? -1 : a.length != b.length;
}

QJSIntlStatus qjs_intl_dayperiod_find(const QJSIntlDayPeriodData *data,
                                      QJSIntlDayPeriodRuleSetKind kind, QJSIntlBytes origin,
                                      uint32_t *map)
{
    size_t lo = 0, hi;
    if (map)
        *map = 0;
    if (!data || !map || (unsigned int)kind >= QJS_INTL_DAYPERIOD_KIND_COUNT || (!origin.data && origin.length))
        return QJS_INTL_INVALID_ARGUMENT;
    hi = data->row_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const uint8_t *record = data->rows + mid * ROW_WIDTH * 4;
        uint32_t current_kind = word(record, 0);
        QJSIntlBytes current;
        int order;
        qjs_intl_data_string(&data->data, word(record, 1), &current);
        order = current_kind != (uint32_t)kind ? (current_kind < (uint32_t)kind ? -1 : 1) : compare_bytes(current, origin);
        if (!order) {
            *map = word(record, 2);
            return QJS_INTL_OK;
        }
        if (order < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_dayperiod_map(const QJSIntlDayPeriodData *data, uint32_t index,
                                     uint32_t *start, uint32_t *count)
{
    if (start)
        *start = 0;
    if (count)
        *count = 0;
    if (!data || !start || !count || index >= data->map_count)
        return QJS_INTL_INVALID_ARGUMENT;
    *start = word(data->maps, (size_t)index * MAP_WIDTH);
    *count = word(data->maps, (size_t)index * MAP_WIDTH + 1);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_dayperiod_rule(const QJSIntlDayPeriodData *data, uint32_t index,
                                      QJSIntlDayPeriodRule *out)
{
    const uint8_t *record;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!data || !out || index >= data->rule_count)
        return QJS_INTL_INVALID_ARGUMENT;
    record = data->rules + (size_t)index * RULE_WIDTH * 4;
    out->code = (QJSIntlDayPeriodCode)word(record, 0);
    out->form = (QJSIntlDayPeriodRuleForm)word(record, 1);
    out->start = word(record, 2);
    out->end = word(record, 3);
    return QJS_INTL_OK;
}
