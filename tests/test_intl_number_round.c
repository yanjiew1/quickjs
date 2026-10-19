/* Exact scale-before-round policy, carries, signs and allocation failures.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/number/round.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        state->live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static QJSIntlMathematicalValue value(const char *text)
{
    return (QJSIntlMathematicalValue){QJS_INTL_FINITE, {text, strlen(text)}};
}

static QJSIntlNumberRounding options(void)
{
    QJSIntlNumberRounding result = {0};
    result.digits = (QJSIntlDecimalOptions){1,
                                            0,
                                            2,
                                            1,
                                            21,
                                            1,
                                            QJS_INTL_ROUND_HALF_EXPAND,
                                            QJS_INTL_ROUND_FRACTION,
                                            QJS_INTL_TRAILING_ZERO_AUTO,
                                            4096};
    return result;
}

static void check_value(const QJSIntlAllocator *allocator, const QJSIntlNumberRounding *settings,
                        QJSIntlMathematicalValue input, const char *expected, int32_t exponent,
                        int negative)
{
    QJSIntlRoundedNumber result;
    assert(!qjs_intl_number_round(allocator, settings, &input, &result));
    assert(result.exponent == exponent && result.number.rounded.negative == negative);
    assert(result.number.length == strlen(expected) && !strcmp(result.number.text, expected));
    assert(result.kind ==
           (negative && result.number.rounded.length == 1 && result.number.rounded.digits[0] == '0'
                ? QJS_INTL_NEGATIVE_ZERO
                : QJS_INTL_FINITE));
    qjs_intl_number_round_clear(allocator, &result);
    assert(!result.number.text && !result.number.rounded.digits && !result.exponent);
    qjs_intl_number_round_clear(allocator, &result);
}

static void check(const QJSIntlAllocator *allocator, const QJSIntlNumberRounding *settings,
                  const char *input, const char *expected, int32_t exponent, int negative)
{
    check_value(allocator, settings, value(input), expected, exponent, negative);
}

static void test_notations(const QJSIntlAllocator *allocator)
{
    QJSIntlNumberRounding settings = options();
    static const QJSIntlCompactScale scales[] = {{3, 3}, {4, 3}, {5, 3}, {6, 6}, {9, 9}};
    static const QJSIntlCompactScale fallback[] = {{3, 0}, {4, 3}};
    check(allocator, &settings, "1234.567", "1234.57", 0, 0);
    check(allocator, &settings, "-0.001", "0", 0, 1);
    check(allocator, &settings, "-0", "0", 0, 1);
    check(allocator, &settings, "123456789012345678901234567890.125",
          "123456789012345678901234567890.13", 0, 0);
    settings.percent = 1;
    check(allocator, &settings, ".123456", "12.35", 0, 0);
    check(allocator, &settings, "-.001", "0.1", 0, 1);
    settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    check(allocator, &settings, "1234.567", "1.23", 5, 0);
    settings.percent = 0;
    check(allocator, &settings, "1234.567", "1.23", 3, 0);
    check(allocator, &settings, "0.0001234567", "1.23", -4, 0);
    check(allocator, &settings, "-1234.567", "1.23", 3, 1);
    check(allocator, &settings, "1e1000000000", "1", 1000000000, 0);
    check(allocator, &settings, "1e-1000000000", "1", -1000000000, 0);
    settings.percent = 1;
    check(allocator, &settings, "1e1000000000", "1", 1000000002, 0);
    settings.percent = 0;
    settings.notation = QJS_INTL_NUMBER_ENGINEERING;
    check(allocator, &settings, "1234.567", "1.23", 3, 0);
    check(allocator, &settings, "12.34567", "12.35", 0, 0);
    check(allocator, &settings, "0.0001234567", "123.46", -6, 0);
    check(allocator, &settings, "0.01234567", "12.35", -3, 0);
    check(allocator, &settings, "1e-1000000000", "100", -1000000002, 0);
    settings.digits.maximum_fraction_digits = 1;
    check(allocator, &settings, "999.95", "1", 3, 0);
    settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    check(allocator, &settings, "999.5", "1", 3, 0);
    settings.notation = QJS_INTL_NUMBER_COMPACT;
    settings.compact = scales;
    settings.compact_count = sizeof(scales) / sizeof(scales[0]);
    check(allocator, &settings, "999.95", "1", 3, 0);
    check(allocator, &settings, "12345", "12.3", 3, 0);
    check(allocator, &settings, "1234567", "1.2", 6, 0);
    check(allocator, &settings, "0.1234", "0.1", 0, 0);
    settings.digits.maximum_fraction_digits = 0;
    check(allocator, &settings, "999499", "999", 3, 0);
    check(allocator, &settings, "999500", "1", 6, 0);
    check(allocator, &settings, "1000000000000000", "1000000", 9, 0);
    settings.compact = fallback;
    settings.compact_count = sizeof(fallback) / sizeof(fallback[0]);
    check(allocator, &settings, "1000", "1000", 0, 0);
    check(allocator, &settings, "9999.5", "10", 3, 0);
    settings.compact = NULL;
    settings.compact_count = 0;
    check(allocator, &settings, "1234.5", "1235", 0, 0);
    settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    settings.digits.rounding_type = QJS_INTL_ROUND_SIGNIFICANT;
    settings.digits.minimum_significant_digits = settings.digits.maximum_significant_digits = 3;
    check(allocator, &settings, "999.95", "1.00", 3, 0);
    settings.digits.minimum_integer_digits = 4;
    check(allocator, &settings, "1234", "0001.23", 3, 0);
    settings.digits.trailing_zero_display = QJS_INTL_TRAILING_ZERO_STRIP_IF_INTEGER;
    check(allocator, &settings, "999.95", "0001", 3, 0);
    settings = options();
    settings.percent = 1;
    settings.digits.minimum_fraction_digits = settings.digits.maximum_fraction_digits = 2;
    settings.digits.rounding_increment = 25;
    check(allocator, &settings, "0.0126", "1.25", 0, 0);
}

static void test_modes(const QJSIntlAllocator *allocator)
{
    QJSIntlNumberRounding settings = options();
    int mode;
    settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    settings.digits.maximum_fraction_digits = 0;
    for (mode = QJS_INTL_ROUND_CEIL; mode <= QJS_INTL_ROUND_HALF_EVEN; mode++) {
        const char *positive = "1", *negative = "1";
        settings.digits.rounding_mode = mode;
        if (mode == QJS_INTL_ROUND_CEIL || mode == QJS_INTL_ROUND_EXPAND ||
            mode == QJS_INTL_ROUND_HALF_CEIL || mode == QJS_INTL_ROUND_HALF_EXPAND ||
            mode == QJS_INTL_ROUND_HALF_EVEN)
            positive = "2";
        if (mode == QJS_INTL_ROUND_FLOOR || mode == QJS_INTL_ROUND_EXPAND ||
            mode == QJS_INTL_ROUND_HALF_FLOOR || mode == QJS_INTL_ROUND_HALF_EXPAND ||
            mode == QJS_INTL_ROUND_HALF_EVEN)
            negative = "2";
        check(allocator, &settings, "150", positive, 2, 0);
        check(allocator, &settings, "-150", negative, 2, 1);
    }
    /* ComputeExponent probes abs(x), before the final signed rounding. */
    settings.digits.rounding_mode = QJS_INTL_ROUND_CEIL;
    check(allocator, &settings, "-9.99", "0", 1, 1);
    settings.digits.rounding_mode = QJS_INTL_ROUND_FLOOR;
    check(allocator, &settings, "-9.99", "10", 0, 1);
    settings.digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
    check_value(allocator, &settings, (QJSIntlMathematicalValue){QJS_INTL_NEGATIVE_ZERO, {0}}, "0",
                0, 1);
    check(allocator, &settings, "0", "0", 0, 0);
}

