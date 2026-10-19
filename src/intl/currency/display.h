/* Native currency display selection over immutable source facts.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_CURRENCY_DISPLAY_H
#define QJS_INTL_CURRENCY_DISPLAY_H
#include "data.h"
#include "../number/layout-data.h"
#include "../plural/rules.h"

/* Require unchanged open data and its exact locale index. The code is three
   uppercase ASCII letters, including well-formed codes absent from the pack.
   Regular symbols fall back to the supplied code; narrow symbols fall back to
   regular symbols first. Explicit empty text succeeds; blocked text does not
   resume same-field parent lookup. narrow is 0 or 1.

   Both operations allocate nothing and retain no input. Output must be
   disjoint; errors reset it. Successful text borrows unchanged pack storage
   or, for code fallback, the supplied code's storage. Empty text is valid. */
QJSIntlStatus qjs_intl_currency_symbol(const QJSIntlCurrencyData *, uint32_t locale,
                                        QJSIntlBytes code, unsigned int narrow, QJSIntlBytes *);

/* Select after rounding. exact is -1 (neither), 0 or 1 for the unsigned value;
   the caller supplies its independently evaluated cardinal category. Try an
   exact selector, the category, OTHER, the uncounted name, then the code.
   Counted names exclude root; uncounted names may come from root. */
QJSIntlStatus qjs_intl_currency_name(const QJSIntlCurrencyData *, uint32_t locale,
                                      QJSIntlBytes code, QJSIntlPluralCategory, int exact,
                                      QJSIntlBytes *);

/* Require locale-level projected layouts from currency_unit_layouts_at.
   Select exact/category independently of name fallback; selected counts
   exclude root but the OTHER fallback includes it. An absent/blocked OTHER
   returns UNSUPPORTED. Explicit empty text succeeds. Borrowed/no-allocation
   and error-reset contracts match the display operations above. */
QJSIntlStatus qjs_intl_currency_unit_pattern(const QJSIntlCurrencyUnitLayouts *,
                                              QJSIntlPluralCategory, int exact, QJSIntlBytes *);
#endif
