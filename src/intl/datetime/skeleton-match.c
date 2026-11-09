/* Native source-skeleton selection and field-width comparison. */
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
#include "skeleton-match.h"
#include "basic-match.h"
#include <limits.h>
#include <string.h>
#define BIT(name) (1U << QJS_INTL_DATETIME_ ## name)

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

unsigned int qjs_intl_date_skeleton_mask(QJSIntlBytes skeleton)
{
    unsigned int mask = 0;
    size_t i;
    for (i = 0; i < skeleton.length; i++) {
        unsigned int c = (unsigned char)skeleton.data[i];
        int field;
        if (c == 'y' || c == 'r' || c == 'U' || (c == 'Y' && skeleton_calendar_year(skeleton)))
            field = QJS_INTL_DATETIME_YEAR;
        else if (c == 'M' || c == 'L') field = QJS_INTL_DATETIME_MONTH;
        else if (c == 'd') field = QJS_INTL_DATETIME_DAY;
        else if (c == 'G') field = QJS_INTL_DATETIME_ERA;
        else if (c == 'E' || c == 'e' || c == 'c') field = QJS_INTL_DATETIME_WEEKDAY;
        else if (c && strchr("hHKkjJC", c)) field = QJS_INTL_DATETIME_HOUR;
        else if (c == 'm') field = QJS_INTL_DATETIME_MINUTE;
        else if (c == 's') field = QJS_INTL_DATETIME_SECOND;
        else if (c == 'S') field = QJS_INTL_DATETIME_FRACTION;
        else if (c == 'b' || c == 'B') field = QJS_INTL_DATETIME_DAY_PERIOD;
        else if (c && strchr("zZvVOXx", c)) field = QJS_INTL_DATETIME_ZONE;
        else if (c == 'a') continue; /* Implicit in twelve-hour skeletons. */
        else return UINT_MAX;
        mask |= 1U << field;
    }
    return mask;
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
        /* Numeric/text month changes outweigh all within-kind width changes. */
        if (i == 1 && wanted && source_width && ((width <= 2) != (source_width <= 2)))
            distance += 100;
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

QJSIntlStatus qjs_intl_date_skeleton_select(const QJSIntlDateSourceCandidates *pool,
                                            const QJSIntlDateTimeComponents *request,
                                            QJSIntlDateHourCycle cycle, int partial,
                                            size_t *selected, QJSIntlDateTimeComponents *basis)
{
    static const unsigned int priority[] = {
        QJS_INTL_DATETIME_YEAR, QJS_INTL_DATETIME_MONTH, QJS_INTL_DATETIME_DAY,
        QJS_INTL_DATETIME_ERA, QJS_INTL_DATETIME_WEEKDAY, QJS_INTL_DATETIME_HOUR,
        QJS_INTL_DATETIME_MINUTE, QJS_INTL_DATETIME_SECOND,
        QJS_INTL_DATETIME_DAY_PERIOD, QJS_INTL_DATETIME_ZONE, QJS_INTL_DATETIME_FRACTION
    };
    unsigned int wanted = qjs_intl_datetime_component_mask(request);
    unsigned int best_count = 0, best_priority = 0;
    int best_distance = INT_MAX, best_score = INT_MIN, best_year = -1, best_period = -1;
    size_t i;
    *selected = SIZE_MAX;
    QJSIntlStatus status;
    int twelve = cycle <= QJS_INTL_DATE_H12;
    int era_required = 1;

    /* An unrequested era is implicit only when all available year patterns
       require it (not merely because one matching source includes G). */
    for (i = 0; i < pool->count; i++) {
        const QJSIntlDateTimeComponents *components = &pool->records[i].info.components;
        if (components->values[QJS_INTL_DATETIME_YEAR] && !components->values[QJS_INTL_DATETIME_ERA]) {
            era_required = 0;
            break;
        }
    }
    for (i = 0; i < pool->count; i++) {
        const QJSIntlDateSourceCandidate *raw = &pool->records[i];
        QJSIntlDateTimeComponents query = *request;
        unsigned int fields = qjs_intl_date_skeleton_mask(raw->source.code);
        unsigned int covered, extra, count = 0, rank = 0;
        unsigned char widths[5];
        size_t j;
        int distance, score, numeric_year, genuine_period, better;
        unsigned int hour = raw->info.hour_symbol;
        if (!fields || fields == UINT_MAX ||
            (!partial && raw->info.year_kinds == QJS_INTL_DATE_YEAR_CYCLIC) ||
            (!hour && raw->info.period_symbol == 'a') ||
            (hour && (((hour == 'h' || hour == 'K') != twelve) ||
             (twelve && !raw->info.period_symbol) || (!twelve && raw->info.period_symbol == 'a'))))
            continue;
        covered = fields & wanted;
        extra = fields & ~wanted;
        /* An era can be required by the calendar, and a twelve-hour key can
           spell out a period that an ordinary hour request leaves implicit. */
        if (partial && era_required && (wanted & BIT(YEAR)) && (fields & BIT(YEAR)))
            extra &= ~BIT(ERA);
        if (hour && !(wanted & BIT(DAY_PERIOD)))
            extra &= ~BIT(DAY_PERIOD);
        if (extra)
            continue;
        if ((wanted & BIT(FRACTION)) && (fields & BIT(SECOND)))
            covered |= BIT(FRACTION);
        if ((wanted & BIT(DAY_PERIOD)) && hour && raw->info.period_symbol == 'a')
            covered |= BIT(DAY_PERIOD);
        if (!partial && covered != wanted)
            continue;
        for (j = 0; j < QJS_INTL_DATETIME_COMPONENT_COUNT; j++) {
            if (!(fields & (1U << j)) && j != QJS_INTL_DATETIME_FRACTION)
                query.values[j] = 0;
            else if ((fields & (1U << j)) && !query.values[j])
                query.values[j] = raw->info.components.values[j];
        }
        if (!(fields & (BIT(SECOND) | BIT(FRACTION))))
            query.values[QJS_INTL_DATETIME_FRACTION] = 0;
        for (j = 0; j < sizeof(priority) / sizeof(*priority); j++) {
            unsigned int present = (covered >> priority[j]) & 1U;
            count += present;
            rank = (rank << 1) | present;
        }
        if (!count || count < best_count)
            continue;
        distance = qjs_intl_date_skeleton_distance(raw->source.code, &query,
                                                    cycle, widths);
        if (distance >= QJS_INTL_DATE_SKELETON_UNMATCHED)
            continue;
        status = qjs_intl_datetime_basic_score(request, &raw->info.components, &score);
        if (status)
            return status;
        numeric_year = !(wanted & BIT(YEAR)) || raw->info.year_kinds != QJS_INTL_DATE_YEAR_CYCLIC;
        genuine_period = !(wanted & BIT(DAY_PERIOD)) || (fields & BIT(DAY_PERIOD)) != 0;
        /* CLDR uses implicit a only when no corresponding b/B source matches.
           Source width differences must not make that substitute win early. */
        if (count != best_count) better = count > best_count;
        else if (genuine_period != best_period) better = genuine_period > best_period;
        else if (distance != best_distance) better = distance < best_distance;
        else if (rank != best_priority) better = rank > best_priority;
        else if (numeric_year != best_year) better = numeric_year > best_year;
        else better = score > best_score;
        if (better) {
            *selected = i;
            best_count = count;
            best_distance = distance;
            best_priority = rank;
            best_score = score;
            best_year = numeric_year;
            best_period = genuine_period;
            *basis = query;
        }
    }
    return QJS_INTL_OK;
}
