/* Whole date/clock candidates; source selection and renderer admission stay separate.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_DATE_CLOCK_PROGRAMS_H
#define QJS_INTL_DATETIME_DATE_CLOCK_PROGRAMS_H
#include "ordinary-programs.h"

typedef struct QJSIntlDateClockProgram {
    QJSIntlDateTimeComponents components;
    QJSIntlDatePattern normal, twelve;
    QJSIntlDateCombinationRecord connector;
    QJSIntlDateStyleWidth connector_width;
    size_t date_index, clock_index;
} QJSIntlDateClockProgram;

typedef struct QJSIntlDateClockPrograms {
    QJSIntlAllocator allocator;
    QJSIntlDateClockProgram *records;
    size_t count;
} QJSIntlDateClockPrograms;

/* Combine unchanged whole no-hour date patterns with representative ordinary
   clock pairs: HM, HMS and HMS with one/two/three fractional digits, retaining
   each actually admitted timezone-name style independently. Missing clock/style
   pairs are not synthesized, and only present pairs determine allocation size.
   A date must contain year, month or day; weekday-only append layouts are not
   handled here. A NULL date is absent; optional admitted flags belong to the
   ordinary owner.
   Preserve real date/clock widths, raw year kinds and literals. Select the exact
   ordinary STANDARD connector from the date fields (wide month with weekday:
   FULL; wide month: LONG; abbreviated month: MEDIUM; otherwise SHORT).
   Missing/blocked/empty connectors contribute no candidate, never another width.

   Allocate no result when there are no combinations. Own both output programs,
   independent of input pattern/array lifetimes. Connector provenance still
   borrows its immutable pack; retained input indexes are diagnostic only.
   Names, chronology, matching, range construction and capability are not proved
   here. Require malloc/free and fresh disjoint output; reset/unwind errors.
   Clear with the retained allocator, including after failure or repeated clear. */
QJSIntlStatus qjs_intl_date_clock_programs_open(const QJSIntlDatePattern *const *dates,
                                               size_t date_count,
                                               const QJSIntlDateOrdinaryPrograms *ordinary,
                                               const unsigned char *admitted,
                                               const QJSIntlDateCombinationData *connectors,
                                               uint32_t locale, uint32_t calendar,
                                               const QJSIntlAllocator *allocator,
                                               QJSIntlDateClockPrograms *out);
void qjs_intl_date_clock_programs_clear(QJSIntlDateClockPrograms *programs);
#endif
