/* Sanctioned single/division units for native NumberFormat, independent of JS values.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef QJS_INTL_NUMBER_UNIT_H
#define QJS_INTL_NUMBER_UNIT_H
#include "../unit/format.h"
#include "../unit/grammar-data.h"
#include "../plural/data.h"

typedef struct QJSIntlNumberUnitComposition {
    uint32_t full, numerator, denominator, division;
    QJSIntlUnitGrammarRule plural, case_rule;
} QJSIntlNumberUnitComposition;

typedef struct QJSIntlNumberUnit {
    QJSIntlUnitData data;
    QJSIntlPluralData plurals;
    QJSIntlNumberUnitComposition local, root;
    unsigned int active, compound;
} QJSIntlNumberUnit;

/* Borrow unchanged owners from the same pack. Retain no caller text.
   Configuration may outlive the service, but not immutable pack storage.
   root must identify the pack's root locale. Cache local/root composition heads
   and independent supplemental grammar, not selected numeric categories.
   Allocate nothing; reset non-NULL output on errors. */
QJSIntlStatus qjs_intl_number_unit_open(const QJSIntlUnitData *, const QJSIntlPluralData *,
                                        const QJSIntlUnitGrammarData *, uint32_t locale, uint32_t root,
                                        QJSIntlUnitWidth, QJSIntlBytes identifier,
                                        QJSIntlNumberUnit *);

/* The caller supplies final rounded operands and notation expansion, plus
   exact -1/0/1. NULL operands means nonfinite OTHER and requires exact -1.
   Resolve each selected source's own cardinal rules, never requester rules.
   No origin is advertised as an available locale by this lookup.
   Prefer a complete precomposed form, then a denominator perUnitPattern, then
   generic division with derived component features. If local composition is
   ineligible, retry the entire unit expression from root while preserving the
   localized numeric body. Consulted malformed templates remain errors.
   Formatting copies the body/result according to unit/format.h. */
QJSIntlStatus qjs_intl_number_unit_format(const QJSIntlNumberUnit *, const QJSIntlAllocator *,
                                          const QJSIntlPluralOperands *, uint32_t decimal_shift,
                                          int exact, const QJSIntlFormatted *body, size_t limit,
                                          QJSIntlFormatted *);
#endif
