/* One-operand literal templates, independent of service and LDML quoting.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMMON_UNARY_PATTERN_H
#define QJS_INTL_COMMON_UNARY_PATTERN_H
#include "types.h"

typedef struct QJSIntlUnaryPattern {
    QJSIntlBytes prefix, suffix;
} QJSIntlUnaryPattern;

/* Borrow validated scalar UTF-8 containing exactly one {0}; apostrophes are
   literal. Empty affixes are allowed. Literal-only text is UNSUPPORTED; repeated
   or unsupported placeholders/braces are DATA_ERROR. No allocation or retained
   state. Reset non-NULL output on failure. */
QJSIntlStatus qjs_intl_unary_pattern_parse(QJSIntlBytes, QJSIntlUnaryPattern *);
#endif
