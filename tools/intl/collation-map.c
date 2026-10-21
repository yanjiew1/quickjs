/* Hashed trie edges avoid quadratic insertion of large starred tailorings.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-map.h"
#include <string.h>

#define NONE QJS_INTL_COLLATION_NODE_NONE
#define MASK QJS_INTL_COLLATION_SCALAR_MASK

typedef struct QJSIntlMapNode {
    uint32_t parent, child, sibling, sequence, suffix, code, live_previous, live_next;
    int suppressed;
} Node;

typedef struct QJSIntlMapEdge {
    uint32_t parent, code, node;
} Edge;

static int scalar(uint32_t code)
{
    return code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff);
}

static QJSIntlStatus reserve_nodes(QJSIntlCollationMap *map)
{
    size_t capacity;
    Node *next;
    if (map->node_count == NONE)
        return QJS_INTL_OVERFLOW;
    if (map->node_count < map->node_capacity)
        return QJS_INTL_OK;
    capacity = map->node_capacity ? map->node_capacity * 2 : 64;
    if (capacity < map->node_capacity || capacity > NONE)
        capacity = NONE;
    if (capacity > SIZE_MAX / sizeof(*next))
        return QJS_INTL_OVERFLOW;
    next = map->nodes ? map->allocator.realloc(map->allocator.opaque, map->nodes,
                                               capacity * sizeof(*next)) :
        map->allocator.malloc(map->allocator.opaque, capacity * sizeof(*next));
    if (!next)
        return QJS_INTL_NO_MEMORY;
    map->nodes = next;
    map->node_capacity = capacity;
    return QJS_INTL_OK;
}

static uint32_t append_node(QJSIntlCollationMap *map, uint32_t parent, uint32_t code)
{
    uint32_t id = map->node_count++;
    map->nodes[id] = (Node){.parent = parent, .child = NONE, .sibling = NONE,
        .sequence = NONE, .suffix = NONE, .code = code, .live_previous = NONE, .live_next = NONE};
    return id;
}

QJSIntlStatus qjs_intl_collation_map_init(QJSIntlCollationMap *map, const QJSIntlAllocator *allocator)
{
    QJSIntlStatus status;
    if (!map)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(map, 0, sizeof(*map));
    if (!allocator || !allocator->malloc || !allocator->realloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    map->allocator = *allocator;
    map->live_first = map->live_last = NONE;
    status = reserve_nodes(map);
    if (status)
        return status;
    append_node(map, NONE, NONE); /* Reverse-prefix trie root, grouped by first scalar. */
    return QJS_INTL_OK;
}

void qjs_intl_collation_map_clear(QJSIntlCollationMap *map)
{
    uint32_t i;
    if (!map)
        return;
    for (i = 0; i < map->sequence_count; i++)
        qjs_intl_relation_mapping_clear(&map->allocator, map->sequences + i);
    if (map->sequences)
        map->allocator.free(map->allocator.opaque, map->sequences);
    if (map->edges)
        map->allocator.free(map->allocator.opaque, map->edges);
    if (map->nodes)
        map->allocator.free(map->allocator.opaque, map->nodes);
    memset(map, 0, sizeof(*map));
}

static size_t edge_hash(uint32_t parent, uint32_t code)
{
    uint64_t key = (uint64_t)parent << 32 | code;
    key ^= key >> 30;
    key *= UINT64_C(0xbf58476d1ce4e5b9);
    key ^= key >> 27;
    key *= UINT64_C(0x94d049bb133111eb);
    return (size_t)(key ^ (key >> 31));
}

static Edge *edge_slot(Edge *edges, size_t capacity, uint32_t parent, uint32_t code)
{
    size_t at = edge_hash(parent, code) & (capacity - 1);
    while (edges[at].node != NONE &&
           (edges[at].parent != parent || edges[at].code != code))
        at = (at + 1) & (capacity - 1);
    return edges + at;
}

static uint32_t child(const QJSIntlCollationMap *map, uint32_t parent, uint32_t code)
{
    if (!map->edge_capacity)
        return NONE;
    return edge_slot(map->edges, map->edge_capacity, parent, code)->node;
}

