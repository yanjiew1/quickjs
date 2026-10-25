/* Exact decimal CLDR plural relations, independent of engine and backend.
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
#ifndef QJS_INTL_PLURAL_RULES_H
#define QJS_INTL_PLURAL_RULES_H
#include "../common/types.h"

typedef enum QJSIntlPluralCategory {
    QJS_INTL_PLURAL_ZERO,
    QJS_INTL_PLURAL_ONE,
    QJS_INTL_PLURAL_TWO,
    QJS_INTL_PLURAL_FEW,
    QJS_INTL_PLURAL_MANY,
    QJS_INTL_PLURAL_OTHER,
    QJS_INTL_PLURAL_CATEGORY_COUNT
} QJSIntlPluralCategory;

typedef enum QJSIntlPluralType {
    QJS_INTL_PLURAL_CARDINAL,
    QJS_INTL_PLURAL_ORDINAL
} QJSIntlPluralType;

typedef struct QJSIntlPluralOperands {
    /* Borrowed unsigned plain decimal, already expanded by the notation
       exponent. For source 1.2005c3, pass "1200.5" and exponent 3. Retain
       visible zeros; this evaluator does not shift the supplied decimal. */
    const char *decimal;
    size_t length;
    int32_t exponent; /* CLDR notation operands c/e, zero for standard notation. */
} QJSIntlPluralOperands;

/* Evaluate one CLDR relation, not a category name or sample list. An empty
   relation is universally true (the other category). No allocations or binary
   floating point; arbitrarily long decimal coefficients remain exact.
   Supports n/i/v/w/f/t/c/e, integer constants through UINT32_MAX, modulo,
   equality, integer/continuous ranges, lists, and conjunction before disjunction.
   Input slices borrow valid storage, have lengths at most PTRDIFF_MAX and
   must not alias the output. matches is
   reset to zero on error. Invalid operands -> INVALID_ARGUMENT, malformed rule
   -> DATA_ERROR. Even short-circuited logical arms are syntax-checked. */
QJSIntlStatus qjs_intl_plural_rule_evaluate(QJSIntlBytes rule, const QJSIntlPluralOperands *number,
                                            int *matches);

/* As above, but shift the supplied decimal right by decimal_shift places
   before computing n/i/v/w/f/t. Unlike the struct's ordinary contract, pass
   the visible unexpanded mantissa here; exponent still sets c/e independently.
   For 1.20050c3, pass "1.20050", exponent 3 and decimal_shift 3. No expanded
   string or allocation: work is bounded by actual input/rule lengths and
   logarithmically by decimal_shift, including UINT32_MAX. */
QJSIntlStatus qjs_intl_plural_rule_evaluate_shifted(QJSIntlBytes rule,
                                                    const QJSIntlPluralOperands *number,
                                                    uint32_t decimal_shift, int *matches);
#endif
