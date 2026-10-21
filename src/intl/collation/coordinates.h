/* Logical collation coordinates shared by root readers and tailoring.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_COORDINATES_H
#define QJS_INTL_COLLATION_COORDINATES_H
#include "../common/types.h"

/* The root owner defines ordinary/ignorable bands and common aliases. */
typedef struct QJSIntlCollationRootLimits {
    uint32_t primary_max, secondary_max, tertiary_max;
    uint32_t secondary_common, tertiary_common;
    uint32_t secondary_boundary, tertiary_boundary;
} QJSIntlCollationRootLimits;

enum {
    QJS_INTL_COLLATION_LOWER,
    QJS_INTL_COLLATION_MIXED,
    QJS_INTL_COLLATION_UPPER
};

typedef struct QJSIntlCollationCoordinate {
    uint32_t weights[4]; /* Root or stable tailored IDs, not frozen ranks. */
    uint32_t root_primary; /* Original root anchor for grouping/variable policy. */
    unsigned char case_value, variable, implicit;
} QJSIntlCollationCoordinate;
#endif
