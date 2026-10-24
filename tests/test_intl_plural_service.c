/* Native plural service: exact rounding, notation, ranges and owned lifetime.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/plural/plural.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *result;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    result = malloc(size);
    if (result)
        state->live++;
    return result;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static QJSIntlPluralOptions options(void)
{
    QJSIntlPluralOptions result = {0};
    result.digits.minimum_integer_digits = 1;
    result.digits.maximum_fraction_digits = 3;
    result.digits.minimum_significant_digits = 1;
    result.digits.maximum_significant_digits = 21;
    result.digits.rounding_increment = 1;
    result.digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
    result.digits.maximum_output_length = 8192;
    return result;
}

static QJSIntlMathematicalValue number(const char *text)
{
    return (QJSIntlMathematicalValue){QJS_INTL_FINITE, {text, strlen(text)}};
}

static size_t locale_id(const QJSIntlPluralService *service, const char *text)
{
    const QJSIntlBytes *tags;
    size_t count, i;
    assert(!qjs_intl_plural_available(service, &tags, &count));
    for (i = 0; i < count; i++) {
        if (tags[i].length == strlen(text) && !memcmp(tags[i].data, text, tags[i].length))
            return i;
    }
    abort();
}

static QJSIntlPluralFormatter *open_formatter(const QJSIntlPluralService *service,
                                              const char *locale, QJSIntlPluralOptions *opts)
{
    QJSIntlPluralFormatter *formatter;
    assert(!qjs_intl_plural_formatter_open(service, locale_id(service, locale), opts, &formatter));
    return formatter;
}

static void select_value(const QJSIntlPluralFormatter *formatter, const char *text,
                         QJSIntlPluralCategory expected)
{
    QJSIntlMathematicalValue value = number(text);
    QJSIntlPluralCategory category;
    assert(!qjs_intl_plural_select(formatter, &value, &category));
    if (category != expected)
        fprintf(stderr, "plural selection of %s: expected %u, got %u\n", text,
                (unsigned int)expected, (unsigned int)category);
    assert(category == expected);
}

static void check_service(QJSIntlPluralService *service)
{
    QJSIntlPluralOptions opts = options();
    QJSIntlPluralFormatter *formatter = open_formatter(service, "en", &opts);
    QJSIntlMathematicalValue start = number("1.1"), end = number("1.2");
    QJSIntlPluralCategory category;
    unsigned int mask;
    select_value(formatter, "1", QJS_INTL_PLURAL_ONE);
    select_value(formatter, "-1", QJS_INTL_PLURAL_ONE);
    select_value(formatter, "1.2", QJS_INTL_PLURAL_OTHER);
    select_value(formatter, "1000000000000000000000000000000000000000000000001",
                 QJS_INTL_PLURAL_OTHER);
    assert(!qjs_intl_plural_categories(formatter, &mask));
    assert(mask == ((1u << QJS_INTL_PLURAL_ONE) | (1u << QJS_INTL_PLURAL_OTHER)));
    start.kind = QJS_INTL_NAN;
    assert(!qjs_intl_plural_select(formatter, &start, &category) &&
           category == QJS_INTL_PLURAL_OTHER);
    assert(qjs_intl_plural_select_range(formatter, &start, &end, &category) ==
           QJS_INTL_INVALID_ARGUMENT);
    start.kind = QJS_INTL_POSITIVE_INFINITY;
    end.kind = QJS_INTL_NEGATIVE_INFINITY;
    assert(!qjs_intl_plural_select_range(formatter, &start, &end, &category));
    assert(category == QJS_INTL_PLURAL_OTHER);
    start.kind = QJS_INTL_NEGATIVE_ZERO;
    assert(!qjs_intl_plural_select(formatter, &start, &category) &&
           category == QJS_INTL_PLURAL_OTHER);
    start = number("1");
    end = number("-1");
    assert(!qjs_intl_plural_select_range(formatter, &start, &end, &category));
    assert(category == QJS_INTL_PLURAL_ONE); /* Compare unsigned rounded text. */
    qjs_intl_plural_formatter_close(formatter);

    opts.digits.maximum_fraction_digits = 0;
    formatter = open_formatter(service, "en", &opts);
    start = number("1.1");
    end = number("1.2");
    assert(!qjs_intl_plural_select_range(formatter, &start, &end, &category));
    assert(category == QJS_INTL_PLURAL_ONE); /* Equal after specified rounding. */
    qjs_intl_plural_formatter_close(formatter);
    opts.digits.rounding_mode = QJS_INTL_ROUND_CEIL;
    formatter = open_formatter(service, "en", &opts);
    select_value(formatter, "-1.1", QJS_INTL_PLURAL_ONE);
    select_value(formatter, "1.1", QJS_INTL_PLURAL_OTHER);
    qjs_intl_plural_formatter_close(formatter);
    opts.digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
    opts.digits.minimum_fraction_digits = opts.digits.maximum_fraction_digits = 2;
    formatter = open_formatter(service, "en", &opts);
    select_value(formatter, "1", QJS_INTL_PLURAL_OTHER);
    qjs_intl_plural_formatter_close(formatter);
    opts.type = QJS_INTL_PLURAL_ORDINAL;
    opts.digits.minimum_fraction_digits = 0;
    formatter = open_formatter(service, "en", &opts);
    select_value(formatter, "1", QJS_INTL_PLURAL_ONE);
    select_value(formatter, "2", QJS_INTL_PLURAL_TWO);
    select_value(formatter, "3", QJS_INTL_PLURAL_FEW);
    select_value(formatter, "11", QJS_INTL_PLURAL_OTHER);
    qjs_intl_plural_formatter_close(formatter);

    if (TEST_INTL_PROFILE) {
        opts = options();
        opts.digits.maximum_fraction_digits = 0;
        opts.notation = QJS_INTL_PLURAL_SCIENTIFIC;
        formatter = open_formatter(service, "pl", &opts);
        select_value(formatter, "1202", QJS_INTL_PLURAL_FEW);
        qjs_intl_plural_formatter_close(formatter);
        opts.notation = QJS_INTL_PLURAL_ENGINEERING;
        formatter = open_formatter(service, "pl", &opts);
        select_value(formatter, "1202", QJS_INTL_PLURAL_FEW);
        qjs_intl_plural_formatter_close(formatter);
        opts.notation = QJS_INTL_PLURAL_COMPACT;
        opts.digits.rounding_type = QJS_INTL_ROUND_MORE_PRECISION;
        opts.digits.maximum_significant_digits = 2;
        formatter = open_formatter(service, "pl", &opts);
        select_value(formatter, "1202", QJS_INTL_PLURAL_FEW);
        qjs_intl_plural_formatter_close(formatter);
        opts = options();
        formatter = open_formatter(service, "fr", &opts);
        select_value(formatter, "1500000", QJS_INTL_PLURAL_OTHER);
        qjs_intl_plural_formatter_close(formatter);
        opts.notation = QJS_INTL_PLURAL_COMPACT;
        formatter = open_formatter(service, "fr", &opts);
        select_value(formatter, "1500000", QJS_INTL_PLURAL_MANY);
        qjs_intl_plural_formatter_close(formatter);
        formatter = open_formatter(service, "it", &opts);
        select_value(formatter, "1000", QJS_INTL_PLURAL_OTHER);
        qjs_intl_plural_formatter_close(formatter);
        opts = options();
        opts.notation = QJS_INTL_PLURAL_SCIENTIFIC;
        opts.digits.maximum_fraction_digits = 4;
        formatter = open_formatter(service, "fr", &opts);
        select_value(formatter, "0.0001", QJS_INTL_PLURAL_ONE);
        qjs_intl_plural_formatter_close(formatter);
        opts.notation = QJS_INTL_PLURAL_ENGINEERING;
        formatter = open_formatter(service, "fr", &opts);
        select_value(formatter, "0.0001", QJS_INTL_PLURAL_ONE);
        qjs_intl_plural_formatter_close(formatter);
        formatter = open_formatter(service, "it", &opts);
        select_value(formatter, "0.1", QJS_INTL_PLURAL_MANY); /* floor(-1 / 3) * 3 = -3. */
        qjs_intl_plural_formatter_close(formatter);
        opts.notation = QJS_INTL_PLURAL_SCIENTIFIC;
        opts.digits.maximum_fraction_digits = 0;
        formatter = open_formatter(service, "fr", &opts);
        select_value(formatter, "999999.9", QJS_INTL_PLURAL_MANY);
        qjs_intl_plural_formatter_close(formatter);
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlPluralService *service;
    QJSIntlPluralFormatter *formatter;
    QJSIntlPluralOptions opts = options();
    const QJSIntlBytes *available;
    QJSIntlPluralCategory category;
    QJSIntlMathematicalValue value = number("1.125");
    size_t count, calls, fail, live;
    unsigned int mask;
    assert(qjs_intl_plural_open(NULL, NULL, &service) == QJS_INTL_INVALID_ARGUMENT && !service);
    assert(qjs_intl_plural_open(&allocator, NULL, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_plural_open(&allocator, NULL, &service));
    calls = state.calls;
    assert(!qjs_intl_plural_available(service, &available, &count) && count >= 2);
    assert(qjs_intl_plural_available(NULL, &available, &count) == QJS_INTL_INVALID_ARGUMENT);
    assert(!available && !count);
    assert(!qjs_intl_plural_available(service, &available, &count));
    check_service(service);
    {
        size_t i;
        QJSIntlPluralOptions compact = options();
        compact.notation = QJS_INTL_PLURAL_COMPACT;
        for (i = 0; i < count; i++) {
            assert(!qjs_intl_plural_formatter_open(service, i, &compact, &formatter));
            assert(!qjs_intl_plural_select(formatter, &value, &category));
            qjs_intl_plural_formatter_close(formatter);
        }
    }
    assert(qjs_intl_plural_formatter_open(service, count, &opts, &formatter) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!formatter);
    opts.notation = (QJSIntlPluralNotation)-1;
    assert(qjs_intl_plural_formatter_open(service, 0, &opts, &formatter) ==
           QJS_INTL_INVALID_ARGUMENT);
    opts = options();
    live = state.live;
    state.fail_at = state.calls + 1;
    assert(qjs_intl_plural_formatter_open(service, 0, &opts, &formatter) == QJS_INTL_NO_MEMORY);
    assert(!formatter && state.live == live);
    state.fail_at = 0;
    opts.digits.maximum_output_length = 5;
    formatter = open_formatter(service, "en", &opts);
    {
        QJSIntlMathematicalValue huge = number("1e20");
        assert(qjs_intl_plural_select(formatter, &huge, &category) == QJS_INTL_OVERFLOW);
        assert(category == QJS_INTL_PLURAL_OTHER);
    }
    qjs_intl_plural_formatter_close(formatter);
    opts = options();
    formatter = open_formatter(service, "en", &opts);
    qjs_intl_plural_close(service);
    live = state.live;
    fail = state.calls;
    assert(!qjs_intl_plural_select(formatter, &value, &category));
    calls = state.calls - fail;
    assert(state.live == live);
    for (fail = 1; fail <= calls; fail++) {
        state.fail_at = state.calls + fail;
        assert(qjs_intl_plural_select(formatter, &value, &category) == QJS_INTL_NO_MEMORY);
        assert(category == QJS_INTL_PLURAL_OTHER && state.live == live);
    }
    state.fail_at = 0;
    fail = state.calls;
    assert(!qjs_intl_plural_select_range(formatter, &value, &value, &category));
    calls = state.calls - fail;
    for (fail = 1; fail <= calls; fail++) {
        state.fail_at = state.calls + fail;
        assert(qjs_intl_plural_select_range(formatter, &value, &value, &category) ==
               QJS_INTL_NO_MEMORY);
        assert(category == QJS_INTL_PLURAL_OTHER && state.live == live);
    }
    state.fail_at = 0;
    select_value(formatter, "1", QJS_INTL_PLURAL_ONE); /* Service already closed. */
    assert(qjs_intl_plural_select(NULL, &value, &category) == QJS_INTL_INVALID_ARGUMENT);
    assert(category == QJS_INTL_PLURAL_OTHER);
    assert(qjs_intl_plural_select(formatter, NULL, &category) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_plural_select(formatter, &value, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_plural_categories(NULL, &mask) == QJS_INTL_INVALID_ARGUMENT && !mask);
    qjs_intl_plural_formatter_close(formatter);
    assert(!state.live);

    state.calls = 0;
    assert(!qjs_intl_plural_open(&allocator, NULL, &service));
    calls = state.calls;
    qjs_intl_plural_close(service);
    for (fail = 1; fail <= calls; fail++) {
        state.calls = 0;
        state.fail_at = fail;
        assert(qjs_intl_plural_open(&allocator, NULL, &service) == QJS_INTL_NO_MEMORY);
        assert(!service && !state.live);
    }
    qjs_intl_plural_close(NULL);
    qjs_intl_plural_formatter_close(NULL);
    return 0;
}
