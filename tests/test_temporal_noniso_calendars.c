/* Consumer limits and unchecked-input contracts, outside the JS surface. */
#include "../src/temporal/calendar.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int main(void)
{
    QJSTemporalCalendarFields fields = {0};
    QJSTemporalISODate result, saved = {2024, 2, 29};
    QJSTemporalCalendarDate metadata;
    QJSTemporalOverflow constrain = QJS_TEMPORAL_OVERFLOW_CONSTRAIN;
    QJSTemporalOverflow reject = QJS_TEMPORAL_OVERFLOW_REJECT;

    fields.present = QJS_TEMPORAL_FIELD_YEAR | QJS_TEMPORAL_FIELD_MONTH_CODE |
                     QJS_TEMPORAL_FIELD_DAY;
    fields.year = 5784;
    fields.day = 31;
    strcpy(fields.month_code, "M05L");
    assert(!qjs_temporal_calendar_date_from_fields(QJS_CAL_HEBREW, &fields, constrain, &result));
    assert(!qjs_temporal_calendar_fields(QJS_CAL_HEBREW, result, &metadata));
    assert(metadata.year == 5784 && metadata.month == 6 && metadata.day == 30);
    assert(metadata.has_era && !strcmp(metadata.era, "am"));
    assert(!metadata.has_week && metadata.days_in_week == 7);
    result = saved;
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_HEBREW, &fields, reject, &result));
    assert(!memcmp(&result, &saved, sizeof(result)));
    fields.day = 1.5;
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_HEBREW, &fields, constrain, &result));
    fields.day = 1;
    fields.year = 5784.5;
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_HEBREW, &fields, constrain, &result));
    fields.year = NAN;
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_HEBREW, &fields, constrain, &result));
    fields.year = 5784;
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_HEBREW, &fields, (QJSTemporalOverflow)99, &result));
    assert(!memcmp(&result, &saved, sizeof(result)));
    fields.year = 500000;
    strcpy(fields.month_code, "M01");
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_GREGORY, &fields, constrain, &result));
    assert(!memcmp(&result, &saved, sizeof(result)));
    fields.present = QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR |
                     QJS_TEMPORAL_FIELD_MONTH | QJS_TEMPORAL_FIELD_DAY;
    strcpy(fields.era, "heisei");
    fields.era_year = 32;
    fields.month = 1;
    fields.day = 1;
    assert(!qjs_temporal_calendar_date_from_fields(QJS_CAL_JAPANESE, &fields, reject, &result));
    assert(result.year == 2020 && result.month == 1 && result.day == 1);
    assert(!qjs_temporal_calendar_fields(QJS_CAL_JAPANESE, result, &metadata));
    assert(!strcmp(metadata.era, "reiwa") && metadata.era_year == 2);
    fields.present |= QJS_TEMPORAL_FIELD_YEAR;
    fields.year = 2019;
    result = saved;
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_JAPANESE, &fields, reject, &result));
    assert(!memcmp(&result, &saved, sizeof(result)));
    assert(qjs_temporal_calendar_extra_fields(QJS_CAL_HEBREW, QJS_TEMPORAL_FIELD_YEAR) ==
           (QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR));
    assert(!qjs_temporal_calendar_extra_fields(QJS_CAL_CHINESE, QJS_TEMPORAL_FIELD_YEAR));
    assert(qjs_temporal_calendar_fields_to_ignore(QJS_CAL_JAPANESE, QJS_TEMPORAL_FIELD_DAY) ==
           (QJS_TEMPORAL_FIELD_DAY | QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR));
    assert(!qjs_temporal_calendar_fields(QJS_CAL_CHINESE, saved, &metadata));
    assert(!metadata.has_era && !metadata.era[0] && !metadata.has_week);

    memset(&fields, 0, sizeof(fields));
    fields.present = QJS_TEMPORAL_FIELD_YEAR | QJS_TEMPORAL_FIELD_MONTH_CODE |
                     QJS_TEMPORAL_FIELD_DAY;
    fields.year = 5783;
    fields.day = 100000000000000000000.0;
    strcpy(fields.month_code, "M05L");
    assert(!qjs_temporal_calendar_year_month_from_fields(QJS_CAL_HEBREW, &fields,
                                                        constrain, &result));
    assert(!qjs_temporal_calendar_fields(QJS_CAL_HEBREW, result, &metadata));
    assert(metadata.day == 1 && metadata.month == 6 && !strcmp(metadata.month_code, "M06"));
    result = saved;
    assert(qjs_temporal_calendar_year_month_from_fields(QJS_CAL_HEBREW, &fields, reject, &result));
    assert(!memcmp(&result, &saved, sizeof(result)));
    fields.present |= QJS_TEMPORAL_FIELD_MONTH;
    fields.month = 5;
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_HEBREW, &fields, constrain, &result));
    assert(!memcmp(&result, &saved, sizeof(result)));
    fields.present &= ~QJS_TEMPORAL_FIELD_MONTH_CODE;
    fields.month = 1e300;
    assert(!qjs_temporal_calendar_date_from_fields(QJS_CAL_HEBREW, &fields, constrain, &result));
    assert(!qjs_temporal_calendar_fields(QJS_CAL_HEBREW, result, &metadata));
    assert(metadata.month == 12 && metadata.day == metadata.days_in_month);
    fields.present = QJS_TEMPORAL_FIELD_YEAR | QJS_TEMPORAL_FIELD_MONTH |
                     QJS_TEMPORAL_FIELD_DAY | QJS_TEMPORAL_FIELD_ERA;
    fields.year = 0;
    fields.month = fields.day = 1;
    strcpy(fields.era, "ad");
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_GREGORY, &fields, constrain, &result) ==
           QJS_TEMPORAL_ERROR_MISSING);
    fields.present |= QJS_TEMPORAL_FIELD_ERA_YEAR;
    fields.era_year = 0;
    assert(!qjs_temporal_calendar_date_from_fields(QJS_CAL_GREGORY, &fields, constrain, &result));
    assert(result.year == 0);
    fields.present &= ~QJS_TEMPORAL_FIELD_ERA;
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_GREGORY, &fields, constrain, &result) ==
           QJS_TEMPORAL_ERROR_MISSING);
    fields.present &= ~(QJS_TEMPORAL_FIELD_ERA_YEAR | QJS_TEMPORAL_FIELD_MONTH);
    fields.present |= QJS_TEMPORAL_FIELD_MONTH_CODE;
    strcpy(fields.month_code, "M00");
    result = saved;
    assert(qjs_temporal_calendar_date_from_fields(QJS_CAL_GREGORY, &fields, constrain, &result));
    assert(!memcmp(&result, &saved, sizeof(result)));
    return 0;
}
