/* Sort virtual paths once, then construct via adjacent longest common prefixes.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-trie.h"
#include "../../src/intl/native/wire.h"
#include <stdlib.h>
#include <string.h>

#define NONE QJS_INTL_COLLATION_NODE_NONE
#define SEPARATOR UINT32_C(0x110000)

typedef struct Entry {
    const uint8_t *key;
    uint32_t count, prefix, mapping;
} Entry;

typedef struct Node {
    uint32_t count, suffix, terminal;
} Node;

typedef struct Edge {
    uint32_t parent, code, child;
} Edge;

static void put32(uint8_t *p, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        p[i] = value >> (i * 8);
}

/* A virtual path groups by first mapped scalar, reverses the prefix, crosses
   a non-scalar separator, then includes the entire forward suffix. Never copy
   or normalize key text during indexing. The separator becomes a suffix link. */
static uint32_t token(const Entry *entry, uint32_t at)
{
    if (!at)
        at = entry->prefix;
    else if (at <= entry->prefix)
        at = entry->prefix - at;
    else if (at == entry->prefix + 1)
        return SEPARATOR;
    else
        at -= 2;
    return qjs_intl_read_u32(entry->key + (size_t)at * 4);
}

static int order(const void *a, const void *b)
{
    const Entry *left = a, *right = b;
    uint32_t ln = left->count + 2, rn = right->count + 2, i, n = ln < rn ? ln : rn;
    for (i = 0; i < n; i++) {
        uint32_t x = token(left, i), y = token(right, i);
        if (x != y)
            return x < y ? -1 : 1;
    }
    return ln < rn ? -1 : ln != rn;
}

static void *array(const QJSIntlAllocator *a, uint64_t count, size_t size, QJSIntlStatus *status)
{
    void *p;
    if (*status || !count)
        return NULL;
    if (count > SIZE_MAX / size) {
        *status = QJS_INTL_OVERFLOW;
        return NULL;
    }
    p = a->malloc(a->opaque, (size_t)count * size);
    if (!p)
        *status = QJS_INTL_NO_MEMORY;
    return p;
}

void qjs_intl_compiled_trie_clear(QJSIntlCompiledTrie *out)
{
    if (!out)
        return;
    if (out->node_bytes)
        out->allocator.free(out->allocator.opaque, out->node_bytes);
    if (out->edge_bytes)
        out->allocator.free(out->allocator.opaque, out->edge_bytes);
    if (out->suppressed_bytes)
        out->allocator.free(out->allocator.opaque, out->suppressed_bytes);
    memset(out, 0, sizeof(*out));
}

