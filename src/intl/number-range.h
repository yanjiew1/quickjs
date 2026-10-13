/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_RANGE_H
#define QJS_INTL_NUMBER_RANGE_H
#include "provider.h"
typedef struct QJSIntlNumberRangeSlices {
    QJSIntlBytes prefix, separator, suffix;
    uint8_t first_endpoint;       /* raw range pattern order,0=start,1=end */
} QJSIntlNumberRangeSlices;
/* Raw LDML approximately{0} or range{0},{1}. No unknown/repeated tokens.
 * NumberFormat validates scalar UTF8 separately. Slices borrow input. */
int qjs_intl_number_range_slices(QJSIntlBytes, unsigned int range,
                                QJSIntlNumberRangeSlices *);
int qjs_intl_number_range_equal(const QJSIntlFormatted *, const QJSIntlFormatted *);
#endif
