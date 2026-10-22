/* Native style programs; ranges and renderer admission remain separate.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_STYLE_PROGRAM_H
#define QJS_INTL_DATETIME_STYLE_PROGRAM_H
#include "style-data.h"
#include "combination-data.h"
#include "single-program.h"

typedef struct QJSIntlDateStyleSource {
    QJSIntlAllocator allocator;
    QJSIntlDatePattern pattern;
    QJSIntlDatePatternInfo info;
    QJSIntlDateStyleRecord source, skeleton;
    QJSIntlDateStyleFamily family;
    QJSIntlDateStyleWidth width; /* Requested width, not an alias origin width. */
} QJSIntlDateStyleSource;

typedef struct QJSIntlDateStylePrograms {
    QJSIntlAllocator allocator;
    QJSIntlDateTimeComponents components;
    QJSIntlDatePattern normal, twelve;
    QJSIntlDateStyleRecord date_source, time_source;
    QJSIntlDateCombinationRecord connector;
} QJSIntlDateStylePrograms;

/* Compile the selected nonempty style pattern, retaining its independent
   skeleton record and raw numbering overrides as provenance, not numeric
   renderer options. Apply the month/no-week CLDR year workaround to owned
   tokens. Use the style owner's own locale/calendar/alternate indexes.
   Reject unsupported projections and date/time family mixing; time requires an
   hour. Missing/BLOCK/empty selected patterns return UNSUPPORTED, without trying
   another style, alternate or parent. Own compiled text; source records borrow
   the unchanged pack. Require malloc/free, fresh disjoint output, reset errors. */
QJSIntlStatus qjs_intl_date_style_source_open(const QJSIntlDateStyleData *data,
                                            uint32_t locale, uint32_t calendar,
                                            QJSIntlDateStyleFamily family, QJSIntlDateStyleWidth width,
                                            uint32_t alternate, const QJSIntlAllocator *allocator,
                                            QJSIntlDateStyleSource *out);
/* Explicit native alternate preference, independently for pattern and skeleton.
   Missing/blocked/empty fields use their fallback key; preferred source syntax
   or projection failures also try the fallback source. Operational errors
   propagate. The exact source_open API retains its no-fallback contract. */
QJSIntlStatus qjs_intl_date_style_preferred_record(const QJSIntlDateStyleData *, uint32_t locale,
                                                   uint32_t calendar, QJSIntlDateStyleFamily,
                                                   QJSIntlDateStyleWidth, QJSIntlDateStyleField,
                                                   uint32_t preferred, uint32_t fallback,
                                                   QJSIntlDateStyleRecord *);
QJSIntlStatus qjs_intl_date_style_source_preferred(const QJSIntlDateStyleData *, uint32_t locale,
                                                   uint32_t calendar, QJSIntlDateStyleFamily,
                                                   QJSIntlDateStyleWidth, uint32_t preferred,
                                                   uint32_t fallback, const QJSIntlAllocator *,
                                                   QJSIntlDateStyleSource *);
void qjs_intl_date_style_source_clear(QJSIntlDateStyleSource *source);

/* DateTimeStyleFormat's single-date composition, before range construction.
   At least one source is required. Time requires its complete prepared pair
   with matching component/period semantics; no-hour date is copied unchanged.
   Inputs are unchanged source_open results (or style-date's prepared DATE
   style) and a complete single-program pair, not fake available candidates.
   Inputs can be cleared after this call.
   With both sources use the connector owner's independent locale/calendar
   indexes, ordinary alternate and DATE's requested width: prefer usable atTime,
   then standard, never relative/another width or a masked parent. Without both
   sources ignore connector arguments entirely. Compose tokens, not source text;
   reject extra connector fields that change the date/time component union.
   Own both programs; provenance still borrows unchanged packs. Numeric rendering
   must use the resolved ECMA NumberingSystem, not raw numbers overrides. Require
   malloc/free and fresh disjoint output, reset/unwind failures. No range record,
   missing hour-family synthesis or public formatter capability is supplied. */
QJSIntlStatus qjs_intl_date_style_programs_open(const QJSIntlDateStyleSource *date,
                                              const QJSIntlDateStyleSource *time,
                                              const QJSIntlDateSinglePrograms *time_programs,
                                              const QJSIntlDateCombinationData *connectors,
                                              uint32_t locale, uint32_t calendar,
                                              const QJSIntlAllocator *allocator,
                                              QJSIntlDateStylePrograms *out);
void qjs_intl_date_style_programs_clear(QJSIntlDateStylePrograms *programs);
#endif
