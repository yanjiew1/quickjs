/* Native segmentation over the selected immutable CLDR/Unicode data pack.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "segment.h"
#include "native-input.h"
#include "suppression-data.h"
#include <string.h>

/* The service is also the shared configuration owner. A segmenter retains it,
   but a prepared input owns only its final indexes and allocator. No traversal
   cursor or input text is stored in this configuration. */
struct QJSIntlSegmentService {
    QJSIntlAllocator allocator;
    size_t references;
    QJSIntlData data;
    QJSIntlSegmentProgramData programs;
    QJSIntlSegmentSuppressionData suppressions;
    QJSIntlSegmentDictionaryData dictionaries;
    QJSIntlSegmentRouting *routing;
    QJSIntlBytes *available;
    uint32_t root;
};

struct QJSIntlSegmenter {
    QJSIntlSegmentService *service;
    QJSIntlGranularity granularity;
    QJSIntlSegmentProgram selected, grapheme;
    QJSIntlSegmentLiteralGraph suppressions;
    QJSIntlSegmentNativeRules rules;
};

void qjs_intl_segment_close(QJSIntlSegmentService *service)
{
    QJSIntlAllocator allocator;

    if (!service || --service->references)
        return;
    allocator = service->allocator;
    qjs_intl_segment_routing_close(service->routing);
    if (service->available)
        allocator.free(allocator.opaque, service->available);
    allocator.free(allocator.opaque, service);
}

static QJSIntlStatus admit(QJSIntlSegmentService *service,
                            const QJSIntlData *checked_data)
{
    QJSIntlStatus status;
    uint32_t locale, kind, count;
    size_t at = 0;

    if (checked_data) {
        service->data = *checked_data;
        status = QJS_INTL_OK;
    } else {
        status = qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                                    &service->allocator, &service->data);
    }
    if (status)
        return status;
    if (service->data.pack.profile != 1)
        return QJS_INTL_UNSUPPORTED;
    status = qjs_intl_segment_program_data_open(&service->data, &service->programs);
    if (!status)
        status = qjs_intl_segment_suppression_data_open(&service->data, &service->allocator,
                                                       &service->suppressions);
    if (status)
        return status;
    if (memcmp(service->programs.fingerprint, service->suppressions.fingerprint, 32))
        return QJS_INTL_DATA_ERROR;
    count = service->data.locales.count;
    /* Route coverage belongs to this provider. Shared locale metadata alone
       cannot establish an executable Segmenter inventory. Check cardinality
       before allocating an inventory, then validate each actual route below. */
    if (count < 2 || (uint64_t)count * 3 != service->programs.binding_count ||
        count != service->suppressions.binding_count)
        return QJS_INTL_UNSUPPORTED;
    for (locale = 0; locale < count; locale++) {
        QJSIntlSegmentProgram program;
        QJSIntlSegmentLiteralGraph list;

        for (kind = 0; kind < 3; kind++) {
            status = qjs_intl_segment_program_find(&service->programs, locale,
                                                    (QJSIntlGranularity)kind, &program);
            if (status)
                return status;
        }
        status = qjs_intl_segment_suppression_find(&service->suppressions, locale, &list);
        if (status)
            return status;
    }
    status = qjs_intl_segment_dictionary_data_open(&service->data.pack, &service->allocator,
                                                   &service->dictionaries);
    if (status)
        return status;
    if (service->dictionaries.present != (1u << QJS_INTL_DICTIONARY_FAMILY_COUNT) - 1)
        return QJS_INTL_UNSUPPORTED;
    if (sizeof(*service->available) > SIZE_MAX / (count - 1))
        return QJS_INTL_OVERFLOW;
    service->available = service->allocator.malloc(service->allocator.opaque,
                                                   (size_t)(count - 1) * sizeof(*service->available));
    if (!service->available)
        return QJS_INTL_NO_MEMORY;
    service->root = UINT32_MAX;
    for (locale = 0; locale < count; locale++) {
        QJSIntlBytes tag;
        uint32_t parent;

        status = qjs_intl_data_locale(&service->data, locale, &tag, &parent);
        if (status)
            return status;
        if (parent == UINT32_MAX) {
            if (service->root != UINT32_MAX || tag.length != 4 || memcmp(tag.data, "root", 4))
                return QJS_INTL_DATA_ERROR;
            service->root = locale;
        } else {
            if (at == count - 1)
                return QJS_INTL_DATA_ERROR;
            service->available[at++] = tag;
        }
    }
    if (service->root == UINT32_MAX || at != count - 1)
        return QJS_INTL_DATA_ERROR;
    return qjs_intl_segment_routing_open(&service->allocator, &service->routing);
}

