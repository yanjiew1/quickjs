/* Compile-time native implementation of the common locale identity contract.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "locale.h"
#include "native-id.h"

struct QJSIntlLocaleService {
    QJSIntlAllocator allocator;
    QJSIntlLocaleMetadata metadata;
};

QJSIntlStatus qjs_intl_locale_open(const QJSIntlAllocator *allocator,
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
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    service->allocator = *allocator;
    status = qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                                allocator, &data);
    if (!status)
        status =
            qjs_intl_locale_metadata_open(&data, allocator, &service->metadata);
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
