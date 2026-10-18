/* Pure calendar regression coverage without JS allocation. */
#include <assert.h>
#include <string.h>
#include "../src/temporal/calendar.h"

int main(void)
{
    QJSCalendarId calendar;
    QJSTemporalISODate leap = {2024,2,29}, target;
    QJSTemporalCalendarDate fields;
    QJSTemporalCalendarFields input = {0};
    QJSTemporalDateDuration duration = {1,0,0,0}, difference;
    assert(!qjs_temporal_calendar_fields(QJS_CAL_ISO8601, leap, &fields));
    assert(fields.day_of_year == 60 && fields.day_of_week == 4);
    assert(fields.has_week && fields.week_of_year == 9);
    assert(!qjs_temporal_calendar_date_add(QJS_CAL_ISO8601, leap,
        duration, QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &target));
    assert(target.year == 2025 && target.month == 2 && target.day == 28);
    assert(qjs_temporal_calendar_date_add(QJS_CAL_ISO8601, leap,
        duration, QJS_TEMPORAL_OVERFLOW_REJECT, &target));
    assert(!qjs_temporal_calendar_date_until(QJS_CAL_ISO8601,
        (QJSTemporalISODate){2023,1,31}, (QJSTemporalISODate){2023,2,28},
        QJS_TEMPORAL_MONTH, &difference));
    assert(difference.months == 0 && difference.days == 28);
    input.present = QJS_TEMPORAL_FIELD_MONTH_CODE | QJS_TEMPORAL_FIELD_DAY | QJS_TEMPORAL_FIELD_YEAR;
    strcpy(input.month_code, "M02"); input.day = 29; input.year = 1e300;
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_ISO8601,
        &input, QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &target));
    assert(target.year == 1972 && target.day == 29);
    assert(qjs_temporal_calendar_from_identifier(&calendar, "islamic", 7));
    assert(!qjs_temporal_calendar_from_identifier(&calendar, "IsO8601", 7));
    assert(calendar == QJS_CAL_ISO8601);
    assert(qjs_temporal_calendar_from_identifier(&calendar, "gregory", 7));
    assert(calendar == QJS_CAL_ISO8601);
    assert(qjs_temporal_calendar_identifier(QJS_CAL_GREGORY) == NULL);
    assert(qjs_temporal_calendar_fields(QJS_CAL_GREGORY, leap,
        &fields) == QJS_TEMPORAL_ERROR_UNSUPPORTED);
    input.year = 275761;
    target = leap;
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_ISO8601,
        &input, QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &target));
    assert(!memcmp(&target, &leap, sizeof(target)));
    input.year = 2024;
    input.month = 3;
    input.present |= QJS_TEMPORAL_FIELD_MONTH;
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_ISO8601,
        &input, QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &target));
    assert(!memcmp(&target, &leap, sizeof(target)));

    return 0;
}
