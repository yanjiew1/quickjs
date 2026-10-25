/* Composite lookup fixtures: provider identity must not change matching rules.
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
#include "../src/intl/collation/match.h"
#include "../src/intl/collation/contexts.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t qjs_intl_collation_fixture_bytes[];
extern const size_t qjs_intl_collation_fixture_length;

#define NONE QJS_INTL_COLLATION_NODE_NONE
#define MASK QJS_INTL_COLLATION_SCALAR_MASK
#define OVERLAY(id) (UINT32_C(0x80000000) | (id))
#define SECONDARY(s) (UINT32_C(0x10000000) | (s))
#define SCALAR(s) (UINT32_C(0x40000000) | (s))

/* A tiny authored adapter, deliberately unrelated to the future production
   hash/frozen indexes. Linear child lists suffice for these short fixtures. */
typedef struct Node {
    uint32_t code, child, sibling, sequence;
} Node;

typedef struct Group {
    uint32_t first, prefix[8], length, root;
} Group;

typedef struct Source {
    Node nodes[128];
    Group groups[16];
    uint32_t node_count, group_count, suppressed;
} Source;

static uint32_t new_node(Source *source, uint32_t code)
{
    uint32_t id = source->node_count++;
    assert(id < sizeof(source->nodes) / sizeof(*source->nodes));
    source->nodes[id] = (Node){code, NONE, NONE, NONE};
    return id;
}

static int prefix_equal(const Group *group, const uint32_t *text, uint32_t length)
{
    uint32_t i;
    if (group->length != length)
        return 0;
    for (i = 0; i < length; i++)
        if (group->prefix[i] != (text[i] & MASK))
            return 0;
    return 1;
}

static uint32_t step(const void *opaque, uint32_t id, uint32_t code)
{
    const Source *source = opaque;
    for (id = source->nodes[id].child; id != NONE; id = source->nodes[id].sibling)
        if (source->nodes[id].code == code)
            return id;
    return NONE;
}

static void add(Source *source, const uint32_t *prefix, uint32_t prefix_count,
                 const uint32_t *suffix, uint32_t suffix_count, uint32_t sequence)
{
    uint32_t i, node, group;
    assert(suffix_count && prefix_count <= 8);
    for (group = 0; group < source->group_count; group++)
        if (source->groups[group].first == suffix[0] &&
            prefix_equal(source->groups + group, prefix, prefix_count))
            break;
    if (group == source->group_count) {
        Group *next = source->groups + source->group_count++;
        assert(group < sizeof(source->groups) / sizeof(*source->groups));
        *next = (Group){.first = suffix[0], .length = prefix_count,
                        .root = new_node(source, 0)};
        for (i = 0; i < prefix_count; i++)
            next->prefix[i] = prefix[i];
    }
    node = source->groups[group].root;
    for (i = 0; i < suffix_count; i++) {
        uint32_t child = step(source, node, suffix[i]);
        if (child == NONE) {
            child = new_node(source, suffix[i]);
            source->nodes[child].sibling = source->nodes[node].child;
            source->nodes[node].child = child;
        }
        node = child;
    }
    source->nodes[node].sequence = sequence;
}

static uint32_t longest_prefix(const void *opaque, const uint32_t *text,
                                uint32_t start, uint32_t limit)
{
    const Source *source = opaque;
    uint32_t i, best = 0;
    for (i = 0; i < source->group_count; i++) {
        const Group *group = source->groups + i;
        if (group->first == (text[start] & MASK) && group->length > best &&
            group->length <= start && group->length < limit &&
            prefix_equal(group, text + start - group->length, group->length))
            best = group->length;
    }
    return best;
}

static uint32_t begin(const void *opaque, uint32_t first, const uint32_t *prefix, uint32_t count)
{
    const Source *source = opaque;
    uint32_t i;
    for (i = 0; i < source->group_count; i++)
        if (source->groups[i].first == first && prefix_equal(source->groups + i, prefix, count))
            return source->groups[i].root;
    return NONE;
}

static uint32_t terminal(const void *opaque, uint32_t node)
{
    return ((const Source *)opaque)->nodes[node].sequence;
}

static int has_children(const void *opaque, uint32_t node)
{
    return ((const Source *)opaque)->nodes[node].child != NONE;
}

static int suppressed(const void *opaque, uint32_t code)
{
    return code && code == ((const Source *)opaque)->suppressed;
}

static void *allocate(void *opaque, size_t size)
{
    (void)opaque;
    return malloc(size);
}

static void *resize(void *opaque, void *ptr, size_t size)
{
    (void)opaque;
    return realloc(ptr, size);
}

static void release(void *opaque, void *ptr)
{
    (void)opaque;
    free(ptr);
}

static QJSIntlCollationOverlay overlay(const Source *source)
{
    return (QJSIntlCollationOverlay){source, longest_prefix, begin, step,
                                     terminal, has_children, suppressed};
}

