/* Calendar and local-clock values shared by arithmetic and numeric rendering.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_FIELDS_H
#define QJS_INTL_DATETIME_FIELDS_H
#include "../common/types.h"

typedef struct QJSIntlDateNumericFields {
    int32_t year, related_year;
    unsigned int month, day, hour, minute, second, millisecond;
} QJSIntlDateNumericFields;

#endif
