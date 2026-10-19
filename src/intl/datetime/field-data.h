/* Shared native date-field labels; no formatting or availability policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_FIELD_DATA_H
#define QJS_INTL_DATETIME_FIELD_DATA_H
#include "../native/data.h"

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

typedef enum QJSIntlDateFieldWidth {
    QJS_INTL_DATE_FIELD_LONG,
    QJS_INTL_DATE_FIELD_SHORT,
    QJS_INTL_DATE_FIELD_NARROW,
    QJS_INTL_DATE_FIELD_WIDTH_COUNT
} QJSIntlDateFieldWidth;

typedef enum QJSIntlDateFieldState {
    QJS_INTL_DATE_FIELD_MISSING,
    QJS_INTL_DATE_FIELD_VALUE,
    QJS_INTL_DATE_FIELD_BLOCKED
} QJSIntlDateFieldState;

typedef struct QJSIntlDateFieldLabel {
    QJSIntlDateFieldState state;
    QJSIntlBytes text, origin_locale, origin_alternate;
    QJSIntlDateField origin_field; /* Origin fields meaningful unless MISSING. */
    QJSIntlDateFieldWidth origin_width;
} QJSIntlDateFieldLabel;

typedef struct QJSIntlDateFieldData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *alternates, *locale_rows, *rows, *records;
    uint32_t alternate_count, row_count, row_width, record_count;
} QJSIntlDateFieldData;

/* Borrow an unchanged validated shared owner. Require malloc/free and release
   validation scratch on every path. Validate canonical sharing, references,
   states and source identity. Follow pack.h's disjoint-output contract; reset
   non-NULL output on failure. No formatter capability is inferred. */
QJSIntlStatus qjs_intl_date_field_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                            QJSIntlDateFieldData *);
/* Sorted alternate inventory; index zero is the ordinary empty selector.
   Borrow returned slices from the owner. These queries allocate nothing. */
QJSIntlStatus qjs_intl_date_field_alternate_at(const QJSIntlDateFieldData *, uint32_t index,
                                               QJSIntlBytes *);
/* Exact locale/field/width/alternate indexes. No extra width/style fallback.
   MISSING returns OK with a zero label, distinct from BLOCKED and empty VALUE. */
QJSIntlStatus qjs_intl_date_field_label_at(const QJSIntlDateFieldData *, uint32_t locale,
                                           QJSIntlDateField, QJSIntlDateFieldWidth,
                                           uint32_t alternate, QJSIntlDateFieldLabel *);
#endif
