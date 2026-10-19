/* Literal two-operand templates; no service or backend state.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMMON_PAIR_PATTERN_H
#define QJS_INTL_COMMON_PAIR_PATTERN_H
#include "types.h"

typedef struct QJSIntlPairPattern {
    QJSIntlBytes prefix, separator, suffix;
    unsigned int first; /* Index of the first operand (0 or 1). */
} QJSIntlPairPattern;

/* Borrow literal fragments around exactly one {0} and one {1}, in either
   order. Missing operands return UNSUPPORTED; invalid/repeated placeholders
   return DATA_ERROR. Apostrophes are literal. Input comes from scalar UTF-8
   data; this parser only checks template syntax. Allocate/retain nothing,
   require disjoint output and reset non-NULL output on failure. */
QJSIntlStatus qjs_intl_pair_pattern_parse(QJSIntlBytes, QJSIntlPairPattern *);

#endif