static void check(const QJSIntlCollationContexts *data, const Source *source,
                   const uint16_t *text, size_t length, const uint32_t *expected, size_t count)
{
    QJSIntlAllocator allocator = {NULL, allocate, resize, release};
    QJSIntlCollationOverlay view = overlay(source);
    QJSIntlCollationMatch match;
    QJSIntlCollationKeyIndex keys;
    QJSIntlCollationMatchResult result;
    int present;
    size_t i;
    assert(!qjs_intl_collation_contexts_keys(data, &keys));
    assert(!qjs_intl_collation_match_init_overlay(&match, &keys, text, length, &view, &allocator));
    memset(&view, 0, sizeof(view)); /* Callbacks are copied, not borrowed. */
    for (i = 0; i < count; i++) {
        uint32_t value;
        assert(!qjs_intl_collation_match_next(&match, &result, NULL, &present) && present);
        if (result.kind == QJS_INTL_COLLATION_MATCH_OVERLAY) {
            value = OVERLAY(result.sequence);
        } else if (result.kind == QJS_INTL_COLLATION_MATCH_SCALAR) {
            value = SCALAR(result.scalar);
        } else {
            QJSIntlCollationElement ce;
            assert(result.kind == QJS_INTL_COLLATION_MATCH_ROOT);
            QJSIntlCollationEntry entry;
            assert(!qjs_intl_collation_contexts_mapping(data, result.mapping_id, &entry));
            assert(entry.element_count == 1);
            assert(!qjs_intl_collation_element_at(&entry, 0, &ce));
            value = ce.primary ? ce.primary : SECONDARY(ce.secondary);
        }
        assert(value == expected[i]);
    }
    assert(!qjs_intl_collation_match_next(&match, &result, NULL, &present) && !present);
    qjs_intl_collation_match_clear(&match);
}

#define VALUES(...) __VA_ARGS__
#define CHECK(text, expected) do { \
    const uint16_t input[] = VALUES text; \
    const uint32_t tokens[] = VALUES expected; \
    check(data, &source, input, sizeof(input) / sizeof(*input), \
          tokens, sizeof(tokens) / sizeof(*tokens)); \
} while (0)

static void competition(const QJSIntlCollationContexts *data)
{
    Source source = {0};
    const uint32_t a[] = {'a'}, abc[] = {'a', 'b', 'c'}, missing[] = {0x378};
    CHECK(({'a','b','c'}), ({0x2000})); /* Empty overlay is root-equivalent. */
    CHECK(({0x378}), ({SCALAR(0x378)}));
    add(&source, NULL, 0, a, 1, 1);
    CHECK(({'a','b','c'}), ({0x2000})); /* Short overlay cannot hide root abc. */
    add(&source, NULL, 0, abc, 3, 0);
    CHECK(({'a','b','c'}), ({OVERLAY(0)})); /* Zero is a valid terminal ID. */
    CHECK(({'a','b','x'}), ({OVERLAY(1),0x1300,0x1200}));
    add(&source, NULL, 0, missing, 1, 2);
    CHECK(({0x378}), ({OVERLAY(2)}));
}

static void prefixes(const QJSIntlCollationContexts *data)
{
    Source source = {0};
    const uint32_t l[] = {'l'}, xbl[] = {'x','b','l'}, dot[] = {0xb7}, dotz[] = {0xb7,'z'};
    add(&source, l, 1, dot, 1, 0);
    CHECK(({'b','l',0xb7}), ({0x3500,SECONDARY(0x42)})); /* Longer root prefix. */
    add(&source, l, 1, dotz, 2, 2);
    CHECK(({'b','l',0xb7,'z'}), ({0x3500,SECONDARY(0x42),0x1900}));
    CHECK(({'o','l',0xb7}), ({0x3300,OVERLAY(0)})); /* Failed longer root prefix. */
    add(&source, xbl, 3, dotz, 2, 1);
    CHECK(({'x','b','l',0xb7,'z'}), ({0x1200,0x3500,OVERLAY(1)}));
    CHECK(({'x','b','l',0xb7,'x'}), ({0x1200,0x3500,SECONDARY(0x42),0x1200}));
    /* Prefix text is still visible after an overlapping root contraction. */
    CHECK(({'x','l',0xb7}), ({0x3400,OVERLAY(0)}));
}

static void discontiguous(const QJSIntlCollationContexts *data)
{
    Source source = {0};
    const uint32_t key[] = {'a',0x30a,0x301,0x302};
    const uint32_t partial[] = {'a',0x323};
    const uint32_t l[] = {'l'}, colon[] = {':',0x30a};
    /* Both intermediate full checkpoints come from root; the final one is
       overlay-owned. Successful equal-CCC extensions do not block each other. */
    add(&source, NULL, 0, key, 4, 0);
    CHECK(({'a',0x323,0x30a,0x301,0x302}), ({OVERLAY(0),SECONDARY(0x35)}));
    /* Conversely, an overlay-only checkpoint can have ONE longer root row,
       without a root terminal at the checkpoint itself. */
    add(&source, NULL, 0, partial, 2, 3);
    CHECK(({'a',0x323,0x331,0x301}), ({0x3800,SECONDARY(0x37)}));
    add(&source, l, 1, colon, 2, 1);
    CHECK(({'l',':',0x323,0x30a}), ({0x1500,0x1800,SECONDARY(0x35),SECONDARY(0x34)}));
    /* A real contextual singleton now permits the otherwise forbidden skip. */
    add(&source, l, 1, colon, 1, 2);
    CHECK(({'l',':',0x323,0x30a}), ({0x1500,OVERLAY(1),SECONDARY(0x35)}));
    CHECK(({'l',':',0x304,0x30a}), ({0x1500,OVERLAY(2),SECONDARY(0x32),SECONDARY(0x34)}));
}

