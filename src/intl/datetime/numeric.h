/* Numeric date fields through the selected NumberFormat service.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_NUMERIC_H
#define QJS_INTL_DATETIME_NUMERIC_H
#include "pattern-info.h"
#include "../number/number.h"

typedef enum QJSIntlDateHourCycle {
    QJS_INTL_DATE_H11,
    QJS_INTL_DATE_H12,
    QJS_INTL_DATE_H23,
    QJS_INTL_DATE_H24
} QJSIntlDateHourCycle;

typedef struct QJSIntlDateNumericFields {
    int32_t year, related_year;
    unsigned int month, day, hour, minute, second, millisecond;
} QJSIntlDateNumericFields;

typedef struct QJSIntlDateNumberFormats {
    const QJSIntlNumberFormatter *numeric, *two_digit;
    const QJSIntlNumberFormatter *fraction[3];
} QJSIntlDateNumberFormats;

/* Render an unchanged compiled ordinary numeric/literal program from specified
   local fields, not an epoch timestamp. Named fields return UNSUPPORTED; this
   operation is the numeric kernel, not complete DateTimeFormat admission.
   Local fields are fully specified (month 1..13, day 1..31, clock 0..23/59/59,
   millisecond 0..999); chronology owns calendar validity and numeric month/year
   interpretation. Apply the resolved cycle independently of source h/H/K/k.
   y<=0 becomes 1-y; relatedYear stays signed. Truncate fractional milliseconds,
   and truncate two-digit formatted values by Unicode code points, not UTF-16.

   Supply selected-locale/system decimal NumberFormat instances without grouping
   and with no fraction digits: minimum integer 1 for numeric, 2 for two_digit,
   1/2/3 for fraction[0/1/2]. Only instances used by the program are required.
   These immutable instances were opened with this allocator and outlive the
   call. Collapse each complete numeric result into one date-field part, retaining
   localized signs/digits/literals without NumberFormat subpart leakage.
   Inputs remain unchanged; fresh disjoint output is zeroed on failure. Own text
   and parts independently of all inputs, clear with formatted_clear and the same
   allocator. Require malloc/free and a nonzero UTF-16 output bound. */
QJSIntlStatus qjs_intl_date_numeric_format(const QJSIntlAllocator *allocator,
                                          const QJSIntlDatePattern *pattern,
                                          const QJSIntlDateNumericFields *local,
                                          QJSIntlDateHourCycle cycle,
                                          const QJSIntlDateNumberFormats *numbers,
                                          size_t maximum_output_length,
                                          QJSIntlFormatted *out);
#endif
