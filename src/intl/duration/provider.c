/* Foreign duration locale admission over selected Number/List providers.
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
#include "composition.h"
#include "digital.h"
#include "provider.h"
#include <string.h>

typedef struct DurationLocale {
    size_t number, list;
} DurationLocale;

struct QJSIntlDurationService {
    QJSIntlAllocator allocator;
    QJSIntlNumberService *number;
    QJSIntlListService *list;
    QJSIntlBytes *available; /* Names borrow the Number service's inventory. */
    DurationLocale *locales;
    size_t count;
};

static int compare(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t common = a.length < b.length ? a.length : b.length;
    int order = common ? memcmp(a.data, b.data, common) : 0;

    return order ? order : (a.length > b.length) - (a.length < b.length);
}


QJSIntlStatus qjs_intl_duration_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                     QJSIntlDurationService **out)
{
    QJSIntlDurationService *service;
    QJSIntlStatus error;
    const QJSIntlBytes *numbers, *lists;
    size_t number_count, list_count, capacity, i = 0, j = 0;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    error = qjs_intl_number_open(allocator, NULL, &service->number);
    if (!error)
        error = qjs_intl_list_open(allocator, NULL, &service->list);
    if (!error)
        error = qjs_intl_number_available(service->number, &numbers, &number_count);
    if (!error)
        error = qjs_intl_list_available(service->list, &lists, &list_count);
    if (error)
        goto fail;
    capacity = number_count < list_count ? number_count : list_count;
    if (!capacity) {
        error = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    if (capacity > SIZE_MAX / sizeof(*service->available) ||
        capacity > SIZE_MAX / sizeof(*service->locales)) {
        error = QJS_INTL_OVERFLOW;
        goto fail;
    }
    service->available =
        allocator->malloc(allocator->opaque, capacity * sizeof(*service->available));
    if (service->available)
        service->locales =
            allocator->malloc(allocator->opaque, capacity * sizeof(*service->locales));
    if (!service->available || !service->locales) {
        error = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    /* Admission is the intersection of the two actual providers. Keep exact
       indexes; formatting never falls back to an unrelated process locale. */
    while (i < number_count && j < list_count) {
        int order = compare(numbers[i], lists[j]);
        char *pattern = NULL;

        if (order < 0) {
            i++;
            continue;
        }
        if (order > 0) {
            j++;
            continue;
        }
        error = qjs_intl_duration_digital_pattern(allocator, numbers[i], &pattern);
        if (pattern)
            allocator->free(allocator->opaque, pattern);
        if (error)
            goto fail;
        service->available[service->count] = numbers[i];
        service->locales[service->count++] = (DurationLocale){i++, j++};
    }
    if (!service->count) {
        error = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_duration_close(service);
    return error;
}

void qjs_intl_duration_close(QJSIntlDurationService *service)
{
    if (service) {
        QJSIntlAllocator allocator = service->allocator;

        if (service->available)
            allocator.free(allocator.opaque, service->available);
        if (service->locales)
            allocator.free(allocator.opaque, service->locales);
        qjs_intl_list_close(service->list);
        qjs_intl_number_close(service->number);
        allocator.free(allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_duration_available(const QJSIntlDurationService *service,
                                          const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!out || !count || !service)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->available;
    *count = service->count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_duration_default_system(const QJSIntlDurationService *service, size_t locale,
                                               QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !service || locale >= service->count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_number_default_system(service->number, service->locales[locale].number, out);
}

QJSIntlStatus qjs_intl_duration_system_supported(const QJSIntlDurationService *service,
                                                 size_t locale, QJSIntlBytes system, int *out)
{
    if (out)
        *out = 0;
    if (!out || !service || locale >= service->count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_number_system_supported(service->number, service->locales[locale].number,
                                            system, out);
}

QJSIntlStatus qjs_intl_duration_two_digit_hours(const QJSIntlDurationService *service,
                                                size_t locale, int *out)
{
    /* This flag changes resolved text-hour options too; raw hms width alone
       is not a justification to override the normative unit option choices. */
    if (out)
        *out = 0;
    return out && service && locale < service->count ? QJS_INTL_OK : QJS_INTL_INVALID_ARGUMENT;
}

QJSIntlStatus qjs_intl_duration_formatter_open(const QJSIntlDurationService *service, size_t locale,
                                               QJSIntlBytes system,
                                               const QJSIntlDurationOptions *options,
                                               QJSIntlDurationFormatter **out)
{
    QJSIntlStatus error;
    char *pattern = NULL;

    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->count || qjs_intl_duration_options_validate(options))
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_intl_duration_digital_pattern(&service->allocator, service->available[locale], &pattern);
    if (!error)
        error = qjs_intl_duration_composition_open(
            &service->allocator, service->number, service->locales[locale].number, service->list,
            service->locales[locale].list, (QJSIntlBytes){pattern, strlen(pattern)}, system,
            options, out);
    if (pattern)
        service->allocator.free(service->allocator.opaque, pattern);
    return error;
}
