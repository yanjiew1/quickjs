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
#include "../../src/icu4x/currency-number.h"
#include <assert.h>
#include <string.h>

static void precision(const char *code, uint32_t expected)
{
    uint32_t digits = UINT32_MAX;
    assert(!qjs_icu4x_currency_digits((const uint8_t *)code, strlen(code), &digits));
    assert(digits == expected);
}

static void symbol(const char *locale, const char *code, unsigned narrow, const char *expected,
                   unsigned first, unsigned last)
{
    QJSICU4XCurrencySymbolHandle *handle = NULL;
    QJSICU4XCurrencySymbol result;
    size_t length = expected ? strlen(expected) : 0;

    assert(!qjs_icu4x_currency_symbol_open((const uint8_t *)locale, strlen(locale),
                                           (const uint8_t *)code, strlen(code), narrow, &handle));
    assert(!qjs_icu4x_currency_symbol(handle, &result));
    assert(result.found == !!expected);
    assert(result.length == length);
    if (expected) {
        assert(!length || !memcmp(result.text, expected, length));
        assert(result.starts_with_letter == first && result.ends_with_letter == last);
    } else {
        assert(!result.text && !result.starts_with_letter && !result.ends_with_letter);
    }
    qjs_icu4x_currency_symbol_close(handle);
}

static void currency_decimal(void)
{
    QJSICU4XCurrencyDecimalHandle *handle = NULL;
    QJSICU4XCurrencyDecimal result;
    size_t i;
    static const char *const systems[] = {"", "latn"};

    /* PTE changes decimal/group separators even though its localized symbol
       is explicitly empty. Retrieve metadata without formatting an amount. */
    for (i = 0; i < 2; i++) {
        assert(!qjs_icu4x_currency_decimal_open((const uint8_t *)"pt-PT", 5,
                                                (const uint8_t *)systems[i], strlen(systems[i]),
                                                (const uint8_t *)"PTE", 3, &handle));
        assert(!qjs_icu4x_currency_decimal(handle, &result));
        assert(result.decimal.length == 1 && result.decimal.data[0] == '$');
        assert(result.group.length == 1 && result.group.data[0] == ',');
        assert(result.primary == 3 && result.secondary == 3 && result.minimum);
        qjs_icu4x_currency_decimal_close(handle);
    }
    handle = (QJSICU4XCurrencyDecimalHandle *)(uintptr_t)1;
    /* The unqualified PTE record is Latin. It must not masquerade as an
       exact Arabic-system override through an unrelated default-system fallback. */
    assert(qjs_icu4x_currency_decimal_open((const uint8_t *)"pt-PT", 5,
                                           (const uint8_t *)"arab", 4,
                                           (const uint8_t *)"PTE", 3, &handle) == QJS_ICU4X_UNSUPPORTED);
    assert(handle == (QJSICU4XCurrencyDecimalHandle *)(uintptr_t)1);
    assert(qjs_icu4x_currency_decimal_open((const uint8_t *)"pt-PT", 5, (const uint8_t *)"latn", 4,
                                           (const uint8_t *)"EUR", 3,
                                           &handle) == QJS_ICU4X_UNSUPPORTED);
    assert(handle == (QJSICU4XCurrencyDecimalHandle *)(uintptr_t)1);
    assert(qjs_icu4x_currency_decimal_open((const uint8_t *)"pt-PT", 5, (const uint8_t *)"bad--", 5,
                                           (const uint8_t *)"PTE", 3, &handle) == QJS_ICU4X_RANGE);
    assert(handle == (QJSICU4XCurrencyDecimalHandle *)(uintptr_t)1);
    memset(&result, 0x5a, sizeof(result));
    assert(qjs_icu4x_currency_decimal(NULL, &result) == QJS_ICU4X_RANGE);
    assert(result.primary == 0x5a5a5a5a);
    qjs_icu4x_currency_decimal_close(NULL);
}

int main(void)
{
    QJSICU4XCurrencySymbolHandle *handle = (QJSICU4XCurrencySymbolHandle *)(uintptr_t)1;
    QJSICU4XCurrencySymbol result;
    uint32_t digits = UINT32_MAX;

    currency_decimal();
    precision("USD", 2);
    precision("JPY", 0);
    precision("KWD", 3);
    precision("CLF", 4);
    precision("ZZZ", 2);
    /* Cash rounding/digits must not leak into CurrencyDigits. */
    precision("CHF", 2);
    precision("SEK", 2);
    precision("HUF", 0);
    symbol("en", "USD", 0, "$", 0, 0);
    symbol("en-US", "USD", 0, "$", 0, 0);
    symbol("en", "CAD", 0, "CA$", 1, 0);
    symbol("en", "CAD", 1, "$", 0, 0);
    symbol("en", "ZZZ", 0, NULL, 0, 0);
    /* Public ICU4X normalizes the CLDR zero-width PTE symbol to an explicit
       empty payload. It must stay distinct from the absent ZZZ entry. */
    symbol("pt-PT", "PTE", 0, "", 0, 0);
    symbol("pt-PT", "PTE", 1, "", 0, 0);
    assert(qjs_icu4x_currency_digits((const uint8_t *)"usd", 3, &digits) == QJS_ICU4X_RANGE);
    assert(digits == UINT32_MAX);
    assert(qjs_icu4x_currency_digits(NULL, 3, &digits) == QJS_ICU4X_RANGE);
    assert(digits == UINT32_MAX);
    assert(qjs_icu4x_currency_symbol_open((const uint8_t *)"en", 2, (const uint8_t *)"USD", 3, 2,
                                          &handle) == QJS_ICU4X_RANGE);
    assert(handle == (QJSICU4XCurrencySymbolHandle *)(uintptr_t)1);
    memset(&result, 0x5a, sizeof(result));
    assert(qjs_icu4x_currency_symbol(NULL, &result) == QJS_ICU4X_RANGE);
    assert(result.found == 0x5a5a5a5a);
    qjs_icu4x_currency_symbol_close(NULL);
    return 0;
}
