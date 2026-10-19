/* Single-unit adapter for native NumberFormat, independent of JS values.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_UNIT_H
#define QJS_INTL_NUMBER_UNIT_H
#include "../unit/format.h"
#include "../plural/data.h"

typedef struct QJSIntlNumberUnit {
    QJSIntlUnitData data;
    QJSIntlPluralData plurals;
    uint32_t head;
} QJSIntlNumberUnit;

/* Borrow unchanged owners from the same pack. Parse/copy no caller text.
   Configuration may outlive the service, but not immutable pack storage.
   Valid division identifiers return UNSUPPORTED until compound grammar support.
   Allocate nothing; reset non-NULL output on errors. */
QJSIntlStatus qjs_intl_number_unit_open(const QJSIntlUnitData *, const QJSIntlPluralData *,
                                        uint32_t locale, QJSIntlUnitWidth, QJSIntlBytes identifier,
                                        QJSIntlNumberUnit *);

/* The caller supplies final rounded operands and notation expansion, plus
   exact -1/0/1. NULL operands means nonfinite OTHER and requires exact -1.
   Resolve each selected source's own cardinal rules, never requester rules.
   No origin is advertised as an available locale by this lookup.
   Formatting copies the body/result according to unit/format.h. */
QJSIntlStatus qjs_intl_number_unit_format(const QJSIntlNumberUnit *, const QJSIntlAllocator *,
                                          const QJSIntlPluralOperands *, uint32_t decimal_shift,
                                          int exact, const QJSIntlFormatted *body, size_t limit,
                                          QJSIntlFormatted *);
#endif
