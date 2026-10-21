/* Stable symbolic insertions between virtual root coordinates.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-weights.h"
#include <stdlib.h>
#include <string.h>

#define TAILORED UINT32_C(0x80000000)
#define NONE QJS_INTL_WEIGHT_NONE

typedef struct QJSIntlWeightNode {
    uint32_t root, previous, next, rank;
    QJSIntlWeightParents parents;
} Node;

typedef struct QJSIntlWeightGap {
    uint32_t root, first, last, count;
} Gap;

typedef struct QJSIntlWeightOffset {
    uint32_t root, added;
} Offset;

QJSIntlStatus qjs_intl_weight_graph_init(QJSIntlWeightGraph *graph,
                                        const QJSIntlAllocator *allocator, uint32_t root_max)
{
    if (!graph)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(graph, 0, sizeof(*graph));
    if (!allocator || !allocator->malloc || !allocator->realloc || !allocator->free ||
        root_max >= TAILORED)
        return QJS_INTL_INVALID_ARGUMENT;
    graph->allocator = *allocator;
    graph->root_max = root_max;
    return QJS_INTL_OK;
}

void qjs_intl_weight_graph_clear(QJSIntlWeightGraph *graph)
{
    if (!graph)
        return;
    if (graph->nodes)
        graph->allocator.free(graph->allocator.opaque, graph->nodes);
    if (graph->gaps)
        graph->allocator.free(graph->allocator.opaque, graph->gaps);
    if (graph->offsets)
        graph->allocator.free(graph->allocator.opaque, graph->offsets);
    memset(graph, 0, sizeof(*graph));
}

static uint32_t hash_root(uint32_t root)
{
    root ^= root >> 16;
    root *= UINT32_C(0x7feb352d);
    root ^= root >> 15;
    root *= UINT32_C(0x846ca68b);
    return root ^ (root >> 16);
}

static Gap *gap_slot(Gap *gaps, size_t capacity, uint32_t root)
{
    size_t position = hash_root(root) & (capacity - 1);
    while (gaps[position].root != NONE && gaps[position].root != root)
        position = (position + 1) & (capacity - 1);
    return gaps + position;
}

static QJSIntlStatus reserve_gap(QJSIntlWeightGraph *graph, uint32_t root, Gap **out)
{
    Gap *gaps, *slot;
    size_t capacity, i;
    if (graph->gap_capacity) {
        slot = gap_slot(graph->gaps, graph->gap_capacity, root);
        if (slot->root != NONE) {
            *out = slot;
            return QJS_INTL_OK;
        }
    }
    if (!graph->gap_capacity || graph->gap_count >= graph->gap_capacity / 2) {
        if (graph->gap_capacity > SIZE_MAX / 2)
            return QJS_INTL_OVERFLOW;
        capacity = graph->gap_capacity ? graph->gap_capacity * 2 : 16;
        if (capacity > SIZE_MAX / sizeof(*gaps))
            return QJS_INTL_OVERFLOW;
        gaps = graph->allocator.malloc(graph->allocator.opaque, capacity * sizeof(*gaps));
        if (!gaps)
            return QJS_INTL_NO_MEMORY;
        for (i = 0; i < capacity; i++)
            gaps[i].root = NONE;
        for (i = 0; i < graph->gap_capacity; i++) {
            Gap old = graph->gaps[i];
            if (old.root != NONE)
                *gap_slot(gaps, capacity, old.root) = old;
        }
        if (graph->gaps)
            graph->allocator.free(graph->allocator.opaque, graph->gaps);
        graph->gaps = gaps;
        graph->gap_capacity = capacity;
    }
    slot = gap_slot(graph->gaps, graph->gap_capacity, root);
    *slot = (Gap){root, NONE, NONE, 0};
    graph->gap_count++;
    *out = slot;
    return QJS_INTL_OK;
}

static QJSIntlStatus reserve_node(QJSIntlWeightGraph *graph)
{
    size_t capacity;
    Node *nodes;
    if (graph->node_count == TAILORED - 1)
        return QJS_INTL_OVERFLOW;
    if (graph->node_count < graph->node_capacity)
        return QJS_INTL_OK;
    capacity = graph->node_capacity ? graph->node_capacity * 2 : 64;
    if (capacity > TAILORED - 1)
        capacity = TAILORED - 1;
    if (capacity > SIZE_MAX / sizeof(*nodes))
        return QJS_INTL_OVERFLOW;
    if (graph->nodes)
        nodes = graph->allocator.realloc(graph->allocator.opaque, graph->nodes,
                                          capacity * sizeof(*nodes));
    else
        nodes = graph->allocator.malloc(graph->allocator.opaque, capacity * sizeof(*nodes));
    if (!nodes)
        return QJS_INTL_NO_MEMORY;
    graph->nodes = nodes;
    graph->node_capacity = capacity;
    return QJS_INTL_OK;
}

static int valid_id(const QJSIntlWeightGraph *graph, uint32_t id)
{
    return id & TAILORED ? (id & ~TAILORED) < graph->node_count : id <= graph->root_max;
}

QJSIntlStatus qjs_intl_weight_insert(QJSIntlWeightGraph *graph, uint32_t reference, int before,
                                    const QJSIntlWeightParents *parents, uint32_t *id)
{
    QJSIntlStatus status;
    uint32_t root, previous, next, added;
    Gap *gap;
    if (id)
        *id = NONE;
    if (!graph || !id || !graph->allocator.malloc || graph->frozen ||
        !valid_id(graph, reference) || (before && !reference))
        return QJS_INTL_INVALID_ARGUMENT;
    root = reference & TAILORED ? graph->nodes[reference & ~TAILORED].root :
        reference - (before != 0);
    status = reserve_node(graph);
    if (status)
        return status;
    status = reserve_gap(graph, root, &gap);
    if (status)
        return status;
    /* All allocation is complete. Commit links/identity together; arena and
       hash growth never retain pointers into one another's movable storage. */
    if (reference & TAILORED) {
        Node *target = graph->nodes + (reference & ~TAILORED);
        previous = before ? target->previous : reference;
        next = before ? reference : target->next;
    } else {
        previous = before ? gap->last : NONE;
        next = before ? NONE : gap->first;
    }
    added = TAILORED | graph->node_count++;
    graph->nodes[added & ~TAILORED] = (Node){root, previous, next, 0,
        parents ? *parents : (QJSIntlWeightParents){0}};
    if (previous != NONE)
        graph->nodes[previous & ~TAILORED].next = added;
    else
        gap->first = added;
    if (next != NONE)
        graph->nodes[next & ~TAILORED].previous = added;
    else
        gap->last = added;
    gap->count++;
    *id = added;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_weight_parents(const QJSIntlWeightGraph *graph, uint32_t id,
                                     QJSIntlWeightParents *out)
{
    if (out)
        *out = (QJSIntlWeightParents){0};
    if (!graph || !out || !(id & TAILORED) || !valid_id(graph, id))
        return QJS_INTL_INVALID_ARGUMENT;
    *out = graph->nodes[id & ~TAILORED].parents;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_weight_compare(const QJSIntlWeightGraph *graph, uint32_t left,
                                      uint32_t right, int *order)
{
    uint32_t a, b, id, i;
    if (order)
        *order = 0;
    if (!graph || !graph->allocator.malloc || !order ||
        !valid_id(graph, left) || !valid_id(graph, right))
        return QJS_INTL_INVALID_ARGUMENT;
    if (left == right)
        return QJS_INTL_OK;
    if (graph->frozen) {
        qjs_intl_weight_resolve(graph, left, &a);
        qjs_intl_weight_resolve(graph, right, &b);
        *order = a < b ? -1 : 1;
        return QJS_INTL_OK;
    }
    a = left & TAILORED ? graph->nodes[left & ~TAILORED].root : left;
    b = right & TAILORED ? graph->nodes[right & ~TAILORED].root : right;
    if (a != b)
        *order = a < b ? -1 : 1;
    else if (!(left & TAILORED))
        *order = -1; /* The root precedes every insertion in its own gap. */
    else if (!(right & TAILORED))
        *order = 1;
    else {
        /* Numeric IDs record allocation order, not adjacency. Walking this
           one gap leaves all stable identities and future insertion intact. */
        id = graph->nodes[left & ~TAILORED].next;
        for (i = 0; i < graph->node_count; i++) {
            if (id == NONE || id == right) {
                *order = id == right ? -1 : 1;
                return QJS_INTL_OK;
            }
            if (!(id & TAILORED) || !valid_id(graph, id))
                return QJS_INTL_DATA_ERROR;
            id = graph->nodes[id & ~TAILORED].next;
        }
        return QJS_INTL_DATA_ERROR;
    }
    return QJS_INTL_OK;
}

static int offset_order(const void *left, const void *right)
{
    uint32_t a = ((const Offset *)left)->root, b = ((const Offset *)right)->root;
    return a < b ? -1 : a != b;
}

QJSIntlStatus qjs_intl_weight_freeze(QJSIntlWeightGraph *graph)
{
    Offset *offsets;
    uint32_t count = 0, i, previous_added = 0;
    size_t at;
    if (!graph || !graph->allocator.malloc)
        return QJS_INTL_INVALID_ARGUMENT;
    if (graph->frozen)
        return QJS_INTL_OK;
    if ((uint64_t)graph->root_max + graph->node_count >= UINT32_MAX)
        return QJS_INTL_OVERFLOW;
    if (!graph->gap_count) {
        graph->frozen = 1;
        return QJS_INTL_OK;
    }
    if ((uint64_t)graph->gap_count * sizeof(*offsets) > SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    offsets = graph->allocator.malloc(graph->allocator.opaque,
                                       (size_t)graph->gap_count * sizeof(*offsets));
    if (!offsets)
        return QJS_INTL_NO_MEMORY;
    for (at = 0; at < graph->gap_capacity; at++) {
        if (graph->gaps[at].root != NONE)
            offsets[count++] = (Offset){graph->gaps[at].root, 0};
    }
    qsort(offsets, count, sizeof(*offsets), offset_order);
    for (i = 0; i < count; i++) {
        Gap *gap = gap_slot(graph->gaps, graph->gap_capacity, offsets[i].root);
        uint32_t id = gap->first, position = 0;
        while (id != NONE) {
            Node *node = graph->nodes + (id & ~TAILORED);
            node->rank = gap->root + previous_added + ++position;
            id = node->next;
        }
        previous_added += gap->count;
        offsets[i].added = previous_added;
    }
    graph->offsets = offsets;
    graph->frozen = 1;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_weight_resolve(const QJSIntlWeightGraph *graph, uint32_t id, uint32_t *rank)
{
    uint32_t low = 0, high;
    if (rank)
        *rank = 0;
    if (!graph || !rank || !graph->frozen || !valid_id(graph, id))
        return QJS_INTL_INVALID_ARGUMENT;
    if (id & TAILORED) {
        *rank = graph->nodes[id & ~TAILORED].rank;
        return QJS_INTL_OK;
    }
    high = graph->gap_count;
    /* A root precedes insertions in its OWN gap. Include only gaps below it,
       keeping root aliases equal and preserving zero even after gap0 inserts. */
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        if (graph->offsets[middle].root < id)
            low = middle + 1;
        else
            high = middle;
    }
    *rank = id + (low ? graph->offsets[low - 1].added : 0);
    return QJS_INTL_OK;
}
