/* ECMA-402 DateTimeFormat option components, independent of source patterns.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_COMPONENTS_H
#define QJS_INTL_DATETIME_COMPONENTS_H

typedef enum QJSIntlDateTimeComponent {
    QJS_INTL_DATETIME_WEEKDAY, QJS_INTL_DATETIME_ERA,
    QJS_INTL_DATETIME_YEAR, QJS_INTL_DATETIME_MONTH, QJS_INTL_DATETIME_DAY,
    QJS_INTL_DATETIME_DAY_PERIOD, QJS_INTL_DATETIME_HOUR, QJS_INTL_DATETIME_MINUTE,
    QJS_INTL_DATETIME_SECOND, QJS_INTL_DATETIME_FRACTION, QJS_INTL_DATETIME_ZONE,
    QJS_INTL_DATETIME_COMPONENT_COUNT
} QJSIntlDateTimeComponent;

typedef enum QJSIntlDateTimeWidth {
    QJS_INTL_DATETIME_ABSENT, QJS_INTL_DATETIME_TWO_DIGIT, QJS_INTL_DATETIME_NUMERIC,
    QJS_INTL_DATETIME_NARROW, QJS_INTL_DATETIME_SHORT, QJS_INTL_DATETIME_LONG
} QJSIntlDateTimeWidth;

typedef enum QJSIntlDateTimeZoneStyle {
    QJS_INTL_DATETIME_ZONE_SHORT = 1, QJS_INTL_DATETIME_ZONE_LONG,
    QJS_INTL_DATETIME_ZONE_SHORT_OFFSET, QJS_INTL_DATETIME_ZONE_LONG_OFFSET,
    QJS_INTL_DATETIME_ZONE_SHORT_GENERIC, QJS_INTL_DATETIME_ZONE_LONG_GENERIC
} QJSIntlDateTimeZoneStyle;

typedef enum QJSIntlDateHourCycle {
    QJS_INTL_DATE_H11,
    QJS_INTL_DATE_H12,
    QJS_INTL_DATE_H23,
    QJS_INTL_DATE_H24
} QJSIntlDateHourCycle;

typedef struct QJSIntlDateTimeComponents {
    /* Zero means absent; fraction uses 1..3, zone uses ZoneStyle, other fields
       use Width. Field-specific legal values follow ECMA-402 Table 16. */
    int values[QJS_INTL_DATETIME_COMPONENT_COUNT];
} QJSIntlDateTimeComponents;

/* Presence only; field-specific value validation belongs to consumers. */
static inline unsigned int qjs_intl_datetime_component_mask(const QJSIntlDateTimeComponents *components)
{
    unsigned int result = 0, i;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        if (components->values[i])
            result |= 1U << i;
    return result;
}

#endif
