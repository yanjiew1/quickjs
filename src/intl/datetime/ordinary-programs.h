/* Native ordinary single-date formats, before renderer/range admission.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_ORDINARY_PROGRAMS_H
#define QJS_INTL_DATETIME_ORDINARY_PROGRAMS_H
#include "single-program.h"
#include "combination-data.h"

enum {
    QJS_INTL_DATE_ORDINARY_APPEND = 1,
    QJS_INTL_DATE_ORDINARY_PERIOD = 2,
    QJS_INTL_DATE_ORDINARY_FRACTION = 4,
    QJS_INTL_DATE_ORDINARY_COMBINE = 8,
    QJS_INTL_DATE_ORDINARY_FIELD = 16
};

enum {
    QJS_INTL_DATE_MONTH_SOURCE_NUMERIC = 1,
    QJS_INTL_DATE_MONTH_SOURCE_TEXT = 2
};

typedef struct QJSIntlDateOrdinaryProgram {
    QJSIntlDateSinglePrograms single;
    /* Original clock widths before adaptation: normal, then twelve-hour.
       Zero denotes no retained clock source. Derived programs inherit these. */
    unsigned char source_hour_width[2];
    /* Declared skeleton month kinds for the selected raw pattern and exact
       text aliases, separately for normal/twelve source families. */
    unsigned char source_month_kinds[2];
    /* Raw b/B skeleton run lengths, bit (1 << width), for the same symbol
       as the selected pattern. Zero means no evidence for width adaptation. */
    unsigned char source_period_widths[2];
    unsigned int construction;
    size_t first, second; /* Earlier result indexes, SIZE_MAX when not used. */
    QJSIntlDateAvailableEntry append;
    QJSIntlDateAvailableNode origin;
    QJSIntlDateCombinationRecord connector;
    QJSIntlBytes fraction_decimal; /* Borrowed source for FRACTION construction. */
} QJSIntlDateOrdinaryProgram;

typedef struct QJSIntlDateOrdinaryPrograms {
    QJSIntlAllocator allocator;
    QJSIntlDateOrdinaryProgram *records;
    size_t count;
} QJSIntlDateOrdinaryPrograms;

/* Copy complete useful ordinary source pairs in stable group order, then add
   the fourteen required ECMA component-presence subsets. Deduplicate by the
   full component vector. Build missing era-free dates from actual M/Md sources
   and Year/Day-Of-Week append templates, not by stripping G. Plain twelve-hour
   clocks may explicitly replace B by a when no whole AM/PM clock is available;
   explicit dayPeriod records remain unchanged. Fractional seconds use the
   selected numbering system's decimal separator; mixed formats use the exact
   ordinary SHORT standard connector. Include useful date/clock combinations
   without an added weekday, optional whole era-bearing date/clock pairs, and
   date-only zone styles through the genuine
   Date-Timezone append template. Retain source/construction provenance.
   Also offer single flexible-day-period fields at all three text widths;
   these are native field programs, not claimed available-format sources.

   Supply the unchanged pool/grouping pair and exact resolved APPEND inventory.
   Connector locale/calendar indexes belong to their own owner. Decimal is
   nonempty scalar UTF-8. Own all patterns independently of input programs;
   provenance still borrows unchanged packs and the supplied decimal bytes,
   which must outlive this owner.
   No chronology, names, ranges or public locale capability is admitted here.
   Require malloc/free and fresh disjoint output; reset and unwind on failure.
   Missing construction evidence is UNSUPPORTED, never a fabricated template. */
QJSIntlStatus qjs_intl_date_ordinary_programs_open(const QJSIntlDateSourceCandidates *pool,
                                                   const QJSIntlDateFormatSources *groups,
                                                   const QJSIntlDateAvailableSources *append,
                                                   const QJSIntlDateCombinationData *connectors,
                                                   uint32_t locale, uint32_t calendar,
                                                   QJSIntlBytes decimal,
                                                   const QJSIntlAllocator *allocator,
                                                   QJSIntlDateOrdinaryPrograms *out);
void qjs_intl_date_ordinary_programs_clear(QJSIntlDateOrdinaryPrograms *programs);
#endif
