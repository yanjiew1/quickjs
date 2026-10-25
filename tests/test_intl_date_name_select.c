/* Native name policy without calendar arithmetic or numeric/text rendering.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../src/intl/datetime/name-select.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void *allocate(void *opaque, size_t size)
{
    size_t *calls = opaque;
    assert(size);
    ++*calls;
    return malloc(size);
}

static void release(void *opaque, void *ptr)
{
    (void)opaque;
    assert(ptr);
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static uint32_t calendar_id(const QJSIntlCalendarNameData *data, const char *name)
{
    uint32_t i;
    for (i = 0; i < data->calendar_count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_calendar_name_calendar_at(data, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}

static uint32_t locale_id(const QJSIntlCalendarNameData *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->section.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_locale(&data->data, i, &text, &parent));
        if (equal(text, name))
            return i;
    }
    abort();
}

static void check(const QJSIntlCalendarNameData *data)
{
    static const struct {
        const char *calendar;
        unsigned int symbol;
        size_t width;
        const char *code;
        unsigned int leap;
        QJSIntlDateNameState state;
        const char *text, *origin;
        QJSIntlCalendarNameYear year;
        QJSIntlCalendarNameWidth source_width;
        QJSIntlCalendarNameContext context;
    } cases[] = {
        {"gregorian", 'M', 3, "1", 0, QJS_INTL_DATE_NAME_TEXT, "child-January", "en", 0, QJS_INTL_CAL_NAME_WIDE, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'M', 4, "1", 0, QJS_INTL_DATE_NAME_TEXT, "child-January", "en", 0, QJS_INTL_CAL_NAME_WIDE, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'M', 5, "1", 0, QJS_INTL_DATE_NAME_TEXT, "J", "root", 0, QJS_INTL_CAL_NAME_NARROW, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'L', 4, "1", 0, QJS_INTL_DATE_NAME_TEXT, "standalone-January", "root", 0, QJS_INTL_CAL_NAME_WIDE, QJS_INTL_CAL_NAME_STAND_ALONE},
        {"gregorian", 'L', 3, "1", 0, QJS_INTL_DATE_NAME_MISSING, "", "", 0, 0, 0},
        {"gregorian", 'M', 4, "7", 0, QJS_INTL_DATE_NAME_TEXT, "common-seven", "root", QJS_INTL_CAL_NAME_STANDARD_YEAR, QJS_INTL_CAL_NAME_WIDE, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'M', 4, "7", 1, QJS_INTL_DATE_NAME_TEXT, "leap-seven", "root", QJS_INTL_CAL_NAME_LEAP_YEAR, QJS_INTL_CAL_NAME_WIDE, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'M', 4, "6", 1, QJS_INTL_DATE_NAME_TEXT, "ordinary-six", "root", 0, QJS_INTL_CAL_NAME_WIDE, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'M', 4, "8", 1, QJS_INTL_DATE_NAME_BLOCKED, "", "en", QJS_INTL_CAL_NAME_LEAP_YEAR, QJS_INTL_CAL_NAME_WIDE, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'M', 4, "9", 1, QJS_INTL_DATE_NAME_EMPTY, "", "en", QJS_INTL_CAL_NAME_LEAP_YEAR, QJS_INTL_CAL_NAME_WIDE, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'E', 1, "sun", 0, QJS_INTL_DATE_NAME_TEXT, "Sun", "root", 0, QJS_INTL_CAL_NAME_ABBREVIATED, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'e', 3, "sun", 0, QJS_INTL_DATE_NAME_TEXT, "Sun", "root", 0, QJS_INTL_CAL_NAME_ABBREVIATED, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'E', 4, "sun", 0, QJS_INTL_DATE_NAME_TEXT, "Sunday", "root", 0, QJS_INTL_CAL_NAME_WIDE, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'E', 5, "sun", 0, QJS_INTL_DATE_NAME_TEXT, "S", "root", 0, QJS_INTL_CAL_NAME_NARROW, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'E', 6, "sun", 0, QJS_INTL_DATE_NAME_TEXT, "Su", "root", 0, QJS_INTL_CAL_NAME_SHORT, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'E', 6, "tue", 0, QJS_INTL_DATE_NAME_TEXT, "Tue", "root", 0, QJS_INTL_CAL_NAME_ABBREVIATED, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'E', 6, "wed", 0, QJS_INTL_DATE_NAME_BLOCKED, "", "en", 0, QJS_INTL_CAL_NAME_SHORT, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'E', 6, "mon", 0, QJS_INTL_DATE_NAME_EMPTY, "", "root", 0, QJS_INTL_CAL_NAME_SHORT, QJS_INTL_CAL_NAME_FORMAT},
        {"gregorian", 'c', 6, "sun", 0, QJS_INTL_DATE_NAME_TEXT, "standalone-Su", "root", 0, QJS_INTL_CAL_NAME_SHORT, QJS_INTL_CAL_NAME_STAND_ALONE},
        {"gregorian", 'G', 1, "1", 0, QJS_INTL_DATE_NAME_TEXT, "AD", "root", 0, QJS_INTL_CAL_NAME_ABBREVIATED, QJS_INTL_CAL_NAME_NO_CONTEXT},
        {"gregorian", 'G', 4, "1", 0, QJS_INTL_DATE_NAME_TEXT, "AD-wide", "root", 0, QJS_INTL_CAL_NAME_WIDE, QJS_INTL_CAL_NAME_NO_CONTEXT},
        {"gregorian", 'G', 5, "1", 0, QJS_INTL_DATE_NAME_TEXT, "A", "root", 0, QJS_INTL_CAL_NAME_NARROW, QJS_INTL_CAL_NAME_NO_CONTEXT},
        {"gregorian", 'G', 1, "0", 0, QJS_INTL_DATE_NAME_MISSING, "", "", 0, 0, 0},
        {"japanese", 'G', 4, "236", 0, QJS_INTL_DATE_NAME_TEXT, "Reiwa", "root", 0, QJS_INTL_CAL_NAME_WIDE, QJS_INTL_CAL_NAME_NO_CONTEXT},
        {"japanese", 'G', 4, "reiwa", 0, QJS_INTL_DATE_NAME_MISSING, "", "", 0, 0, 0},
        {"chinese", 'U', 4, "1", 0, QJS_INTL_DATE_NAME_TEXT, "cycle-one", "root", 0, QJS_INTL_CAL_NAME_ABBREVIATED, QJS_INTL_CAL_NAME_FORMAT},
        {"chinese", 'U', 3, "60", 0, QJS_INTL_DATE_NAME_TEXT, "cycle-sixty", "root", 0, QJS_INTL_CAL_NAME_ABBREVIATED, QJS_INTL_CAL_NAME_FORMAT},
        {"chinese", 'U', 5, "1", 0, QJS_INTL_DATE_NAME_TEXT, "C", "root", 0, QJS_INTL_CAL_NAME_NARROW, QJS_INTL_CAL_NAME_FORMAT}
    };
    uint32_t locale = locale_id(data, "en-GB"), calendar = calendar_id(data, "gregorian");
    QJSIntlDateNameSelection result, zero = {0};
    QJSIntlDatePeriodNameSelection period, period_zero = {0};
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        QJSIntlCalendarNameGroup group;
        assert(!qjs_intl_date_name_select(data, locale, calendar_id(data, cases[i].calendar), cases[i].symbol,
                                         cases[i].width, bytes(cases[i].code), cases[i].leap, &result));
        assert(result.state == cases[i].state && equal(result.entry.text, cases[i].text));
        assert(equal(result.origin.origin_locale, cases[i].origin));
        assert(result.entry.year == cases[i].year);
        if (result.state == QJS_INTL_DATE_NAME_MISSING) {
            assert(!memcmp(&result, &zero, sizeof(result)));
        } else {
            assert(!qjs_intl_calendar_name_group_at(data, result.origin.group, &group));
            assert(group.width == cases[i].source_width && group.context == cases[i].context);
            assert(!result.entry.alt.length);
        }
    }
    for (i = 1; i <= 5; i++) {
        assert(!qjs_intl_date_period_name_select(data, locale, calendar, 'a', i, QJS_INTL_DAYPERIOD_AM,
                                                QJS_INTL_DAYPERIOD_AM, &period));
        assert(period.primary_state == QJS_INTL_DATE_NAME_TEXT && period.selected.state == QJS_INTL_DATE_NAME_TEXT);
        assert(equal(period.selected.entry.text, i == 4 ? "AM-wide" : i == 5 ? "a" : "AM"));
    }
    assert(!qjs_intl_date_period_name_select(data, locale, calendar, 'B', 3, QJS_INTL_DAYPERIOD_MORNING1,
                                            QJS_INTL_DAYPERIOD_AM, &period));
    assert(period.primary_state == QJS_INTL_DATE_NAME_BLOCKED && equal(period.selected.entry.text, "AM"));
    assert(equal(period.selected.origin.origin_locale, "root"));
    assert(!qjs_intl_date_period_name_select(data, locale, calendar, 'b', 3, QJS_INTL_DAYPERIOD_NOON,
                                            QJS_INTL_DAYPERIOD_PM, &period));
    assert(period.primary_state == QJS_INTL_DATE_NAME_EMPTY && equal(period.selected.entry.text, "PM"));
    assert(!qjs_intl_date_period_name_select(data, locale, calendar, 'B', 3, QJS_INTL_DAYPERIOD_MIDNIGHT,
                                            QJS_INTL_DAYPERIOD_AM, &period));
    assert(period.primary_state == QJS_INTL_DATE_NAME_BLOCKED && equal(period.selected.entry.text, "AM"));
    assert(!qjs_intl_date_period_name_select(data, locale, calendar, 'b', 4, QJS_INTL_DAYPERIOD_NOON,
                                            QJS_INTL_DAYPERIOD_PM, &period));
    assert(period.primary_state == QJS_INTL_DATE_NAME_MISSING && equal(period.selected.entry.text, "PM-wide"));
    assert(!qjs_intl_date_period_name_select(data, locale, calendar, 'B', 4, QJS_INTL_DAYPERIOD_MORNING1,
                                            QJS_INTL_DAYPERIOD_AM, &period));
    assert(period.primary_state == QJS_INTL_DATE_NAME_TEXT && equal(period.selected.entry.text, "morning-wide"));
    assert(!qjs_intl_date_period_name_select(data, locale, calendar_id(data, "japanese"), 'B', 3,
                                            QJS_INTL_DAYPERIOD_NIGHT1, QJS_INTL_DAYPERIOD_PM, &period));
    assert(period.primary_state == QJS_INTL_DATE_NAME_MISSING && period.selected.state == QJS_INTL_DATE_NAME_MISSING);
    for (i = 0; i < 4; i++) {
        unsigned int symbol = i == 0 ? 'M' : i == 1 ? 'e' : i == 2 ? 'd' : 'Q';
        assert(qjs_intl_date_name_select(data, locale, calendar, symbol, 1, bytes("1"), 0, &result) == QJS_INTL_UNSUPPORTED);
        assert(!memcmp(&result, &zero, sizeof(result)));
    }
    assert(qjs_intl_date_name_select(data, locale, calendar, 'G', SIZE_MAX, bytes("1"), 0, &result) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_date_name_select(data, locale, calendar, 'M', 0, bytes("1"), 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_name_select(data, locale, calendar, 'M', 3, bytes("1"), 2, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_name_select(data, locale, calendar, 'M', 3, (QJSIntlBytes){NULL, 1}, 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_name_select(data, data->section.count, calendar, 'M', 3, bytes("1"), 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_name_select(data, locale, data->calendar_count, 'M', 3, bytes("1"), 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_name_select(NULL, locale, calendar, 'M', 3, bytes("1"), 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_name_select(data, locale, calendar, 'M', 3, bytes("1"), 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    for (i = 0; i < 5; i++) {
        unsigned int symbol = i == 0 ? 'x' : i == 1 ? 'a' : 'b';
        QJSIntlDayPeriodCode primary = i == 3 ? (QJSIntlDayPeriodCode)-1 :
                                       i == 4 ? QJS_INTL_DAYPERIOD_NOON : QJS_INTL_DAYPERIOD_MORNING1;
        memset(&period, 0xff, sizeof(period));
        assert(qjs_intl_date_period_name_select(data, locale, calendar, symbol, 3, primary,
                                                QJS_INTL_DAYPERIOD_AM, &period) == QJS_INTL_INVALID_ARGUMENT);
        assert(!memcmp(&period, &period_zero, sizeof(period)));
    }
    assert(qjs_intl_date_period_name_select(data, locale, calendar, 'B', 3, QJS_INTL_DAYPERIOD_AM,
                                            QJS_INTL_DAYPERIOD_NOON, &period) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_period_name_select(data, locale, calendar, 'B', 3, QJS_INTL_DAYPERIOD_AM,
                                            QJS_INTL_DAYPERIOD_PM, &period) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_period_name_select(data, locale, calendar, 'a', 3, QJS_INTL_DAYPERIOD_AM,
                                            QJS_INTL_DAYPERIOD_AM, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    size_t calls = 0, before;
    QJSIntlAllocator allocator = {&calls, allocate, NULL, release};
    QJSIntlData shared;
    QJSIntlCalendarNameData data;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &shared));
    assert(!qjs_intl_calendar_name_data_open(&shared, &allocator, &data));
    before = calls;
    check(&data);
    assert(calls == before);
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &shared));
    assert(!qjs_intl_calendar_name_data_open(&shared, &allocator, &data));
    before = calls;
    check(&data);
    assert(calls == before);
    free(unaligned);
    return 0;
}
