/* Source-proved native AM/PM spacing policy, before range rendering.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_PERIOD_SPACING_H
#define QJS_INTL_DATETIME_PERIOD_SPACING_H
#include "pattern.h"

/* Return h/m/s adjacency bits only for an actually selected ascii alternate
   containing one automatic a with an isolated unquoted ASCII space before it.
   Unknown/quoted/mixed layouts grant no proof. Inputs are validated raw LDML. */
unsigned int qjs_intl_date_period_ascii_source(QJSIntlBytes source, QJSIntlBytes alternate);
/* Intersect raw source evidence with an unchanged actual ordinary program.
   Constructor adaptation/composition may have changed its adjacent layout. */
unsigned int qjs_intl_date_period_ascii_mask(const QJSIntlDatePattern *, unsigned int proof);
/* Apply proved suffix spacing to an owned interval_compile result from this
   same raw source, after field-width adaptation. Only isolated unquoted
   U+202F/U+2009 gaps before a after the matching clock field are changed.
   Preserve token count, field metadata and all other literals. Allocate nothing;
   the unchanged raw pack remains separate from the mutable owned token storage. */
void qjs_intl_date_period_ascii_interval(QJSIntlDatePattern *owned,
                                         QJSIntlBytes source, unsigned int proof);
#endif
