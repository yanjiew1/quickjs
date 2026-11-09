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
#include "program-adapt.h"
#include <string.h>

/* An available-format skeleton describes the requested field widths, which
   can intentionally differ from the pattern selected by CLDR. */
static int skeleton_calendar_year(QJSIntlBytes skeleton)
{
    /* Match the workaround for CLDR entries that mistakenly use Y in
       month-based, non-week patterns; this is not a standards rule. */
    return skeleton.length && memchr(skeleton.data, 'Y', skeleton.length) &&
           (memchr(skeleton.data, 'M', skeleton.length) ||
            memchr(skeleton.data, 'L', skeleton.length)) &&
           !memchr(skeleton.data, 'w', skeleton.length) &&
           !memchr(skeleton.data, 'W', skeleton.length);
}

void qjs_intl_date_skeleton_widths(QJSIntlBytes skeleton, unsigned char widths[5])
{
    int calendar_year = skeleton_calendar_year(skeleton);
    size_t i = 0;

    memset(widths, 0, 5);
    while (i < skeleton.length) {
        unsigned int symbol = (unsigned char)skeleton.data[i];
        size_t start = i++, length;
        int index;
        while (i < skeleton.length && (unsigned char)skeleton.data[i] == symbol)
            i++;
        length = i - start;
        if (symbol == 'y' || (symbol == 'Y' && calendar_year)) {
            index = 0;
        } else if (symbol == 'r' || symbol == 'U') {
            /* Related and cyclic years do not have two-digit year semantics. */
            widths[0] = QJS_INTL_DATE_NONCALENDAR_YEAR_WIDTH;
            continue;
        } else if (symbol == 'M' || symbol == 'L') {
            index = 1;
        } else if (symbol == 'd') {
            index = 2;
        } else if (symbol == 'G' || symbol == 'E' || symbol == 'e' || symbol == 'c') {
            index = symbol == 'G' ? 3 : 4;
        } else {
            continue;
        }
        if (!widths[index]) {
            if (index == 0)
                widths[index] = length < QJS_INTL_DATE_NONCALENDAR_YEAR_WIDTH ?
                                length : QJS_INTL_DATE_NONCALENDAR_YEAR_WIDTH - 1;
            else if (length <= (index == 4 ? 6 : 5))
                widths[index] = length;
        }
    }
}

/* Rank a complete available-format key, rather than combining field widths
   from different aliases of the same pattern. */
