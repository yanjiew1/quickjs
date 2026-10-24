/* NumberFormat handles shared by native date and zone text.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_NUMBER_FORMATS_H
#define QJS_INTL_DATETIME_NUMBER_FORMATS_H
#include "../number/number.h"

typedef struct QJSIntlDateNumberFormats {
    const QJSIntlNumberFormatter *numeric, *two_digit;
    const QJSIntlNumberFormatter *fraction[3];
} QJSIntlDateNumberFormats;

/* Selected-locale decimal NumberFormat handles: no grouping or fraction
   digits, minimum integer widths 1/2 and 1/2/3 for fractional milliseconds.
   Only handles used by the program are required. They remain immutable and
   outlive rendering, using the same allocator as the date text result. */
#endif
