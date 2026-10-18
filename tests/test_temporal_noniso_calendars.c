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

    memset(&fields, 0, sizeof(fields));
    fields.present = QJS_TEMPORAL_FIELD_MONTH_CODE | QJS_TEMPORAL_FIELD_DAY;
    fields.day = 30;
    strcpy(fields.month_code, "M01");
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_CHINESE, &fields, reject, &result));
    assert(result.year == 1970);
    strcpy(fields.month_code, "M03");
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_DANGI, &fields, reject, &result));
    assert(result.year == 1968);
    strcpy(fields.month_code, "M05L");
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_HEBREW, &fields, reject, &result));
    assert(result.year == 1970);
    strcpy(fields.month_code, "M11L");
    fields.day = 10;
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_CHINESE, &fields, reject, &result));
    assert(result.year == 2033);
    fields.day = 11;
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_CHINESE, &fields, reject, &result));
    assert(result.year == 2034);
    fields.day = 30;
    result = saved;
    assert(qjs_temporal_calendar_month_day_from_fields(QJS_CAL_CHINESE, &fields, reject, &result));
    assert(!memcmp(&result, &saved, sizeof(result)));
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_CHINESE, &fields, constrain, &result));
    assert(!qjs_temporal_calendar_fields(QJS_CAL_CHINESE, result, &metadata));
    assert(!strcmp(metadata.month_code, "M11") && metadata.day == 30);
    strcpy(fields.month_code, "M13");
    fields.day = 6;
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_COPTIC, &fields, reject, &result));
    assert(result.year == 1971);
    fields.day = 7;
    assert(qjs_temporal_calendar_month_day_from_fields(QJS_CAL_COPTIC, &fields, reject, &result));
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_COPTIC, &fields, constrain, &result));
    assert(!qjs_temporal_calendar_fields(QJS_CAL_COPTIC, result, &metadata));
    assert(metadata.day == 6);
    fields.present |= QJS_TEMPORAL_FIELD_YEAR;
    fields.year = 1e300;
    result = saved;
    assert(qjs_temporal_calendar_month_day_from_fields(QJS_CAL_COPTIC, &fields, constrain, &result));
    assert(!memcmp(&result, &saved, sizeof(result)));
    fields.present &= ~QJS_TEMPORAL_FIELD_YEAR;
    fields.present |= QJS_TEMPORAL_FIELD_MONTH;
    fields.month = 1;
    assert(qjs_temporal_calendar_month_day_from_fields(QJS_CAL_HEBREW, &fields,
            constrain, &result) == QJS_TEMPORAL_ERROR_MISSING);
    assert(!memcmp(&result, &saved, sizeof(result)));
    fields.present = QJS_TEMPORAL_FIELD_YEAR | QJS_TEMPORAL_FIELD_MONTH_CODE |
                     QJS_TEMPORAL_FIELD_DAY;
    fields.year = 275760;
    fields.day = 31;
    strcpy(fields.month_code, "M12");
    assert(qjs_temporal_calendar_month_day_from_fields(QJS_CAL_GREGORY, &fields,
            constrain, &result) == QJS_TEMPORAL_ERROR_RANGE);
    assert(!memcmp(&result, &saved, sizeof(result)));
    fields.year = -271821;
    fields.day = 18;
    strcpy(fields.month_code, "M04");
    assert(qjs_temporal_calendar_month_day_from_fields(QJS_CAL_GREGORY, &fields,
            constrain, &result) == QJS_TEMPORAL_ERROR_RANGE);
    assert(!memcmp(&result, &saved, sizeof(result)));
    fields.day = 19;
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_GREGORY, &fields,
                                                      constrain, &result));
    assert(result.year == 1972 && result.month == 4 && result.day == 19);
    fields.year = 275760;
    fields.day = 13;
    strcpy(fields.month_code, "M09");
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_GREGORY, &fields,
                                                      constrain, &result));
    assert(result.year == 1972 && result.month == 9 && result.day == 13);
    fields.day = 14;
    assert(qjs_temporal_calendar_month_day_from_fields(QJS_CAL_GREGORY, &fields,
            constrain, &result) == QJS_TEMPORAL_ERROR_RANGE);
    fields.present &= ~QJS_TEMPORAL_FIELD_YEAR;
    fields.day = 26;
    strcpy(fields.month_code, "M04");
    assert(!qjs_temporal_calendar_month_day_from_fields(QJS_CAL_HEBREW, &fields,
                                                      constrain, &result));
    assert(result.year == 1972 && result.month == 12 && result.day == 31);
    for (int id = QJS_CAL_BUDDHIST; id < QJS_CAL_COUNT; id++) {
        int count = id == QJS_CAL_COPTIC || id == QJS_CAL_ETHIOAA ||
                    id == QJS_CAL_ETHIOPIC ? 13 : 12;

        memset(&fields, 0, sizeof(fields));
        fields.present = QJS_TEMPORAL_FIELD_MONTH_CODE | QJS_TEMPORAL_FIELD_DAY;
        for (int month = 1; month <= count; month++) {
            fields.month_code[0] = 'M';
            fields.month_code[1] = '0' + month / 10;
            fields.month_code[2] = '0' + month % 10;
            fields.day = 1e300;
            assert(!qjs_temporal_calendar_month_day_from_fields((QJSCalendarId)id,
                    &fields, constrain, &result));
            assert(!qjs_temporal_calendar_fields((QJSCalendarId)id, result, &metadata));
            assert(!strcmp(metadata.month_code, fields.month_code));
            assert(metadata.day >= 6 && metadata.day <= 31);
        }
    }
    return 0;
}
