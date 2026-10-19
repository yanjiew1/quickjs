/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19:
 * PartitionNumberPattern, ComputeExponent, ComputeExponentForMagnitude.
 */
#include "round.h"
#include <limits.h>
#include <string.h>

void qjs_intl_number_round_clear(const QJSIntlAllocator *allocator, QJSIntlRoundedNumber *out)
{
    if (!out)
        return;
    qjs_intl_decimal_result_clear(allocator, &out->number);
    memset(out, 0, sizeof(*out));
}

static int zero(const QJSIntlDecimal *decimal)
{
    return decimal->length == 1 && decimal->digits[0] == '0';
}

static int options_valid(const QJSIntlNumberRounding *options)
{
    size_t i;
    if (!options || (unsigned int)options->notation > QJS_INTL_NUMBER_COMPACT ||
        options->percent > 1 || qjs_intl_decimal_options_validate(&options->digits) ||
        (options->compact_count && !options->compact) ||
        (options->notation != QJS_INTL_NUMBER_COMPACT && options->compact_count))
        return 0;
    for (i = 0; i < options->compact_count; i++) {
        const QJSIntlCompactScale *scale = &options->compact[i];
        if (scale->magnitude > QJS_INTL_DECIMAL_EXPONENT_LIMIT ||
            scale->exponent > scale->magnitude ||
            (i && scale->magnitude <= options->compact[i - 1].magnitude))
            return 0;
    }
    return 1;
}

static QJSIntlStatus exponent_for_magnitude(const QJSIntlNumberRounding *options, int64_t magnitude,
                                            int32_t *out)
{
    int64_t exponent = 0;
    if (options->notation == QJS_INTL_NUMBER_SCIENTIFIC)
        exponent = magnitude;
    else if (options->notation == QJS_INTL_NUMBER_ENGINEERING) {
        int64_t remainder = magnitude % 3;
        exponent = magnitude - (remainder < 0 ? remainder + 3 : remainder);
    } else if (options->notation == QJS_INTL_NUMBER_COMPACT && magnitude >= 0) {
        size_t lo = 0, hi = options->compact_count;
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (options->compact[mid].magnitude <= (uint64_t)magnitude)
                lo = mid + 1;
            else
                hi = mid;
        }
        if (lo)
            exponent = options->compact[lo - 1].exponent;
    }
    if (exponent < INT32_MIN || exponent > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    *out = (int32_t)exponent;
    return QJS_INTL_OK;
}

static QJSIntlStatus format_scaled(const QJSIntlAllocator *allocator,
                                   const QJSIntlNumberRounding *options,
                                   const QJSIntlDecimal *decimal, int64_t input_exponent,
                                   int32_t exponent, int positive, QJSIntlDecimalResult *out)
{
    QJSIntlDecimal scaled = *decimal;
    int64_t shifted = input_exponent - exponent;
    if (shifted < -QJS_INTL_DECIMAL_EXPONENT_LIMIT || shifted > QJS_INTL_DECIMAL_EXPONENT_LIMIT)
        return QJS_INTL_OVERFLOW;
    scaled.exponent = (int32_t)shifted;
    if (positive)
        scaled.negative = 0;
    return qjs_intl_decimal_format_numeric(allocator, &scaled, &options->digits, out);
}

QJSIntlStatus qjs_intl_number_round(const QJSIntlAllocator *allocator,
                                    const QJSIntlNumberRounding *options,
                                    const QJSIntlMathematicalValue *value,
                                    QJSIntlRoundedNumber *out)
{
    QJSIntlDecimal input = {0};
    QJSIntlDecimalResult probe = {0};
    QJSIntlRoundedNumber result = {0};
    QJSIntlStatus status;
    int64_t input_exponent, magnitude;
    int32_t exponent = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !options_valid(options) ||
        !value || (unsigned int)value->kind > QJS_INTL_NEGATIVE_ZERO)
        return QJS_INTL_INVALID_ARGUMENT;
    if (value->kind == QJS_INTL_NAN || value->kind == QJS_INTL_POSITIVE_INFINITY ||
        value->kind == QJS_INTL_NEGATIVE_INFINITY) {
        out->kind = value->kind;
        return QJS_INTL_OK;
    }
    status = qjs_intl_decimal_from_value(allocator, value, &input);
    if (status)
        return status;
    input_exponent = input.exponent;
    if (options->percent && !zero(&input))
        input_exponent += 2;
    magnitude = (int64_t)input.length + input_exponent - 1;
    if (!zero(&input) && options->notation != QJS_INTL_NUMBER_STANDARD) {
        status = exponent_for_magnitude(options, magnitude, &exponent);
        if (status)
            goto done;
        status = format_scaled(allocator, options, &input, input_exponent, exponent, 1, &probe);
        if (status)
            goto done;
        if (!zero(&probe.rounded) &&
            (int64_t)probe.rounded.length + probe.rounded.exponent - 1 != magnitude - exponent) {
            status = exponent_for_magnitude(options, magnitude + 1, &exponent);
            if (status)
                goto done;
        } else if (!input.negative) {
            /* The successful positive probe is also the final result; avoid
               rounding and allocating the identical value a second time. */
            result.number = probe;
            memset(&probe, 0, sizeof(probe));
            goto publish;
        }
    }
    status = format_scaled(allocator, options, &input, input_exponent, exponent, 0, &result.number);
    if (status)
        goto done;
publish:
    result.exponent = exponent;
    result.kind = result.number.rounded.negative && zero(&result.number.rounded)
                      ? QJS_INTL_NEGATIVE_ZERO
                      : QJS_INTL_FINITE;
    *out = result;
    memset(&result, 0, sizeof(result));
    status = QJS_INTL_OK;
done:
    qjs_intl_decimal_clear(allocator, &input);
    qjs_intl_decimal_result_clear(allocator, &probe);
    qjs_intl_number_round_clear(allocator, &result);
    return status;
}
