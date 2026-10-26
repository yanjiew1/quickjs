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
#include "collation.h"
#include "../icu4x/common.h"
#include "../icu4x/locale-data.h"
#include "../../icu4x/collation.h"
#include <string.h>

typedef struct CollationLocale {
    QJSIntlCollationOptions defaults[2];
} CollationLocale;

struct QJSIntlCollationService {
    QJSIntlAllocator allocator;
    QJSICU4XInventory locales, types;
    CollationLocale *rows;
    /* One membership byte per locale/type; the final row is their union. */
    uint8_t *supported;
};

struct QJSIntlCollator {
    QJSIntlAllocator allocator;
    QJSICU4XCollator *handle;
};

static int location(const QJSIntlCollationService *service, size_t locale,
                    QJSIntlCollationUsage usage)
{
    return service && locale < service->locales.count &&
           (unsigned)usage <= QJS_INTL_COLLATION_SEARCH;
}

static int enumerate_locales(void *opaque, QJSICU4XEmit *emit)
{
    const char *version = qjs_icu4x_locale_strings + qjs_icu4x_locale_aliases.version;
    size_t i;
    int status = qjs_icu4x_collation_check_version((const uint8_t *)version, strlen(version));

    if (status)
        return status;
    /* Root-equivalent locales have no explicit collation payload. Use the
       selected release's source registry, then verify both actual constructors
       in open; inventory membership alone is not executable support. */
    for (i = 0; i < qjs_icu4x_locale_registry_count; i++) {
        const char *tag = qjs_icu4x_locale_strings + qjs_icu4x_locale_registry[i];

        status = emit(opaque, (const uint8_t *)tag, strlen(tag));
        if (status)
            return status;
    }
    return QJS_ICU4X_OK;
}

