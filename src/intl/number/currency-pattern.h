/* Currency pattern families, name placement and legacy spacing policy.
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
#ifndef QJS_INTL_NUMBER_CURRENCY_PATTERN_H
#define QJS_INTL_NUMBER_CURRENCY_PATTERN_H
#include "pattern-data.h"
#include "../currency/data.h"
#include "../currency/boundary.h"
#include "layout-data.h"
#include "../plural/rules.h"

typedef struct QJSIntlCurrencyPattern {
    QJSIntlNumberPattern pattern;
    QJSIntlBytes before_currency, after_currency;
} QJSIntlCurrencyPattern;

/* Select standard/accounting and alpha forms for an already selected symbol.
   Only standard currencySign permits the legacy per-currency pattern override.
   An alpha-capable family never also applies legacy spacing. Returned gap text
   already passed its currency-edge predicate; the caller must still check the
   actual adjacent numeric scalar against General_Category Nd when formatting.

   Require unchanged validated owners, exact locale/system indexes, canonical
   code and accounting 0/1. Support the pinned CLDR currencySpacing predicates;
   an unknown predicate returns UNSUPPORTED. No allocation/retained input.
   Output borrows the owners; disjoint output resets on errors. */
QJSIntlStatus qjs_intl_number_currency_pattern(const QJSIntlNumberPatternData *,
                                              const QJSIntlCurrencyData *,
                                              const QJSIntlNumberLayoutData *,
                                              uint32_t locale, uint32_t system, QJSIntlBytes code,
                                              QJSIntlBytes symbol, unsigned int accounting,
                                              QJSIntlCurrencyPattern *);
/* Legacy currency-edge filtering for a resolved numbering-system layout.
   Return borrowed before/after insertions; the caller still tests numeric Nd
   and direct token adjacency. Supports pinned predicates only. No allocation;
   disjoint outputs are both reset on error. */
QJSIntlStatus qjs_intl_number_currency_spacing(const QJSIntlNumberLayouts *,
                                              const QJSIntlCurrencyBoundary *,
                                              QJSIntlBytes *before, QJSIntlBytes *after);
/* Select a currency-name placement template independently of name fallback.
   Require locale-level projections from currency_unit_layouts_at. Exact and
   selected-category forms exclude root; OTHER may include root. An absent or
   blocked OTHER returns UNSUPPORTED; explicit empty text succeeds. Borrow all
   text, allocate nothing, and reset disjoint non-NULL output on error. */
QJSIntlStatus qjs_intl_currency_unit_pattern(const QJSIntlCurrencyUnitLayouts *,
                                              QJSIntlPluralCategory, int exact, QJSIntlBytes *);
#endif
