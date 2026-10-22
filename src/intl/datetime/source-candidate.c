/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "source-candidate.h"
#include <string.h>

static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}

static QJSIntlStatus selected(const QJSIntlDateAvailableSources *sources,
                              const QJSIntlDateAvailableEntry *entry,
                              QJSIntlBytes preferred, QJSIntlBytes fallback, int *out)
{
    QJSIntlDateAvailableEntry other;
    QJSIntlDateAvailableNode origin;
    QJSIntlStatus status;
    *out = 0;
    if (entry->plural != QJS_INTL_DATE_NO_PLURAL)
        return QJS_INTL_OK;
    if (!equal(entry->alt, preferred)) {
        if (!equal(entry->alt, fallback))
            return QJS_INTL_OK;
        /* Process a key once, at its preferred record when present. That
           attempt may choose the separately resolved fallback key below. */
        status = qjs_intl_date_available_source_find(sources, entry->code, entry->plural,
                                                      preferred, &other, &origin);
        if (!status)
            return QJS_INTL_OK;
        if (status != QJS_INTL_UNSUPPORTED)
            return status;
    }
    *out = !equal(preferred, fallback) || (!entry->blocked && entry->text.length);
    return QJS_INTL_OK;
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

static QJSIntlStatus compile_candidate(const QJSIntlAllocator *allocator,
                                       QJSIntlDateSourceCandidate *record)
{
    QJSIntlStatus status;
    if (record->source.blocked || !record->source.text.length)
        return QJS_INTL_UNSUPPORTED;
    status = qjs_intl_date_pattern_compile(allocator, record->source.text, &record->pattern);
    if (!status) {
        qjs_intl_date_pattern_fix_calendar_year(&record->pattern);
        status = qjs_intl_date_pattern_info(&record->pattern, &record->info);
        if (!status && !useful(&record->info))
            status = QJS_INTL_UNSUPPORTED;
    }
    if (status)
        qjs_intl_date_pattern_clear(allocator, &record->pattern);
    return status;
}

QJSIntlStatus qjs_intl_date_source_candidates_preferred(const QJSIntlDateAvailableSources *sources,
                                                        QJSIntlBytes leaf_alt, QJSIntlBytes fallback,
                                                        const QJSIntlAllocator *allocator,
                                                        QJSIntlDateSourceCandidates *out)
{
    QJSIntlDateSourceCandidates result = {0};
    QJSIntlStatus status;
    size_t i, capacity = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !sources || !sources->data.section.bytes || (!leaf_alt.data && leaf_alt.length) ||
        (!fallback.data && fallback.length) || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = sources->data;
    result.allocator = *allocator;
    /* Validate the semantic family before filtering any leaf, including an
       otherwise ineligible append template. Never interpret templates here. */
    for (i = 0; i < sources->count; i++) {
        QJSIntlDateAvailableEntry entry;
        QJSIntlDateAvailableNode origin;
        QJSIntlDateAvailableGroup group;
        int choose;
        status = qjs_intl_date_available_source_at(sources, i, &entry, &origin);
        if (status)
            return status;
        status = qjs_intl_date_available_group_at(&sources->data, origin.group, &group);
        if (status)
            return status;
        if (group.family != QJS_INTL_DATE_AVAILABLE_PATTERN)
            return QJS_INTL_INVALID_ARGUMENT;
        status = selected(sources, &entry, leaf_alt, fallback, &choose);
        if (status)
            return status;
        capacity += choose;
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
        int choose;
        status = qjs_intl_date_available_source_at(sources, i, &record.source, &record.origin);
        if (status)
            goto fail;
        status = selected(sources, &record.source, leaf_alt, fallback, &choose);
        if (status)
            goto fail;
        if (!choose)
            continue;
        status = compile_candidate(allocator, &record);
        if ((status == QJS_INTL_UNSUPPORTED || status == QJS_INTL_DATA_ERROR) &&
            !equal(leaf_alt, fallback) && equal(record.source.alt, leaf_alt)) {
            QJSIntlStatus unusable = status;
            /* Fallback is another exact visible key, not a masked ancestor
               of the failed preferred key. Operational errors never fall back. */
            status = qjs_intl_date_available_source_find(sources, record.source.code, record.source.plural,
                                                          fallback, &record.source, &record.origin);
            if (!status)
                status = compile_candidate(allocator, &record);
            else if (status == QJS_INTL_UNSUPPORTED)
                status = unusable;
        }
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (status)
            goto fail;
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

QJSIntlStatus qjs_intl_date_source_candidates_open(const QJSIntlDateAvailableSources *sources,
                                                 QJSIntlBytes leaf_alt,
                                                 const QJSIntlAllocator *allocator,
                                                 QJSIntlDateSourceCandidates *out)
{
    return qjs_intl_date_source_candidates_preferred(sources, leaf_alt, leaf_alt, allocator, out);
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
