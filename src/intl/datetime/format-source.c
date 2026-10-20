/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "format-source.h"
#include <stdlib.h>
#include <string.h>

struct QJSIntlDateFormatSourceMember {
    QJSIntlDateTimeComponents components;
    size_t index;
};

struct QJSIntlDateFormatSourceGroup {
    QJSIntlDateTimeComponents components;
    size_t start, count, first;
};

static int compare_components(const QJSIntlDateTimeComponents *a, const QJSIntlDateTimeComponents *b)
{
    size_t i;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        if (a->values[i] != b->values[i])
            return a->values[i] < b->values[i] ? -1 : 1;
    }
    return 0;
}

static int compare_members(const void *left, const void *right)
{
    const QJSIntlDateFormatSourceMember *a = left, *b = right;
    int order = compare_components(&a->components, &b->components);
    return order ? order : (a->index > b->index) - (a->index < b->index);
}

static int compare_groups(const void *left, const void *right)
{
    const QJSIntlDateFormatSourceGroup *a = left, *b = right;
    return (a->first > b->first) - (a->first < b->first);
}

void qjs_intl_date_format_sources_clear(QJSIntlDateFormatSources *sources)
{
    if (!sources)
        return;
    if (sources->groups)
        sources->allocator.free(sources->allocator.opaque, sources->groups);
    if (sources->members)
        sources->allocator.free(sources->allocator.opaque, sources->members);
    memset(sources, 0, sizeof(*sources));
}

QJSIntlStatus qjs_intl_date_format_sources_open(const QJSIntlDateSourceCandidates *sources,
                                              const QJSIntlAllocator *allocator,
                                              QJSIntlDateFormatSources *out)
{
    QJSIntlDateFormatSources result = {0};
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !sources || (sources->count && !sources->records) ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    if (sources->count > SIZE_MAX / sizeof(*result.members) ||
        sources->count > SIZE_MAX / sizeof(*result.groups))
        return QJS_INTL_OVERFLOW;
    result.allocator = *allocator;
    result.source_count = sources->count;
    if (!sources->count) {
        *out = result;
        return QJS_INTL_OK;
    }
    result.members = allocator->malloc(allocator->opaque, sources->count * sizeof(*result.members));
    if (!result.members)
        return QJS_INTL_NO_MEMORY;
    result.groups = allocator->malloc(allocator->opaque, sources->count * sizeof(*result.groups));
    if (!result.groups) {
        qjs_intl_date_format_sources_clear(&result);
        return QJS_INTL_NO_MEMORY;
    }
    for (i = 0; i < sources->count; i++) {
        result.members[i].components = sources->records[i].info.components;
        result.members[i].index = i;
    }
    if (sources->count > 1)
        qsort(result.members, sources->count, sizeof(*result.members), compare_members);
    for (i = 0; i < sources->count; i++) {
        QJSIntlDateFormatSourceGroup *group;
        if (!result.count || compare_components(&result.groups[result.count - 1].components,
                                                &result.members[i].components)) {
            group = &result.groups[result.count++];
            *group = (QJSIntlDateFormatSourceGroup){result.members[i].components, i, 0, result.members[i].index};
        }
        group = &result.groups[result.count - 1];
        group->count++;
    }
    if (result.count > 1)
        qsort(result.groups, result.count, sizeof(*result.groups), compare_groups);
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_format_source_group(const QJSIntlDateFormatSources *sources,
                                              size_t group, QJSIntlDateTimeComponents *components,
                                              size_t *member_count)
{
    if (components)
        memset(components, 0, sizeof(*components));
    if (member_count)
        *member_count = 0;
    if (!sources || !components || !member_count || group >= sources->count)
        return QJS_INTL_INVALID_ARGUMENT;
    *components = sources->groups[group].components;
    *member_count = sources->groups[group].count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_format_source_member(const QJSIntlDateFormatSources *sources,
                                               size_t group, size_t member, size_t *source_index)
{
    if (source_index)
        *source_index = 0;
    if (!sources || !source_index || group >= sources->count || member >= sources->groups[group].count)
        return QJS_INTL_INVALID_ARGUMENT;
    *source_index = sources->members[sources->groups[group].start + member].index;
    return QJS_INTL_OK;
}