int qjs_intl_date_skeleton_distance(QJSIntlBytes skeleton, const QJSIntlDateTimeComponents *request,
                                    QJSIntlDateHourCycle cycle, unsigned char widths[5])
{
    static const size_t fields[] = {QJS_INTL_DATETIME_YEAR, QJS_INTL_DATETIME_MONTH,
        QJS_INTL_DATETIME_DAY, QJS_INTL_DATETIME_ERA, QJS_INTL_DATETIME_WEEKDAY};
    const unsigned int time_mask = (1U << QJS_INTL_DATETIME_HOUR) |
        (1U << QJS_INTL_DATETIME_MINUTE) | (1U << QJS_INTL_DATETIME_SECOND) |
        (1U << QJS_INTL_DATETIME_FRACTION) | (1U << QJS_INTL_DATETIME_DAY_PERIOD) |
        (1U << QJS_INTL_DATETIME_ZONE);
    unsigned int time_fields = 0, requested = qjs_intl_datetime_component_mask(request);
    int calendar_year = skeleton_calendar_year(skeleton);
    int distance = 0;
    size_t i;

    qjs_intl_date_skeleton_widths(skeleton, widths);
    for (i = 0; i < 5; i++) {
        int wanted = request->values[fields[i]];
        int source_width = widths[i];
        int width = wanted == QJS_INTL_DATETIME_TWO_DIGIT ? 2 :
                    wanted == QJS_INTL_DATETIME_SHORT ? 3 :
                    wanted == QJS_INTL_DATETIME_LONG ? 4 :
                    wanted == QJS_INTL_DATETIME_NARROW ? 5 : 1;
        if (widths[i] == QJS_INTL_DATE_NONCALENDAR_YEAR_WIDTH) {
            if (!wanted)
                return QJS_INTL_DATE_SKELETON_UNMATCHED;
            continue;
        }
        if (i >= 3 && source_width && source_width < 3)
            source_width = 3;
        if (wanted && source_width)
            distance += source_width > width ? source_width - width : width - source_width;
        else if (!wanted && source_width)
            return QJS_INTL_DATE_SKELETON_UNMATCHED;
        else if (wanted)
            return QJS_INTL_DATE_SKELETON_UNMATCHED;
    }
    for (i = 0; i < skeleton.length;) {
        size_t start = i;
        unsigned int c = (unsigned char)skeleton.data[i++];
        int field = -1;
        size_t length;
        while (i < skeleton.length && (unsigned char)skeleton.data[i] == c)
            i++;
        length = i - start;
        if (!c)
            continue;
        if (strchr("hHKkjJC", c)) {
            time_fields |= 1U << QJS_INTL_DATETIME_HOUR;
            field = QJS_INTL_DATETIME_HOUR;
            if (request->values[field] && strchr("hHKk", c) &&
                ((c == 'h' || c == 'K') != (cycle <= QJS_INTL_DATE_H12)))
                distance += 500;
        } else if (c == 'm') {
            time_fields |= 1U << QJS_INTL_DATETIME_MINUTE;
            field = QJS_INTL_DATETIME_MINUTE;
        } else if (c == 's') {
            time_fields |= 1U << QJS_INTL_DATETIME_SECOND;
            field = QJS_INTL_DATETIME_SECOND;
        } else if (c == 'S') {
            time_fields |= 1U << QJS_INTL_DATETIME_FRACTION;
            field = QJS_INTL_DATETIME_FRACTION;
        } else if (c == 'b' || c == 'B') {
            time_fields |= 1U << QJS_INTL_DATETIME_DAY_PERIOD;
            field = QJS_INTL_DATETIME_DAY_PERIOD;
        } else if (strchr("zZvVOXx", c)) {
            time_fields |= 1U << QJS_INTL_DATETIME_ZONE;
            if (request->values[QJS_INTL_DATETIME_ZONE]) {
                int style = 0;
                if (c == 'z' && length <= 4)
                    style = length == 4 ? QJS_INTL_DATETIME_ZONE_LONG : QJS_INTL_DATETIME_ZONE_SHORT;
                else if (c == 'v' && (length == 1 || length == 4))
                    style = length == 4 ? QJS_INTL_DATETIME_ZONE_LONG_GENERIC : QJS_INTL_DATETIME_ZONE_SHORT_GENERIC;
                else if (c == 'O' && (length == 1 || length == 4))
                    style = length == 4 ? QJS_INTL_DATETIME_ZONE_LONG_OFFSET : QJS_INTL_DATETIME_ZONE_SHORT_OFFSET;
                if (style != request->values[QJS_INTL_DATETIME_ZONE])
                    distance += 500;
            }
        } else if (c == 'Y' && !calendar_year) {
            return QJS_INTL_DATE_SKELETON_UNMATCHED;
        } else if (c == 'a') {
            if (!request->values[QJS_INTL_DATETIME_HOUR])
                return QJS_INTL_DATE_SKELETON_UNMATCHED;
            if (cycle > QJS_INTL_DATE_H12)
                distance += 500;
        } else if (!strchr("yYMrUMdGLEec", c)) {
            /* Quarter, week and other fields have no ECMA counterpart. */
            return QJS_INTL_DATE_SKELETON_UNMATCHED;
        }
        if (field >= 0 && request->values[field]) {
            size_t wanted = field == QJS_INTL_DATETIME_DAY_PERIOD ?
                            (request->values[field] == QJS_INTL_DATETIME_NARROW ? 5 :
                             request->values[field] == QJS_INTL_DATETIME_LONG ? 4 : 3) :
                            field == QJS_INTL_DATETIME_FRACTION ? request->values[field] :
                            request->values[field] == QJS_INTL_DATETIME_TWO_DIGIT ? 2 : 1;
            if (field == QJS_INTL_DATETIME_DAY_PERIOD && length < 3)
                length = 3;
            size_t difference = length > wanted ? length - wanted : wanted - length;
            distance += difference > 20 ? 20 : (int)difference;
        }
        if (distance >= QJS_INTL_DATE_SKELETON_UNMATCHED)
            return QJS_INTL_DATE_SKELETON_UNMATCHED;
    }
    if (time_fields & ~requested)
        return QJS_INTL_DATE_SKELETON_UNMATCHED;
    time_fields ^= requested & time_mask;
    /* A request for fractional seconds may match a seconds-only skeleton. */
    if (time_fields == (1U << QJS_INTL_DATETIME_FRACTION) &&
        (requested & (1U << QJS_INTL_DATETIME_SECOND)))
        time_fields = 0;
    if (time_fields)
        return QJS_INTL_DATE_SKELETON_UNMATCHED;
    return distance;
}

