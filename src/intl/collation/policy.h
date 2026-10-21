/* Plain comparison policy shared by host compilation and collation consumers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_POLICY_H
#define QJS_INTL_COLLATION_POLICY_H

typedef enum QJSIntlCollationSensitivity {
    QJS_INTL_COLLATION_VARIANT,
    QJS_INTL_COLLATION_BASE,
    QJS_INTL_COLLATION_ACCENT,
    QJS_INTL_COLLATION_CASE
} QJSIntlCollationSensitivity;

typedef enum QJSIntlCollationCaseFirst {
    QJS_INTL_COLLATION_CASE_OFF,
    QJS_INTL_COLLATION_UPPER_FIRST,
    QJS_INTL_COLLATION_LOWER_FIRST
} QJSIntlCollationCaseFirst;

typedef enum QJSIntlCollationStrength {
    QJS_INTL_COLLATION_STRENGTH_PRIMARY = 1,
    QJS_INTL_COLLATION_STRENGTH_SECONDARY,
    QJS_INTL_COLLATION_STRENGTH_TERTIARY,
    QJS_INTL_COLLATION_STRENGTH_QUATERNARY,
    QJS_INTL_COLLATION_STRENGTH_IDENTICAL
} QJSIntlCollationStrength;

typedef enum QJSIntlCollationMaxVariable {
    QJS_INTL_COLLATION_VARIABLE_SPACE,
    QJS_INTL_COLLATION_VARIABLE_PUNCT,
    QJS_INTL_COLLATION_VARIABLE_SYMBOL,
    QJS_INTL_COLLATION_VARIABLE_CURRENCY
} QJSIntlCollationMaxVariable;

typedef struct QJSIntlCollationPolicy {
    QJSIntlCollationStrength strength;
    QJSIntlCollationCaseFirst case_first;
    QJSIntlCollationMaxVariable max_variable;
    int shifted, backwards_secondary, normalization, case_level, numeric;
} QJSIntlCollationPolicy;
#endif
