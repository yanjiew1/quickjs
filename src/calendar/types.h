/* Calendar identities shared by independent calendar and Temporal libraries.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_CALENDAR_TYPES_H
#define QUICKJS_CALENDAR_TYPES_H

typedef enum QJSCalendarId {
    QJS_CAL_ISO8601,
    QJS_CAL_BUDDHIST,
    QJS_CAL_CHINESE,
    QJS_CAL_COPTIC,
    QJS_CAL_DANGI,
    QJS_CAL_ETHIOAA,
    QJS_CAL_ETHIOPIC,
    QJS_CAL_GREGORY,
    QJS_CAL_HEBREW,
    QJS_CAL_INDIAN,
    QJS_CAL_ISLAMIC_CIVIL,
    QJS_CAL_ISLAMIC_TBLA,
    QJS_CAL_ISLAMIC_UMALQURA,
    QJS_CAL_JAPANESE,
    QJS_CAL_PERSIAN,
    QJS_CAL_ROC,
    QJS_CAL_COUNT,
} QJSCalendarId;

#endif /* QUICKJS_CALENDAR_TYPES_H */