static QJSIntlStatus reserve_edges(QJSIntlCollationMap *map)
{
    size_t capacity, i;
    Edge *next;
    if (map->edge_capacity && map->edge_count < map->edge_capacity / 2)
        return QJS_INTL_OK;
    if (map->edge_capacity > SIZE_MAX / 2)
        return QJS_INTL_OVERFLOW;
    capacity = map->edge_capacity ? map->edge_capacity * 2 : 128;
    if (capacity > SIZE_MAX / sizeof(*next))
        return QJS_INTL_OVERFLOW;
    next = map->allocator.malloc(map->allocator.opaque, capacity * sizeof(*next));
    if (!next)
        return QJS_INTL_NO_MEMORY;
    for (i = 0; i < capacity; i++)
        next[i].node = NONE;
    for (i = 0; i < map->edge_capacity; i++) {
        Edge old = map->edges[i];
        if (old.node != NONE)
            *edge_slot(next, capacity, old.parent, old.code) = old;
    }
    if (map->edges)
        map->allocator.free(map->allocator.opaque, map->edges);
    map->edges = next;
    map->edge_capacity = capacity;
    return QJS_INTL_OK;
}

static QJSIntlStatus ensure_child(QJSIntlCollationMap *map, uint32_t parent, uint32_t code,
                                   uint32_t *out)
{
    QJSIntlStatus status;
    uint32_t id = child(map, parent, code);
    if (id == NONE) {
        status = reserve_nodes(map);
        if (status)
            return status;
        status = reserve_edges(map);
        if (status)
            return status;
        id = append_node(map, parent, code);
        map->nodes[id].sibling = map->nodes[parent].child;
        map->nodes[parent].child = id;
        *edge_slot(map->edges, map->edge_capacity, parent, code) = (Edge){parent, code, id};
        map->edge_count++;
    }
    *out = id;
    return QJS_INTL_OK;
}

static QJSIntlStatus reserve_sequences(QJSIntlCollationMap *map)
{
    size_t capacity;
    QJSIntlRelationMapping *next;
    if (map->sequence_count == NONE)
        return QJS_INTL_OVERFLOW;
    if (map->sequence_count < map->sequence_capacity)
        return QJS_INTL_OK;
    capacity = map->sequence_capacity ? map->sequence_capacity * 2 : 64;
    if (capacity < map->sequence_capacity || capacity > NONE)
        capacity = NONE;
    if (capacity > SIZE_MAX / sizeof(*next))
        return QJS_INTL_OVERFLOW;
    next = map->sequences ? map->allocator.realloc(map->allocator.opaque, map->sequences,
                                                   capacity * sizeof(*next)) :
        map->allocator.malloc(map->allocator.opaque, capacity * sizeof(*next));
    if (!next)
        return QJS_INTL_NO_MEMORY;
    map->sequences = next;
    map->sequence_capacity = capacity;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_map_put(QJSIntlCollationMap *map,
                                        const uint32_t *prefix, uint32_t prefix_count,
                                        const uint32_t *suffix, uint32_t suffix_count,
                                        const QJSIntlRelationElement *elements, size_t element_count,
                                        uint32_t *sequence)
{
    QJSIntlRelationMapping copy = {0};
    QJSIntlStatus status;
    uint32_t node, i;
    if (sequence)
        *sequence = NONE;
    if (!map || !map->node_count || !sequence || (!prefix && prefix_count) ||
        !suffix || !suffix_count || (!elements && element_count))
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < prefix_count; i++)
        if (!scalar(prefix[i]))
            return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < suffix_count; i++)
        if (!scalar(suffix[i]))
            return QJS_INTL_INVALID_ARGUMENT;
    if (element_count > SIZE_MAX / sizeof(*elements))
        return QJS_INTL_OVERFLOW;
    if (element_count) {
        copy.elements = map->allocator.malloc(map->allocator.opaque, element_count * sizeof(*elements));
        if (!copy.elements)
            return QJS_INTL_NO_MEMORY;
        memcpy(copy.elements, elements, element_count * sizeof(*elements));
    }
    copy.count = element_count;
    status = reserve_sequences(map);
    if (status)
        goto fail;
    status = ensure_child(map, 0, suffix[0], &node);
    if (status)
        goto fail;
    /* Reverse prefix keys make longest contiguous lookback independent of the
       number of other prefixes/locales. Each prefix has its own suffix trie. */
    for (i = prefix_count; i; i--) {
        status = ensure_child(map, node, prefix[i - 1], &node);
        if (status)
            goto fail;
    }
    if (map->nodes[node].suffix == NONE) {
        status = reserve_nodes(map);
        if (status)
            goto fail;
        /* A suffix root's parent records its reverse-prefix owner. Traversal
           of either trie still stops at its own explicit root. */
        map->nodes[node].suffix = append_node(map, node, NONE);
    }
    node = map->nodes[node].suffix;
    for (i = 0; i < suffix_count; i++) {
        status = ensure_child(map, node, suffix[i], &node);
        if (status)
            goto fail;
    }
    /* Only this terminal publication changes the visible mapping. Old sequence
       arrays remain immutable for copied expansions; current visits follow
       only the live terminals, not the retained sequence arena. */
    *sequence = map->sequence_count++;
    map->sequences[*sequence] = copy;
    if (map->nodes[node].sequence == NONE) {
        map->nodes[node].live_previous = map->live_last;
        if (map->live_last == NONE)
            map->live_first = node;
        else
            map->nodes[map->live_last].live_next = node;
        map->live_last = node;
        map->live_count++;
    }
    map->nodes[node].sequence = *sequence;
    return QJS_INTL_OK;
