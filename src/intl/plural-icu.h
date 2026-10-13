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
/* Private ICU resource adapter for the engine-independent decimal evaluator.
 * ICU 78.3 / CLDR 48 rule schema; no QuickJS types or interpreter state. */
#ifndef JS_INTL_PLURAL_ICU_H
#define JS_INTL_PLURAL_ICU_H
#include "plural.h"
#ifdef CONFIG_INTL
#include <unicode/utypes.h>

typedef void *JSIntlPluralRealloc(void *opaque, void *ptr, size_t size);
typedef struct JSIntlPluralRuleSet {
    char *rules[6]; /* zero, one, two, few, many, other */
    JSIntlPluralRealloc *realloc;
    void *opaque;
} JSIntlPluralRuleSet;

/* On failure the initialized result is empty and safe to free.
 * Allocator opaque must live as long as the rule set (runtime, not context). */
int js_intl_plural_icu_load(JSIntlPluralRuleSet *set, const char *locale,
                          int ordinal, JSIntlPluralRealloc *realloc,
                          void *opaque, UErrorCode *status);
void js_intl_plural_icu_free(JSIntlPluralRuleSet *set);
int js_intl_plural_icu_select(const JSIntlPluralRuleSet *set,
                            const JSIntlPluralOperands *number, int *category);
#endif
#endif