int qjs_intl_date_best_alias(const QJSIntlDateSourceCandidates *pool,
                             const QJSIntlDateSourceCandidate *selected,
                             const QJSIntlDateTimeComponents *request,
                             QJSIntlDateHourCycle cycle, unsigned char widths[5])
{
    QJSIntlDateTimeComponents relevant = *request;
    int distance;
    size_t i;

    /* Synthesized records can obtain fields from other operands. */
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        if (!selected->info.components.values[i])
            relevant.values[i] = 0;
    distance = qjs_intl_date_skeleton_distance(selected->source.code, &relevant, cycle, widths);

    for (i = 0; i < pool->count; i++) {
        const QJSIntlDateSourceCandidate *alias = &pool->records[i];
        QJSIntlBytes a = alias->source.text, b = selected->source.text;
        unsigned char alternative[5];
        int other;

        if (alias == selected || a.length != b.length ||
            (a.length && a.data != b.data && memcmp(a.data, b.data, a.length)))
            continue;
        other = qjs_intl_date_skeleton_distance(alias->source.code, &relevant, cycle, alternative);
        if (other < distance) {
            distance = other;
            memcpy(widths, alternative, sizeof(alternative));
        }
    }
    return distance;
}

static unsigned int width_flag(size_t field)
{
    return field == QJS_INTL_DATETIME_YEAR ? QJS_INTL_DATE_ADAPT_YEAR_WIDTH :
           field == QJS_INTL_DATETIME_MONTH ? QJS_INTL_DATE_ADAPT_MONTH_WIDTH :
           field == QJS_INTL_DATETIME_DAY ? QJS_INTL_DATE_ADAPT_DAY_WIDTH :
           field == QJS_INTL_DATETIME_ERA ? QJS_INTL_DATE_ADAPT_ERA_WIDTH :
           field == QJS_INTL_DATETIME_WEEKDAY ? QJS_INTL_DATE_ADAPT_WEEKDAY_WIDTH :
           field == QJS_INTL_DATETIME_HOUR ? QJS_INTL_DATE_ADAPT_HOUR_WIDTH :
           field == QJS_INTL_DATETIME_MINUTE ? QJS_INTL_DATE_ADAPT_MINUTE_WIDTH :
           field == QJS_INTL_DATETIME_SECOND ? QJS_INTL_DATE_ADAPT_SECOND_WIDTH :
           field == QJS_INTL_DATETIME_DAY_PERIOD ? QJS_INTL_DATE_ADAPT_PERIOD_WIDTH : 0;
}

static int legal_width(size_t field, int value)
{
    if (field == QJS_INTL_DATETIME_DAY_PERIOD || field == QJS_INTL_DATETIME_ERA ||
        field == QJS_INTL_DATETIME_WEEKDAY)
        return value >= QJS_INTL_DATETIME_NARROW && value <= QJS_INTL_DATETIME_LONG;
    if (field == QJS_INTL_DATETIME_MONTH)
        return value >= QJS_INTL_DATETIME_TWO_DIGIT && value <= QJS_INTL_DATETIME_LONG;
    return value == QJS_INTL_DATETIME_NUMERIC || value == QJS_INTL_DATETIME_TWO_DIGIT;
}

