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
/* Exact decimal plural operands and rule grammar; no ICU dependency. */
#include "../src/intl/plural/rules.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(const char *rule, const char *number, int32_t exponent, int expected)
{
    QJSIntlPluralOperands operands = {number, strlen(number), exponent};
    int matches = -1;
    assert(qjs_intl_plural_rule_evaluate((QJSIntlBytes){rule, strlen(rule)}, &operands, &matches) ==
           0);
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
    check("", "0", 0, 1);
    check(" \t\r\n", "2.000", 0, 1);
    check("\v\fi\xc2\x85=\xe2\x80\x8e"
          "1\xe2\x80\x8f",
          "1", 0, 1);
    check("i\xe2\x80\xa8=\xe2\x80\xa9"
          "1",
          "1", 0, 1);
    check("i = 1or i = 2", "1", 0, 1);
    check("imod10=1", "21", 0, 1);
    check("iis1", "1", 0, 1);
    check("iisnot1", "2", 0, 1);
    check("iin1", "1", 0, 1);
    check("i=2orv=0", "1", 0, 1);
    check("nnotwithin1..2", "2.5", 0, 1);
    check("n = 1", "0001.000", 0, 1);
    check("n = 1", "1.001", 0, 0);
    check("n is 1", "1.000", 0, 1);
    check("n is 1", "1.001", 0, 0);
    check("n is not 1", "1.001", 0, 1);
    check("n within 1..2", "1.500", 0, 1);
    check("n in 1..2", "1.500", 0, 0);
    check("n not in 1..2", "1.500", 0, 1);
    check("n % 10 within 1..2", "10000000000000000000001.500", 0, 1);
    check("n % 10 within 1..2", "10000000000000000000002.001", 0, 0);
    check("v = 4 and w = 2 and f = 1200 and t = 12", "1.1200", 0, 1);
    check("v = 4 and w = 0 and f = 0 and t = 0", "1.0000", 0, 1);
    check("v = 19 and w = 17 and f % 100 = 0 and t % 100 = 21", "0.9007199254740992100", 0, 1);
    check("i = 1 and e = 0 and c = 0", "1.00", 0, 1);
    check("n = 1000000 and v = 1 and e = 6 and c = 6", "1000000.0", 6, 1);
    check("e not in 0..5 and c != 0..5", "0.001", -3, 1);
    check("e mod 10 is not 1", "1", INT32_MIN, 1);
    check("e mod 10 is 0 and c % 10 = 0", "1", -20, 1);
    check("e mod 10 is not 1", "1", -21, 1);
    check("i mod 4294967295 is 1", "4294967296", 0, 1);
    check("i is not 4294967295", "4294967296", 0, 1);
    check("i = 4294967295", "4294967295", 0, 1);
    check("i % 4294967295 = 0", "4294967295", 0, 1);
    check("i = 1,3..5,7 and v = 0", "5", 0, 1);
    check("i = 1 or i = 2 and v = 0", "1.5", 0, 1);
    check("i = 1 or i = 2 and v = 0", "2.5", 0, 0);
    check("n = 1000 and i = 1000 and v = 1 and w = 0 and f = 0 and t = 0 and e = 3 and c = 3",
          "1000.0", 3, 1);
    check("i = 1200 and f = 5 and v = 1 and c = 3", "1200.5", 3, 1);
    check("n within 0..1 and i = 0 and v = 5 and w = 3 and f = 100 and t = 1 and e not in 0..5",
          "0.00100", -3, 1);
    check("e % 10 is not 3 and c not in 0..5", "0.00100", -3, 1);
}

static void test_invalid(void)
{
    static const char *const rules[] = {"x = 0",
                                        "i > 0",
                                        "i mod % 10 = 1",
                                        "i % 0 = 0",
                                        "i = 1 or",
                                        "i = 2..1",
                                        "i = 1,,2",
                                        "i is 1..2",
                                        "i = 4294967296",
                                        "i = 1 garbage",
                                        "i and v = 0",
                                        "n within 0.5..1",
                                        "n = 1 @integer 1",
                                        "i = 1 or x = 0",
                                        "i = 2 and x = 0",
                                        "i = 1 andy = 2",
                                        "i\xc2\xa0= 1",
                                        "i = 1\xc2",
                                        "i = 1\xe2\x80"};
    static const char *const decimals[] = {"", "-1", "1e3", ".5", "1.", "1..0"};
    QJSIntlPluralOperands number = {"1", 1, 0};
    int matches;
    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); i++) {
        matches = 42;
        assert(qjs_intl_plural_rule_evaluate((QJSIntlBytes){rules[i], strlen(rules[i])}, &number,
                                             &matches) == QJS_INTL_DATA_ERROR);
        assert(matches == 0);
    }
    for (size_t i = 0; i < sizeof(decimals) / sizeof(decimals[0]); i++) {
        number.decimal = decimals[i];
        number.length = strlen(decimals[i]);
        assert(qjs_intl_plural_rule_evaluate((QJSIntlBytes){"n = 1", 5}, &number, &matches) ==
               QJS_INTL_INVALID_ARGUMENT);
    }
    number.decimal = "1";
    number.length = 1;
    assert(qjs_intl_plural_rule_evaluate((QJSIntlBytes){"\0 = 0", 5}, &number, &matches) ==
           QJS_INTL_DATA_ERROR);
    assert(qjs_intl_plural_rule_evaluate((QJSIntlBytes){"n = 1\0", 6}, &number, &matches) ==
           QJS_INTL_DATA_ERROR);
    assert(qjs_intl_plural_rule_evaluate((QJSIntlBytes){"n = 1", 5}, NULL, &matches) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_plural_rule_evaluate((QJSIntlBytes){"n = 1", 5}, &number, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_plural_rule_evaluate((QJSIntlBytes){NULL, 0}, &number, &matches) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_plural_rule_evaluate((QJSIntlBytes){"", SIZE_MAX}, &number, &matches) ==
           QJS_INTL_INVALID_ARGUMENT);
    number.length = SIZE_MAX;
    assert(qjs_intl_plural_rule_evaluate((QJSIntlBytes){"n = 1", 5}, &number, &matches) ==
           QJS_INTL_INVALID_ARGUMENT);
    number.decimal = "1\0";
    number.length = 2;
    assert(qjs_intl_plural_rule_evaluate((QJSIntlBytes){"n = 1", 5}, &number, &matches) ==
           QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    test_decimal_relations();
    test_operands();
    test_invalid();
    puts("exact Intl plural tests passed");
    return 0;
}
