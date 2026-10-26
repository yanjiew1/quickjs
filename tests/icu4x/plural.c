/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../../src/icu4x/plural.h"
#include "../../src/icu4x/decimal.h"
#include "../../src/intl/icu4x/locale-data.h"
#include "../../src/intl/plural/rules.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static QJSIntlPluralCategory select(const QJSICU4XPluralData *data, const char *text, int exponent)
{
    QJSIntlPluralOperands operands = {text, strlen(text), exponent};
    unsigned int category;

    for (category = 0; category < QJS_INTL_PLURAL_CATEGORY_COUNT; category++) {
        int matches;
        QJSICU4XRuleBytes rule = data->rules[category];
        if (!(data->mask & (1u << category)))
            continue;
        assert(!qjs_intl_plural_rule_evaluate((QJSIntlBytes){(const char *)rule.data, rule.length},
                                              &operands, &matches));
        if (matches)
            return (QJSIntlPluralCategory)category;
    }
    assert(0); // The exported other rule is empty and always matches.
    return QJS_INTL_PLURAL_OTHER;
}

static void check(const char *locale, unsigned ordinal, const char *text, int exponent,
                  QJSIntlPluralCategory expected)
{
    QJSICU4XPlural *handle = NULL;
    QJSICU4XPluralData data;
    assert(!qjs_icu4x_plural_open((const uint8_t *)locale, strlen(locale), ordinal, &handle));
    assert(!qjs_icu4x_plural_data(handle, &data));
    QJSIntlPluralCategory result = select(&data, text, exponent);
    if (result != expected) {
        fprintf(stderr, "%s %u %s exponent=%d got=%u expected=%u\n", locale, ordinal, text,
                exponent, result, expected);
        assert(0);
    }
    qjs_icu4x_plural_close(handle);
}

int main(void)
{
    QJSICU4XPlural *handle = (QJSICU4XPlural *)(uintptr_t)1;
    QJSICU4XPluralData data, saved;
    size_t i;
    unsigned int kind, category;

    {
        static const char *const locales[] = {"fr", "ja", "bn"};
        static const int expected[] = {6, 4, 5};
        for (i = 0; i < sizeof(locales) / sizeof(*locales); i++) {
            QJSICU4XCompact *compact = NULL;
            int32_t exponent;
            assert(!qjs_icu4x_compact_open((const uint8_t *)locales[i], strlen(locales[i]), 1,
                                           &compact));
            assert(!qjs_icu4x_compact_exponent(compact, 6, &exponent));
            assert(exponent == expected[i]);
            assert(!qjs_icu4x_compact_exponent(compact, INT32_MIN, &exponent) && exponent == 0);
            assert(!qjs_icu4x_compact_exponent(compact, INT32_MAX, &exponent));
            assert(exponent >= expected[i]);
            qjs_icu4x_compact_close(compact);
        }
        qjs_icu4x_compact_close(NULL);
    }
    check("ar", 0, "100000000000000003", 0, QJS_INTL_PLURAL_FEW);
    check("ar", 0, "1.0000000000000000001", 0, QJS_INTL_PLURAL_OTHER);
    check("ar", 0, "1.0000000000000000000", 0, QJS_INTL_PLURAL_ONE);
    check("ar", 0, "11", 0, QJS_INTL_PLURAL_MANY);
    check("pt", 0, "0", 0, QJS_INTL_PLURAL_ONE);
    check("pt-PT", 0, "0", 0, QJS_INTL_PLURAL_OTHER);
    check("pt-u-sd-pt11", 0, "0", 0, QJS_INTL_PLURAL_OTHER);
    check("en-US", 0, "1", 0, QJS_INTL_PLURAL_ONE);
    check("en-US", 0, "1.0", 0, QJS_INTL_PLURAL_OTHER);
    check("en", 1, "3", 0, QJS_INTL_PLURAL_FEW);
    check("en", 1, "13", 0, QJS_INTL_PLURAL_OTHER);
    check("fr", 0, "1000000", 0, QJS_INTL_PLURAL_MANY);
    check("fr", 0, "0.001", -3, QJS_INTL_PLURAL_ONE);
    check("fr", 0, "2", 0, QJS_INTL_PLURAL_OTHER);
    check("fr", 0, "2", -3, QJS_INTL_PLURAL_MANY);
    check("ro", 0, "1.0000000000000000001", 0, QJS_INTL_PLURAL_FEW);

    // Validate every real exported relation, including short-circuited grammar,
    // against the exact evaluator. Candidate membership comes from the matching
    // CLDR registry; constructors alone are not an available-locale inventory.
    for (i = 0; i < qjs_icu4x_locale_registry_count; i++) {
        const char *locale = qjs_icu4x_locale_strings + qjs_icu4x_locale_registry[i];
        for (kind = 0; kind < 2; kind++) {
            assert(!qjs_icu4x_plural_open((const uint8_t *)locale, strlen(locale), kind, &handle));
            assert(!qjs_icu4x_plural_data(handle, &data));
            assert((data.mask & (1u << QJS_INTL_PLURAL_OTHER)) && !(data.mask & ~63u));
            for (category = 0; category < 36; category++)
                assert(data.ranges[category] < QJS_INTL_PLURAL_CATEGORY_COUNT);
            (void)select(&data, "0.0000000000000000001", -3);
            for (category = 0; category < QJS_INTL_PLURAL_CATEGORY_COUNT; category++) {
                int matched;
                QJSIntlPluralOperands operands = {"100000000000000003", 18, 0};
                if (data.mask & (1u << category))
                    assert(!qjs_intl_plural_rule_evaluate(
                        (QJSIntlBytes){(const char *)data.rules[category].data,
                                       data.rules[category].length},
                        &operands, &matched));
            }
            qjs_icu4x_plural_close(handle);
        }
    }
    {
        char locale[] = "sl";
        assert(!qjs_icu4x_plural_open((const uint8_t *)locale, strlen(locale), 0, &handle));
        memset(locale, 'x', 2); // The handle must not borrow its input spelling.
        assert(!qjs_icu4x_plural_data(handle, &data));
        assert(data.ranges[QJS_INTL_PLURAL_ONE * 6 + QJS_INTL_PLURAL_ONE] == QJS_INTL_PLURAL_FEW);
        qjs_icu4x_plural_close(handle);
        assert(!qjs_icu4x_plural_open((const uint8_t *)"sl", 2, 1, &handle));
        assert(!qjs_icu4x_plural_data(handle, &data));
        assert(data.ranges[QJS_INTL_PLURAL_OTHER * 6 + QJS_INTL_PLURAL_ONE] == QJS_INTL_PLURAL_FEW);
        qjs_icu4x_plural_close(handle);
    }
    handle = (QJSICU4XPlural *)(uintptr_t)1;
    assert(qjs_icu4x_plural_open((const uint8_t *)"en", 2, 2, &handle) == QJS_ICU4X_RANGE);
    assert(handle == (QJSICU4XPlural *)(uintptr_t)1);
    memset(&data, 0xa5, sizeof(data));
    memcpy(&saved, &data, sizeof(data));
    assert(qjs_icu4x_plural_data(NULL, &data) == QJS_ICU4X_RANGE);
    assert(!memcmp(&data, &saved, sizeof(data)));
    qjs_icu4x_plural_close(NULL);
    return 0;
}
