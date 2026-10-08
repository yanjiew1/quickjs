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
/* Exact decimal witnesses and all pinned CLDR 48 rule syntax.
 * Pure evaluator tests run without ICU; adapter tests require CONFIG_INTL. */
#include "../src/intl/plural.h"
#include "fixtures/intl-plural-cldr48.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef CONFIG_INTL
#include "../src/intl/plural-icu.h"
#endif

static void check(const char *rule, const char *number, int32_t exponent,
                  int expected)
{
    JSIntlPluralOperands operands = { number, strlen(number), exponent };
    int matches = -1;
    assert(js_intl_plural_rule_evaluate(rule, strlen(rule), &operands, &matches) == 0);
    assert(matches == expected);
}

static void test_decimal_relations(void)
{
    const char *one = "v = 0 and i % 10 = 1 and i % 100 != 11";
    check(one, "90071992547409921", 0, 1);
    check(one, "90071992547409911", 0, 0);
    check(one, "100000000000000000000000000000000000000021", 0, 1);
    check(one, "90071992547409921.0", 0, 0);
}

static void test_operands(void)
{
    check("n = 1", "0001.000", 0, 1);
    check("n = 1", "1.001", 0, 0);
    check("n within 1..2", "1.500", 0, 1);
    check("n in 1..2", "1.500", 0, 0);
    check("n not in 1..2", "1.500", 0, 1);
    check("n % 10 within 1..2", "10000000000000000000001.500", 0, 1);
    check("n % 10 within 1..2", "10000000000000000000002.001", 0, 0);
    check("v = 4 and w = 2 and f = 1200 and t = 12", "1.1200", 0, 1);
    check("v = 4 and w = 0 and f = 0 and t = 0", "1.0000", 0, 1);
    check("v = 19 and w = 17 and f % 100 = 0 and t % 100 = 21",
          "0.9007199254740992100", 0, 1);
    check("i = 1 and e = 0 and c = 0", "1.00", 0, 1);
    check("n = 1000000 and v = 1 and e = 6 and c = 6", "1000000.0", 6, 1);
    check("e not in 0..5 and c != 0..5", "0.001", -3, 1);
    check("e mod 10 is not 1", "1", INT32_MIN, 1);
    check("i mod 4294967295 is 1", "4294967296", 0, 1);
    check("i is not 4294967295", "4294967296", 0, 1);
    check("i = 1,3..5,7 and v = 0", "5", 0, 1);
    check("i = 1 or i = 2 and v = 0", "1.5", 0, 1);
    check("i = 1 or i = 2 and v = 0", "2.5", 0, 0);
    check("n = 1 @integer 1, 21, ... @decimal 1.0", "1", 0, 1);
    check("n = 1000 and i = 1000 and v = 1 and w = 0 and f = 0 and t = 0 and e = 3 and c = 3",
          "1000.0", 3, 1);
    check("n within 0..1 and i = 0 and v = 5 and w = 3 and f = 100 and t = 1 and e not in 0..5",
          "0.00100", -3, 1);
    check("e % 10 is not 3 and c not in 0..5", "0.00100", -3, 1);
}

static void test_invalid(void)
{
    static const char *const rules[] = {
        "x = 0", "i > 0", "i mod % 10 = 1", "i % 0 = 0", "i = 1 or",
        "i = 2..1", "i = 1,,2", "i is 1..2", "i = 4294967296",
        "i = 1 garbage", "i and v = 0", "n within 0.5..1"
    };
    static const char *const decimals[] = { "", "-1", "1e3", ".5", "1.", "1..0" };
    JSIntlPluralOperands number = { "1", 1, 0 };
    int matches;
    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); i++)
        assert(js_intl_plural_rule_evaluate(rules[i], strlen(rules[i]), &number, &matches) < 0);
    for (size_t i = 0; i < sizeof(decimals) / sizeof(decimals[0]); i++) {
        number.decimal = decimals[i]; number.length = strlen(decimals[i]);
        assert(js_intl_plural_rule_evaluate("n = 1", 5, &number, &matches) < 0);
    }
    number.decimal = "1"; number.length = 1;
    assert(js_intl_plural_rule_evaluate("\0 = 0", 5, &number, &matches) < 0);
}