fail:
    qjs_intl_relation_mapping_clear(&map->allocator, &copy);
    return status;
}

QJSIntlStatus qjs_intl_collation_map_sequence(const QJSIntlCollationMap *map, uint32_t id,
                                             QJSIntlRelationMapping *out)
{
    if (out)
        *out = (QJSIntlRelationMapping){0};
    if (!map || !out || id >= map->sequence_count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = map->sequences[id];
    return QJS_INTL_OK;
}

/* Depth-first traversal without recursion or scratch allocation. Prefix and
   suffix roots are separate trees; all sibling/parent links are stable IDs. */
static uint32_t subtree_next(const QJSIntlCollationMap *map, uint32_t root, uint32_t node)
{
    if (map->nodes[node].child != NONE)
        return map->nodes[node].child;
    while (node != root) {
        if (map->nodes[node].sibling != NONE)
            return map->nodes[node].sibling;
        node = map->nodes[node].parent;
    }
    return NONE;
}

static void unlink_terminal(QJSIntlCollationMap *map, uint32_t id)
{
    Node *node = map->nodes + id;
    if (node->sequence == NONE)
        return;
    if (node->live_previous == NONE)
        map->live_first = node->live_next;
    else
        map->nodes[node->live_previous].live_next = node->live_next;
    if (node->live_next == NONE)
        map->live_last = node->live_previous;
    else
        map->nodes[node->live_next].live_previous = node->live_previous;
    node->live_previous = node->live_next = NONE;
    node->sequence = NONE;
    map->live_count--;
}

QJSIntlStatus qjs_intl_collation_map_suppress(QJSIntlCollationMap *map, uint32_t code)
{
    QJSIntlStatus status;
    uint32_t first, prefix;
    if (!map || !map->node_count || !scalar(code))
        return QJS_INTL_INVALID_ARGUMENT;
    /* Materialize the first-scalar node even when no overlay mapping exists,
       so suppression of inherited root contexts has a persistent owner. */
    status = ensure_child(map, 0, code, &first);
    if (status)
        return status;
    map->nodes[first].suppressed = 1;
    for (prefix = first; prefix != NONE; prefix = subtree_next(map, first, prefix)) {
        uint32_t root = map->nodes[prefix].suffix, node;
        if (root == NONE)
            continue;
        for (node = root; node != NONE; node = subtree_next(map, root, node))
            if (prefix != first || map->nodes[node].parent != root)
                unlink_terminal(map, node);
    }
    return QJS_INTL_OK;
}

static uint32_t overlay_prefix(const void *opaque, const uint32_t *text, uint32_t start,
                                uint32_t limit)
{
    const QJSIntlCollationMap *map = opaque;
    uint32_t node = child(map, 0, text[start] & MASK), count, best = 0;
    for (count = 1; node != NONE && count <= start && count < limit; count++) {
        node = child(map, node, text[start - count] & MASK);
        if (node != NONE && map->nodes[node].suffix != NONE)
            best = count;
    }
    return best;
}

static uint32_t overlay_begin(const void *opaque, uint32_t first, const uint32_t *prefix,
                               uint32_t count)
{
    const QJSIntlCollationMap *map = opaque;
    uint32_t node = child(map, 0, first), i;
    for (i = count; node != NONE && i; i--)
        node = child(map, node, prefix[i - 1] & MASK);
    return node == NONE ? NONE : map->nodes[node].suffix;
}

static uint32_t overlay_step(const void *opaque, uint32_t node, uint32_t code)
{
    return child(opaque, node, code);
}

static uint32_t overlay_terminal(const void *opaque, uint32_t node)
{
    return ((const QJSIntlCollationMap *)opaque)->nodes[node].sequence;
}

static int overlay_children(const void *opaque, uint32_t node)
{
    return ((const QJSIntlCollationMap *)opaque)->nodes[node].child != NONE;
}

static int overlay_suppressed(const void *opaque, uint32_t code)
{
    const QJSIntlCollationMap *map = opaque;
    uint32_t node = child(map, 0, code);
    return node != NONE && map->nodes[node].suppressed;
}

QJSIntlStatus qjs_intl_collation_map_overlay(const QJSIntlCollationMap *map,
                                            QJSIntlCollationOverlay *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!map || !map->node_count || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = (QJSIntlCollationOverlay){map, overlay_prefix, overlay_begin, overlay_step,
                                     overlay_terminal, overlay_children, overlay_suppressed};
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_map_visit(const QJSIntlCollationMap *map,
                                          QJSIntlCollationMapVisitor visitor, void *opaque)
{
    uint32_t node, count = 0;
    if (!map || !map->node_count || !visitor)
        return QJS_INTL_INVALID_ARGUMENT;
    for (node = map->live_first; node != NONE; node = map->nodes[node].live_next) {
        uint32_t sequence;
        QJSIntlStatus status;
        if (node >= map->node_count || count++ >= map->live_count ||
            (sequence = map->nodes[node].sequence) >= map->sequence_count)
            return QJS_INTL_DATA_ERROR;
        status = visitor(opaque, node, sequence, map->sequences + sequence);
        if (status)
            return status;
    }
    return count == map->live_count ? QJS_INTL_OK : QJS_INTL_DATA_ERROR;
}

typedef struct KeyInfo {
    uint32_t prefix_count, suffix_count, prefix_owner;
} KeyInfo;

static QJSIntlStatus key_info(const QJSIntlCollationMap *map, uint32_t id, KeyInfo *out)
{
    uint32_t node, steps = 0;
    *out = (KeyInfo){0};
    if (!map || !map->node_count || id >= map->node_count || map->nodes[id].sequence == NONE)
        return QJS_INTL_INVALID_ARGUMENT;
    /* Reverse the suffix-parent chain only when copying; counting allocates
       nothing and checks the complete path before either output is modified. */
    node = id;
    while (node < map->node_count && map->nodes[node].code != NONE) {
        if (++steps > map->node_count)
            return QJS_INTL_DATA_ERROR;
        out->suffix_count++;
        node = map->nodes[node].parent;
    }
    if (node >= map->node_count || !out->suffix_count ||
        (node = map->nodes[node].parent) >= map->node_count)
        return QJS_INTL_DATA_ERROR;
    out->prefix_owner = node;
    steps = 0;
    while (map->nodes[node].parent != 0) {
        if (++steps > map->node_count || map->nodes[node].code == NONE)
            return QJS_INTL_DATA_ERROR;
        out->prefix_count++;
        node = map->nodes[node].parent;
        if (node >= map->node_count)
            return QJS_INTL_DATA_ERROR;
    }
    return map->nodes[node].code == NONE ? QJS_INTL_DATA_ERROR : QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_map_key_size(const QJSIntlCollationMap *map, uint32_t id,
                                             uint32_t *prefix_count, uint32_t *suffix_count)
{
    KeyInfo key;
    QJSIntlStatus status;
    if (prefix_count)
        *prefix_count = 0;
    if (suffix_count)
        *suffix_count = 0;
    if (!prefix_count || !suffix_count)
        return QJS_INTL_INVALID_ARGUMENT;
    status = key_info(map, id, &key);
    if (!status) {
        *prefix_count = key.prefix_count;
        *suffix_count = key.suffix_count;
    }
    return status;
}

QJSIntlStatus qjs_intl_collation_map_key_copy(const QJSIntlCollationMap *map, uint32_t id,
                                             uint32_t *prefix, size_t prefix_capacity,
                                             uint32_t *suffix, size_t suffix_capacity)
{
    KeyInfo key;
    QJSIntlStatus status = key_info(map, id, &key);
    uint32_t node = id, i;
    if (status)
        return status;
    if ((!prefix && key.prefix_count) || !suffix)
        return QJS_INTL_INVALID_ARGUMENT;
    if (prefix_capacity < key.prefix_count || suffix_capacity < key.suffix_count)
        return QJS_INTL_OVERFLOW;
    for (i = key.suffix_count; i; i--) {
        suffix[i - 1] = map->nodes[node].code;
        node = map->nodes[node].parent;
    }
    /* Reverse-prefix insertion stores the last prefix scalar nearest its
       first-suffix group. Walking back from its owner restores original order. */
    node = key.prefix_owner;
    for (i = 0; i < key.prefix_count; i++) {
        prefix[i] = map->nodes[node].code;
        node = map->nodes[node].parent;
    }
    return QJS_INTL_OK;
}