QJSIntlStatus qjs_intl_date_program_adapt(const QJSIntlAllocator *allocator,
                                        const QJSIntlDatePattern *source,
                                        const QJSIntlDateTimeComponents *components,
                                        unsigned int allowed_widths, unsigned int twelve,
                                        QJSIntlDatePattern *out, unsigned int *adaptations)
{
    QJSIntlDatePatternInfo before, after;
    QJSIntlDatePattern result = {0};
    QJSIntlStatus status;
    unsigned int changed = 0;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (adaptations)
        *adaptations = 0;
    if (!out || !adaptations || !components || !allocator || !allocator->malloc || !allocator->free ||
        twelve > 1 || (allowed_widths & ~(QJS_INTL_DATE_ADAPT_HOUR_WIDTH |
                                         QJS_INTL_DATE_ADAPT_MINUTE_WIDTH | QJS_INTL_DATE_ADAPT_SECOND_WIDTH |
                                         QJS_INTL_DATE_ADAPT_PERIOD_WIDTH | QJS_INTL_DATE_ADAPT_YEAR_WIDTH |
                                         QJS_INTL_DATE_ADAPT_MONTH_WIDTH | QJS_INTL_DATE_ADAPT_DAY_WIDTH |
                                         QJS_INTL_DATE_ADAPT_ERA_WIDTH | QJS_INTL_DATE_ADAPT_WEEKDAY_WIDTH)))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_pattern_info(source, &before);
    if (status)
        return status;
    if (before.components.values[QJS_INTL_DATETIME_YEAR] != components->values[QJS_INTL_DATETIME_YEAR] &&
        before.year_kinds != QJS_INTL_DATE_YEAR_CALENDAR)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        if (before.components.values[i] != components->values[i] &&
            (!(width_flag(i) & allowed_widths) || !legal_width(i, before.components.values[i]) ||
             !legal_width(i, components->values[i]) ||
             (i == QJS_INTL_DATETIME_MONTH &&
              ((before.components.values[i] <= QJS_INTL_DATETIME_NUMERIC) !=
               (components->values[i] <= QJS_INTL_DATETIME_NUMERIC)))))
            return QJS_INTL_UNSUPPORTED;
    }
    status = qjs_intl_date_pattern_copy(allocator, source, &result);
    if (status)
        return status;
    for (i = 0; i < result.count; i++) {
        QJSIntlDatePatternToken *token = &result.tokens[i];
        size_t field, width;
        int hour = token->symbol == 'h' || token->symbol == 'H' || token->symbol == 'K' || token->symbol == 'k';
        if (token->symbol == 'y' || token->symbol == 'M' || token->symbol == 'L' || token->symbol == 'd') {
            field = token->symbol == 'y' ? QJS_INTL_DATETIME_YEAR :
                    token->symbol == 'd' ? QJS_INTL_DATETIME_DAY : QJS_INTL_DATETIME_MONTH;
            if (before.components.values[field] != components->values[field]) {
                int wanted = components->values[field];
                token->width = wanted == QJS_INTL_DATETIME_TWO_DIGIT ? 2 :
                               wanted == QJS_INTL_DATETIME_NARROW ? 5 :
                               wanted == QJS_INTL_DATETIME_LONG ? 4 :
                               wanted == QJS_INTL_DATETIME_SHORT ? 3 : 1;
                changed |= width_flag(field);
            }
            continue;
        }
        if (token->symbol == 'G' || token->symbol == 'E' || token->symbol == 'e' || token->symbol == 'c') {
            int wanted;
            field = token->symbol == 'G' ? QJS_INTL_DATETIME_ERA : QJS_INTL_DATETIME_WEEKDAY;
            wanted = components->values[field];
            if (before.components.values[field] != wanted) {
                token->width = wanted == QJS_INTL_DATETIME_NARROW ? 5 :
                               wanted == QJS_INTL_DATETIME_LONG ? 4 : 3;
                changed |= width_flag(field);
            }
            continue;
        }
        if (token->symbol == 'b' || token->symbol == 'B') {
            int value = components->values[QJS_INTL_DATETIME_DAY_PERIOD];
            if (before.components.values[QJS_INTL_DATETIME_DAY_PERIOD] != value) {
                token->width = value == QJS_INTL_DATETIME_NARROW ? 5 : value == QJS_INTL_DATETIME_LONG ? 4 : 1;
                changed |= QJS_INTL_DATE_ADAPT_PERIOD_WIDTH;
            }
            continue;
        }
        if (!hour && token->symbol != 'm' && token->symbol != 's')
            continue;
        field = hour ? QJS_INTL_DATETIME_HOUR : token->symbol == 'm' ? QJS_INTL_DATETIME_MINUTE : QJS_INTL_DATETIME_SECOND;
        width = components->values[field] == QJS_INTL_DATETIME_TWO_DIGIT ? 2 : 1;
        if (token->width != width) {
            token->width = width;
            changed |= width_flag(field);
        }
        if (hour && (unsigned int)(token->symbol == 'h' || token->symbol == 'K') != twelve) {
            token->symbol = twelve ? 'h' : 'H';
            changed |= QJS_INTL_DATE_ADAPT_HOUR_FAMILY;
        }
    }
    status = qjs_intl_date_pattern_info(&result, &after);
    if (!status) {
        for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
            if (after.components.values[i] != components->values[i]) {
                status = QJS_INTL_DATA_ERROR;
                break;
            }
        }
        if (after.year_kinds != before.year_kinds || after.period_symbol != before.period_symbol)
            status = QJS_INTL_DATA_ERROR;
    }
    if (status) {
        qjs_intl_date_pattern_clear(allocator, &result);
        return status;
    }
    *out = result;
    *adaptations = changed;
    return QJS_INTL_OK;
}
