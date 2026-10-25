/* Standard/compact alpha boundaries and exact legacy S/Z/Nd predicates.
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
#include "../src/intl/currency/boundary.h"
#include "../src/cutils/cutils.h"
#include "../src/unicode/libunicode.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

int main(void)
{
    static const struct {
        const char *text;
        QJSIntlCurrencyBoundary expected;
    } cases[] = {
        {"", {0, 0, 0, 0, 0}},
        {"USD", {1, 1, 1, 1, 1}},
        {"$CA", {1, 0, 1, 1, 0}},
        {"CA$", {0, 1, 0, 0, 1}},
        {"Á", {0, 1, 1, 1, 1}},
        {"Á⃝", {0, 1, 1, 1, 1}},
        {"$́", {0, 0, 0, 1, 0}},
        {"́A", {1, 0, 1, 1, 1}},
        {"字", {1, 1, 1, 1, 1}},
        {"𐐀́", {0, 1, 1, 1, 1}},
        {"ʰ", {1, 1, 1, 1, 1}},
        {"Ⅰ", {0, 0, 0, 1, 1}},
        {"ͅ", {0, 0, 0, 1, 1}},
        {"A؜", {0, 1, 0, 1, 1}},
        {"؜A", {1, 0, 1, 1, 1}},
        {"A ", {0, 1, 0, 0, 1}},
        {" A", {1, 0, 1, 1, 0}},
        {"Fr.", {0, 1, 0, 1, 1}},
    };
    static const char *invalid[] = {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xc2"};
    QJSIntlCurrencyBoundary boundary, zero = {0};
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        QJSIntlBytes text = {cases[i].text, strlen(cases[i].text)};
        assert(!qjs_intl_currency_boundary(text, &boundary));
        assert(!memcmp(&boundary, &cases[i].expected, sizeof(boundary)));
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        memset(&boundary, 0xff, sizeof(boundary));
        assert(qjs_intl_currency_boundary((QJSIntlBytes){invalid[i], strlen(invalid[i])},
                                           &boundary) == QJS_INTL_INVALID_ARGUMENT);
        assert(!memcmp(&boundary, &zero, sizeof(boundary)));
    }
    assert(!qjs_intl_currency_boundary((QJSIntlBytes){0}, &boundary));
    assert(!memcmp(&boundary, &zero, sizeof(boundary)));
    assert(qjs_intl_currency_boundary((QJSIntlBytes){NULL, 1}, &boundary) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_boundary((QJSIntlBytes){"A", (size_t)INT_MAX + 1}, &boundary) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_boundary((QJSIntlBytes){"A", 1}, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_boundary((QJSIntlBytes){"A\0B", 3}, &boundary) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&boundary, &zero, sizeof(boundary)));
    assert(lre_is_decimal_digit('0') && lre_is_decimal_digit('9'));
    assert(lre_is_decimal_digit(0x661) && lre_is_decimal_digit(0x1d7d9));
    assert(!lre_is_decimal_digit('A') && !lre_is_decimal_digit(0x4e00));
    assert(!lre_is_decimal_digit(0x2160) && !lre_is_decimal_digit(0x110000));
    assert(lre_is_symbol_or_separator('$') && lre_is_symbol_or_separator('+'));
    assert(lre_is_symbol_or_separator(' ') && lre_is_symbol_or_separator(0xa0));
    assert(lre_is_symbol_or_separator(0x2028) && lre_is_symbol_or_separator(0x2029));
    assert(!lre_is_symbol_or_separator('.') && !lre_is_symbol_or_separator('A'));
    return 0;
}
