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
#include "plural-icu.h"
#ifdef CONFIG_INTL
#include <unicode/uloc.h>
#include <unicode/ures.h>
#include <limits.h>
#include <string.h>

static const char *const plural_names[] = {
    "zero", "one", "two", "few", "many", "other"
};

void js_intl_plural_icu_free(JSIntlPluralRuleSet *set)
{
    for (int i = 0; i < 6; i++) {
        if (set->rules[i]) set->realloc(set->opaque, set->rules[i], 0);
        set->rules[i] = NULL;
    }
}

/* Public resource operations mirror the schema described by ICU 78.3
   plurrule.cpp getRuleFromResource, independently implemented in C. */
int js_intl_plural_icu_load(JSIntlPluralRuleSet *set, const char *locale,
                          int ordinal, JSIntlPluralRealloc *realloc,
                          void *opaque, UErrorCode *status)
{
    UResourceBundle *bundle = NULL, *locales = NULL, *rules = NULL, *rule_set = NULL;
    UErrorCode lookup;
    const UChar *text = NULL;
    char *base = NULL, *parent = NULL, key[256];
    int32_t length, capacity, parent_length;
    int result = -1;
    memset(set, 0, sizeof(*set));
    set->realloc = realloc;
    set->opaque = opaque;
    if (U_FAILURE(*status)) return -1;
    if (!locale || !realloc) { *status = U_ILLEGAL_ARGUMENT_ERROR; return -1; }
    length = uloc_getBaseName(locale, NULL, 0, status);
    if (*status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(*status)) goto done;
    *status = U_ZERO_ERROR;
    if (length < 0 || length == INT32_MAX) { *status = U_MEMORY_ALLOCATION_ERROR; goto done; }
    capacity = length + 1;
    base = realloc(opaque, NULL, capacity);
    parent = realloc(opaque, NULL, capacity);
    if (!base || !parent) { *status = U_MEMORY_ALLOCATION_ERROR; goto done; }
    uloc_getBaseName(locale, base, capacity, status);
    bundle = ures_openDirect(NULL, "plurals", status);
    locales = ures_getByKey(bundle, ordinal ? "locales_ordinals" : "locales",
                           NULL, status);
    if (U_FAILURE(*status)) goto done;
    for (;;) {
        lookup = U_ZERO_ERROR;
        text = ures_getStringByKey(locales, base, &length, &lookup);
        if (U_SUCCESS(lookup)) break;
        if (lookup != U_MISSING_RESOURCE_ERROR) { *status = lookup; goto done; }
        parent_length = uloc_getParent(base, parent, capacity, status);
        if (U_FAILURE(*status)) goto done;
        if (!parent_length) { result = 0; goto done; } /* all other */
        memcpy(base, parent, (size_t)parent_length + 1);
    }
    if (length <= 0 || length >= (int32_t)sizeof(key)) {
        *status = U_INVALID_FORMAT_ERROR; goto done;
    }
    for (int32_t i = 0; i < length; i++) {
        if (!text[i] || text[i] > 127) { *status = U_INVALID_FORMAT_ERROR; goto done; }
        key[i] = text[i];
    }
    key[length] = 0;
    rules = ures_getByKey(bundle, "rules", NULL, status);
    rule_set = ures_getByKey(rules, key, NULL, status);
    if (U_FAILURE(*status)) goto done;
    for (int i = 0; i < 6; i++) {
        JSIntlPluralOperands zero = { "0", 1, 0 };
        int matches;
        lookup = U_ZERO_ERROR;
        text = ures_getStringByKey(rule_set, plural_names[i], &length, &lookup);
        if (lookup == U_MISSING_RESOURCE_ERROR) continue;
        if (U_FAILURE(lookup)) { *status = lookup; goto done; }
        /* Samples are Unicode metadata, not relation syntax. */
        for (int32_t j = 0; j < length; j++) {
            if (text[j] == '@') { length = j; break; }
            if (!text[j] || text[j] > 127) { *status = U_INVALID_FORMAT_ERROR; goto done; }
        }
        if (length < 0 || length == INT32_MAX) { *status = U_MEMORY_ALLOCATION_ERROR; goto done; }
        set->rules[i] = realloc(opaque, NULL, (size_t)length + 1);
        if (!set->rules[i]) { *status = U_MEMORY_ALLOCATION_ERROR; goto done; }
        for (int32_t j = 0; j < length; j++) set->rules[i][j] = text[j];
        set->rules[i][length] = 0;
        if (js_intl_plural_rule_evaluate(set->rules[i], length, &zero, &matches) < 0) {
            *status = U_INVALID_FORMAT_ERROR; goto done;
        }
    }
    result = 0;
 done:
    if (rule_set) ures_close(rule_set);
    if (rules) ures_close(rules);
    if (locales) ures_close(locales);
    if (bundle) ures_close(bundle);
    if (base) realloc(opaque, base, 0);
    if (parent) realloc(opaque, parent, 0);
    if (result < 0) js_intl_plural_icu_free(set);
    return result;
}

int js_intl_plural_icu_select(const JSIntlPluralRuleSet *set,
                            const JSIntlPluralOperands *number, int *category)
{
    for (int i = 0; i < 5; i++) {
        int matches;
        if (!set->rules[i]) continue;
        if (js_intl_plural_rule_evaluate(set->rules[i], strlen(set->rules[i]),
                                        number, &matches) < 0) return -1;
        if (matches) { *category = i; return 0; }
    }
    /* Validate the decimal even for locales whose only category is other. */
    if (js_intl_plural_rule_evaluate("", 0, number, category) < 0) return -1;
    *category = 5;
    return 0;
}
#endif
