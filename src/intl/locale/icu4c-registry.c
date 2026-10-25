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
#include "icu4c-private.h"
#include "../icu4c/identity.h"
#include <limits.h>
#include <stdlib.h>
#include <unicode/uloc.h>
#include <unicode/ures.h>

static int type_order(const void *a, const void *b)
{
    const ICUType *left = a, *right = b;
    int order = memcmp(left->key, right->key, 2);

    return order ? order : strcmp(left->source, right->source);
}

static int zone_order(const void *a, const void *b)
{
    const ICUZone *left = a, *right = b;
    int order = strcmp(left->region, right->region);

    return order ? order : strcmp(left->name, right->name);
}

static QJSIntlStatus add_type(QJSIntlLocaleService *service, const char key[2], const char *source)
{
    ICUType *row;
    char k[3] = {key[0], key[1], 0};
    char *owned;
    const char *canonical;
    size_t i, length = strlen(source);
    QJSIntlStatus error;

    error = qjs_icu4c_copy_bytes(&service->allocator, (QJSIntlBytes){source, length}, &owned);
    if (error)
        return error;
    if (key[0] == 't' && key[1] == 'z') {
        for (i = 0; i < length; i++) {
            if (owned[i] == ':')
                owned[i] = '/';
        }
    }
    canonical = uloc_toUnicodeLocaleType(k, owned);
    if (!canonical || canonical == owned) {
        service->allocator.free(service->allocator.opaque, owned);
        return QJS_INTL_OK; /* Variable/unknown types are not registry entries. */
    }
    if (service->type_count == service->type_capacity) {
        size_t capacity = service->type_capacity ? service->type_capacity * 2 : 64;
        ICUType *larger;

        if (capacity < service->type_capacity || capacity > SIZE_MAX / sizeof(*larger)) {
            service->allocator.free(service->allocator.opaque, owned);
            return QJS_INTL_OVERFLOW;
        }
        larger = service->allocator.realloc(service->allocator.opaque, service->types,
                                            capacity * sizeof(*larger));
        if (!larger) {
            service->allocator.free(service->allocator.opaque, owned);
            return QJS_INTL_NO_MEMORY;
        }
        service->types = larger;
        service->type_capacity = capacity;
    }
    row = &service->types[service->type_count++];
    memcpy(row->key, key, 2);
    row->source = owned;
    row->name = (QJSIntlBytes){canonical, strlen(canonical)};
    return QJS_INTL_OK;
}

static QJSIntlStatus load_types(QJSIntlLocaleService *service)
{
    static const char *const tables[] = {"typeMap", "typeAlias", "bcpTypeAlias"};
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *bundle = ures_openDirect(NULL, "keyTypeData", &status);
    QJSIntlStatus error = qjs_icu4c_status(status);
    size_t kind, read, write;

    if (error)
        goto done;
    for (kind = 0; kind < sizeof(tables) / sizeof(*tables); kind++) {
        UResourceBundle *table = ures_getByKey(bundle, tables[kind], NULL, &status);
        int32_t count = U_SUCCESS(status) ? ures_getSize(table) : 0, index;

        if (status == U_MISSING_RESOURCE_ERROR && kind) {
            status = U_ZERO_ERROR;
            ures_close(table);
            continue;
        }
        if (U_FAILURE(status)) {
            error = qjs_icu4c_status(status);
            ures_close(table);
            goto done;
        }
        for (index = 0; index < count; index++) {
            UResourceBundle *category = ures_getByIndex(table, index, NULL, &status);
            const char *source_key = U_SUCCESS(status) ? ures_getKey(category) : NULL;
            const char *key = source_key ? uloc_toUnicodeLocaleKey(source_key) : NULL;
            int32_t values = U_SUCCESS(status) ? ures_getSize(category) : 0, item;

            if (U_FAILURE(status) || !source_key) {
                error = U_FAILURE(status) ? qjs_icu4c_status(status) : QJS_INTL_DATA_ERROR;
                ures_close(category);
                ures_close(table);
                goto done;
            }
            if (!key || strlen(key) != 2 || key[1] < 'a' || key[1] > 'z') {
                ures_close(category);
                continue;
            }
            for (item = 0; item < values; item++) {
                UResourceBundle *entry = ures_getByIndex(category, item, NULL, &status);
                const char *value = U_SUCCESS(status) ? ures_getKey(entry) : NULL;

                if (U_FAILURE(status) || !value)
                    error = U_FAILURE(status) ? qjs_icu4c_status(status) : QJS_INTL_DATA_ERROR;
                else {
                    size_t before = service->type_count;

                    error = add_type(service, key, value);
                    if (!error && service->type_count > before) {
                        ICUType *row = &service->types[service->type_count - 1];
                        if (strcmp(row->source, row->name.data))
                            error = add_type(service, key, row->name.data);
                    }
                }
                ures_close(entry);
                if (error) {
                    ures_close(category);
                    ures_close(table);
                    goto done;
                }
            }
            ures_close(category);
        }
        ures_close(table);
    }
    if (!service->type_count) {
        error = QJS_INTL_DATA_ERROR;
        goto done;
    }
    qsort(service->types, service->type_count, sizeof(*service->types), type_order);
    for (read = write = 0; read < service->type_count; read++) {
        if (write && !type_order(&service->types[write - 1], &service->types[read])) {
            service->allocator.free(service->allocator.opaque, service->types[read].source);
        } else {
            service->types[write++] = service->types[read];
        }
    }
    service->type_count = write;
done:
    ures_close(bundle);
    return error;
}

