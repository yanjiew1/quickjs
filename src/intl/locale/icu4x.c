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
#include "locale.h"
#include "preferences.h"
#include "identifier.h"
#include "matching.h"
#include "../common/locale-grammar.h"
#include "../common/timezone-syntax.h"
#include "../icu4x/common.h"
#include "../icu4x/locale-data.h"
#include "../../icu4x/locale.h"
#include "../../icu4x/list.h"
#include <string.h>

struct QJSIntlLocaleService {
    QJSIntlAllocator allocator;
    QJSICU4XLocale *identity;
    QJSICU4XLocales defaults;
};

QJSIntlStatus qjs_intl_locale_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                   QJSIntlLocaleService **out)
{
    QJSIntlLocaleService *service;
    QJSIntlStatus error;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    error = qjs_icu4x_status(qjs_icu4x_locale_open(&qjs_icu4x_locale_aliases, &service->identity));
    if (!error)
        error = qjs_icu4x_locales_open(allocator, qjs_icu4x_list_locales, &service->defaults);
    if (error) {
        qjs_intl_locale_close(service);
        return error;
    }
    *out = service;
    return QJS_INTL_OK;
}

void qjs_intl_locale_close(QJSIntlLocaleService *service)
{
    if (!service)
        return;
    qjs_icu4x_locale_close(service->identity);
    qjs_icu4x_locales_clear(&service->defaults);
    service->allocator.free(service->allocator.opaque, service);
}

void qjs_intl_locale_result_free(const QJSIntlLocaleService *service, char *result)
{
    if (result)
        service->allocator.free(service->allocator.opaque, result);
}

