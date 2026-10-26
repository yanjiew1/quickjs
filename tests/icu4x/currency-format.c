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
#include "../../src/icu4x/currency-format.h"
#include <assert.h>
#include <string.h>

static void name(const char *locale, const char *code, unsigned category, int exact,
                 const char *expected)
{
    QJSICU4XCurrencyNameHandle *handle;
    QJSICU4XCurrencyName result;
    assert(!qjs_icu4x_currency_name_open((const uint8_t *)locale, strlen(locale),
                                         (const uint8_t *)code, 3, &handle));
    assert(!qjs_icu4x_currency_name(handle, category, exact, &result));
    assert(result.name.length == strlen(expected));
    assert(!memcmp(result.name.data, expected, result.name.length));
    assert(result.first <= 1);
    qjs_icu4x_currency_name_close(handle);
}

int main(void)
{
    QJSICU4XCurrencyNameHandle *handle, *failed = (QJSICU4XCurrencyNameHandle *)(uintptr_t)1;
    QJSICU4XCurrencyName result;
    unsigned category;

    name("en", "USD", 1, -1, "US dollar");
    name("en", "USD", 5, -1, "US dollars");
    /* Artificial selectors must not reclassify an amount: the exact evaluator
       chooses the category independently of explicit 0/1 metadata lookup. */
    name("en", "USD", 1, 0, "US dollar");
    name("en", "USD", 5, 1, "US dollars");
    name("en", "ZZZ", 1, -1, "ZZZ");
    name("en", "ZZZ", 5, 0, "ZZZ");
    /* The public bake retains the singular even though raw XML has no other
       entry. Do not infer a missing payload solely from source-provider code. */
    name("yrl", "CLF", 1, -1, "Yepesawa Muapiresawa Xiriwara");
    name("yrl", "CLF", 5, -1, "Yepesawa-ita Muapiresawa Xiriwara");
    assert(!qjs_icu4x_currency_name_open((const uint8_t *)"en", 2, (const uint8_t *)"USD", 3,
                                         &handle));
    for (category = 0; category < 6; category++) {
        assert(!qjs_icu4x_currency_name(handle, category, -1, &result));
        assert(result.first == 0 && !result.literals[0].length && !result.literals[2].length);
        assert(result.literals[1].length == 1 && result.literals[1].data[0] == ' ');
    }
    memset(&result, 0x5a, sizeof(result));
    assert(qjs_icu4x_currency_name(handle, 6, -1, &result) == QJS_ICU4X_RANGE);
    assert(result.first == 0x5a5a5a5a);
    assert(qjs_icu4x_currency_name(handle, 0, 2, &result) == QJS_ICU4X_RANGE);
    assert(result.first == 0x5a5a5a5a);
    assert(qjs_icu4x_currency_name(NULL, 0, -1, &result) == QJS_ICU4X_RANGE);
    assert(result.first == 0x5a5a5a5a);
    assert(qjs_icu4x_currency_name_open((const uint8_t *)"en", 2, (const uint8_t *)"usd", 3,
                                        &failed) == QJS_ICU4X_RANGE);
    assert(failed == (QJSICU4XCurrencyNameHandle *)(uintptr_t)1);
    assert(qjs_icu4x_currency_name_open(NULL, 0, (const uint8_t *)"USD", 3, &failed) ==
           QJS_ICU4X_RANGE);
    assert(failed == (QJSICU4XCurrencyNameHandle *)(uintptr_t)1);
    qjs_icu4x_currency_name_close(handle);
    qjs_icu4x_currency_name_close(NULL);
    return 0;
}
