/* Validate once; subsequent matcher callbacks allocate nothing and cannot fail.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "trie.h"
#include "../native/wire.h"
#include <string.h>

#define NONE QJS_INTL_COLLATION_NODE_NONE
#define MASK QJS_INTL_COLLATION_SCALAR_MASK
enum { ROOT = 1, PREFIX, SUFFIX_ROOT, SUFFIX };

static uint32_t field(const QJSIntlCollationTrie *view, uint32_t node, unsigned int word)
{
    return qjs_intl_read_u32(view->nodes + (size_t)node * 16 + word * 4);
}

static int scalar(uint32_t code)
{
    return code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff);
}

static uint32_t child(const QJSIntlCollationTrie *view, uint32_t node, uint32_t code)
{
    uint32_t start = field(view, node, 0), low = 0, high = field(view, node, 1);
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t *edge = view->edges + (size_t)(start + middle) * 8;
        uint32_t found = qjs_intl_read_u32(edge);
        if (found == code)
            return qjs_intl_read_u32(edge + 4);
        if (found < code)
            low = middle + 1;
        else
            high = middle;
    }
    return NONE;
}

static uint32_t longest_prefix(const void *opaque, const uint32_t *text, uint32_t start, uint32_t limit)
{
    const QJSIntlCollationTrie *view = opaque;
    uint32_t node = child(view, 0, text[start] & MASK), count, best = 0;
    for (count = 1; node != NONE && count <= start && count < limit; count++) {
        node = child(view, node, text[start - count] & MASK);
        if (node != NONE && field(view, node, 2) != NONE)
            best = count;
    }
    return best;
}

static uint32_t begin(const void *opaque, uint32_t first, const uint32_t *prefix, uint32_t count)
{
    const QJSIntlCollationTrie *view = opaque;
    uint32_t node = child(view, 0, first), i;
    for (i = count; node != NONE && i; i--)
        node = child(view, node, prefix[i - 1] & MASK);
    return node == NONE ? NONE : field(view, node, 2);
}

static uint32_t step(const void *opaque, uint32_t node, uint32_t code)
{
    return child(opaque, node, code);
}

static uint32_t terminal(const void *opaque, uint32_t node)
{
    return field(opaque, node, 3);
}

static int has_children(const void *opaque, uint32_t node)
{
    return field(opaque, node, 1) != 0;
}

static int is_suppressed(const void *opaque, uint32_t code)
{
    const QJSIntlCollationTrie *view = opaque;
    uint32_t low = 0, high = view->suppressed_count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        uint32_t found = qjs_intl_read_u32(view->suppressed + (size_t)middle * 4);
        if (found == code)
            return 1;
        if (found < code)
            low = middle + 1;
        else
            high = middle;
    }
    return 0;
}

QJSIntlStatus qjs_intl_collation_trie_overlay(const QJSIntlCollationTrie *view, QJSIntlCollationOverlay *out)
{
    if (out)
        *out = (QJSIntlCollationOverlay){0};
    if (!view || !view->nodes || !view->node_count || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = (QJSIntlCollationOverlay){view, longest_prefix, begin, step, terminal, has_children, is_suppressed};
    return QJS_INTL_OK;
}

static int sections(const QJSIntlSection *nodes, const QJSIntlSection *edges, const QJSIntlSection *suppressed)
{
    return nodes->bytes && nodes->count && (uint64_t)nodes->count * 16 == nodes->length &&
        (!edges->length || edges->bytes) && (uint64_t)edges->count * 8 == edges->length &&
        (!suppressed->length || suppressed->bytes) && (uint64_t)suppressed->count * 4 == suppressed->length &&
        (uint64_t)nodes->length + edges->length + suppressed->length <= QJS_INTL_COLLATION_TRIE_MAX_SIZE;
}

static int targets(const QJSIntlCollationTrie *view, const QJSIntlCollationKeyIndex *keys,
                   uint8_t *types, uint8_t *terminals)
{
    uint32_t id, edge_at = 0, terminal_count = 0;
    types[0] = ROOT;
    for (id = 0; id < view->node_count; id++) {
        uint32_t start = field(view, id, 0), count = field(view, id, 1);
        uint32_t suffix = field(view, id, 2), mapping = field(view, id, 3), i, previous = 0;
        unsigned int type = types[id];
        if (!type || start != edge_at || count > view->edge_count - edge_at)
            return 0;
        if (type == ROOT || type == PREFIX) {
            if (mapping != NONE || (type == ROOT && suffix != NONE) ||
                (type == PREFIX && !count && suffix == NONE))
                return 0;
            if (suffix != NONE) {
                if (suffix <= id || suffix >= view->node_count || types[suffix])
                    return 0;
                types[suffix] = SUFFIX_ROOT;
            }
        } else {
            if (suffix != NONE || (type == SUFFIX_ROOT && (mapping != NONE || !count)) ||
                (type == SUFFIX && !count && mapping == NONE))
                return 0;
            if (mapping != NONE) {
                if (mapping >= keys->row_count || terminals[mapping])
                    return 0;
                terminals[mapping] = 1;
                terminal_count++;
            }
        }
        for (i = 0; i < count; i++) {
            const uint8_t *edge = view->edges + (size_t)(edge_at + i) * 8;
            uint32_t code = qjs_intl_read_u32(edge), child_id = qjs_intl_read_u32(edge + 4);
            if (!scalar(code) || (i && code <= previous) || child_id <= id ||
                child_id >= view->node_count || types[child_id])
                return 0;
            previous = code;
            types[child_id] = type <= PREFIX ? PREFIX : SUFFIX;
        }
        edge_at += count;
    }
    return edge_at == view->edge_count && terminal_count == keys->row_count;
}

static int key_agreement(const QJSIntlCollationTrie *view, const QJSIntlCollationKeyIndex *keys, uint8_t *visited)
{
    uint32_t id;
    visited[0] = 1;
    for (id = 0; id < keys->row_count; id++) {
        QJSIntlCollationKeySpan key;
        uint32_t prefix = qjs_intl_collation_keys_prefix(keys, id), node, i;
        if (qjs_intl_collation_keys_at(keys, id, &key) || prefix >= key.count)
            return 0;
        node = child(view, 0, qjs_intl_read_u32(key.keys + (size_t)prefix * 4));
        if (node != NONE)
            visited[node] = 1;
        for (i = prefix; node != NONE && i; i--) {
            node = child(view, node, qjs_intl_read_u32(key.keys + (size_t)(i - 1) * 4));
            if (node != NONE)
                visited[node] = 1;
        }
        if (node == NONE || (node = field(view, node, 2)) == NONE)
            return 0;
        visited[node] = 1;
        for (i = prefix; node != NONE && i < key.count; i++) {
            node = child(view, node, qjs_intl_read_u32(key.keys + (size_t)i * 4));
            if (node != NONE)
                visited[node] = 1;
        }
        if (node == NONE || field(view, node, 3) != id)
            return 0;
    }
    /* Prove the graph is exactly the union of mapping paths, excluding extra
       branches that could inflate prefix candidates or scan work. */
    for (id = 0; id < view->node_count; id++) {
        if (!visited[id])
            return 0;
    }
    return 1;
}

