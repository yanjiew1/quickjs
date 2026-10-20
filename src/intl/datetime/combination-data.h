/* Shared date/time connectors; no form fallback or calendar capability policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_COMBINATION_DATA_H
#define QJS_INTL_DATETIME_COMBINATION_DATA_H
#include "../native/data.h"
#include "style-types.h"

typedef enum QJSIntlDateCombinationForm {
    QJS_INTL_DATE_COMBINATION_STANDARD,
    QJS_INTL_DATE_COMBINATION_AT_TIME,
    QJS_INTL_DATE_COMBINATION_RELATIVE,
    QJS_INTL_DATE_COMBINATION_FORM_COUNT
} QJSIntlDateCombinationForm;

typedef enum QJSIntlDateCombinationState {
    QJS_INTL_DATE_COMBINATION_MISSING,
    QJS_INTL_DATE_COMBINATION_VALUE,
    QJS_INTL_DATE_COMBINATION_BLOCKED
} QJSIntlDateCombinationState;

typedef struct QJSIntlDateCombinationRecord {
    QJSIntlDateCombinationState state;
    QJSIntlBytes text, numbers, origin_locale, origin_calendar, origin_alternate;
    QJSIntlDateStyleWidth origin_width;
    QJSIntlDateCombinationForm origin_form;
} QJSIntlDateCombinationRecord;

typedef struct QJSIntlDateCombinationData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *calendars, *alternates, *locale_rows, *rows, *records;
    uint32_t calendar_count, alternate_count, row_count, row_width, record_count;
} QJSIntlDateCombinationData;

/* Borrow an unchanged validated shared owner. Validation uses malloc/free
   scratch, freed on every path, and retains no allocation or teardown state.
   Check canonical sharing, source identities and states. Follow pack.h's
   disjoint-output contract. Text and numbering overrides remain raw source
   facts; pattern interpretation and form fallback have later owners. */
QJSIntlStatus qjs_intl_date_combination_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                                  QJSIntlDateCombinationData *);
/* Inventories and records borrow immutable storage and allocate nothing.
   Calendar IDs are raw CLDR source IDs, including data-only generic.
   Alternate zero is the ordinary empty selector. */
QJSIntlStatus qjs_intl_date_combination_calendar_at(const QJSIntlDateCombinationData *, uint32_t,
                                                    QJSIntlBytes *);
QJSIntlStatus qjs_intl_date_combination_alternate_at(const QJSIntlDateCombinationData *, uint32_t,
                                                     QJSIntlBytes *);
/* Exact selectors only; no standard/atTime/relative fallback. MISSING returns
   OK with a zero record; BLOCKED and empty VALUE remain distinct. Origin fields
   are meaningful unless MISSING. Reset non-NULL output on every error. */
QJSIntlStatus qjs_intl_date_combination_record_at(const QJSIntlDateCombinationData *, uint32_t locale,
                                                  uint32_t calendar, QJSIntlDateStyleWidth,
                                                  QJSIntlDateCombinationForm, uint32_t alternate,
                                                  QJSIntlDateCombinationRecord *);
#endif
