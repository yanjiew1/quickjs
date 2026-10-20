/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "source-candidate.h"
#include <string.h>

static int eligible(const QJSIntlDateAvailableEntry *entry, QJSIntlBytes alt)
{
    return entry->plural == QJS_INTL_DATE_NO_PLURAL && !entry->blocked && entry->text.length &&
           entry->alt.length == alt.length && (!alt.length || !memcmp(entry->alt.data, alt.data, alt.length));
}

static int useful(const QJSIntlDatePatternInfo *info)
{
    size_t i;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        if (info->components.values[i])
            return 1;
    }
    return 0;
}

void qjs_intl_date_source_candidates_clear(QJSIntlDateSourceCandidates *candidates)
{
    size_t i;
    if (!candidates)
        return;
    for (i = 0; i < candidates->count; i++)
        qjs_intl_date_pattern_clear(&candidates->allocator, &candidates->records[i].pattern);
    if (candidates->records)
        candidates->allocator.free(candidates->allocator.opaque, candidates->records);
    memset(candidates, 0, sizeof(*candidates));
}

QJSIntlStatus qjs_intl_date_source_candidates_open(const QJSIntlDateAvailableSources *sources,
                                                 QJSIntlBytes leaf_alt,
                                                 const QJSIntlAllocator *allocator,
                                                 QJSIntlDateSourceCandidates *out)
{
    QJSIntlDateSourceCandidates result = {0};
    QJSIntlStatus status;
    size_t i, capacity = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !sources || !sources->data.section.bytes || (!leaf_alt.data && leaf_alt.length) ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = sources->data;
    result.allocator = *allocator;
    /* Validate the semantic family before filtering any leaf, including an
       otherwise ineligible append template. Never interpret templates here. */
    for (i = 0; i < sources->count; i++) {
        QJSIntlDateAvailableEntry entry;
        QJSIntlDateAvailableNode origin;
        QJSIntlDateAvailableGroup group;
        status = qjs_intl_date_available_source_at(sources, i, &entry, &origin);
        if (status)
            return status;
        status = qjs_intl_date_available_group_at(&sources->data, origin.group, &group);
        if (status)
            return status;
        if (group.family != QJS_INTL_DATE_AVAILABLE_PATTERN)
            return QJS_INTL_INVALID_ARGUMENT;
        if (eligible(&entry, leaf_alt))
            capacity++;
    }
    if (capacity > SIZE_MAX / sizeof(*result.records))
        return QJS_INTL_OVERFLOW;
    if (capacity) {
        result.records = allocator->malloc(allocator->opaque, capacity * sizeof(*result.records));
        if (!result.records)
            return QJS_INTL_NO_MEMORY;
    }
    for (i = 0; i < sources->count; i++) {
        QJSIntlDateSourceCandidate record = {0};
        status = qjs_intl_date_available_source_at(sources, i, &record.source, &record.origin);
        if (status)
            goto fail;
        if (!eligible(&record.source, leaf_alt))
            continue;
        status = qjs_intl_date_pattern_compile(allocator, record.source.text, &record.pattern);
        if (status)
            goto fail;
        status = qjs_intl_date_pattern_info(&record.pattern, &record.info);
        if (status || !useful(&record.info)) {
            qjs_intl_date_pattern_clear(allocator, &record.pattern);
            if (status && status != QJS_INTL_UNSUPPORTED)
                goto fail;
            continue;
        }
        result.records[result.count++] = record;
    }
    if (!result.count && result.records) {
        allocator->free(allocator->opaque, result.records);
        result.records = NULL;
    }
    *out = result;
    return QJS_INTL_OK;
fail:
    qjs_intl_date_source_candidates_clear(&result);
    return status;
}

QJSIntlStatus qjs_intl_date_source_candidate_at(const QJSIntlDateSourceCandidates *candidates,
                                              size_t index, const QJSIntlDateSourceCandidate **out)
{
    if (out)
        *out = NULL;
    if (!out || !candidates || index >= candidates->count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = &candidates->records[index];
    return QJS_INTL_OK;
}
