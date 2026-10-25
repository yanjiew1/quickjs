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
#include "../src/intl/common/decimal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *allocate(void *opaque, size_t size)
{
    (void)opaque;
    return malloc(size);
}
static void release(void *opaque, void *ptr)
{
    (void)opaque;
    free(ptr);
}

static void test_rounding(const QJSIntlAllocator *allocator)
{
    static const struct {
        const char *input;
        QJSIntlRoundingMode mode;
        const char *expected;
    } cases[] = {
        {"1.25", QJS_INTL_ROUND_CEIL, "1.3"},
        {"1.25", QJS_INTL_ROUND_FLOOR, "1.2"},
        {"1.25", QJS_INTL_ROUND_EXPAND, "1.3"},
        {"1.25", QJS_INTL_ROUND_TRUNC, "1.2"},
        {"1.25", QJS_INTL_ROUND_HALF_CEIL, "1.3"},
        {"-1.25", QJS_INTL_ROUND_HALF_CEIL, "1.2"},
        {"1.25", QJS_INTL_ROUND_HALF_FLOOR, "1.2"},
        {"-1.25", QJS_INTL_ROUND_HALF_FLOOR, "1.3"},
        {"1.25", QJS_INTL_ROUND_HALF_EXPAND, "1.3"},
        {"1.25", QJS_INTL_ROUND_HALF_TRUNC, "1.2"},
        {"1.25", QJS_INTL_ROUND_HALF_EVEN, "1.2"},
        {"1.35", QJS_INTL_ROUND_HALF_EVEN, "1.4"},
        {"-0.04", QJS_INTL_ROUND_HALF_EXPAND, "0.0"},
        {"-1.21", QJS_INTL_ROUND_CEIL, "1.2"},
        {"-1.21", QJS_INTL_ROUND_FLOOR, "1.3"},
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        QJSIntlDecimal input;
        QJSIntlDecimalResult output;
        QJSIntlBytes text = {cases[i].input, strlen(cases[i].input)};
        assert(qjs_intl_decimal_parse(allocator, text, &input) == QJS_INTL_OK);
        assert(qjs_intl_decimal_to_raw_fixed(allocator, &input, 1, 1, 1,
                    cases[i].mode, 64, &output) == QJS_INTL_OK);
        assert(!strcmp(output.text, cases[i].expected));
        assert(output.rounded.negative == (cases[i].input[0] == '-'));
        qjs_intl_decimal_result_clear(allocator, &output);
        qjs_intl_decimal_clear(allocator, &input);
    }
    {
        QJSIntlDecimal input;
        QJSIntlDecimalResult output;
        QJSIntlBytes text = {"12345", 5};
        assert(qjs_intl_decimal_parse(allocator, text, &input) == QJS_INTL_OK);
        assert(qjs_intl_decimal_to_raw_precision(allocator, &input, 3, 3,
                    QJS_INTL_ROUND_HALF_EXPAND, 64, &output) == QJS_INTL_OK);
        assert(!strcmp(output.text, "12300"));
        qjs_intl_decimal_result_clear(allocator, &output);
        assert(qjs_intl_decimal_to_raw_fixed(allocator, &input, 0, 0, 1,
                    QJS_INTL_ROUND_TRUNC, 2, &output) == QJS_INTL_OVERFLOW);
        assert(!output.text && !output.rounded.digits);
        qjs_intl_decimal_result_clear(allocator, &output);
        qjs_intl_decimal_clear(allocator, &input);
    }
}

static QJSIntlDecimal parse(const QJSIntlAllocator *allocator, const char *text)
{
    QJSIntlDecimal value;
    QJSIntlBytes bytes = {text, strlen(text)};

    assert(qjs_intl_decimal_validate(bytes) == QJS_INTL_OK);
    assert(qjs_intl_decimal_parse(allocator, bytes, &value) == QJS_INTL_OK);
    return value;
}

