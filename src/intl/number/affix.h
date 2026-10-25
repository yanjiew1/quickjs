/* Typed number-pattern composition over an already formatted numeric body.
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
#ifndef QJS_INTL_NUMBER_AFFIX_H
#define QJS_INTL_NUMBER_AFFIX_H
#include "number.h"
#include "pattern-data.h"
#include "round.h"

/* GetNumberFormatPattern sign selection. Require the unchanged successful
   rounded result. NaN is positive-zero for this decision; signed infinities
   are nonzero. Return POSITIVE, NEGATIVE or PLUS from pattern-data.h.
   No allocation; disjoint non-NULL output resets to POSITIVE on error. */
QJSIntlStatus qjs_intl_number_sign_pattern(QJSIntlNumberSign, const QJSIntlRoundedNumber *,
                                           unsigned int *form);

typedef struct QJSIntlNumberAffixSymbols {
    QJSIntlBytes minus, plus, percent, per_mille, currency;
    QJSIntlBytes before_currency, after_currency;
} QJSIntlNumberAffixSymbols;

/* Tokens are decoded from the selected native outer standard pattern.
   Compact notation composes its own body and part types before this step; a
   raw compact pattern (including a literal-only form) is not this contract.
   Exactly one NUMBER token inserts all body parts unchanged. Other tokens
   emit scalar UTF-8 literals or the supplied symbol; every currency token
   uses the caller's already resolved display text. Empty replacements remain
   empty parts. The body must be a nonempty, completely partitioned numeric
   result; its text and part offsets are copied exactly.

   ensure_plus inserts a leading plus if no PLUS token exists, for positive
   signDisplay when the LDML positive variant lacks a sign (e.g. accounting).
   Optional before_currency/after_currency insert literal parts only at direct
   NUMBER/CURRENCY token boundaries. Callers already checked the currency and
   numeric scalar predicates; no gap is emitted for an empty currency.
   Pattern selection, currency spacing and plural/style policy are callers'
   responsibilities. No localized string is invented by this operation.

   Borrow inputs unchanged. Fresh, disjoint output required; errors reset it.
   malloc/free required; realloc unused. Bound the complete UTF-16 output;
   clear with qjs_intl_number_body_clear and the creating allocator. */
QJSIntlStatus qjs_intl_number_affix_format(const QJSIntlAllocator *,
                                           const QJSIntlNumberPatternToken *, size_t count,
                                           const QJSIntlNumberAffixSymbols *,
                                           const QJSIntlFormatted *body, unsigned int ensure_plus,
                                           size_t maximum_output_length, QJSIntlFormatted *);
#endif
