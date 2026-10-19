/* Relative source selection and composition of Number/Plural service facades.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "relative.h"
#include "data.h"
#include "../common/number-decimal.h"
#include "../number/number.h"
#include "../plural/plural.h"
#include <math.h>
#include <string.h>

typedef struct RelativeLocale {
    size_t number, plural;
} RelativeLocale;

struct QJSIntlRelativeService {
    QJSIntlAllocator allocator;
    QJSIntlRelativeData data;
    QJSIntlNumberService *number;
    QJSIntlPluralService *plural;
    QJSIntlBytes *available;
    RelativeLocale *locales;
    uint32_t root;
};

struct QJSIntlRelativeFormatter {
    QJSIntlAllocator allocator;
    QJSIntlRelativeData data;
    QJSIntlRelativeMap maps[QJS_INTL_RELATIVE_UNIT_COUNT];
    QJSIntlNumberFormatter *number;
    QJSIntlPluralFormatter *plural;
    QJSIntlRelativeOptions options;
};

static int compare_bytes(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t n = a.length < b.length ? a.length : b.length;
    int order = n ? memcmp(a.data, b.data, n) : 0;
    return order ? order : (a.length > b.length) - (a.length < b.length);
}

static QJSIntlStatus find_locale(const QJSIntlBytes *tags, size_t count, QJSIntlBytes key, size_t *out)
{
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (compare_bytes(tags[mid], key) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == count || compare_bytes(tags[lo], key))
        return QJS_INTL_DATA_ERROR;
    *out = lo;
    return QJS_INTL_OK;
}

static QJSIntlStatus open_maps(const QJSIntlRelativeData *data, uint32_t locale,
                               QJSIntlRelativeWidth width, QJSIntlRelativeMap *maps)
{
    unsigned int unit, tense;
    QJSIntlStatus status;
    for (unit = 0; unit < QJS_INTL_RELATIVE_UNIT_COUNT; unit++) {
        status = qjs_intl_relative_map_at(data, locale, unit, width, &maps[unit]);
        if (status)
            return status;
        /* The specification falls back only when the entire style field is
           absent, not when one of its literal/plural selectors is absent. */
        if (!maps[unit].count && width != QJS_INTL_RELATIVE_LONG) {
            status = qjs_intl_relative_map_at(data, locale, unit, QJS_INTL_RELATIVE_LONG, &maps[unit]);
            if (status)
                return status;
        }
        for (tense = 0; tense < 2; tense++) {
            QJSIntlRelativeForm form;
            status = qjs_intl_relative_pattern_find(data, &maps[unit], tense, QJS_INTL_PLURAL_OTHER, &form);
            if (status || form.blocked)
                return QJS_INTL_DATA_ERROR;
        }
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_relative_open(const QJSIntlAllocator *allocator, QJSIntlRelativeService **out)
{
    QJSIntlRelativeService *service;
    QJSIntlData data;
    QJSIntlStatus status;
    const QJSIntlBytes *numbers, *plurals;
    size_t number_count, plural_count, count, index = 0;
    uint32_t i;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    status = qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, allocator, &data);
    if (status)
        goto fail;
    status = qjs_intl_relative_data_open(&data, allocator, &service->data);
    if (status)
        goto fail;
    status = qjs_intl_number_open(allocator, &service->number);
    if (status)
        goto fail;
    status = qjs_intl_plural_open(allocator, &service->plural);
    if (status)
        goto fail;
    if (data.locales.count < 2) {
        status = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    count = data.locales.count - 1;
    if (count > SIZE_MAX / sizeof(*service->available) || count > SIZE_MAX / sizeof(*service->locales)) {
        status = QJS_INTL_OVERFLOW;
        goto fail;
    }
    service->available = allocator->malloc(allocator->opaque, count * sizeof(*service->available));
    if (service->available)
        service->locales = allocator->malloc(allocator->opaque, count * sizeof(*service->locales));
    if (!service->available || !service->locales) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    status = qjs_intl_number_available(service->number, &numbers, &number_count);
    if (!status)
        status = qjs_intl_plural_available(service->plural, &plurals, &plural_count);
    if (status)
        goto fail;
    for (i = 0; i < data.locales.count; i++) {
        QJSIntlBytes tag;
        QJSIntlRelativeMap maps[QJS_INTL_RELATIVE_UNIT_COUNT];
        uint32_t parent;
        unsigned int width;
        qjs_intl_data_locale(&data, i, &tag, &parent);
        if (parent == UINT32_MAX) {
            service->root = i;
            continue;
        }
        service->available[index] = tag;
        status = find_locale(numbers, number_count, tag, &service->locales[index].number);
        if (!status)
            status = find_locale(plurals, plural_count, tag, &service->locales[index].plural);
        if (status)
            goto fail;
        for (width = 0; width < QJS_INTL_RELATIVE_WIDTH_COUNT; width++) {
            status = open_maps(&service->data, i, width, maps);
            if (status)
                goto fail;
        }
        index++;
    }
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_relative_close(service);
    return status;
}

