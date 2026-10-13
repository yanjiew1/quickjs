/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/decimal.h"
#include <assert.h>
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

int main(void)
{
    QJSIntlAllocator allocator = {NULL, allocate, NULL, release};
    test_rounding(&allocator);
    return 0;
}
