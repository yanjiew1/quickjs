/* Native PluralRuleSelect policy and shared ResolvePlural rounding.
 * ECMA402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "plural.h"
#include "native-engine.h"
#include "../number/compact-data.h"
#include <limits.h>
#include <string.h>

struct QJSIntlPluralService {
    QJSIntlAllocator allocator;
    QJSIntlPluralData plurals;
    QJSIntlCompactData compact;
    QJSIntlBytes *available;
    uint32_t root;
};

struct QJSIntlPluralFormatter {
    QJSIntlAllocator allocator;
    QJSIntlPluralRuleSet rules;
    QJSIntlCompactData compact;
    QJSIntlCompactMap map;
    QJSIntlPluralOptions options;
};

QJSIntlStatus qjs_intl_plural_open(const QJSIntlAllocator *allocator,
                                   const struct QJSIntlData *checked_data,
                                   QJSIntlPluralService **out)
{
    QJSIntlPluralService *service;
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlStatus status;
    uint32_t i;
    size_t count = 0;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    if (checked_data && !checked_data->pack.bytes)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    if (checked_data) {
        data = *checked_data;
        status = QJS_INTL_OK;
    } else {
        status = qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, allocator, &data);
    }
    if (status)
        goto fail;
    status = qjs_intl_plural_data_open(&data, allocator, &service->plurals);
    if (status)
        goto fail;
    status = qjs_intl_numbering_data_open(&data, allocator, &numbering);
    if (status)
        goto fail;
    status = qjs_intl_compact_data_open(&numbering, allocator, &service->compact);
    if (status)
        goto fail;
    if (data.locales.count < 2) {
        status = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    count = data.locales.count - 1;
    if (count > SIZE_MAX / sizeof(*service->available)) {
        status = QJS_INTL_OVERFLOW;
        goto fail;
    }
    service->available = allocator->malloc(allocator->opaque, count * sizeof(*service->available));
    if (!service->available) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    count = 0;
    for (i = 0; i < data.locales.count; i++) {
        QJSIntlBytes tag;
        uint32_t parent;
        qjs_intl_data_locale(&data, i, &tag, &parent);
        if (parent == UINT32_MAX)
            service->root = i;
        else
            service->available[count++] = tag;
    }
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_plural_close(service);
    return status;
}

void qjs_intl_plural_close(QJSIntlPluralService *service)
{
    if (service) {
        QJSIntlAllocator allocator = service->allocator;
        if (service->available)
            allocator.free(allocator.opaque, service->available);
        allocator.free(allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_plural_available(const QJSIntlPluralService *service,
                                        const QJSIntlBytes **locales, size_t *count)
{
    if (locales)
        *locales = NULL;
    if (count)
        *count = 0;
    if (!service || !locales || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *locales = service->available;
    *count = service->plurals.data.locales.count - 1;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_plural_formatter_open(const QJSIntlPluralService *service, size_t locale,
                                             const QJSIntlPluralOptions *options,
                                             QJSIntlPluralFormatter **out)
{
    QJSIntlPluralFormatter result = {0}, *formatter;
    QJSIntlStatus status;
    uint32_t source;
    if (out)
        *out = NULL;
    if (!out || !service || !options || locale >= service->plurals.data.locales.count - 1 ||
        (unsigned int)options->type > QJS_INTL_PLURAL_ORDINAL ||
        (unsigned int)options->notation > QJS_INTL_PLURAL_COMPACT || options->compact_long > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_decimal_options_validate(&options->digits);
    if (status)
        return status;
    source = (uint32_t)locale + (locale >= service->root);
    status = qjs_intl_plural_rules_at(&service->plurals, source, options->type, &result.rules);
    if (status)
        return status;
    if (options->notation == QJS_INTL_PLURAL_COMPACT) {
        QJSIntlNumberingPreferences preferences;
        status = qjs_intl_numbering_preferences(&service->compact.numbering,
                                                service->available[locale], &preferences);
        if (status)
            return status;
        status = qjs_intl_compact_map_at(&service->compact, source, preferences.standard,
                                         options->compact_long ? QJS_INTL_COMPACT_DECIMAL_LONG
                                                               : QJS_INTL_COMPACT_DECIMAL_SHORT,
                                         &result.map);
        if (status)
            return status;
        result.compact = service->compact;
    }
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    result.allocator = service->allocator;
    result.options = *options;
    *formatter = result;
    *out = formatter;
    return QJS_INTL_OK;
}

void qjs_intl_plural_formatter_close(QJSIntlPluralFormatter *formatter)
{
    if (formatter)
        formatter->allocator.free(formatter->allocator.opaque, formatter);
}

QJSIntlStatus qjs_intl_plural_categories(const QJSIntlPluralFormatter *formatter,
                                         unsigned int *mask)
{
    if (mask)
        *mask = 0;
    if (!formatter || !mask)
        return QJS_INTL_INVALID_ARGUMENT;
    *mask = formatter->rules.present;
    return QJS_INTL_OK;
}

static QJSIntlStatus notation_exponent(const QJSIntlPluralFormatter *formatter,
                                       const QJSIntlDecimal *rounded, int32_t *out)
{
    int64_t magnitude, exponent = 0;
    *out = 0;
    if ((rounded->length == 1 && rounded->digits[0] == '0') ||
        formatter->options.notation == QJS_INTL_PLURAL_STANDARD)
        return QJS_INTL_OK;
    magnitude = (int64_t)rounded->length + rounded->exponent - 1;
    if (formatter->options.notation == QJS_INTL_PLURAL_SCIENTIFIC) {
        exponent = magnitude;
    } else if (formatter->options.notation == QJS_INTL_PLURAL_ENGINEERING) {
        int64_t remainder = magnitude % 3;
        exponent = magnitude - (remainder < 0 ? remainder + 3 : remainder);
    } else {
        uint32_t lo = 0, hi = formatter->map.count;
        QJSIntlCompactPattern pattern;
        if (magnitude < 0)
            return QJS_INTL_OK;
        /* Upper bound by magnitude, then fetch that magnitude's required
           standard other pattern. Exact/plural affixes do not choose scale. */
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            qjs_intl_compact_pattern_at(&formatter->compact, &formatter->map, mid, &pattern);
            if (pattern.magnitude <= (uint64_t)magnitude)
                lo = mid + 1;
            else
                hi = mid;
        }
        if (!lo)
            return QJS_INTL_OK;
        qjs_intl_compact_pattern_at(&formatter->compact, &formatter->map, lo - 1, &pattern);
        qjs_intl_compact_pattern_find(&formatter->compact, &formatter->map, pattern.magnitude,
                                      QJS_INTL_COMPACT_OTHER, QJS_INTL_COMPACT_STANDARD, &pattern);
        exponent = pattern.exponent;
    }
    if (exponent < INT32_MIN || exponent > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    *out = exponent;
    return QJS_INTL_OK;
}