static void suppression(const QJSIntlCollationContexts *data)
{
    Source source = {.suppressed = 'a'};
    const uint32_t key[] = {'a',0x30a};
    const uint32_t l[] = {'l'}, dot[] = {0xb7};
    CHECK(({'a','b','c'}), ({0x1000,0x1300,0x1400}));
    CHECK(({'a',0x30a}), ({0x1000,SECONDARY(0x34)}));
    add(&source, NULL, 0, key, 2, 0);
    CHECK(({'a',0x30a}), ({OVERLAY(0)})); /* Later overlay contexts survive. */
    source.suppressed = 0xb7;
    CHECK(({'b','l',0xb7}), ({0x3500,0x1700})); /* Root prefix is suppressed too. */
    add(&source, l, 1, dot, 1, 1);
    CHECK(({'b','l',0xb7}), ({0x3500,OVERLAY(1)}));
}

static void put32(uint8_t *out, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        out[i] = value >> (8 * i);
}

static void key_index_splits(void)
{
    static const char *const texts[] = {
        "a", "abc", "abc", "abc", "abcd", "b", "c", "d", "x", "xa", "xab", "ya"
    };
    static const struct {
        uint16_t text[4];
        uint32_t length, ids[2], suffixes[2], count;
    } cases[] = {
        {{'a','b','c'}, 3, {1}, {3}, 1},
        {{'a','b','c','d'}, 4, {4}, {4}, 1},
        {{'x','a','b','c'}, 4, {10,3}, {3,1}, 2},
        {{'y','a','b','c'}, 4, {11,2}, {2,2}, 2}
    };
    uint8_t rows[12 * 16], keys[25 * 4], contexts[2 * 12];
    QJSIntlCollationKeyIndex index = {rows, keys, contexts, 12, 25, 2};
    QJSIntlAllocator allocator = {NULL, allocate, resize, release};
    uint32_t i, at = 0;
    size_t c;
    for (i = 0; i < 12; i++) {
        uint32_t length = strlen(texts[i]), j;
        put32(rows + i * 16, at);
        put32(rows + i * 16 + 4, length);
        /* Opaque payload fields belong to an absent test emitter. The matcher
           must never form CE pointers or interpret their values. */
        put32(rows + i * 16 + 8, UINT32_MAX);
        put32(rows + i * 16 + 12, UINT32_MAX);
        for (j = 0; j < length; j++)
            put32(keys + at++ * 4, texts[i][j]);
    }
    assert(at == 25);
    for (i = 0; i < 2; i++) {
        put32(contexts + i * 12, i + 2);
        put32(contexts + i * 12 + 4, i + 1);
        put32(contexts + i * 12 + 8, UINT32_MAX); /* Reader-specific, not CE skip here. */
    }
    /* The three abc terminals have prefix splits 0/1/2. Stepping to abcd must
       skip ALL terminals; selecting context must use its exact prefix split. */
    for (c = 0; c < sizeof(cases) / sizeof(*cases); c++) {
        QJSIntlCollationMatch match;
        QJSIntlCollationMatchResult result;
        int present;
        assert(!qjs_intl_collation_match_init(&match, &index, cases[c].text,
                                             cases[c].length, &allocator));
        for (i = 0; i < cases[c].count; i++) {
            assert(!qjs_intl_collation_match_next(&match, &result, NULL, &present) && present);
            assert(result.kind == QJS_INTL_COLLATION_MATCH_ROOT);
            assert(result.mapping_id == cases[c].ids[i]);
            assert(result.suffix_length == cases[c].suffixes[i]);
        }
        assert(!qjs_intl_collation_match_next(&match, &result, NULL, &present) && !present);
        qjs_intl_collation_match_clear(&match);
    }
}

int main(void)
{
    QJSIntlPack pack;
    QJSIntlCollationContexts data;
    QJSIntlCollationMatch match;
    QJSIntlCollationKeyIndex keys;
    QJSIntlCollationOverlay invalid = {0};
    QJSIntlAllocator allocator = {NULL, allocate, resize, release};
    assert(!qjs_intl_pack_open(qjs_intl_collation_fixture_bytes,
                              qjs_intl_collation_fixture_length, &pack));
    assert(!qjs_intl_collation_contexts_open(&pack, &data));
    competition(&data);
    prefixes(&data);
    discontiguous(&data);
    suppression(&data);
    key_index_splits();
    assert(!qjs_intl_collation_contexts_keys(&data, &keys));
    assert(qjs_intl_collation_match_init_overlay(&match, &keys, NULL, 0, &invalid, &allocator) ==
           QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_collation_match_clear(&match);
    return 0;
}
