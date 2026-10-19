/* Localized numeric bodies, before sign/style/notation affix composition.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_BODY_H
#define QJS_INTL_NUMBER_BODY_H
#include "number.h"

typedef struct QJSIntlNumberBody {
    uint32_t digits[10]; /* Distinct Unicode scalars for ASCII digits 0..9. */
    uint32_t primary_grouping, secondary_grouping; /* Both zero, or 1..9. */
    uint32_t minimum_grouping_digits;              /* 1..9, used by AUTO. */
    QJSIntlNumberGrouping grouping;
    QJSIntlUTF16 decimal, group, nan, infinity;
    size_t maximum_output_length; /* Nonzero UTF-16 bound. */
} QJSIntlNumberBody;

/* Numeric part of PartitionNotationSubPattern. The service supplies resolved
   grouping preferences, localized symbols and an already rounded unsigned
   plain decimal: digits, optionally '.' followed by digits. Leading integer
   and trailing fraction zeros are preserved. No rounding, sign, percent scale,
   currency substitution or compact/scientific affix selection occurs here.
   Nonfinite kinds require empty decimal text and emit the supplied symbol.
   NEGATIVE_ZERO is a finite body; its sign belongs to the outer pattern.

   Borrow all inputs unchanged during the call. Fresh output must be disjoint
   from inputs/storage; errors reset it. malloc/free are required; realloc is
   unused. Text and parts are separately owned, with no trailing NUL promised.
   Clear using the creating allocator; results do not borrow layout storage. */
QJSIntlStatus qjs_intl_number_body_format(const QJSIntlAllocator *, const QJSIntlNumberBody *,
                                          QJSIntlMathematicalKind, QJSIntlBytes decimal,
                                          QJSIntlFormatted *);
void qjs_intl_number_body_clear(const QJSIntlAllocator *, QJSIntlFormatted *);
#endif
