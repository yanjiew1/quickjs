/* Native currency display selection over immutable source facts.
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
#ifndef QJS_INTL_CURRENCY_DISPLAY_H
#define QJS_INTL_CURRENCY_DISPLAY_H
#include "data.h"
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

#endif