static QJSIntlStatus load_zones(QJSIntlLocaleService *service)
{
    size_t i, read, write;

    if (service->type_count > SIZE_MAX / sizeof(*service->zones))
        return QJS_INTL_OVERFLOW;
    service->zones = service->allocator.malloc(service->allocator.opaque,
                                               service->type_count * sizeof(*service->zones));
    if (!service->zones)
        return QJS_INTL_NO_MEMORY;
    for (i = 0; i < service->type_count; i++) {
        const ICUType *type = &service->types[i];
        const char *name;
        ICUZone row = {0};
        char region[4];
        size_t length, region_length;
        QJSIntlStatus error;

        if (type->key[0] != 't' || type->key[1] != 'z')
            continue;
        name = uloc_toLegacyType("tz", type->name.data);
        if (!name)
            return QJS_INTL_DATA_ERROR;
        length = strlen(name);
        error = qjs_icu4c_zone_region((QJSIntlBytes){name, length}, region, &region_length);
        if (error) {
            if (error == QJS_INTL_INVALID_ARGUMENT)
                continue; /* An obsolete registry type has no current zone. */
            return error;
        }
        if (region_length != 2)
            continue; /* Etc/UTC and other world-only, nonterritorial entries. */
        memcpy(row.region, region, 3);
        error = qjs_icu4c_copy_bytes(&service->allocator, (QJSIntlBytes){name, length}, &row.name);
        if (error)
            return error;
        service->zones[service->zone_count++] = row;
    }
    qsort(service->zones, service->zone_count, sizeof(*service->zones), zone_order);
    for (read = write = 0; read < service->zone_count; read++) {
        if (write && !zone_order(&service->zones[write - 1], &service->zones[read]))
            service->allocator.free(service->allocator.opaque, service->zones[read].name);
        else
            service->zones[write++] = service->zones[read];
    }
    service->zone_count = write;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_locale_source_type(const QJSIntlLocaleService *service, QJSIntlBytes key,
                                          QJSIntlBytes value, QJSIntlBytes *out)
{
    size_t low = 0, high;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !service || !key.data || key.length != 2 || (value.length && !value.data))
        return QJS_INTL_INVALID_ARGUMENT;
    high = service->type_count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const ICUType *row = &service->types[middle];
        int order = memcmp(row->key, key.data, 2);

        if (!order)
            order = qjs_icu4c_locale_bytes_order((QJSIntlBytes){row->source, strlen(row->source)},
                                                 value);
        if (!order) {
            *out = row->name;
            return QJS_INTL_OK;
        }
        if (order < 0)
            low = middle + 1;
        else
            high = middle;
    }
    return QJS_INTL_UNSUPPORTED;
}

static int territory_valid(QJSIntlBytes territory)
{
    size_t i;

    if (!territory.data || (territory.length != 2 && territory.length != 3))
        return 0;
    for (i = 0; i < territory.length; i++) {
        if (territory.length == 2 ? territory.data[i] < 'A' || territory.data[i] > 'Z'
                                  : territory.data[i] < '0' || territory.data[i] > '9')
            return 0;
    }
    return 1;
}

QJSIntlStatus qjs_intl_locale_timezone_count(const QJSIntlLocaleService *service,
                                             QJSIntlBytes territory, uint32_t *out)
{
    size_t i;

    if (out)
        *out = 0;
    if (!out || !service || !territory_valid(territory))
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < service->zone_count; i++) {
        if (territory.length == 2 && !memcmp(service->zones[i].region, territory.data, 2))
            (*out)++;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_locale_timezone_next(const QJSIntlLocaleService *service,
                                            QJSIntlBytes territory, uint32_t *cursor,
                                            QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !cursor || !service || !territory_valid(territory) || *cursor > service->zone_count)
        return QJS_INTL_INVALID_ARGUMENT;
    while (*cursor < service->zone_count) {
        const ICUZone *row = &service->zones[(*cursor)++];

        if (territory.length == 2 && !memcmp(row->region, territory.data, 2)) {
            *out = (QJSIntlBytes){row->name, strlen(row->name)};
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_icu4c_locale_registry_open(QJSIntlLocaleService *service)
{
    QJSIntlStatus error = load_types(service);

    return error ? error : load_zones(service);
}

void qjs_icu4c_locale_registry_clear(QJSIntlLocaleService *service)
{
    QJSIntlAllocator allocator = service->allocator;
    size_t i;

    for (i = 0; i < service->type_count; i++)
        allocator.free(allocator.opaque, service->types[i].source);
    for (i = 0; i < service->zone_count; i++)
        allocator.free(allocator.opaque, service->zones[i].name);
    if (service->types)
        allocator.free(allocator.opaque, service->types);
    if (service->zones)
        allocator.free(allocator.opaque, service->zones);
    service->types = NULL;
    service->zones = NULL;
    service->type_count = service->type_capacity = service->zone_count = 0;
}
