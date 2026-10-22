/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "available-source.h"
#include <stdlib.h>
#include <string.h>

struct QJSIntlDateAvailableSource {
    QJSIntlDateAvailableEntry entry;
    uint32_t node;
    size_t rank;
};

static int compare_bytes(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t common;
    int order;
    if (a.data == b.data && a.length == b.length)
        return 0;
    common = a.length < b.length ? a.length : b.length;
    order = common ? memcmp(a.data, b.data, common) : 0;
    return order ? order : (a.length > b.length) - (a.length < b.length);
}

static int compare_key(const QJSIntlDateAvailableSource *a, const QJSIntlDateAvailableSource *b)
{
    int order = compare_bytes(a->entry.code, b->entry.code);
    if (order)
        return order;
    if (a->entry.plural != b->entry.plural)
        return a->entry.plural < b->entry.plural ? -1 : 1;
    return compare_bytes(a->entry.alt, b->entry.alt);
}

static int compare_record(const void *left, const void *right)
{
    const QJSIntlDateAvailableSource *a = left, *b = right;
    int order = compare_key(a, b);
    return order ? order : (a->rank > b->rank) - (a->rank < b->rank);
}

static QJSIntlStatus gather(const QJSIntlDateAvailableData *data, uint32_t head,
                           uint8_t *seen, QJSIntlDateAvailableSource *records, size_t *out)
{
    size_t count = 0, rank = 0;
    while (head) {
        QJSIntlDateAvailableNode node;
        QJSIntlStatus status = qjs_intl_date_available_node(data, head, &node);
        if (status)
            return status;
        /* Repeating an identical physical map cannot contribute a new key:
           its nearest occurrence already shadows every later use. This also
           bounds gathered entries by physical storage instead of expanding
           each shared map for every node in the source chain. */
        if (!seen[node.map]) {
            uint32_t start, length, i;
            seen[node.map] = 1;
            status = qjs_intl_date_available_map(data, node.map, &start, &length);
            if (status)
                return status;
            if (length > SIZE_MAX - count)
                return QJS_INTL_OVERFLOW;
            if (records) {
                for (i = 0; i < length; i++) {
                    status = qjs_intl_date_available_entry_at(data, start + i, &records[count + i].entry);
                    if (status)
                        return status;
                    records[count + i].node = head;
                    records[count + i].rank = rank;
                }
            }
            count += length;
        }
        head = node.parent;
        rank++;
    }
    *out = count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_available_sources_open(const QJSIntlDateAvailableData *data,
                                                uint32_t head, const QJSIntlAllocator *allocator,
                                                QJSIntlDateAvailableSources *out)
{
    QJSIntlDateAvailableSources result = {0};
    QJSIntlStatus status;
    uint8_t *seen;
    size_t capacity, written, i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || head > data->node_count ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    result.allocator = *allocator;
    if (!head) {
        *out = result;
        return QJS_INTL_OK;
    }
    if (!data->map_count)
        return QJS_INTL_INVALID_ARGUMENT;
    seen = allocator->malloc(allocator->opaque, data->map_count);
    if (!seen)
        return QJS_INTL_NO_MEMORY;
    memset(seen, 0, data->map_count);
    status = gather(data, head, seen, NULL, &capacity);
    if (status)
        goto done;
    if (capacity > SIZE_MAX / sizeof(*result.records)) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    if (capacity) {
        result.records = allocator->malloc(allocator->opaque, capacity * sizeof(*result.records));
        if (!result.records) {
            status = QJS_INTL_NO_MEMORY;
            goto done;
        }
        memset(seen, 0, data->map_count);
        status = gather(data, head, seen, result.records, &written);
        if (status)
            goto done;
        if (written > 1)
            qsort(result.records, written, sizeof(*result.records), compare_record);
        for (i = 0; i < written; i++) {
            if (result.count && !compare_key(&result.records[result.count - 1], &result.records[i]))
                continue;
            if (result.count != i)
                result.records[result.count] = result.records[i];
            result.count++;
        }
    }
    *out = result;
done:
    allocator->free(allocator->opaque, seen);
    if (status && result.records)
        allocator->free(allocator->opaque, result.records);
    return status;
}

void qjs_intl_date_available_sources_clear(QJSIntlDateAvailableSources *sources)
{
    if (!sources)
        return;
    if (sources->records)
        sources->allocator.free(sources->allocator.opaque, sources->records);
    memset(sources, 0, sizeof(*sources));
}

QJSIntlStatus qjs_intl_date_available_source_at(const QJSIntlDateAvailableSources *sources,
                                             size_t index, QJSIntlDateAvailableEntry *entry,
                                             QJSIntlDateAvailableNode *origin)
{
    QJSIntlStatus status;
    if (entry)
        memset(entry, 0, sizeof(*entry));
    if (origin)
        memset(origin, 0, sizeof(*origin));
    if (!sources || !entry || !origin || index >= sources->count)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_available_node(&sources->data, sources->records[index].node, origin);
    if (status)
        return status;
    *entry = sources->records[index].entry;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_available_source_find(const QJSIntlDateAvailableSources *sources,
                                                  QJSIntlBytes code, QJSIntlDateAvailablePlural plural,
                                                  QJSIntlBytes leaf_alt,
                                                  QJSIntlDateAvailableEntry *entry,
                                                  QJSIntlDateAvailableNode *origin)
{
    QJSIntlDateAvailableSource key = {0};
    size_t lo = 0, hi;
    if (entry)
        memset(entry, 0, sizeof(*entry));
    if (origin)
        memset(origin, 0, sizeof(*origin));
    if (!sources || !entry || !origin || !sources->data.section.bytes ||
        (!code.data && code.length) || (!leaf_alt.data && leaf_alt.length) ||
        (unsigned int)plural >= QJS_INTL_DATE_PLURAL_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    key.entry.code = code;
    key.entry.plural = plural;
    key.entry.alt = leaf_alt;
    hi = sources->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (compare_key(&sources->records[mid], &key) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == sources->count || compare_key(&sources->records[lo], &key))
        return QJS_INTL_UNSUPPORTED;
    return qjs_intl_date_available_source_at(sources, lo, entry, origin);
}
