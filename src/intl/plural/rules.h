/* Exact decimal CLDR plural relations, independent of engine and backend.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_PLURAL_RULES_H
#define QJS_INTL_PLURAL_RULES_H
#include "../common/types.h"

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
#endif