typedef struct Resolution {
    QJSIntlDecimalResult rounded;
    QJSIntlBytes text;
    QJSIntlPluralCategory category;
} Resolution;

static QJSIntlStatus resolve(const QJSIntlPluralFormatter *formatter,
                             const QJSIntlMathematicalValue *value, Resolution *out)
{
    QJSIntlDecimal decimal;
    QJSIntlPluralOperands operands;
    QJSIntlStatus status;
    memset(out, 0, sizeof(*out));
    out->category = QJS_INTL_PLURAL_OTHER;
    if (!value || (unsigned int)value->kind > QJS_INTL_NEGATIVE_ZERO)
        return QJS_INTL_INVALID_ARGUMENT;
    switch (value->kind) {
    case QJS_INTL_NAN:
        out->text = (QJSIntlBytes){"NaN", 3};
        return QJS_INTL_OK;
    case QJS_INTL_POSITIVE_INFINITY:
        out->text = (QJSIntlBytes){"Infinity", 8};
        return QJS_INTL_OK;
    case QJS_INTL_NEGATIVE_INFINITY:
        out->text = (QJSIntlBytes){"-Infinity", 9};
        return QJS_INTL_OK;
    default:
        break;
    }
    status = qjs_intl_decimal_from_value(&formatter->allocator, value, &decimal);
    if (status)
        return status;
    /* ResolvePlural rounds the original value, not NumberFormat's scaled
       mantissa. Notation affects only native CLDR exponent metadata. */
    status = qjs_intl_decimal_format_numeric(&formatter->allocator, &decimal,
                                             &formatter->options.digits, &out->rounded);
    qjs_intl_decimal_clear(&formatter->allocator, &decimal);
    if (status)
        return status;
    out->text = (QJSIntlBytes){out->rounded.text, out->rounded.length};
    operands.decimal = out->text.data;
    operands.length = out->text.length;
    status = notation_exponent(formatter, &out->rounded.rounded, &operands.exponent);
    if (!status)
        status = qjs_intl_native_plural_select(&formatter->rules, &operands, &out->category);
    return status;
}

QJSIntlStatus qjs_intl_plural_select(const QJSIntlPluralFormatter *formatter,
                                     const QJSIntlMathematicalValue *value,
                                     QJSIntlPluralCategory *out)
{
    Resolution result;
    QJSIntlStatus status;
    if (out)
        *out = QJS_INTL_PLURAL_OTHER;
    if (!out || !formatter)
        return QJS_INTL_INVALID_ARGUMENT;
    status = resolve(formatter, value, &result);
    if (!status)
        *out = result.category;
    qjs_intl_decimal_result_clear(&formatter->allocator, &result.rounded);
    return status;
}

QJSIntlStatus qjs_intl_plural_select_range(const QJSIntlPluralFormatter *formatter,
                                           const QJSIntlMathematicalValue *start,
                                           const QJSIntlMathematicalValue *end,
                                           QJSIntlPluralCategory *out)
{
    Resolution a = {0}, b = {0};
    QJSIntlStatus status;
    if (out)
        *out = QJS_INTL_PLURAL_OTHER;
    if (!out || !formatter || !start || !end || start->kind == QJS_INTL_NAN ||
        end->kind == QJS_INTL_NAN)
        return QJS_INTL_INVALID_ARGUMENT;
    status = resolve(formatter, start, &a);
    if (!status)
        status = resolve(formatter, end, &b);
    if (!status) {
        if (a.text.length == b.text.length && !memcmp(a.text.data, b.text.data, a.text.length))
            *out = a.category;
        else
            status = qjs_intl_native_plural_range(&formatter->rules, a.category, b.category, out);
    }
    qjs_intl_decimal_result_clear(&formatter->allocator, &a.rounded);
    qjs_intl_decimal_result_clear(&formatter->allocator, &b.rounded);
    return status;
}
