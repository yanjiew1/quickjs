/* Canonical era rules from the ECMA-402 era/monthCode proposal.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "eras.h"
#include "data.h"
#include <string.h>

int qjs_cal_era_at(QJSCalendarId calendar, size_t index, const char **out)
{
    const char *first = NULL, *second = NULL;
    switch (calendar) {
    case QJS_CAL_BUDDHIST: first = "be"; break;
    case QJS_CAL_COPTIC: case QJS_CAL_HEBREW: first = "am"; break;
    case QJS_CAL_ETHIOAA: first = "aa"; break;
    case QJS_CAL_ETHIOPIC: first = "am"; second = "aa"; break;
    case QJS_CAL_GREGORY: first = "ce"; second = "bce"; break;
    case QJS_CAL_INDIAN: first = "shaka"; break;
    case QJS_CAL_ISLAMIC_CIVIL: case QJS_CAL_ISLAMIC_TBLA:
    case QJS_CAL_ISLAMIC_UMALQURA: first = "ah"; second = "bh"; break;
    case QJS_CAL_PERSIAN: first = "ap"; break;
    case QJS_CAL_ROC: first = "roc"; second = "broc"; break;
    case QJS_CAL_JAPANESE: {
        size_t count = qjs_calendar_data_japanese_era_count(&qjs_native_calendar_data);
        if (index < count) {
            QJSCalendarEra era;
            int error = qjs_calendar_data_japanese_era(&qjs_native_calendar_data, index, &era);
            if (error)
                return error;
            *out = era.name;
            return QJS_CAL_OK;
        }
        index -= count;
        first = "ce";
        second = "bce";
        break;
    }
    default: break;
    }
    if (!first || index > 1 || (index && !second))
        return QJS_CAL_UNSUPPORTED;
    *out = index ? second : first;
    return QJS_CAL_OK;
}

void qjs_cal_set_era(QJSCalendarId calendar, int64_t epoch_day,
                     QJSCalendarDate *date)
{
    const char *name = NULL;
    int32_t year = date->year, era_year = year;
    size_t i;

    switch (calendar) {
    case QJS_CAL_BUDDHIST: name = "be"; break;
    case QJS_CAL_COPTIC: case QJS_CAL_HEBREW: name = "am"; break;
    case QJS_CAL_ETHIOAA: name = "aa"; break;
    case QJS_CAL_ETHIOPIC:
        name = year > 0 ? "am" : "aa";
        if (year <= 0)
            era_year += 5500;
        break;
    case QJS_CAL_JAPANESE:
        i = qjs_calendar_data_japanese_era_count(&qjs_native_calendar_data);
        while (i) {
            QJSCalendarEra era;

            if (!qjs_calendar_data_japanese_era(&qjs_native_calendar_data, --i, &era) &&
                epoch_day >= era.first_day) {
                name = era.name;
                era_year -= era.year_offset;
                break;
            }
        }
        if (name)
            break;
        /* Before 1873, Japanese dates use Gregorian epochs. */
        /* fall through */
    case QJS_CAL_GREGORY:
        name = year > 0 ? "ce" : "bce";
        if (year <= 0)
            era_year = 1 - year;
        break;
    case QJS_CAL_INDIAN: name = "shaka"; break;
    case QJS_CAL_ISLAMIC_CIVIL: case QJS_CAL_ISLAMIC_TBLA:
    case QJS_CAL_ISLAMIC_UMALQURA:
        name = year > 0 ? "ah" : "bh";
        if (year <= 0)
            era_year = 1 - year;
        break;
    case QJS_CAL_PERSIAN: name = "ap"; break;
    case QJS_CAL_ROC:
        name = year > 0 ? "roc" : "broc";
        if (year <= 0)
            era_year = 1 - year;
        break;
    default:
        break;
    }
    date->has_era = name != NULL;
    date->era_year = name ? era_year : 0;
    date->era[0] = 0;
    if (name)
        strcpy(date->era, name);
}

int qjs_cal_year_from_era(QJSCalendarId calendar, const char *era,
                         int32_t era_year, int32_t *result)
{
    int64_t year = era_year;
    size_t i, count;

    switch (calendar) {
    case QJS_CAL_BUDDHIST:
        if (strcmp(era, "be"))
            return QJS_CAL_RANGE;
        break;
    case QJS_CAL_COPTIC: case QJS_CAL_HEBREW:
        if (strcmp(era, "am"))
            return QJS_CAL_RANGE;
        break;
    case QJS_CAL_ETHIOAA:
        if (strcmp(era, "aa"))
            return QJS_CAL_RANGE;
        break;
    case QJS_CAL_ETHIOPIC:
        if (!strcmp(era, "aa"))
            year -= 5500;
        else if (strcmp(era, "am"))
            return QJS_CAL_RANGE;
        break;
    case QJS_CAL_INDIAN:
        if (strcmp(era, "shaka"))
            return QJS_CAL_RANGE;
        break;
    case QJS_CAL_ISLAMIC_CIVIL: case QJS_CAL_ISLAMIC_TBLA:
    case QJS_CAL_ISLAMIC_UMALQURA:
        if (!strcmp(era, "bh"))
            year = 1 - year;
        else if (strcmp(era, "ah"))
            return QJS_CAL_RANGE;
        break;
    case QJS_CAL_PERSIAN:
        if (strcmp(era, "ap"))
            return QJS_CAL_RANGE;
        break;
    case QJS_CAL_ROC:
        if (!strcmp(era, "broc"))
            year = 1 - year;
        else if (strcmp(era, "roc"))
            return QJS_CAL_RANGE;
        break;
    case QJS_CAL_JAPANESE:
        count = qjs_calendar_data_japanese_era_count(&qjs_native_calendar_data);
        for (i = 0; i < count; i++) {
            QJSCalendarEra record;

            if (!qjs_calendar_data_japanese_era(&qjs_native_calendar_data, i, &record) &&
                !strcmp(era, record.name)) {
                year += record.year_offset;
                goto resolved;
            }
        }
        /* Gregorian era inputs also resolve leniently beyond 1872. */
        /* fall through */
    case QJS_CAL_GREGORY:
        if (!strcmp(era, "bce") || !strcmp(era, "bc"))
            year = 1 - year;
        else if (strcmp(era, "ce") && strcmp(era, "ad"))
            return QJS_CAL_RANGE;
        break;
    default:
        return QJS_CAL_RANGE;
    }
resolved:
    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    *result = year;
    return QJS_CAL_OK;
}