static void test_pinned_grammar(void)
{
    JSIntlPluralOperands number = { "0", 1, 0 };
    int matches;
    for (size_t i = 0; i < sizeof(cldr48_plural_rules) / sizeof(cldr48_plural_rules[0]); i++)
        assert(js_intl_plural_rule_evaluate(cldr48_plural_rules[i],
                   strlen(cldr48_plural_rules[i]), &number, &matches) == 0);
}

#ifdef CONFIG_INTL
typedef struct TestAllocator { size_t calls, live, fail_at; } TestAllocator;
static void *test_realloc(void *opaque, void *ptr, size_t size)
{
    TestAllocator *state = opaque;
    void *result;
    int was_null = ptr == NULL;
    if (!size) { if (ptr) { assert(state->live); state->live--; free(ptr); } return NULL; }
    if (state->calls++ == state->fail_at) return NULL;
    result = realloc(ptr, size);
    if (result && was_null) state->live++;
    return result;
}

static void test_resource_adapter(void)
{
    TestAllocator allocator = { 0, 0, SIZE_MAX };
    JSIntlPluralRuleSet rules;
    JSIntlPluralOperands number = { "90071992547409921", 17, 0 };
    UErrorCode status = U_ZERO_ERROR;
    int category;
    size_t calls;
    assert(js_intl_plural_icu_load(&rules, "ru_RU", 0, test_realloc, &allocator, &status) == 0);
    assert(js_intl_plural_icu_select(&rules, &number, &category) == 0 && category == 1);
    js_intl_plural_icu_free(&rules);
    assert(allocator.live == 0);
    calls = allocator.calls;
    for (size_t fail = 0; fail < calls; fail++) {
        allocator = (TestAllocator){ 0, 0, fail };
        status = U_ZERO_ERROR;
        assert(js_intl_plural_icu_load(&rules, "ru_RU", 0, test_realloc, &allocator, &status) < 0);
        assert(status == U_MEMORY_ALLOCATION_ERROR && allocator.live == 0);
        js_intl_plural_icu_free(&rules);
        assert(allocator.live == 0);
    }
    allocator = (TestAllocator){ 0, 0, SIZE_MAX };
    status = U_ZERO_ERROR;
    assert(js_intl_plural_icu_load(&rules, "en_US", 1, test_realloc, &allocator, &status) == 0);
    number.decimal = "90071992547409922";
    assert(js_intl_plural_icu_select(&rules, &number, &category) == 0 && category == 2);
    js_intl_plural_icu_free(&rules);
    assert(allocator.live == 0);
}
/* Full resource keys precede parent fallback: pt_PT differs from pt. */
static void test_resource_fallback_and_scaling(void)
{
    static const struct {
        const char *locale, *decimal;
        int32_t exponent;
        int category;
    } cases[] = {
        { "pt_PT", "0", 0, 5 },
        { "pt_BR", "0", 0, 1 },
        { "pt_PT_POSIX", "0", 0, 5 },
        { "zz_ZZ", "1", 0, 5 },
        { "ru", "1000.0", 3, 5 },
        { "ru", "1000", 3, 4 },
        { "fr", "1000001", 0, 5 },
        { "fr", "1000001", 6, 4 },
        { "es", "0.001", 0, 5 },
        { "es", "0.001", -3, 4 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        TestAllocator allocator = { 0, 0, SIZE_MAX };
        JSIntlPluralRuleSet rules;
        JSIntlPluralOperands number = {
            cases[i].decimal, strlen(cases[i].decimal), cases[i].exponent
        };
        UErrorCode status = U_ZERO_ERROR;
        int category;
        assert(js_intl_plural_icu_load(&rules, cases[i].locale, 0,
                   test_realloc, &allocator, &status) == 0);
        assert(js_intl_plural_icu_select(&rules, &number, &category) == 0);
        assert(category == cases[i].category);
        js_intl_plural_icu_free(&rules);
        assert(allocator.live == 0);
    }
}
#endif

int main(void)
{
    test_decimal_relations();
    test_operands();
    test_invalid();
    test_pinned_grammar();
#ifdef CONFIG_INTL
    test_resource_adapter();
    test_resource_fallback_and_scaling();
#endif
    puts("exact Intl plural tests passed");
    return 0;
}