QJSIntlStatus qjs_intl_segment_open(const QJSIntlAllocator *allocator,
                                    const struct QJSIntlData *checked_data,
                                    QJSIntlSegmentService **out)
{
    QJSIntlSegmentService *service;
    QJSIntlStatus status;

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
    service->references = 1;
    status = admit(service, checked_data);
    if (status) {
        qjs_intl_segment_close(service);
        return status;
    }
    *out = service;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_available(const QJSIntlSegmentService *service,
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

QJSIntlStatus qjs_intl_segmenter_open(const QJSIntlSegmentService *service, size_t locale,
                                      QJSIntlGranularity granularity, QJSIntlSegmenter **out)
{
    QJSIntlSegmenter *segmenter;
    QJSIntlStatus status;
    uint32_t original;

    if (out)
        *out = NULL;
    if (!service || !out || locale >= service->data.locales.count - 1 ||
        (unsigned int)granularity > QJS_INTL_SEGMENT_SENTENCE)
        return QJS_INTL_INVALID_ARGUMENT;
    if (service->references == SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    segmenter = service->allocator.malloc(service->allocator.opaque, sizeof(*segmenter));
    if (!segmenter)
        return QJS_INTL_NO_MEMORY;
    memset(segmenter, 0, sizeof(*segmenter));
    original = (uint32_t)locale + (locale >= service->root);
    status = qjs_intl_segment_program_find(&service->programs, original, granularity,
                                            &segmenter->selected);
    if (!status)
        status = qjs_intl_segment_program_find(&service->programs, original, QJS_INTL_SEGMENT_GRAPHEME,
                                                &segmenter->grapheme);
    if (!status)
        status = qjs_intl_segment_suppression_find(&service->suppressions, original,
                                                   &segmenter->suppressions);
    if (status) {
        service->allocator.free(service->allocator.opaque, segmenter);
        return status;
    }
    segmenter->service = (QJSIntlSegmentService *)service;
    segmenter->service->references++;
    segmenter->granularity = granularity;
    segmenter->rules = (QJSIntlSegmentNativeRules){
        &segmenter->selected, &segmenter->grapheme, &segmenter->suppressions,
        &service->dictionaries, service->routing
    };
    *out = segmenter;
    return QJS_INTL_OK;
}

void qjs_intl_segmenter_close(QJSIntlSegmenter *segmenter)
{
    if (segmenter) {
        QJSIntlSegmentService *service = segmenter->service;

        service->allocator.free(service->allocator.opaque, segmenter);
        qjs_intl_segment_close(service);
    }
}

QJSIntlStatus qjs_intl_segment_input_prepare(const QJSIntlSegmenter *segmenter, const QJSIntlUTF16 *text,
                                             QJSIntlSegmentInput *out)
{
    QJSIntlSegmentNativeInput *state;
    QJSIntlStatus status;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!segmenter || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_segment_native_input_prepare(&segmenter->service->allocator,
                                                    &segmenter->rules, text, &state);
    if (!status)
        *out = (QJSIntlSegmentInput){*text, segmenter->granularity, state};
    return status;
}

void qjs_intl_segment_input_clear(QJSIntlSegmentInput *input)
{
    if (input) {
        qjs_intl_segment_native_input_close(input->state);
        memset(input, 0, sizeof(*input));
    }
}

QJSIntlStatus qjs_intl_segment_next(const QJSIntlSegmentInput *input, size_t start, QJSIntlSegment *out)
{
    return qjs_intl_segment_native_input_next(input ? input->state : NULL, start, out);
}

QJSIntlStatus qjs_intl_segment_containing(const QJSIntlSegmentInput *input, size_t index,
                                          QJSIntlSegment *out)
{
    return qjs_intl_segment_native_input_containing(input ? input->state : NULL, index, out);
}
