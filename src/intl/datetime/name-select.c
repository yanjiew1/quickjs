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
#include "name-select.h"
#include <string.h>

static QJSIntlStatus field_group(unsigned int symbol, size_t width, QJSIntlCalendarNameGroup *group)
{
    if (!width)
        return QJS_INTL_INVALID_ARGUMENT;
    group->context = QJS_INTL_CAL_NAME_FORMAT;
    group->width = width == 4 ? QJS_INTL_CAL_NAME_WIDE :
                   width == 5 ? QJS_INTL_CAL_NAME_NARROW : QJS_INTL_CAL_NAME_ABBREVIATED;
    switch (symbol) {
    case 'G':
        group->family = QJS_INTL_CAL_NAME_ERA;
        group->context = QJS_INTL_CAL_NAME_NO_CONTEXT;
        break;
    case 'M': case 'L':
        if (width < 3)
            return QJS_INTL_UNSUPPORTED;
        group->family = QJS_INTL_CAL_NAME_MONTH;
        if (symbol == 'L')
            group->context = QJS_INTL_CAL_NAME_STAND_ALONE;
        break;
    case 'E': case 'e': case 'c':
        if (width > 6 || (symbol != 'E' && width < 3))
            return QJS_INTL_UNSUPPORTED;
        group->family = QJS_INTL_CAL_NAME_DAY;
        if (symbol == 'c')
            group->context = QJS_INTL_CAL_NAME_STAND_ALONE;
        if (width == 6)
            group->width = QJS_INTL_CAL_NAME_SHORT;
        return QJS_INTL_OK;
    case 'U':
        group->family = QJS_INTL_CAL_NAME_CYCLIC;
        group->cycle = QJS_INTL_CAL_NAME_CYCLE_YEAR;
        break;
    case 'a': case 'b': case 'B':
        group->family = QJS_INTL_CAL_NAME_DAY_PERIOD;
        break;
    default:
        return QJS_INTL_UNSUPPORTED;
    }
    return width <= 5 ? QJS_INTL_OK : QJS_INTL_UNSUPPORTED;
}

static QJSIntlStatus resolve(const QJSIntlCalendarNameData *data, uint32_t locale,
                             const QJSIntlCalendarNameGroup *group, QJSIntlBytes code,
                             QJSIntlCalendarNameYear year, QJSIntlDateNameSelection *out)
{
    uint32_t index, head;
    QJSIntlStatus status = qjs_intl_calendar_name_find_group(data, group, &index);
    memset(out, 0, sizeof(*out));
    if (!status)
        status = qjs_intl_calendar_name_head(data, locale, index, &head);
    if (!status)
        status = qjs_intl_calendar_name_resolve(data, head, code, year, (QJSIntlBytes){0},
                                               &out->entry, &out->origin);
    if (status == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    if (status)
        return status;
    out->state = out->entry.blocked ? QJS_INTL_DATE_NAME_BLOCKED :
                 out->entry.text.length ? QJS_INTL_DATE_NAME_TEXT : QJS_INTL_DATE_NAME_EMPTY;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_name_select(const QJSIntlCalendarNameData *data,
                                      uint32_t locale, uint32_t calendar, unsigned int symbol,
                                      size_t width, QJSIntlBytes code, unsigned int leap_year,
                                      QJSIntlDateNameSelection *out)
{
    QJSIntlCalendarNameGroup group = {0};
    QJSIntlDateNameSelection result;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        calendar >= data->calendar_count || !code.data || !code.length ||
        code.length > PTRDIFF_MAX || leap_year > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    group.calendar = calendar;
    status = field_group(symbol, width, &group);
    if (status)
        return status;
    if (group.family == QJS_INTL_CAL_NAME_MONTH) {
        status = resolve(data, locale, &group, code, leap_year ? QJS_INTL_CAL_NAME_LEAP_YEAR :
                                                               QJS_INTL_CAL_NAME_STANDARD_YEAR, &result);
        if (status)
            return status;
        if (result.state != QJS_INTL_DATE_NAME_MISSING) {
            *out = result;
            return QJS_INTL_OK;
        }
    }
    status = resolve(data, locale, &group, code, QJS_INTL_CAL_NAME_NO_YEAR, &result);
    if (!status && result.state == QJS_INTL_DATE_NAME_MISSING &&
        group.family == QJS_INTL_CAL_NAME_DAY && group.width == QJS_INTL_CAL_NAME_SHORT) {
        group.width = QJS_INTL_CAL_NAME_ABBREVIATED;
        status = resolve(data, locale, &group, code, QJS_INTL_CAL_NAME_NO_YEAR, &result);
    }
    if (!status)
        *out = result;
    return status;
}

QJSIntlStatus qjs_intl_date_period_name_select(const QJSIntlCalendarNameData *data,
                                             uint32_t locale, uint32_t calendar, unsigned int symbol,
                                             size_t width, QJSIntlDayPeriodCode primary,
                                             QJSIntlDayPeriodCode fallback,
                                             QJSIntlDatePeriodNameSelection *out)
{
    QJSIntlDatePeriodNameSelection result;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (symbol != 'a' && symbol != 'b' && symbol != 'B') ||
        (unsigned int)primary >= QJS_INTL_DAYPERIOD_CODE_COUNT ||
        (fallback != QJS_INTL_DAYPERIOD_AM && fallback != QJS_INTL_DAYPERIOD_PM) ||
        (symbol == 'a' && primary != fallback) ||
        (primary <= QJS_INTL_DAYPERIOD_PM && primary != fallback) ||
        (symbol == 'b' && (primary > QJS_INTL_DAYPERIOD_NOON ||
                           (primary == QJS_INTL_DAYPERIOD_NOON && fallback != QJS_INTL_DAYPERIOD_PM) ||
                           (primary == QJS_INTL_DAYPERIOD_MIDNIGHT && fallback != QJS_INTL_DAYPERIOD_AM))))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_name_select(data, locale, calendar, symbol, width,
                                      qjs_intl_dayperiod_code_name(primary), 0, &result.selected);
    if (status)
        return status;
    result.primary_state = result.selected.state;
    if (result.primary_state != QJS_INTL_DATE_NAME_TEXT && primary != fallback)
        status = qjs_intl_date_name_select(data, locale, calendar, symbol, width,
                                          qjs_intl_dayperiod_code_name(fallback), 0, &result.selected);
    if (!status)
        *out = result;
    return status;
}
