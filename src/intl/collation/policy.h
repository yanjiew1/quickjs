/* Plain comparison policy shared by host compilation and collation consumers.
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
