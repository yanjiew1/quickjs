/*
 * QuickJS native Intl services and tests
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
/* Exact decimal CLDR plural relations. No engine or ICU dependency.
 * decimal is an unsigned, unscaled FormatNumericToString decimal string.
 * exponent is the notation exponent (CLDR c/e); visible zeros are retained.
 * Relations use integer CLDR constants and optional samples after '@'. */
#ifndef JS_INTL_PLURAL_H
#define JS_INTL_PLURAL_H
#include <stddef.h>
#include <stdint.h>

typedef struct JSIntlPluralOperands {
    const char *decimal;
    size_t length;
    int32_t exponent;
} JSIntlPluralOperands;

/* Return 0 and set matches, or -1 for malformed decimal/rule syntax.
 * Supports n/i/v/w/f/t/c/e, %/mod, =/!=/is/in/within, integer ranges/lists,
 * and conjunction before disjunction. No allocation or binary floating point.
 * CLDR 48 shipped rules use constants <= 1000000; uint32_t is supported. */
int js_intl_plural_rule_evaluate(const char *rule, size_t rule_length,
                               const JSIntlPluralOperands *number,
                               int *matches);
#endif