QJSIntlStatus qjs_intl_collation_trie_open(const QJSIntlSection *nodes, const QJSIntlSection *edges,
                                           const QJSIntlSection *suppressed,
                                           const QJSIntlCollationKeyIndex *keys, const QJSIntlAllocator *allocator,
                                           QJSIntlCollationTrie *out)
{
    QJSIntlCollationTrie view;
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    uint64_t scratch_size;
    uint8_t *scratch;
    uint32_t i, previous = 0;
    if (out)
        *out = (QJSIntlCollationTrie){0};
    if (!nodes || !edges || !suppressed || !keys || !allocator || !allocator->malloc || !allocator->free || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!sections(nodes, edges, suppressed) || keys->row_count > nodes->count)
        return QJS_INTL_DATA_ERROR;
    view = (QJSIntlCollationTrie){nodes->bytes, edges->bytes, suppressed->bytes,
                                  nodes->count, edges->count, suppressed->count};
    for (i = 0; i < view.suppressed_count; i++) {
        uint32_t code = qjs_intl_read_u32(view.suppressed + (size_t)i * 4);
        if (!scalar(code) || (i && code <= previous))
            return QJS_INTL_DATA_ERROR;
        previous = code;
    }
    scratch_size = (uint64_t)view.node_count + keys->row_count;
    if (scratch_size > SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    scratch = allocator->malloc(allocator->opaque, (size_t)scratch_size);
    if (!scratch)
        return QJS_INTL_NO_MEMORY;
    memset(scratch, 0, (size_t)scratch_size);
    if (targets(&view, keys, scratch, scratch + view.node_count)) {
        memset(scratch, 0, view.node_count);
        if (key_agreement(&view, keys, scratch)) {
            *out = view;
            status = QJS_INTL_OK;
        }
    }
    allocator->free(allocator->opaque, scratch);
    return status;
}
