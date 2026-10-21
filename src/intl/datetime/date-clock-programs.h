/* Whole date/clock candidates; source selection and renderer admission stay separate.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_DATE_CLOCK_PROGRAMS_H
#define QJS_INTL_DATETIME_DATE_CLOCK_PROGRAMS_H
#include "ordinary-programs.h"
#include "../common/pair-pattern.h"

typedef struct QJSIntlDateClockProgram {
    QJSIntlDateTimeComponents components;
    unsigned char source_hour_width[2]; /* Original normal/twelve clock widths. */
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
   borrows its immutable pack. Retained indexes identify the original inputs
   for callers that keep those inputs unchanged and alive; output patterns
   themselves remain independent of those inputs.
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
/* Apply a date-field append item before the date/time connector. Inputs are
   unchanged validated ordinary patterns, an LDML connector and parsed literal
   append layout. Own one final pattern, reset errors and release all temporary
   patterns on every path. This performs composition, not field admission or
   candidate selection. Follow pattern.h's allocator/disjoint-output contract. */
QJSIntlStatus qjs_intl_date_clock_append_date_field(const QJSIntlAllocator *,
                                                    const QJSIntlDatePattern *date,
                                                    const QJSIntlDatePattern *clock,
                                                    QJSIntlBytes connector,
                                                    const QJSIntlPairPattern *append,
                                                    const QJSIntlDatePattern *field,
                                                    QJSIntlDatePattern *out);
#endif
