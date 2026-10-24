/* Compile-time native implementation of the common locale identity contract.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "locale.h"
#include "native-id.h"
#include "preference-data.h"
#include "matching.h"
#include "type-identity.h"
#include "timezone-identity.h"
#include <string.h>

struct QJSIntlLocaleService {
    QJSIntlAllocator allocator;
    QJSIntlLocaleMetadata metadata;
    QJSIntlLocalePreferences preferences;
};

QJSIntlStatus qjs_intl_locale_open(const QJSIntlAllocator *allocator,
                                   const struct QJSIntlData *checked_data,
                                   QJSIntlLocaleService **out)
{
    QJSIntlLocaleService *service;
    QJSIntlData data;
    QJSIntlStatus status;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc ||
        !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    if (checked_data && !checked_data->pack.bytes)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    service->allocator = *allocator;
    if (checked_data) {
        data = *checked_data;
        status = QJS_INTL_OK;
    } else {
        status = qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                                    allocator, &data);
    }
    if (!status)
        status =
            qjs_intl_locale_metadata_open(&data, allocator, &service->metadata);
    if (!status)
        status = qjs_intl_locale_preferences_open(&data, allocator,
                                                 &service->preferences);
    if (status) {
        allocator->free(allocator->opaque, service);
        return status;
    }
    *out = service;
    return QJS_INTL_OK;
}

void qjs_intl_locale_close(QJSIntlLocaleService *service)
{
    if (service)
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
    const QJSIntlData *data;
    QJSIntlBytes *available;
    QJSIntlLocaleMatch match;
    QJSIntlStatus status;
    size_t count = 0;
    uint32_t i;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    data = &service->metadata.data;
    if (data->locales.count < 2)
        return QJS_INTL_DATA_ERROR;
    if (sizeof(*available) > SIZE_MAX / (data->locales.count - 1))
        return QJS_INTL_OVERFLOW;
    available = service->allocator.malloc(service->allocator.opaque,
                                           (size_t)(data->locales.count - 1) * sizeof(*available));
    if (!available)
        return QJS_INTL_NO_MEMORY;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes tag;
        uint32_t parent;
        qjs_intl_data_locale(data, i, &tag, &parent);
        if (parent != UINT32_MAX)
            available[count++] = tag;
    }
    status = qjs_intl_lookup_matching_locale(available, count, &request, 1, &match);
    if (!status) {
        if (match.found)
            *out = available[match.available_index];
        else
            status = QJS_INTL_UNSUPPORTED;
    }
    service->allocator.free(service->allocator.opaque, available);
    return status;
}

QJSIntlStatus qjs_intl_locale_week_data(const QJSIntlLocaleService *service,
                                        QJSIntlBytes region, QJSIntlWeekInfo *out)
{
    return qjs_intl_preference_week(service ? &service->preferences : NULL, region,
                                    out);
}

QJSIntlStatus qjs_intl_locale_hour_data(const QJSIntlLocaleService *service,
                                        QJSIntlBytes key, QJSIntlBytes *out,
                                        uint32_t *count)
{
    return qjs_intl_preference_list(service ? &service->preferences : NULL, 1, key, out,
                                    count);
}

QJSIntlStatus qjs_intl_locale_calendar_data(const QJSIntlLocaleService *service,
                                            QJSIntlBytes key, QJSIntlBytes *out,
                                            uint32_t *count)
{
    return qjs_intl_preference_list(service ? &service->preferences : NULL, 0, key,
                                    out, count);
}

QJSIntlStatus qjs_intl_locale_source_type(const QJSIntlLocaleService *service,
                                          QJSIntlBytes key, QJSIntlBytes value,
                                          QJSIntlBytes *out)
{
    QJSIntlLocaleType type;
    uint32_t index;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_locale_type_identity(&service->metadata, (QJSIntlBytes){"u", 1},
                                           key, value, &index);
    if (!status)
        status = qjs_intl_locale_type_at(&service->metadata, index, &type);
    if (!status)
        *out = type.name;
    return status;
}

QJSIntlStatus qjs_intl_locale_timezone_count(const QJSIntlLocaleService *service,
                                             QJSIntlBytes territory, uint32_t *out)
{
    return qjs_intl_timezone_region_count(service ? &service->metadata : NULL, territory, out);
}

QJSIntlStatus qjs_intl_locale_timezone_next(const QJSIntlLocaleService *service,
                                            QJSIntlBytes territory, uint32_t *cursor,
                                            QJSIntlBytes *out)
{
    QJSIntlTimeZoneIdentity identity;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_timezone_region_next(&service->metadata, territory, cursor, &identity);
    if (!status)
        *out = identity.identifier;
    return status;
}

QJSIntlStatus qjs_intl_locale_script_direction(const QJSIntlLocaleService *service,
                                               QJSIntlBytes script, unsigned int *out)
{
    return qjs_intl_preference_direction(service ? &service->preferences : NULL, script,
                                         out);
}

QJSIntlStatus qjs_intl_locale_validate(const QJSIntlLocaleService *service,
                                      QJSIntlBytes input, int *well_formed)
{
    return qjs_intl_native_locale_validate(service ? &service->allocator : NULL,
                                           input, well_formed);
}

QJSIntlStatus qjs_intl_locale_canonicalize(const QJSIntlLocaleService *service,
                                           QJSIntlBytes input, char **out)
{
    return qjs_intl_native_locale_canonicalize(
        service ? &service->allocator : NULL,
        service ? &service->metadata : NULL, input, out);
}

QJSIntlStatus qjs_intl_locale_maximize(const QJSIntlLocaleService *service,
                                       QJSIntlBytes input, char **out)
{
    return qjs_intl_native_locale_maximize(service ? &service->allocator : NULL,
                                           service ? &service->metadata : NULL,
                                           input, out);
}

QJSIntlStatus qjs_intl_locale_minimize(const QJSIntlLocaleService *service,
                                       QJSIntlBytes input, char **out)
{
    return qjs_intl_native_locale_minimize(service ? &service->allocator : NULL,
                                           service ? &service->metadata : NULL,
                                           input, out);
}

QJSIntlStatus
qjs_intl_locale_canonicalize_uvalue(const QJSIntlLocaleService *service,
                                    QJSIntlBytes key, QJSIntlBytes value,
                                    char **out)
{
    return qjs_intl_native_locale_canonicalize_uvalue(
        service ? &service->allocator : NULL,
        service ? &service->metadata : NULL, key, value, out);
}