QJSIntlStatus qjs_intl_collation_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                      QJSIntlCollationService **out)
{
    QJSIntlCollationService *service;
    QJSIntlStatus error;
    size_t i, j, entries;
    unsigned usage;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    error = qjs_icu4x_inventory_open(allocator, enumerate_locales, &service->locales);
    if (!error)
        error = qjs_icu4x_inventory_open(allocator, qjs_icu4x_collation_types, &service->types);
    if (error)
        goto fail;
    if (!service->locales.count || !service->types.count) {
        error = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    if (service->locales.count > SIZE_MAX / sizeof(*service->rows) ||
        service->locales.count == SIZE_MAX ||
        service->types.count > SIZE_MAX / (service->locales.count + 1) ||
        service->types.count > UINT32_MAX) {
        error = QJS_INTL_OVERFLOW;
        goto fail;
    }
    entries = service->types.count * (service->locales.count + 1);
    service->rows =
        allocator->malloc(allocator->opaque, service->locales.count * sizeof(*service->rows));
    service->supported = allocator->malloc(allocator->opaque, entries);
    if (!service->rows || !service->supported) {
        error = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    memset(service->supported, 0, entries);
    for (i = 0; i < service->locales.count; i++) {
        QJSIntlBytes tag = service->locales.items[i];

        for (usage = 0; usage < 2; usage++) {
            QJSICU4XCollationOptions source;
            QJSIntlCollationOptions *options = &service->rows[i].defaults[usage];

            error = qjs_icu4x_status(qjs_icu4x_collation_defaults((const uint8_t *)tag.data,
                                                                  tag.length, usage, &source));
            if (error)
                goto fail;
            *options = (QJSIntlCollationOptions){(QJSIntlCollationUsage)usage,
                                                 (QJSIntlCollationSensitivity)source.sensitivity,
                                                 (QJSIntlCollationCaseFirst)source.case_first,
                                                 (int)source.numeric,
                                                 (int)source.ignore_punctuation,
                                                 {0}};
        }
        for (j = 0; j < service->types.count; j++) {
            QJSIntlBytes type = service->types.items[j];
            uint32_t supported;

            error = qjs_icu4x_status(
                qjs_icu4x_collation_supported((const uint8_t *)tag.data, tag.length,
                                              (const uint8_t *)type.data, type.length, &supported));
            if (error)
                goto fail;
            service->supported[i * service->types.count + j] = (uint8_t)supported;
            service->supported[service->locales.count * service->types.count + j] |=
                (uint8_t)supported;
        }
    }
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_collation_close(service);
    return error;
}

void qjs_intl_collation_close(QJSIntlCollationService *service)
{
    if (!service)
        return;
    if (service->rows)
        service->allocator.free(service->allocator.opaque, service->rows);
    if (service->supported)
        service->allocator.free(service->allocator.opaque, service->supported);
    qjs_icu4x_inventory_clear(&service->locales);
    qjs_icu4x_inventory_clear(&service->types);
    service->allocator.free(service->allocator.opaque, service);
}

QJSIntlStatus qjs_intl_collation_available(const QJSIntlCollationService *service,
                                           const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!out || !count || !service)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->locales.items;
    *count = service->locales.count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_defaults(const QJSIntlCollationService *service, size_t locale,
                                          QJSIntlCollationUsage usage, QJSIntlCollationOptions *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !location(service, locale, usage))
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->rows[locale].defaults[usage];
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_type_supported(const QJSIntlCollationService *service,
                                                size_t locale, QJSIntlCollationUsage usage,
                                                QJSIntlBytes name, int *out)
{
    size_t i;

    if (out)
        *out = 0;
    if (!out || !location(service, locale, usage) || (name.length && !name.data))
        return QJS_INTL_INVALID_ARGUMENT;
    if (usage == QJS_INTL_COLLATION_SEARCH || !name.length)
        return QJS_INTL_OK;
    for (i = 0; i < service->types.count; i++) {
        QJSIntlBytes type = service->types.items[i];

        if (type.length == name.length && !memcmp(type.data, name.data, name.length)) {
            *out = service->supported[locale * service->types.count + i];
            break;
        }
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus next_type(const QJSIntlCollationService *service, size_t locale,
                               uint32_t *cursor, QJSIntlBytes *out)
{
    size_t index;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !cursor || !out || *cursor > service->types.count)
        return QJS_INTL_INVALID_ARGUMENT;
    for (index = *cursor; index < service->types.count; index++) {
        if (service->supported[locale * service->types.count + index]) {
            *out = service->types.items[index];
            *cursor = (uint32_t)index + 1;
            return QJS_INTL_OK;
        }
    }
    *cursor = (uint32_t)service->types.count;
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_collation_type_next(const QJSIntlCollationService *service, size_t locale,
                                           QJSIntlCollationUsage usage, uint32_t *cursor,
                                           QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !cursor || !location(service, locale, usage))
        return QJS_INTL_INVALID_ARGUMENT;
    if (usage == QJS_INTL_COLLATION_SEARCH)
        return *cursor ? QJS_INTL_INVALID_ARGUMENT : QJS_INTL_UNSUPPORTED;
    return next_type(service, locale, cursor, out);
}

QJSIntlStatus qjs_intl_collation_available_type_next(const QJSIntlCollationService *service,
                                                     uint32_t *cursor, QJSIntlBytes *out)
{
    /* The final membership row is the union of all admitted locales. */
    return next_type(service, service ? service->locales.count : 0, cursor, out);
}

QJSIntlStatus qjs_intl_collator_open(const QJSIntlCollationService *service, size_t locale,
                                     const QJSIntlCollationOptions *options, QJSIntlCollator **out)
{
    QJSIntlCollator *collator;
    QJSIntlStatus error;
    QJSIntlBytes tag;
    QJSICU4XCollationOptions resolved;
    int supported;

    if (out)
        *out = NULL;
    if (!out || !options || !location(service, locale, options->usage) ||
        (unsigned)options->sensitivity > QJS_INTL_COLLATION_CASE ||
        (unsigned)options->case_first > QJS_INTL_COLLATION_LOWER_FIRST ||
        (unsigned)options->numeric > 1 || (unsigned)options->ignore_punctuation > 1 ||
        (options->collation.length && !options->collation.data))
        return QJS_INTL_INVALID_ARGUMENT;
    if (options->collation.length) {
        error = qjs_intl_collation_type_supported(service, locale, options->usage,
                                                  options->collation, &supported);
        if (error || !supported)
            return error ? error : QJS_INTL_UNSUPPORTED;
    }
    collator = service->allocator.malloc(service->allocator.opaque, sizeof(*collator));
    if (!collator)
        return QJS_INTL_NO_MEMORY;
    collator->allocator = service->allocator;
    collator->handle = NULL;
    tag = service->locales.items[locale];
    resolved = (QJSICU4XCollationOptions){options->sensitivity, options->case_first,
                                          (uint32_t)options->numeric,
                                          (uint32_t)options->ignore_punctuation};
    error = qjs_icu4x_status(
        qjs_icu4x_collator_open((const uint8_t *)tag.data, tag.length, options->usage,
                                (const uint8_t *)options->collation.data, options->collation.length,
                                resolved, &collator->handle));
    if (error) {
        qjs_intl_collator_close(collator);
        return error;
    }
    *out = collator;
    return QJS_INTL_OK;
}

void qjs_intl_collator_close(QJSIntlCollator *collator)
{
    if (collator) {
        qjs_icu4x_collator_close(collator->handle);
        collator->allocator.free(collator->allocator.opaque, collator);
    }
}

QJSIntlStatus qjs_intl_collator_compare(const QJSIntlCollator *collator, const uint16_t *left,
                                        size_t left_length, const uint16_t *right,
                                        size_t right_length, int *out)
{
    int32_t order;
    QJSIntlStatus error;

    if (out)
        *out = 0;
    if (!out || !collator || (left_length && !left) || (right_length && !right))
        return QJS_INTL_INVALID_ARGUMENT;
    if (left_length > PTRDIFF_MAX / sizeof(*left) || right_length > PTRDIFF_MAX / sizeof(*right))
        return QJS_INTL_OVERFLOW;
    error = qjs_icu4x_status(qjs_icu4x_collator_compare(collator->handle, left, left_length, right,
                                                        right_length, &order));
    if (!error)
        *out = order;
    return error;
}
