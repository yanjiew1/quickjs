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

/* Add the requested subsecond run using the selected numbering system. */
QJSIntlStatus qjs_intl_date_pattern_fraction(const QJSIntlAllocator *allocator,
                                             const QJSIntlDatePattern *base, QJSIntlBytes decimal,
                                             size_t digits, QJSIntlDatePattern *out)
{
    QJSIntlDatePatternToken tokens[3] = {{{0}, 0, 0}, {decimal, 0, 0}, {{0}, digits, 'S'}};
    QJSIntlDatePattern replacement = {0};
    size_t i;
    for (i = 0; i < base->count; i++) {
        if (base->tokens[i].symbol == 'S') {
            replacement.tokens = &tokens[2];
            replacement.count = 1;
            return qjs_intl_date_pattern_replace(allocator, base, 'S', &replacement, out);
        }
    }
    for (i = 0; i < base->count; i++)
        if (base->tokens[i].symbol == 's')
            break;
    if (i == base->count)
        return QJS_INTL_DATA_ERROR;
    tokens[0] = base->tokens[i];
    replacement.tokens = tokens;
    replacement.count = 3;
    return qjs_intl_date_pattern_replace(allocator, base, 's', &replacement, out);
}