static void test_increments(const QJSIntlAllocator *allocator)
{
    static const unsigned int increments[] = {
        1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000,
    };
    char input_text[32], expected[32];
    size_t i;

    for (i = 0; i < sizeof(increments) / sizeof(increments[0]); i++) {
        unsigned int increment = increments[i];
        unsigned int twice_input = increment * 5;
        QJSIntlDecimal input;
        QJSIntlDecimalResult output;

        /* Exactly halfway between two even/odd multiples, including odd
           increments whose midpoint has a fractional decimal digit. */
        snprintf(input_text, sizeof(input_text), "%u.%u",
                 twice_input / 2, twice_input % 2 * 5);
        input = parse(allocator, input_text);
        snprintf(expected, sizeof(expected), "%u", increment * 2);
        assert(qjs_intl_decimal_to_raw_fixed(allocator, &input, 0, 0, increment,
                                            QJS_INTL_ROUND_HALF_EVEN, 32,
                                            &output) == QJS_INTL_OK);
        assert(!strcmp(output.text, expected));
        qjs_intl_decimal_result_clear(allocator, &output);
        snprintf(expected, sizeof(expected), "%u", increment * 3);
        assert(qjs_intl_decimal_to_raw_fixed(allocator, &input, 0, 0, increment,
                                            QJS_INTL_ROUND_HALF_EXPAND, 32,
                                            &output) == QJS_INTL_OK);
        assert(!strcmp(output.text, expected));
        qjs_intl_decimal_result_clear(allocator, &output);
        qjs_intl_decimal_clear(allocator, &input);
        snprintf(input_text, sizeof(input_text), "%u.%u",
                 increment * 7 / 2, increment * 7 % 2 * 5);
        input = parse(allocator, input_text);
        snprintf(expected, sizeof(expected), "%u", increment * 4);
        assert(qjs_intl_decimal_to_raw_fixed(allocator, &input, 0, 0, increment,
                                            QJS_INTL_ROUND_HALF_EVEN, 32,
                                            &output) == QJS_INTL_OK);
        assert(!strcmp(output.text, expected));
        qjs_intl_decimal_result_clear(allocator, &output);
        qjs_intl_decimal_clear(allocator, &input);
    }
}

static void test_format(const QJSIntlAllocator *allocator)
{
    QJSIntlDecimalOptions options = {
        1, 2, 2, 2, 2, 1, QJS_INTL_ROUND_HALF_EXPAND,
        QJS_INTL_ROUND_MORE_PRECISION, QJS_INTL_TRAILING_ZERO_AUTO, 128,
    };
    QJSIntlDecimal input = parse(allocator, "1");
    QJSIntlDecimalResult output;

    assert(qjs_intl_decimal_format_numeric(allocator, &input, &options,
                                          &output) == QJS_INTL_OK);
    assert(!strcmp(output.text, "1.00"));
    qjs_intl_decimal_result_clear(allocator, &output);
    options.rounding_type = QJS_INTL_ROUND_LESS_PRECISION;
    assert(qjs_intl_decimal_format_numeric(allocator, &input, &options,
                                          &output) == QJS_INTL_OK);
    assert(!strcmp(output.text, "1.0"));
    qjs_intl_decimal_result_clear(allocator, &output);
    options.minimum_integer_digits = 3;
    options.trailing_zero_display = QJS_INTL_TRAILING_ZERO_STRIP_IF_INTEGER;
    assert(qjs_intl_decimal_format_numeric(allocator, &input, &options,
                                          &output) == QJS_INTL_OK);
    assert(!strcmp(output.text, "001"));
    qjs_intl_decimal_result_clear(allocator, &output);
    qjs_intl_decimal_clear(allocator, &input);

    input = parse(allocator, "1");
    options.minimum_integer_digits = 1;
    options.minimum_fraction_digits = 0;
    options.maximum_fraction_digits = 1;
    options.trailing_zero_display = QJS_INTL_TRAILING_ZERO_AUTO;
    options.rounding_type = QJS_INTL_ROUND_MORE_PRECISION;
    assert(qjs_intl_decimal_format_numeric(allocator, &input, &options,
                                          &output) == QJS_INTL_OK);
    assert(!strcmp(output.text, "1.0"));
    qjs_intl_decimal_result_clear(allocator, &output);
    options.rounding_type = QJS_INTL_ROUND_LESS_PRECISION;
    assert(qjs_intl_decimal_format_numeric(allocator, &input, &options,
                                          &output) == QJS_INTL_OK);
    assert(!strcmp(output.text, "1"));
    qjs_intl_decimal_result_clear(allocator, &output);
    qjs_intl_decimal_clear(allocator, &input);

    input = parse(allocator, "-0");
    assert(qjs_intl_decimal_to_raw_precision(allocator, &input, 3, 3,
                                            QJS_INTL_ROUND_HALF_EVEN, 4,
                                            &output) == QJS_INTL_OK);
    assert(!strcmp(output.text, "0.00") && output.rounded.negative);
    qjs_intl_decimal_result_clear(allocator, &output);
    assert(qjs_intl_decimal_to_raw_precision(allocator, &input, 3, 3,
                                            QJS_INTL_ROUND_HALF_EVEN, 3,
                                            &output) == QJS_INTL_OVERFLOW);
    qjs_intl_decimal_result_clear(allocator, &output);
    qjs_intl_decimal_clear(allocator, &input);

    input = parse(allocator, "0.00012345");
    assert(qjs_intl_decimal_to_raw_precision(allocator, &input, 3, 3,
                                            QJS_INTL_ROUND_HALF_EVEN, 128,
                                            &output) == QJS_INTL_OK);
    assert(!strcmp(output.text, "0.000123"));
    qjs_intl_decimal_result_clear(allocator, &output);
    qjs_intl_decimal_clear(allocator, &input);

    input = parse(allocator, "9.999");
    assert(qjs_intl_decimal_to_raw_precision(allocator, &input, 3, 3,
                                            QJS_INTL_ROUND_HALF_EXPAND, 128,
                                            &output) == QJS_INTL_OK);
    assert(!strcmp(output.text, "10.0"));
    assert(output.rounding_magnitude == -1);
    qjs_intl_decimal_result_clear(allocator, &output);
    qjs_intl_decimal_clear(allocator, &input);

    input = parse(allocator, "9007199254740993123456789.125");
    assert(qjs_intl_decimal_to_raw_fixed(allocator, &input, 2, 2, 1,
                                        QJS_INTL_ROUND_HALF_EVEN, 128,
                                        &output) == QJS_INTL_OK);
    assert(!strcmp(output.text, "9007199254740993123456789.12"));
    qjs_intl_decimal_result_clear(allocator, &output);
    qjs_intl_decimal_clear(allocator, &input);
}

