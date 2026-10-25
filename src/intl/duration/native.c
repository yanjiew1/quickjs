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
#include "composition.h"
#include "digital.h"
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
            status = qjs_intl_duration_digital_validate(pattern.text);
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

QJSIntlStatus qjs_intl_duration_formatter_open(const QJSIntlDurationService *service, size_t locale,
                                             QJSIntlBytes system, const QJSIntlDurationOptions *options,
                                             QJSIntlDurationFormatter **out)
{
    QJSIntlDurationPattern pattern;
    QJSIntlStatus status;

    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->count || qjs_intl_duration_options_validate(options))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_duration_pattern_at(&service->data, service->locales[locale].source,
                                         QJS_INTL_DURATION_HMS, &pattern);
    if (status)
        return status;
    return qjs_intl_duration_composition_open(&service->allocator,
        service->number, service->locales[locale].number,
        service->list, service->locales[locale].list, pattern.text, system, options, out);
}