QJSIntlStatus qjs_intl_locale_profile_match(const QJSIntlLocaleService *service,
                                            QJSIntlBytes request, QJSIntlBytes *out)
{
    QJSIntlLocaleMatch match;
    QJSIntlStatus error;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_intl_lookup_matching_locale(service->defaults.tags, service->defaults.count,
                                            &request, 1, &match);
    if (!error && match.found)
        *out = service->defaults.tags[match.available_index];
    return error ? error : match.found ? QJS_INTL_OK : QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_locale_validate(const QJSIntlLocaleService *service, QJSIntlBytes input,
                                       int *out)
{
    if (out)
        *out = 0;
    if (!service || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = qjs_intl_unicode_locale_well_formed(input.data, input.length);
    return QJS_INTL_OK;
}

typedef struct IdentityOutput {
    const QJSIntlAllocator *allocator;
    char *text;
    QJSIntlStatus error;
} IdentityOutput;

static int write_identity(void *opaque, const uint8_t *text, size_t length)
{
    IdentityOutput *output = opaque;

    // The bridge emits one complete identity. Copy it with the engine allocator
    // so result_free and allocation-failure injection retain their C contracts.
    if (output->text || length == SIZE_MAX) {
        output->error = QJS_INTL_OVERFLOW;
        return QJS_ICU4X_RANGE;
    }
    output->text = output->allocator->malloc(output->allocator->opaque, length + 1);
    if (!output->text) {
        output->error = QJS_INTL_NO_MEMORY;
        return QJS_ICU4X_MEMORY;
    }
    if (length)
        memcpy(output->text, text, length);
    output->text[length] = 0;
    return QJS_ICU4X_OK;
}

static QJSIntlStatus identity(const QJSIntlLocaleService *service, QJSIntlBytes input,
                              uint32_t operation, char **out)
{
    IdentityOutput output = {0};
    QJSIntlStatus error;
    int status;

    if (out)
        *out = NULL;
    if (!out || !service || !qjs_intl_unicode_locale_well_formed(input.data, input.length))
        return QJS_INTL_INVALID_ARGUMENT;
    output.allocator = &service->allocator;
    status = qjs_icu4x_locale_identity(service->identity, (const uint8_t *)input.data, input.length,
                                       operation, (QJSICU4XByteSink){&output, write_identity});
    error = output.error ? output.error : qjs_icu4x_status(status);
    if (error) {
        qjs_intl_locale_result_free(service, output.text);
        return error;
    }
    *out = output.text;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_locale_canonicalize(const QJSIntlLocaleService *service, QJSIntlBytes input,
                                           char **out)
{
    return identity(service, input, 0, out);
}

QJSIntlStatus qjs_intl_locale_maximize(const QJSIntlLocaleService *service, QJSIntlBytes input,
                                       char **out)
{
    return identity(service, input, 1, out);
}

QJSIntlStatus qjs_intl_locale_minimize(const QJSIntlLocaleService *service, QJSIntlBytes input,
                                       char **out)
{
    return identity(service, input, 2, out);
}

static QJSIntlStatus copy_bytes(const QJSIntlAllocator *allocator, QJSIntlBytes input, char **out)
{
    if (input.length == SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    *out = allocator->malloc(allocator->opaque, input.length + 1);
    if (!*out)
        return QJS_INTL_NO_MEMORY;
    if (input.length)
        memcpy(*out, input.data, input.length);
    (*out)[input.length] = 0;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_locale_canonicalize_uvalue(const QJSIntlLocaleService *service,
                                                  QJSIntlBytes key, QJSIntlBytes value, char **out)
{
    char *input = NULL, *canonical = NULL;
    QJSIntlLocaleComponents parts;
    QJSIntlBytes result;
    QJSIntlStatus error;
    size_t length;

    if (out)
        *out = NULL;
    if (!out || !service || !key.data || key.length != 2 || (value.length && !value.data) ||
        (value.length && !qjs_intl_unicode_type_well_formed(value.data, value.length)))
        return QJS_INTL_INVALID_ARGUMENT;
    if (value.length > SIZE_MAX - 10)
        return QJS_INTL_OVERFLOW;
    length = 8 + (value.length ? value.length + 1 : 0);
    input = service->allocator.malloc(service->allocator.opaque, length + 1);
    if (!input)
        return QJS_INTL_NO_MEMORY;
    memcpy(input, "und-u-", 6);
    input[6] = key.data[0];
    input[7] = key.data[1];
    if (value.length) {
        input[8] = '-';
        memcpy(input + 9, value.data, value.length);
    }
    input[length] = 0;
    error = qjs_intl_locale_canonicalize(service, (QJSIntlBytes){input, length}, &canonical);
    if (!error)
        error = qjs_intl_locale_components((QJSIntlBytes){canonical, strlen(canonical)}, &parts);
    if (!error) {
        char normalized[2] = {key.data[0], key.data[1]};
        size_t i;

        for (i = 0; i < 2; i++) {
            if (normalized[i] >= 'A' && normalized[i] <= 'Z')
                normalized[i] += 'a' - 'A';
        }
        result = qjs_intl_locale_keyword(&parts, normalized);
        error = copy_bytes(&service->allocator, result, out);
    }
    if (canonical)
        service->allocator.free(service->allocator.opaque, canonical);
    service->allocator.free(service->allocator.opaque, input);
    return error;
}

QJSIntlStatus qjs_intl_locale_source_type(const QJSIntlLocaleService *service, QJSIntlBytes key,
                                          QJSIntlBytes value, QJSIntlBytes *out)
{
    QJSICU4XBytes result;
    QJSIntlStatus error;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !service)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_icu4x_status(
        qjs_icu4x_locale_source_type(service->identity, (const uint8_t *)key.data, key.length,
                                     (const uint8_t *)value.data, value.length, &result));
    if (!error)
        *out = (QJSIntlBytes){(const char *)result.data, result.length};
    return error;
}

QJSIntlStatus qjs_intl_locale_script_direction(const QJSIntlLocaleService *service,
                                               QJSIntlBytes script, unsigned int *out)
{
    QJSIntlStatus error;
    uint32_t result;
    size_t i;

    if (out)
        *out = QJS_INTL_DIRECTION_UNKNOWN;
    if (!out || !service || !script.data || script.length != 4)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < qjs_icu4x_locale_unknown_script_count; i++) {
        const char *name = qjs_icu4x_locale_strings + qjs_icu4x_locale_unknown_scripts[i];
        if (!memcmp(name, script.data, 4))
            return QJS_INTL_OK;
    }
    error = qjs_icu4x_status(
        qjs_icu4x_locale_script_direction((const uint8_t *)script.data, script.length, &result));
    if (!error)
        *out = result;
    return error;
}

/* Exact metadata queries borrow one compiled CLDR pool. There is no runtime
   pack decoder, fallback allocation or hidden timezone-backend dependency. */
static int bytes_order(QJSIntlBytes left, QJSIntlBytes right)
{
    size_t common = left.length < right.length ? left.length : right.length;
    int order = common ? memcmp(left.data, right.data, common) : 0;
    return order ? order : (left.length > right.length) - (left.length < right.length);
}

static QJSIntlBytes pool_text(uint32_t offset)
{
    const char *text = qjs_icu4x_locale_strings + offset;
    return (QJSIntlBytes){text, strlen(text)};
}

static QJSIntlStatus preference(const QJSIntlLocaleService *service, const QJSICU4XPreference *rows,
                                size_t total, QJSIntlBytes key, QJSIntlBytes *out, uint32_t *count)
{
    size_t low = 0, high = total, i;

    if (out)
        memset(out, 0, sizeof(*out));
    if (count)
        *count = 0;
    if (!service || !out || !count || (key.length && !key.data))
        return QJS_INTL_INVALID_ARGUMENT;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        int order = bytes_order(pool_text(rows[mid].name), key);
        if (!order) {
            *out = pool_text(rows[mid].value);
            *count = out->length != 0;
            for (i = 0; i < out->length; i++)
                *count += out->data[i] == ' ';
            return QJS_INTL_OK;
        }
        if (order < 0)
            low = mid + 1;
        else
            high = mid;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_locale_hour_data(const QJSIntlLocaleService *service, QJSIntlBytes key,
                                        QJSIntlBytes *out, uint32_t *count)
{
    return preference(service, qjs_icu4x_locale_hours, qjs_icu4x_locale_hour_count, key, out,
                      count);
}

QJSIntlStatus qjs_intl_locale_calendar_data(const QJSIntlLocaleService *service, QJSIntlBytes key,
                                            QJSIntlBytes *out, uint32_t *count)
{
    return preference(service, qjs_icu4x_locale_calendars, qjs_icu4x_locale_calendar_count, key,
                      out, count);
}

QJSIntlStatus qjs_intl_locale_week_data(const QJSIntlLocaleService *service, QJSIntlBytes region,
                                        QJSIntlWeekInfo *out)
{
    size_t low = 0, high = qjs_icu4x_locale_week_count;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out || (region.length && !region.data))
        return QJS_INTL_INVALID_ARGUMENT;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        const QJSICU4XWeek *row = &qjs_icu4x_locale_weeks[mid];
        int order = bytes_order((QJSIntlBytes){row->region, strlen(row->region)}, region);
        if (!order) {
            *out = (QJSIntlWeekInfo){row->first_day, row->minimum_days, row->weekend};
            return QJS_INTL_OK;
        }
        if (order < 0)
            low = mid + 1;
        else
            high = mid;
    }
    return QJS_INTL_UNSUPPORTED;
}

static int territory_valid(QJSIntlBytes region)
{
    size_t i;

    if (!region.data || (region.length != 2 && region.length != 3))
        return 0;
    for (i = 0; i < region.length; i++) {
        if (region.length == 2 ? region.data[i] < 'A' || region.data[i] > 'Z'
                               : region.data[i] < '0' || region.data[i] > '9')
            return 0;
    }
    return 1;
}

QJSIntlStatus qjs_intl_locale_timezone_next(const QJSIntlLocaleService *service,
                                            QJSIntlBytes territory, uint32_t *cursor,
                                            QJSIntlBytes *out)
{
    uint32_t i;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out || !cursor || !territory_valid(territory) ||
        *cursor > qjs_icu4x_locale_aliases.type_count)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = *cursor; i < qjs_icu4x_locale_aliases.type_count; i++) {
        const QJSICU4XLocaleType *row = &qjs_icu4x_locale_aliases.types[i];
        const QJSICU4XLocaleKey *key = &qjs_icu4x_locale_aliases.keys[row->key];
        QJSIntlBytes name, aliases, region;
        char inferred[2];
        size_t length;

        if (key->extension != 'u' || strcmp(pool_text(key->name).data, "tz") || row->variable ||
            pool_text(row->preferred).length)
            continue;
        name = pool_text(row->name);
        aliases = pool_text(row->aliases);
        if (!aliases.length)
            continue;
        region = pool_text(row->region);
        if (!region.length && name.length == 5) {
            // CLDR's short-ID prefix records the source territory. Preserve it
            // before resolving a current IANA primary name (e.g. SJ vs NO).
            if (name.data[0] < 'a' || name.data[0] > 'z' || name.data[1] < 'a' ||
                name.data[1] > 'z')
                return QJS_INTL_DATA_ERROR;
            inferred[0] = name.data[0] - 'a' + 'A';
            inferred[1] = name.data[1] - 'a' + 'A';
            region = (QJSIntlBytes){inferred, 2};
        }
        if (region.length && region.length != 2)
            return QJS_INTL_DATA_ERROR;
        if (bytes_order(region, territory))
            continue;
        for (length = 0; length < aliases.length && aliases.data[length] != ' '; length++)
            continue;
        aliases.length = length;
        if (!qjs_intl_timezone_source_identity_valid(aliases, 0))
            return QJS_INTL_DATA_ERROR;
        *out = aliases;
        *cursor = i + 1;
        return QJS_INTL_OK;
    }
    *cursor = qjs_icu4x_locale_aliases.type_count;
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_locale_timezone_count(const QJSIntlLocaleService *service,
                                             QJSIntlBytes territory, uint32_t *out)
{
    QJSIntlBytes zone;
    uint32_t cursor = 0, count = 0;
    QJSIntlStatus error;

    if (out)
        *out = 0;
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    while (!(error = qjs_intl_locale_timezone_next(service, territory, &cursor, &zone)))
        count++;
    if (error != QJS_INTL_UNSUPPORTED)
        return error;
    *out = count;
    return QJS_INTL_OK;
}