static void test_options(void)
{
    QJSIntlDecimalOptions options = {
        1, 0, 2, 1, 3, 1, QJS_INTL_ROUND_HALF_EVEN,
        QJS_INTL_ROUND_FRACTION, QJS_INTL_TRAILING_ZERO_AUTO, 128,
    };

    assert(qjs_intl_decimal_options_validate(&options) == QJS_INTL_OK);
    options.rounding_increment = 3;
    assert(qjs_intl_decimal_options_validate(&options) == QJS_INTL_INVALID_ARGUMENT);
    options.rounding_increment = 5;
    assert(qjs_intl_decimal_options_validate(&options) == QJS_INTL_INVALID_ARGUMENT);
    options.minimum_fraction_digits = 2;
    assert(qjs_intl_decimal_options_validate(&options) == QJS_INTL_OK);
    options.rounding_type = QJS_INTL_ROUND_SIGNIFICANT;
    assert(qjs_intl_decimal_options_validate(&options) == QJS_INTL_INVALID_ARGUMENT);
    options.rounding_increment = 1;
    options.minimum_significant_digits = 0;
    assert(qjs_intl_decimal_options_validate(&options) == QJS_INTL_INVALID_ARGUMENT);
    options.minimum_significant_digits = 1;
    options.maximum_output_length = 0;
    assert(qjs_intl_decimal_options_validate(&options) == QJS_INTL_INVALID_ARGUMENT);
}