QJSIntlStatus qjs_intl_compiled_trie_create(const QJSIntlCollationSnapshot *snapshot, QJSIntlCompiledTrie *out)
{
    QJSIntlCompiledTrie temp = {0};
    QJSIntlStatus status = QJS_INTL_OK;
    Entry *entries = NULL;
    Node *nodes = NULL;
    Edge *edges = NULL;
    uint32_t *stack = NULL, count = 1, edge_count = 0, i, max_depth = 1, edge_at = 0;
    uint64_t maximum;
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = (QJSIntlCompiledTrie){0};
    if (!snapshot || !snapshot->ready)
        return QJS_INTL_INVALID_ARGUMENT;
    temp.allocator = snapshot->allocator;
    maximum = (uint64_t)snapshot->keys.key_count + (uint64_t)snapshot->keys.row_count * 2 + 1;
    if (maximum >= NONE)
        return QJS_INTL_OVERFLOW;
    entries = array(&temp.allocator, snapshot->keys.row_count, sizeof(*entries), &status);
    for (i = 0; !status && i < snapshot->keys.row_count; i++) {
        QJSIntlCollationKeySpan key;
        uint32_t prefix = qjs_intl_collation_keys_prefix(&snapshot->keys, i);
        status = qjs_intl_collation_keys_at(&snapshot->keys, i, &key);
        if (status)
            goto done;
        if (prefix >= key.count || key.count > UINT32_MAX - 3) {
            status = QJS_INTL_DATA_ERROR;
            goto done;
        }
        entries[i] = (Entry){key.keys, key.count, prefix, i};
        if (key.count + 3 > max_depth)
            max_depth = key.count + 3;
    }
    nodes = array(&temp.allocator, maximum, sizeof(*nodes), &status);
    edges = array(&temp.allocator, maximum - 1, sizeof(*edges), &status);
    stack = array(&temp.allocator, max_depth, sizeof(*stack), &status);
    if (status)
        goto done;
    if (snapshot->keys.row_count > 1)
        qsort(entries, snapshot->keys.row_count, sizeof(*entries), order);
    nodes[0] = (Node){0, NONE, NONE};
    stack[0] = 0;
    for (i = 0; i < snapshot->keys.row_count; i++) {
        const Entry *entry = entries + i;
        uint32_t common = 0, length = entry->count + 2, at;
        if (i) {
            uint32_t previous_length = entries[i - 1].count + 2;
            while (common < length && common < previous_length &&
                   token(entry, common) == token(entries + i - 1, common))
                common++;
            if (common == length && common == previous_length) {
                status = QJS_INTL_DATA_ERROR;
                goto done;
            }
        }
        for (at = common; at < length; at++) {
            uint32_t parent = stack[at], code = token(entry, at), id = count++;
            nodes[id] = (Node){0, NONE, NONE};
            stack[at + 1] = id;
            if (code == SEPARATOR) {
                nodes[parent].suffix = id;
            } else {
                nodes[parent].count++;
                edges[edge_count++] = (Edge){parent, code, id};
            }
        }
        nodes[stack[length]].terminal = entry->mapping;
    }
    if ((uint64_t)count * 16 + (uint64_t)edge_count * 8 + (uint64_t)snapshot->suppressed_count * 4 >
        QJS_INTL_COLLATION_TRIE_MAX_SIZE) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    temp.node_bytes = array(&temp.allocator, count, 16, &status);
    temp.edge_bytes = array(&temp.allocator, edge_count, 8, &status);
    temp.suppressed_bytes = array(&temp.allocator, snapshot->suppressed_count, 4, &status);
    if (status)
        goto done;
    for (i = 0; i < count; i++) {
        uint8_t *row = temp.node_bytes + (size_t)i * 16;
        uint32_t children = nodes[i].count;
        put32(row, edge_at);
        put32(row + 4, children);
        put32(row + 8, nodes[i].suffix);
        put32(row + 12, nodes[i].terminal);
        nodes[i].count = edge_at; /* Reuse as the per-parent fill cursor. */
        edge_at += children;
    }
    for (i = 0; i < edge_count; i++) {
        const Edge *edge = edges + i;
        uint8_t *row = temp.edge_bytes + (size_t)nodes[edge->parent].count++ * 8;
        /* Sorted virtual paths visit each parent's scalar children in order,
           even though descendant edges interleave in the temporary array. */
        put32(row, edge->code);
        put32(row + 4, edge->child);
    }
    for (i = 0; i < snapshot->suppressed_count; i++)
        put32(temp.suppressed_bytes + (size_t)i * 4, snapshot->suppressed[i]);
    temp.nodes = (QJSIntlSection){temp.node_bytes, (size_t)count * 16, count};
    temp.edges = (QJSIntlSection){temp.edge_bytes, (size_t)edge_count * 8, edge_count};
    temp.suppressed = (QJSIntlSection){temp.suppressed_bytes, (size_t)snapshot->suppressed_count * 4,
                                       snapshot->suppressed_count};
    *out = temp;
    temp = (QJSIntlCompiledTrie){0};
done:
    if (entries)
        snapshot->allocator.free(snapshot->allocator.opaque, entries);
    if (nodes)
        snapshot->allocator.free(snapshot->allocator.opaque, nodes);
    if (edges)
        snapshot->allocator.free(snapshot->allocator.opaque, edges);
    if (stack)
        snapshot->allocator.free(snapshot->allocator.opaque, stack);
    qjs_intl_compiled_trie_clear(&temp);
    return status;
}