static void rejected(const QJSIntlAllocator *allocator, const QJSIntlNumberRounding *settings,
                     const QJSIntlMathematicalValue *input, QJSIntlStatus expected)
{
    QJSIntlRoundedNumber result, zero = {0};
    Allocation *state = allocator->opaque;
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_number_round(allocator, settings, input, &result) == expected);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state->live);
}

static void test_errors(const QJSIntlAllocator *allocator)
{
    QJSIntlNumberRounding settings = options(), invalid;
    QJSIntlMathematicalValue input = value("1234.5"), nonfinite = {QJS_INTL_NAN, {0}};
    QJSIntlRoundedNumber result;
    Allocation *state = allocator->opaque;
    static const QJSIntlCompactScale descending[] = {{4, 3}, {3, 3}};
    static const QJSIntlCompactScale bad_scale[] = {{3, 4}};
    size_t calls, failures;
    int kind;
    for (kind = QJS_INTL_NAN; kind <= QJS_INTL_NEGATIVE_INFINITY; kind++) {
        calls = state->calls;
        nonfinite.kind = kind;
        assert(!qjs_intl_number_round(allocator, &settings, &nonfinite, &result));
        assert(result.kind == (QJSIntlMathematicalKind)kind && !result.exponent && !result.number.text);
        assert(calls == state->calls);
        qjs_intl_number_round_clear(allocator, &result);
    }
    invalid = settings;
    invalid.notation = -1;
    rejected(allocator, &invalid, &input, QJS_INTL_INVALID_ARGUMENT);
    invalid = settings;
    invalid.percent = 2;
    rejected(allocator, &invalid, &input, QJS_INTL_INVALID_ARGUMENT);
    invalid = settings;
    invalid.digits.rounding_increment = 3;
    rejected(allocator, &invalid, &input, QJS_INTL_INVALID_ARGUMENT);
    invalid = settings;
    invalid.compact_count = 1;
    rejected(allocator, &invalid, &input, QJS_INTL_INVALID_ARGUMENT);
    invalid = settings;
    invalid.compact = bad_scale;
    invalid.compact_count = 1;
    rejected(allocator, &invalid, &input, QJS_INTL_INVALID_ARGUMENT);
    invalid.notation = QJS_INTL_NUMBER_COMPACT;
    rejected(allocator, &invalid, &input, QJS_INTL_INVALID_ARGUMENT);
    invalid.compact = descending;
    invalid.compact_count = 2;
    rejected(allocator, &invalid, &input, QJS_INTL_INVALID_ARGUMENT);
    rejected(allocator, NULL, &input, QJS_INTL_INVALID_ARGUMENT);
    rejected(allocator, &settings, NULL, QJS_INTL_INVALID_ARGUMENT);
    nonfinite.kind = -1;
    rejected(allocator, &settings, &nonfinite, QJS_INTL_INVALID_ARGUMENT);
    input = value("broken");
    rejected(allocator, &settings, &input, QJS_INTL_INVALID_ARGUMENT);
    input = value("1e1000000000");
    settings.percent = 1;
    rejected(allocator, &settings, &input, QJS_INTL_OVERFLOW);
    settings = options();
    settings.digits.maximum_output_length = 4;
    input = value("999.95");
    rejected(allocator, &settings, &input, QJS_INTL_OVERFLOW);
    settings = options();
    settings.notation = QJS_INTL_NUMBER_SCIENTIFIC;
    settings.digits.maximum_fraction_digits = 0;
    input = value("-999.95");
    calls = state->calls;
    assert(!qjs_intl_number_round(allocator, &settings, &input, &result));
    calls = state->calls - calls;
    qjs_intl_number_round_clear(allocator, &result);
    failures = 0;
    while (1) {
        state->fail_at = state->calls + failures + 1;
        memset(&result, 0xff, sizeof(result));
        if (!qjs_intl_number_round(allocator, &settings, &input, &result)) {
            qjs_intl_number_round_clear(allocator, &result);
            break;
        }
        assert(!state->live && !result.number.text && !result.number.rounded.digits);
        failures++;
        assert(failures <= calls);
    }
    assert(failures == calls && !state->live);
    state->fail_at = 0;
    check(allocator, &settings, "-999.95", "1", 3, 1);
    assert(qjs_intl_number_round(NULL, &settings, &input, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_round(allocator, &settings, &input, NULL) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_number_round_clear(allocator, NULL);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    test_notations(&allocator);
    test_modes(&allocator);
    test_errors(&allocator);
    assert(!state.live);
    return 0;
}
