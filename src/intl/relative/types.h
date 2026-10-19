/* Relative-time semantic selectors shared by data and formatting layers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_RELATIVE_TYPES_H
#define QJS_INTL_RELATIVE_TYPES_H
#include "../common/types.h"

typedef enum QJSIntlRelativeUnit {
    QJS_INTL_RELATIVE_SECOND,
    QJS_INTL_RELATIVE_MINUTE,
    QJS_INTL_RELATIVE_HOUR,
    QJS_INTL_RELATIVE_DAY,
    QJS_INTL_RELATIVE_WEEK,
    QJS_INTL_RELATIVE_MONTH,
    QJS_INTL_RELATIVE_QUARTER,
    QJS_INTL_RELATIVE_YEAR,
    QJS_INTL_RELATIVE_UNIT_COUNT
} QJSIntlRelativeUnit;

typedef enum QJSIntlRelativeWidth {
    QJS_INTL_RELATIVE_LONG,
    QJS_INTL_RELATIVE_SHORT,
    QJS_INTL_RELATIVE_NARROW,
    QJS_INTL_RELATIVE_WIDTH_COUNT
} QJSIntlRelativeWidth;

typedef enum QJSIntlRelativeTense {
    QJS_INTL_RELATIVE_PAST,
    QJS_INTL_RELATIVE_FUTURE
} QJSIntlRelativeTense;

#endif
