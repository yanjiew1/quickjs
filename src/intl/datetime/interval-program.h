/* Owned CLDR interval programs with endpoint provenance, not source matching.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_INTERVAL_PROGRAM_H
#define QJS_INTL_DATETIME_INTERVAL_PROGRAM_H
#include "pattern-info.h"
#include "../common/format.h"

typedef struct QJSIntlDateIntervalProgram {
    QJSIntlAllocator allocator;
    QJSIntlDatePattern pattern;
    QJSIntlDateTimeComponents components; /* Union of both actual halves. */
    uint8_t *sources; /* One RangeSource per token, including literals. */
    size_t split; /* First repeated field starts the second endpoint. */
} QJSIntlDateIntervalProgram;

/* Parse a genuine greatestDifference pattern. Preserve decoded literals, year
   kinds and earliestFirst/latestFirst ordering. Repeated fields use their
   endpoint; fields occurring in only one half are shared. Literals between
   different sources are shared; other literals follow their adjacent fields.
   This literal attribution is the native policy, not a CLDR provenance rule.

   Apply textual-width and hour-family/width adaptation from the ordinary
   record, preserving numeric month context and automatic AM/PM layout. A
   twelve-hour interval must contain a real period layout; a 24-hour interval
   cannot retain automatic AM/PM. Extra range fields are allowed: metadata
   describes the actual program, not an ordinary/range equality constraint.
   Source matching, name admission and missing-field policy are caller-owned.

   ordinary is an unchanged pattern_info result. Own both allocations
   independently of source text and ordinary metadata.
   Require malloc/free and fresh disjoint output; reset and unwind failures.
   UNSUPPORTED covers nonrenderable or inconsistent halves. Clear with the
   retained allocator, including after a failed open or repeated clear. */
QJSIntlStatus qjs_intl_date_interval_program_open(const QJSIntlAllocator *allocator,
                                                 QJSIntlBytes source,
                                                 const QJSIntlDatePatternInfo *ordinary,
                                                 QJSIntlDateHourCycle cycle,
                                                 QJSIntlDateIntervalProgram *out);
void qjs_intl_date_interval_program_clear(QJSIntlDateIntervalProgram *program);
#endif