static void test_parse(const QJSIntlAllocator *allocator)
{
    static const char *invalid[] = {
        "", "+", ".", "1.2.3", "1e", "1e+", " 1", "1 ", "0x10", "NaN", "Infinity",
    };
    static const char *overflow[] = {"1e1000000001", "1e-1000000001", "0.1e-1000000000",
                                     "0e1000000001", "0.0e-1000000000", "1e1000000001x"};
    QJSIntlDecimal value;
    QJSIntlDecimalResult output;
    QJSIntlBytes bytes;
    size_t i;

    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        bytes.data = invalid[i];
        bytes.length = strlen(invalid[i]);
        assert(qjs_intl_decimal_parse(allocator, bytes, &value) ==
               QJS_INTL_INVALID_ARGUMENT);
        assert(!value.digits);
        assert(qjs_intl_decimal_validate(bytes) == QJS_INTL_INVALID_ARGUMENT);
    }
    for (i = 0; i < sizeof(overflow) / sizeof(overflow[0]); i++) {
        bytes.data = overflow[i];
        bytes.length = strlen(overflow[i]);
        assert(qjs_intl_decimal_parse(allocator, bytes, &value) == QJS_INTL_OVERFLOW);
        assert(!value.digits);
        assert(qjs_intl_decimal_validate(bytes) == QJS_INTL_OVERFLOW);
    }
    bytes.data = "1\0";
    bytes.length = 2;
    assert(qjs_intl_decimal_parse(allocator, bytes, &value) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_decimal_validate(bytes) == QJS_INTL_INVALID_ARGUMENT);
    bytes = (QJSIntlBytes){"x", (size_t)INT32_MAX + 1};
    assert(qjs_intl_decimal_validate(bytes) == QJS_INTL_OVERFLOW);
    /* Validation and parsing share limits even when zero normalization would
       otherwise erase the exponent. Accepted spelling remains unchanged. */
    {
        static const char *valid[] = {".1", "1.", "+0", "-000.000", "00.10e+2"};
        for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
            bytes = (QJSIntlBytes){valid[i], strlen(valid[i])};
            assert(qjs_intl_decimal_validate(bytes) == QJS_INTL_OK);
        }
    }
    value = parse(allocator, "-000.000");
    assert(value.negative && value.length == 1 && !strcmp(value.digits, "0"));
    qjs_intl_decimal_clear(allocator, &value);
    value = parse(allocator, ".00012e+3");
    assert(value.exponent == -2 && !strcmp(value.digits, "12"));
    qjs_intl_decimal_clear(allocator, &value);
    value = parse(allocator, "1e1000000000");
    assert(qjs_intl_decimal_to_raw_fixed(allocator, &value, 0, 0, 1,
                                        QJS_INTL_ROUND_TRUNC, 128,
                                        &output) == QJS_INTL_OVERFLOW);
    assert(!output.text && !output.rounded.digits);
    qjs_intl_decimal_clear(allocator, &value);
}

typedef struct FailingAllocator {
    size_t calls, fail_at, live;
} FailingAllocator;

static void *fail_allocate(void *opaque, size_t size)
{
    FailingAllocator *state = opaque;
    void *ptr;

    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    assert(ptr);
    state->live++;
    return ptr;
}

static void fail_release(void *opaque, void *ptr)
{
    FailingAllocator *state = opaque;

    if (ptr) {
        assert(state->live);
        state->live--;
        free(ptr);
    }
}

static void test_allocation_failures(void)
{
    FailingAllocator state = {0};
    QJSIntlAllocator allocator = {&state, fail_allocate, NULL, fail_release};
    QJSIntlDecimalOptions options = {
        5, 2, 2, 2, 3, 1, QJS_INTL_ROUND_HALF_EVEN,
        QJS_INTL_ROUND_MORE_PRECISION, QJS_INTL_TRAILING_ZERO_AUTO, 128,
    };
    QJSIntlBytes bytes = {"1.25", 4};
    QJSIntlDecimal value;
    QJSIntlDecimalResult output;
    QJSIntlStatus status;
    size_t fail_at;

    state.fail_at = 1;
    assert(qjs_intl_decimal_parse(&allocator, bytes, &value) == QJS_INTL_NO_MEMORY);
    assert(!value.digits && !state.live);
    state.fail_at = 0;
    value = parse(&allocator, "1.25");
    for (fail_at = 1; ; fail_at++) {
        state.calls = 0;
        state.fail_at = fail_at;
        status = qjs_intl_decimal_format_numeric(&allocator, &value, &options, &output);
        if (status == QJS_INTL_OK) {
            assert(!strcmp(output.text, "00001.25"));
            qjs_intl_decimal_result_clear(&allocator, &output);
            break;
        }
        assert(status == QJS_INTL_NO_MEMORY);
        assert(!output.text && !output.rounded.digits);
        qjs_intl_decimal_result_clear(&allocator, &output);
        assert(state.live == 1);
        assert(fail_at < 10);
    }
    qjs_intl_decimal_clear(&allocator, &value);
    assert(!state.live);
}

int main(void)
{
    QJSIntlAllocator allocator = {NULL, allocate, NULL, release};
    test_rounding(&allocator);
    test_increments(&allocator);
    test_format(&allocator);
    test_options();
    test_parse(&allocator);
    test_allocation_failures();
    return 0;
}
