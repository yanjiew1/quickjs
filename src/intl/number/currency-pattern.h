/* Currency pattern-family selection and legacy spacing policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_CURRENCY_PATTERN_H
#define QJS_INTL_NUMBER_CURRENCY_PATTERN_H
#include "pattern-data.h"
#include "../currency/data.h"
#include "layout-data.h"

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
#endif
