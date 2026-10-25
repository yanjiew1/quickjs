/* Native duration composition over the Number and List service contracts.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "duration.h"
#include "data.h"
#include "decimal.h"
#include "native-digital.h"
#include "../number/number.h"
#include "../list/list.h"
#include "../../temporal/duration.h"
#include <string.h>

typedef struct DurationLocale {
    size_t number, list;
    uint32_t source;
} DurationLocale;

struct QJSIntlDurationService {
    QJSIntlAllocator allocator;
    QJSIntlDurationData data;
    QJSIntlNumberService *number;
    QJSIntlListService *list;
    QJSIntlBytes *available;
    DurationLocale *locales;
    size_t count;
};

struct QJSIntlDurationFormatter {
    QJSIntlAllocator allocator;
    QJSIntlNumberFormatter *numbers[QJS_TEMPORAL_UNIT_COUNT];
    QJSIntlListFormatter *list;
    QJSIntlNativeDurationDigital digital;
    QJSIntlDurationOptions options;
};

static int compare(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t common = a.length < b.length ? a.length : b.length;
    int order = common ? memcmp(a.data, b.data, common) : 0;
    return order ? order : (a.length > b.length) - (a.length < b.length);
}

static QJSIntlStatus find(const QJSIntlBytes *tags, size_t count, QJSIntlBytes key, size_t *out)
{
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (compare(tags[mid], key) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == count || compare(tags[lo], key))
        return QJS_INTL_DATA_ERROR;
    *out = lo;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_duration_open(const QJSIntlAllocator *allocator,
                                     const struct QJSIntlData *checked_data,
                                     QJSIntlDurationService **out)
{
    QJSIntlDurationService *service;
    QJSIntlData data;
    QJSIntlStatus status;
    const QJSIntlBytes *numbers, *lists;
    size_t number_count, list_count, count;
    uint32_t i;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free)
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
    if (!status)
        status = qjs_intl_duration_data_open(&data, allocator, &service->data);
    if (!status)
        status = qjs_intl_number_open(allocator, &data, &service->number);
    if (!status)
        status = qjs_intl_list_open(allocator, &data, &service->list);
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
        status = qjs_intl_list_available(service->list, &lists, &list_count);
    if (status)
        goto fail;
    for (i = 0; i < data.locales.count; i++) {
        QJSIntlBytes tag;
        QJSIntlDurationPattern pattern;
        DurationLocale *locale = &service->locales[service->count];
        uint32_t parent;
        qjs_intl_data_locale(&data, i, &tag, &parent);
        if (parent == UINT32_MAX)
            continue;
        status = qjs_intl_duration_pattern_at(&service->data, i, QJS_INTL_DURATION_HMS, &pattern);
        if (!status && pattern.state != QJS_INTL_DURATION_PATTERN_VALUE)
            status = QJS_INTL_DATA_ERROR;
        if (!status)
            status = qjs_intl_duration_native_digital_validate(pattern.text);
        if (!status)
            status = find(numbers, number_count, tag, &locale->number);
        if (!status)
            status = find(lists, list_count, tag, &locale->list);
        if (status)
            goto fail;
        locale->source = i;
        service->available[service->count++] = tag;
    }
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_duration_close(service);
    return status;
}

void qjs_intl_duration_close(QJSIntlDurationService *service)
{
    if (service) {
        QJSIntlAllocator allocator = service->allocator;
        qjs_intl_number_close(service->number);
        qjs_intl_list_close(service->list);
        if (service->available)
            allocator.free(allocator.opaque, service->available);
        if (service->locales)
            allocator.free(allocator.opaque, service->locales);
        allocator.free(allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_duration_available(const QJSIntlDurationService *service, const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!service || !out || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->available;
    *count = service->count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_duration_default_system(const QJSIntlDurationService *service, size_t locale, QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out || locale >= service->count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_number_default_system(service->number, service->locales[locale].number, out);
}

QJSIntlStatus qjs_intl_duration_system_supported(const QJSIntlDurationService *service, size_t locale,
                                               QJSIntlBytes system, int *out)
{
    if (out)
        *out = 0;
    if (!service || !out || locale >= service->count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_number_system_supported(service->number, service->locales[locale].number, system, out);
}

QJSIntlStatus qjs_intl_duration_two_digit_hours(const QJSIntlDurationService *service, size_t locale, int *out)
{
    if (out)
        *out = 0;
    return service && out && locale < service->count ? QJS_INTL_OK : QJS_INTL_INVALID_ARGUMENT;
}

void qjs_intl_duration_formatter_close(QJSIntlDurationFormatter *formatter)
{
    if (formatter) {
        QJSIntlAllocator allocator = formatter->allocator;
        unsigned int unit;
        for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++)
            qjs_intl_number_formatter_close(formatter->numbers[unit]);
        qjs_intl_list_formatter_close(formatter->list);
        qjs_intl_duration_native_digital_clear(&allocator, &formatter->digital);
        allocator.free(allocator.opaque, formatter);
    }
}

static int numeric(QJSIntlDurationUnitStyle style)
{
    return style == QJS_INTL_DURATION_UNIT_NUMERIC || style == QJS_INTL_DURATION_UNIT_2DIGIT;
}

static int has_fraction(const QJSIntlDurationOptions *options, unsigned int unit)
{
    return unit + 1 < QJS_TEMPORAL_UNIT_COUNT &&
           options->units[unit + 1].style == QJS_INTL_DURATION_UNIT_FRACTIONAL;
}

static QJSIntlNumberOptions number_options(const QJSIntlDurationOptions *options, unsigned int unit)
{
    static const char *const names[] = {"year", "month", "week", "day", "hour", "minute", "second",
                                       "millisecond", "microsecond", "nanosecond"};
    QJSIntlNumberOptions result = {0};
    QJSIntlDurationUnitStyle style = options->units[unit].style;
    int fraction = has_fraction(options, unit);
    result.style = numeric(style) ? QJS_INTL_NUMBER_DECIMAL : QJS_INTL_NUMBER_UNIT;
    result.grouping = numeric(style) ? QJS_INTL_NUMBER_GROUP_OFF : QJS_INTL_NUMBER_GROUP_AUTO;
    result.sign_display = QJS_INTL_NUMBER_SIGN_AUTO;
    result.maximum_output_length = options->maximum_output_length;
    result.digits = (QJSIntlDecimalOptions){.minimum_integer_digits = style == QJS_INTL_DURATION_UNIT_2DIGIT ? 2 : 1,
                                           .minimum_fraction_digits = fraction && options->fractional_digits >= 0 ?
                                               options->fractional_digits : 0,
                                           .maximum_fraction_digits = fraction ?
                                               (options->fractional_digits >= 0 ? options->fractional_digits : 9) : 3,
                                           .minimum_significant_digits = 1, .maximum_significant_digits = 21,
                                           .rounding_increment = 1,
                                           .rounding_mode = fraction ? QJS_INTL_ROUND_TRUNC : QJS_INTL_ROUND_HALF_EXPAND,
                                           .rounding_type = QJS_INTL_ROUND_FRACTION,
                                           .maximum_output_length = options->maximum_output_length};
    result.unit = (QJSIntlBytes){names[unit], strlen(names[unit])};
    result.unit_display = style == QJS_INTL_DURATION_UNIT_LONG ? QJS_INTL_NUMBER_UNIT_LONG :
                          style == QJS_INTL_DURATION_UNIT_NARROW ? QJS_INTL_NUMBER_UNIT_NARROW : QJS_INTL_NUMBER_UNIT_SHORT;
    return result;
}

QJSIntlStatus qjs_intl_duration_formatter_open(const QJSIntlDurationService *service, size_t locale,
                                             QJSIntlBytes system, const QJSIntlDurationOptions *options,
                                             QJSIntlDurationFormatter **out)
{
    QJSIntlDurationFormatter *formatter;
    QJSIntlDurationPattern pattern;
    QJSIntlListStyle list_style;
    QJSIntlStatus status;
    unsigned int unit;
    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->count || qjs_intl_duration_options_validate(options))
        return QJS_INTL_INVALID_ARGUMENT;
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = service->allocator;
    formatter->options = *options;
    status = qjs_intl_duration_pattern_at(&service->data, service->locales[locale].source, QJS_INTL_DURATION_HMS, &pattern);
    if (!status)
        status = qjs_intl_duration_native_digital(&formatter->allocator, pattern.text, &formatter->digital);
    if (status)
        goto fail;
    list_style = options->style == QJS_INTL_DURATION_LONG ? QJS_INTL_LIST_LONG :
                 options->style == QJS_INTL_DURATION_NARROW ? QJS_INTL_LIST_NARROW : QJS_INTL_LIST_SHORT;
    status = qjs_intl_list_formatter_open(service->list, service->locales[locale].list,
                                         QJS_INTL_LIST_UNIT, list_style, &formatter->list);
    if (status)
        goto fail;
    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        QJSIntlNumberOptions number;

        if (options->units[unit].style == QJS_INTL_DURATION_UNIT_FRACTIONAL)
            continue;
        number = number_options(options, unit);
        status = qjs_intl_number_formatter_open(service->number, service->locales[locale].number,
                                                 system, &number, &formatter->numbers[unit]);
        if (status)
            goto fail;
    }
    *out = formatter;
    return QJS_INTL_OK;
fail:
    qjs_intl_duration_formatter_close(formatter);
    return status;
}

static int nonzero(const QJSTemporalDuration *duration, unsigned int unit, int fractional)
{
    unsigned int last = fractional ? QJS_TEMPORAL_NANOSECOND : unit;
    for (; unit <= last; unit++)
        if (duration->fields[unit] != 0)
            return 1;
    return 0;
}

static QJSIntlStatus component(const QJSIntlDurationFormatter *formatter, const QJSTemporalDuration *duration,
                               unsigned int unit, int *show_sign, QJSIntlFormatted *number,
                               QJSIntlDurationPart **parts, QJSIntlDurationComponent *out)
{
    QJSIntlMathematicalValue value;
    QJSIntlStatus status;
    char buffer[64];
    size_t i;
    status = qjs_intl_duration_component_decimal(duration, unit, has_fraction(&formatter->options, unit),
                                                buffer, sizeof(buffer), &value);
    if (status)
        return status;
    if (*show_sign && !nonzero(duration, unit, has_fraction(&formatter->options, unit)) &&
        qjs_temporal_duration_sign(duration) < 0)
        value.kind = QJS_INTL_NEGATIVE_ZERO;
    /* Later components use the unsigned pattern. HALF_EXPAND and TRUNC are
       sign-symmetric, so format their magnitude through the borrowed slice. */
    if (!*show_sign && value.decimal.length && value.decimal.data[0] == '-') {
        value.decimal.data++;
        value.decimal.length--;
    }
    status = qjs_intl_number_format(formatter->numbers[unit], &value, number);
    if (status)
        return status;
    if (number->part_count > SIZE_MAX / sizeof(**parts))
        return QJS_INTL_OVERFLOW;
    if (number->part_count) {
        *parts = formatter->allocator.malloc(formatter->allocator.opaque, number->part_count * sizeof(**parts));
        if (!*parts)
            return QJS_INTL_NO_MEMORY;
    }
    for (i = 0; i < number->part_count; i++)
        (*parts)[i] = (QJSIntlDurationPart){number->parts[i].start, number->parts[i].end, number->parts[i].type, unit};
    *out = (QJSIntlDurationComponent){{number->text, number->length}, *parts, number->part_count};
    *show_sign = 0;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_duration_format(const QJSIntlDurationFormatter *formatter, const QJSTemporalDuration *duration,
                                      QJSIntlDurationFormatted *out)
{
    QJSIntlFormatted numbers[QJS_TEMPORAL_UNIT_COUNT] = {{0}}, list = {0};
    QJSIntlDurationPart *parts[QJS_TEMPORAL_UNIT_COUNT] = {0};
    QJSIntlDurationComponent components[QJS_TEMPORAL_UNIT_COUNT], group[3];
    QJSIntlDurationFormatted digital = {0};
    QJSIntlUTF16 items[QJS_TEMPORAL_UNIT_COUNT];
    QJSIntlStatus status = QJS_INTL_OK;
    size_t count = 0, i;
    unsigned int unit;
    int show_sign = 1;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !duration || !qjs_temporal_duration_is_valid(duration))
        return QJS_INTL_INVALID_ARGUMENT;
    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        QJSIntlDurationUnitOptions options = formatter->options.units[unit];
        if (numeric(options.style)) {
            int shown[3] = {0};
            QJSIntlBytes separators[2];
            unsigned int current;
            size_t group_count = 0;
            shown[0] = unit == QJS_TEMPORAL_HOUR &&
                (duration->fields[QJS_TEMPORAL_HOUR] != 0 || options.display == QJS_INTL_DURATION_ALWAYS);
            shown[2] = nonzero(duration, QJS_TEMPORAL_SECOND, 1) ||
                formatter->options.units[QJS_TEMPORAL_SECOND].display == QJS_INTL_DURATION_ALWAYS;
            shown[1] = unit <= QJS_TEMPORAL_MINUTE && ((shown[0] && shown[2]) ||
                duration->fields[QJS_TEMPORAL_MINUTE] != 0 ||
                formatter->options.units[QJS_TEMPORAL_MINUTE].display == QJS_INTL_DURATION_ALWAYS);
            for (current = QJS_TEMPORAL_HOUR; current <= QJS_TEMPORAL_SECOND; current++) {
                if (!shown[current - QJS_TEMPORAL_HOUR])
                    continue;
                if (group_count)
                    separators[group_count - 1] = current == QJS_TEMPORAL_MINUTE ?
                        formatter->digital.hour_minute : formatter->digital.minute_second;
                status = component(formatter, duration, current, &show_sign, &numbers[current],
                                    &parts[current], &group[group_count++]);
                if (status)
                    goto done;
            }
            if (group_count) {
                status = qjs_intl_duration_join_digital(&formatter->allocator,
                    group, group_count, separators, formatter->options.maximum_output_length, &digital);
                if (status)
                    goto done;
                components[count++] = (QJSIntlDurationComponent){{digital.text, digital.length}, digital.parts, digital.part_count};
            }
            break;
        }
        if (options.display == QJS_INTL_DURATION_ALWAYS || nonzero(duration, unit, has_fraction(&formatter->options, unit))) {
            status = component(formatter, duration, unit, &show_sign, &numbers[unit], &parts[unit], &components[count++]);
            if (status)
                goto done;
        }
        if (has_fraction(&formatter->options, unit))
            break;
    }
    for (i = 0; i < count; i++)
        items[i] = components[i].text;
    status = qjs_intl_list_format(formatter->list, items, count, &list);
    if (!status)
        status = qjs_intl_duration_join_parts(&formatter->allocator, &list, components, count,
                                             formatter->options.maximum_output_length, out);
done:
    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        qjs_intl_number_result_clear(&formatter->allocator, &numbers[unit]);
        if (parts[unit])
            formatter->allocator.free(formatter->allocator.opaque, parts[unit]);
    }
    qjs_intl_duration_result_clear(&formatter->allocator, &digital);
    qjs_intl_list_result_clear(&formatter->allocator, &list);
    return status;
}
