/* Pure calendar regression coverage without JS allocation. */
#include <assert.h>
#include <string.h>
#include "../src/temporal/calendar.h"

int main(void)
{
    QJSTemporalCalendar calendar;
    QJSTemporalISODate leap = {2024,2,29}, target;
    QJSTemporalCalendarDate fields;
    QJSTemporalCalendarFields input = {0};
    QJSTemporalDateDuration duration = {1,0,0,0}, difference;
    assert(!qjs_temporal_calendar_fields(QJS_TEMPORAL_CAL_ISO8601, leap, &fields));
    assert(fields.day_of_year == 60 && fields.day_of_week == 4);
    assert(fields.has_week && fields.week_of_year == 9);
    assert(!qjs_temporal_calendar_date_add(QJS_TEMPORAL_CAL_ISO8601, leap,
        duration, QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &target));
    assert(target.year == 2025 && target.month == 2 && target.day == 28);
    assert(qjs_temporal_calendar_date_add(QJS_TEMPORAL_CAL_ISO8601, leap,
        duration, QJS_TEMPORAL_OVERFLOW_REJECT, &target));
    assert(!qjs_temporal_calendar_date_until(QJS_TEMPORAL_CAL_ISO8601,
        (QJSTemporalISODate){2023,1,31}, (QJSTemporalISODate){2023,2,28},
        QJS_TEMPORAL_MONTH, &difference));
    assert(difference.months == 0 && difference.days == 28);
    input.present = QJS_TEMPORAL_FIELD_MONTH_CODE | QJS_TEMPORAL_FIELD_DAY | QJS_TEMPORAL_FIELD_YEAR;
    strcpy(input.month_code, "M02"); input.day = 29; input.year = 1e300;
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_TEMPORAL_CAL_ISO8601,
        &input, QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &target));
    assert(target.year == 1972 && target.day == 29);
    assert(qjs_temporal_calendar_from_identifier(&calendar, "islamic", 7));
#ifdef CONFIG_ICU
    for (int i = QJS_TEMPORAL_CAL_BUDDHIST; i < QJS_TEMPORAL_CAL_COUNT; i++) {
        assert(!qjs_temporal_calendar_fields((QJSTemporalCalendar)i, leap, &fields));
        input.present = QJS_TEMPORAL_FIELD_YEAR | QJS_TEMPORAL_FIELD_MONTH_CODE | QJS_TEMPORAL_FIELD_DAY;
        input.year = fields.year; input.day = fields.day;
        strcpy(input.month_code, fields.month_code);
        assert(!qjs_temporal_calendar_date_from_fields((QJSTemporalCalendar)i,
            &input, QJS_TEMPORAL_OVERFLOW_REJECT, &target));
        assert(target.year == leap.year && target.month == leap.month && target.day == leap.day);
        assert(!fields.has_week);
    }
    assert(!qjs_temporal_calendar_from_identifier(&calendar, "islamicc", 8));
    assert(calendar == QJS_TEMPORAL_CAL_ISLAMIC_CIVIL);
#else
    assert(qjs_temporal_calendar_from_identifier(&calendar, "gregory", 7));
#endif
    return 0;
}
