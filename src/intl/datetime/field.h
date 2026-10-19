/* ECMA-402 date-field identities, independent of backend data.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_FIELD_H
#define QJS_INTL_DATETIME_FIELD_H
#include "../common/types.h"

typedef enum QJSIntlDateField {
    QJS_INTL_DATE_ERA,
    QJS_INTL_DATE_YEAR,
    QJS_INTL_DATE_QUARTER,
    QJS_INTL_DATE_MONTH,
    QJS_INTL_DATE_WEEK_OF_YEAR,
    QJS_INTL_DATE_WEEKDAY,
    QJS_INTL_DATE_DAY,
    QJS_INTL_DATE_DAY_PERIOD,
    QJS_INTL_DATE_HOUR,
    QJS_INTL_DATE_MINUTE,
    QJS_INTL_DATE_SECOND,
    QJS_INTL_DATE_TIME_ZONE_NAME,
    QJS_INTL_DATE_FIELD_COUNT
} QJSIntlDateField;

/* Exact IsValidDateTimeFieldCode spelling, without case folding. Allocate
   nothing; unknown/malformed input returns INVALID_ARGUMENT. Reset output. */
QJSIntlStatus qjs_intl_date_field_from_code(QJSIntlBytes, QJSIntlDateField *);
#endif
