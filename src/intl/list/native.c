/* Compile-time native implementation of the list service contract.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include "data.h"
#include "list.h"
#include "native-engine.h"
#include <string.h>

struct QJSIntlListService {
    QJSIntlAllocator allocator;
    QJSIntlData data;
    QJSIntlListData patterns;
    QJSIntlBytes *available;
    uint32_t root;
};

QJSIntlStatus qjs_intl_list_open(const QJSIntlAllocator *allocator,
                                 const struct QJSIntlData *checked_data,
                                 QJSIntlListService **out)
{
    QJSIntlListService *service;
    QJSIntlStatus status;
    uint32_t i;
    size_t count = 0;

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
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    if (checked_data) {
        service->data = *checked_data;
        status = QJS_INTL_OK;
    } else {
        status = qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                                    allocator, &service->data);
    }
    if (status)
        goto fail;
    status = qjs_intl_list_data_open(&service->data, allocator, &service->patterns);
    if (status)
        goto fail;
    if (!service->patterns.list_contexts.bytes ||
        service->data.locales.count < 2) {
        status = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    count = service->data.locales.count - 1;
    if (count > SIZE_MAX / sizeof(*service->available)) {
        status = QJS_INTL_OVERFLOW;
        goto fail;
    }
    service->available =
        allocator->malloc(allocator->opaque, count * sizeof(*service->available));
    if (!service->available) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    count = 0;
    /* All nine pattern variants and their contextual rules are complete in
       the compiled prepared profile. Its inventory's canonical identity is
       independently checked against the locale service in repository tests. */
    for (i = 0; i < service->data.locales.count; i++) {
        QJSIntlBytes tag;
        uint32_t parent;
        qjs_intl_data_locale(&service->data, i, &tag, &parent);
        if (parent == UINT32_MAX)
            service->root = i;
        else
            service->available[count++] = tag;
    }
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_list_close(service);
    return status;
}

void qjs_intl_list_close(QJSIntlListService *service)
{
    if (service) {
        QJSIntlAllocator allocator = service->allocator;
        if (service->available)
            allocator.free(allocator.opaque, service->available);
        allocator.free(allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_list_available(const QJSIntlListService *service,
                                      const QJSIntlBytes **locales, size_t *count)
{
    if (locales)
        *locales = NULL;
    if (count)
        *count = 0;
    if (!service || !locales || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *locales = service->available;
    *count = service->data.locales.count - 1;
    return QJS_INTL_OK;
}

/* CharRange uses realloc(ptr, 0) for release, while Intl allocators never
   receive zero-sized allocation requests. Keep that translation local. */
static void *script_realloc(void *opaque, void *ptr, size_t size)
{
    const QJSIntlAllocator *allocator = opaque;
    if (!size) {
        if (ptr)
            allocator->free(allocator->opaque, ptr);
        return NULL;
    }
    return allocator->realloc(allocator->opaque, ptr, size);
}

static QJSIntlStatus open_engine(const QJSIntlAllocator *allocator,
                                 QJSIntlListTemplates *templates,
                                 QJSIntlListFormatter **out)
{
    CharRange script;
    QJSIntlListScriptRange *ranges = NULL;
    QJSIntlAllocator script_allocator = *allocator;
    QJSIntlStatus status;
    int result, i;

    if (templates->context != QJS_INTL_LIST_CONTEXT_HEBREW_AND)
        return qjs_intl_native_list_open(allocator, templates, out);
    cr_init(&script, &script_allocator, script_realloc);
    result = unicode_script(&script, "Hebrew", 0);
    if (result || !script.len) {
        status = result == -1 ? QJS_INTL_NO_MEMORY : QJS_INTL_DATA_ERROR;
        goto done;
    }
    if ((size_t)(script.len / 2) > SIZE_MAX / sizeof(*ranges)) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    ranges = allocator->malloc(allocator->opaque,
                               (size_t)(script.len / 2) * sizeof(*ranges));
    if (!ranges) {
        status = QJS_INTL_NO_MEMORY;
        goto done;
    }
    for (i = 0; i < script.len; i += 2)
        ranges[i / 2] =
            (QJSIntlListScriptRange){script.points[i], script.points[i + 1] - 1};
    templates->hebrew_script = ranges;
    templates->hebrew_script_count = script.len / 2;
    status = qjs_intl_native_list_open(allocator, templates, out);
done:
    if (ranges)
        allocator->free(allocator->opaque, ranges);
    cr_free(&script);
    return status;
}

QJSIntlStatus qjs_intl_list_formatter_open(const QJSIntlListService *service,
                                           size_t locale, QJSIntlListType type,
                                           QJSIntlListStyle style,
                                           QJSIntlListFormatter **out)
{
    QJSIntlListTemplates templates = {0};
    QJSIntlStatus status;
    unsigned int part, kind, variant;
    uint32_t index;

    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->data.locales.count - 1 ||
        (unsigned int)type > QJS_INTL_LIST_UNIT ||
        (unsigned int)style > QJS_INTL_LIST_NARROW)
        return QJS_INTL_INVALID_ARGUMENT;
    index = locale + (locale >= service->root);
    variant = type * 3 + style;
    for (part = 0; part < 4; part++) {
        qjs_intl_list_data_template(&service->patterns, index, variant, part,
                                    &templates.base[part]);
    }
    status =
        qjs_intl_list_data_context(&service->patterns, index, variant, &kind,
                                   &templates.alternate[0], &templates.alternate[1]);
    if (status)
        return status;
    templates.context = (QJSIntlListContext)kind;
    return open_engine(&service->allocator, &templates, out);
}