void qjs_intl_relative_close(QJSIntlRelativeService *service)
{
    if (service) {
        QJSIntlAllocator allocator = service->allocator;
        qjs_intl_number_close(service->number);
        qjs_intl_plural_close(service->plural);
        if (service->available)
            allocator.free(allocator.opaque, service->available);
        if (service->locales)
            allocator.free(allocator.opaque, service->locales);
        allocator.free(allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_relative_available(const QJSIntlRelativeService *service,
                                          const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!service || !out || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->available;
    *count = service->data.section.count - 1;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_relative_default_system(const QJSIntlRelativeService *service, size_t locale,
                                               QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !service || locale >= service->data.section.count - 1)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_number_default_system(service->number, service->locales[locale].number, out);
}

QJSIntlStatus qjs_intl_relative_system_supported(const QJSIntlRelativeService *service, size_t locale,
                                                 QJSIntlBytes system, int *out)
{
    if (out)
        *out = 0;
    if (!out || !service || locale >= service->data.section.count - 1)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_number_system_supported(service->number, service->locales[locale].number, system, out);
}

static QJSIntlDecimalOptions digits(size_t limit)
{
    return (QJSIntlDecimalOptions){.minimum_integer_digits = 1, .minimum_fraction_digits = 0,
                                    .maximum_fraction_digits = 3, .minimum_significant_digits = 1,
                                    .maximum_significant_digits = 21, .rounding_increment = 1,
                                    .rounding_mode = QJS_INTL_ROUND_HALF_EXPAND,
                                    .rounding_type = QJS_INTL_ROUND_FRACTION,
                                    .maximum_output_length = limit};
}

QJSIntlStatus qjs_intl_relative_formatter_open(const QJSIntlRelativeService *service, size_t locale,
                                               QJSIntlBytes system, const QJSIntlRelativeOptions *options,
                                               QJSIntlRelativeFormatter **out)
{
    QJSIntlRelativeFormatter result = {0}, *formatter;
    QJSIntlNumberOptions number = {0};
    QJSIntlPluralOptions plural = {0};
    QJSIntlStatus status;
    uint32_t source;
    if (out)
        *out = NULL;
    if (!out || !service || !options || locale >= service->data.section.count - 1 ||
        (unsigned int)options->width >= QJS_INTL_RELATIVE_WIDTH_COUNT ||
        (unsigned int)options->numeric > QJS_INTL_RELATIVE_AUTO || !options->maximum_output_length)
        return QJS_INTL_INVALID_ARGUMENT;
    result.allocator = service->allocator;
    result.data = service->data;
    result.options = *options;
    source = (uint32_t)locale + (locale >= service->root);
    status = open_maps(&service->data, source, options->width, result.maps);
    if (status)
        return status;
    number.grouping = QJS_INTL_NUMBER_GROUP_AUTO;
    number.digits = digits(options->maximum_output_length);
    number.maximum_output_length = options->maximum_output_length;
    plural.digits = number.digits;
    status = qjs_intl_number_formatter_open(service->number, service->locales[locale].number,
                                            system, &number, &result.number);
    if (!status)
        status = qjs_intl_plural_formatter_open(service->plural, service->locales[locale].plural,
                                                &plural, &result.plural);
    if (status)
        goto fail;
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    *formatter = result;
    *out = formatter;
    return QJS_INTL_OK;
fail:
    qjs_intl_number_formatter_close(result.number);
    qjs_intl_plural_formatter_close(result.plural);
    return status;
}

void qjs_intl_relative_formatter_close(QJSIntlRelativeFormatter *formatter)
{
    if (formatter) {
        QJSIntlAllocator allocator = formatter->allocator;
        qjs_intl_number_formatter_close(formatter->number);
        qjs_intl_plural_formatter_close(formatter->plural);
        allocator.free(allocator.opaque, formatter);
    }
}

QJSIntlStatus qjs_intl_relative_format(const QJSIntlRelativeFormatter *formatter, double value,
                                       QJSIntlRelativeUnit unit, QJSIntlRelativeFormatted *out)
{
    QJSIntlNumberDecimal shortest = {0}, real = {0};
    QJSIntlFormatted body = {0};
    QJSIntlRelativeForm form;
    QJSIntlPluralCategory category;
    QJSIntlRelativeTense tense;
    QJSIntlStatus status;
    const QJSIntlAllocator *allocator;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !isfinite(value) || (unsigned int)unit >= QJS_INTL_RELATIVE_UNIT_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    allocator = &formatter->allocator;
    if (formatter->options.numeric == QJS_INTL_RELATIVE_AUTO) {
        QJSIntlBytes key;
        status = qjs_intl_number_to_intl_decimal(allocator, value, &shortest);
        if (status)
            goto done;
        key = value == 0 ? (QJSIntlBytes){"0", 1} : shortest.value.decimal;
        status = qjs_intl_relative_literal_find(&formatter->data, &formatter->maps[unit], key, &form);
        if (!status && !form.blocked) {
            status = qjs_intl_relative_literal_format(allocator, form.text,
                                                       formatter->options.maximum_output_length, out);
            goto done;
        }
        if (status && status != QJS_INTL_UNSUPPORTED)
            goto done;
    }
    /* ResolvePlural is an internal operation, unlike public select's
       ToIntlMathematicalValue boundary. Preserve its original exact-real
       input, shared with number formatting; source review is documented. */
    status = qjs_intl_number_to_real_decimal(allocator, fabs(value), &real);
    if (!status)
        status = qjs_intl_plural_select(formatter->plural, &real.value, &category);
    if (status)
        goto done;
    tense = signbit(value) ? QJS_INTL_RELATIVE_PAST : QJS_INTL_RELATIVE_FUTURE;
    status = qjs_intl_relative_pattern_find(&formatter->data, &formatter->maps[unit], tense, category, &form);
    if ((status == QJS_INTL_UNSUPPORTED || (!status && form.blocked)) && category != QJS_INTL_PLURAL_OTHER)
        status = qjs_intl_relative_pattern_find(&formatter->data, &formatter->maps[unit], tense,
                                                 QJS_INTL_PLURAL_OTHER, &form);
    if (status || form.blocked) {
        if (!status || status == QJS_INTL_UNSUPPORTED)
            status = QJS_INTL_DATA_ERROR;
        goto done;
    }
    if (form.text.length && memchr(form.text.data, '{', form.text.length)) {
        /* Past/future carries the direction; the inserted numeric magnitude
           is unsigned, including -0. See the draft discrepancy documented in
           tools/intl/README.md beside the native relative service. */
        status = qjs_intl_number_format(formatter->number, &real.value, &body);
        if (status)
            goto done;
    }
    status = qjs_intl_relative_pattern_format(allocator, form.text, body.text ? &body : NULL, unit,
                                               formatter->options.maximum_output_length, out);
done:
    qjs_intl_number_result_clear(allocator, &body);
    qjs_intl_number_decimal_clear(allocator, &real);
    qjs_intl_number_decimal_clear(allocator, &shortest);
    return status;
}
