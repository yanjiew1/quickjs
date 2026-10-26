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
#include "../../src/icu4x/decimal.h"
#include "../../src/intl/number/body.h"
#include "../icu4c/helpers.h"

static QJSICU4XDecimal *open_symbols(const char *locale, const char *system,
                                     QJSICU4XDecimalSymbols *symbols)
{
    QJSICU4XDecimal *handle = NULL;
    assert(!qjs_icu4x_decimal_open((const uint8_t *)locale, strlen(locale), (const uint8_t *)system,
                                   strlen(system), &handle));
    assert(!qjs_icu4x_decimal_symbols(handle, symbols));
    assert(symbols->minimum && symbols->minimum <= 9);
    assert(symbols->primary <= 9 && symbols->secondary <= 9);
    return handle;
}

static void field(QJSICU4XDecimalText text, const char *expected)
{
    assert(text.length == strlen(expected));
    assert(!text.length || !memcmp(text.data, expected, text.length));
}

static void exact_body(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    QJSICU4XDecimalSymbols symbols;
    QJSICU4XDecimal *handle = open_symbols("en", "adlm", &symbols);
    QJSIntlNumberBody body = {0};
    QJSIntlFormatted result;
    size_t i, length = 40000;
    char *input = malloc(length);

    assert(input);
    memset(input, '7', length);
    memcpy(body.digits, symbols.digits, sizeof(body.digits));
    body.primary_grouping = symbols.primary;
    body.secondary_grouping = symbols.secondary;
    body.minimum_grouping_digits = symbols.minimum;
    body.grouping = QJS_INTL_NUMBER_GROUP_OFF;
    body.maximum_output_length = length * 2;
    /* Exact C formatting is not limited by ICU4X's i16 decimal magnitudes.
       Supplementary digits also test UTF-16 lengths rather than UTF-8 bytes. */
    assert(!qjs_intl_number_body_format(&allocator, &body, QJS_INTL_FINITE,
                                        (QJSIntlBytes){input, length}, &result));
    assert(result.length == length * 2 && result.part_count == 1);
    assert(result.parts[0].start == 0 && result.parts[0].end == length * 2);
    assert(result.parts[0].type == QJS_INTL_PART_INTEGER);
    for (i = 0; i < length; i++) {
        assert(result.text[2 * i] == 0xd83a);
        assert(result.text[2 * i + 1] == 0xdd57);
    }
    qjs_icu4x_decimal_close(handle);
    qjs_intl_number_body_clear(&allocator, &result);
    assert(!state.live);
    free(input);
}

int main(void)
{
    QJSICU4XDecimalSymbols symbols;
    QJSICU4XDecimal *handle = open_symbols("en", "", &symbols);
    size_t i;

    field(symbols.fields[0], ".");
    field(symbols.fields[1], ",");
    field(symbols.fields[2], "-");
    field(symbols.fields[4], "+");
    field(symbols.fields[6], "latn");
    assert(symbols.primary == 3 && symbols.secondary == 3 && symbols.minimum == 1);
    for (i = 0; i < 10; i++)
        assert(symbols.digits[i] == '0' + i);
    qjs_icu4x_decimal_close(handle);
    handle = open_symbols("en-IN", "", &symbols);
    assert(symbols.primary == 3 && symbols.secondary == 2);
    qjs_icu4x_decimal_close(handle);
    handle = open_symbols("es", "", &symbols);
    assert(symbols.minimum == 2);
    qjs_icu4x_decimal_close(handle);
    handle = open_symbols("en", "arab", &symbols);
    field(symbols.fields[6], "arab");
    for (i = 0; i < 10; i++)
        assert(symbols.digits[i] == 0x660 + i);
    qjs_icu4x_decimal_close(handle);
    /* Unknown/algorithmic systems may succeed in ICU4X through fallback;
       this exact-capability boundary must reject that silent substitution. */
    handle = (QJSICU4XDecimal *)(uintptr_t)1;
    assert(qjs_icu4x_decimal_open((const uint8_t *)"en", 2, (const uint8_t *)"foobar", 6,
                                  &handle) == QJS_ICU4X_UNSUPPORTED);
    assert(handle == (QJSICU4XDecimal *)(uintptr_t)1);
    assert(qjs_icu4x_decimal_open((const uint8_t *)"en", 2, (const uint8_t *)"roman", 5, &handle) ==
           QJS_ICU4X_UNSUPPORTED);
    assert(handle == (QJSICU4XDecimal *)(uintptr_t)1);
    memset(&symbols, 0x5a, sizeof(symbols));
    assert(qjs_icu4x_decimal_symbols(NULL, &symbols) == QJS_ICU4X_RANGE);
    assert(symbols.primary == 0x5a5a5a5a);
    qjs_icu4x_decimal_close(NULL);
    exact_body();
    return 0;
}
