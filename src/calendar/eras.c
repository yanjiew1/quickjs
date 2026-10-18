/* Canonical era rules from the ECMA-402 era/monthCode proposal.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "eras.h"
#include <string.h>

typedef struct JapaneseEra {
    const char *name;
    int32_t year_offset;
    int32_t first_day;
} JapaneseEra;

/* Unix epoch days: Meiji 6 (1873-01-01), 1912-07-30, 1926-12-25,
   1989-01-08 and 2019-05-01. Earlier dates use Gregorian eras. */
static const JapaneseEra japanese_eras[] = {
    {"meiji", 1867, -35428},
    {"taisho", 1911, -20974},
    {"showa", 1925, -15713},
    {"heisei", 1988, 6947},
    {"reiwa", 2018, 18017},
};

void qjs_cal_set_era(QJSCalendarId calendar, int64_t epoch_day,
                     QJSCalendarDate *date)
{
    const char *name = NULL;
    int32_t year = date->year, era_year = year;
    int i;

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
        for (i = 4; i >= 0; i--) {
            if (epoch_day >= japanese_eras[i].first_day) {
                name = japanese_eras[i].name;
                era_year -= japanese_eras[i].year_offset;
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
    int i;

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
        for (i = 0; i < 5; i++) {
            if (!strcmp(era, japanese_eras[i].name)) {
                year += japanese_eras[i].year_offset;
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
