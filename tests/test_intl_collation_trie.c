/* Frozen queries agree with mutable storage and reject malformed graph bytes.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../tools/intl/collation-trie.h"
#include "../src/intl/collation/match.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t intl_root_fixture[];
extern const size_t intl_root_fixture_size;

typedef struct State { size_t calls, fail_at, live; } State;

static void *allocate(void *opaque, size_t size)
{
    State *state = opaque;
    void *p;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    p = malloc(size);
    assert(p);
    state->live++;
    return p;
}

static void *resize(void *opaque, void *p, size_t size)
{
    State *state = opaque;
    void *next;
    assert(p && size);
    if (++state->calls == state->fail_at)
        return NULL;
    next = realloc(p, size);
    assert(next);
    return next;
}

static void release(void *opaque, void *p)
{
    State *state = opaque;
    assert(p && state->live);
    state->live--;
    free(p);
}

static QJSIntlAllocator allocator(State *state)
{
    return (QJSIntlAllocator){state, allocate, resize, release};
}

static void put32(uint8_t *p, uint32_t word)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        p[i] = word >> (8 * i);
}

static void put(QJSIntlCollationProgram *program, const uint32_t *prefix, uint32_t prefix_count,
                  const uint32_t *suffix, uint32_t count, int empty)
{
    QJSIntlRelationElement ce;
    uint32_t sequence;
    assert(!qjs_intl_root_input_implicit(&program->builder.root, 'g' + program->builder.map.live_count, &ce));
    assert(!qjs_intl_collation_map_put(&program->builder.map, prefix, prefix_count, suffix, count,
                                       empty ? NULL : &ce, empty ? 0 : 1, &sequence));
}

static void compare_matches(QJSIntlCollationProgram *program, const QJSIntlCollationSnapshot *snapshot,
                             const QJSIntlCollationOverlay *frozen, const uint16_t *text, size_t length)
{
    QJSIntlCollationOverlay mutable;
    QJSIntlCollationMatch a, b;
    unsigned int steps = 0;
    assert(!qjs_intl_collation_map_overlay(&program->builder.map, &mutable));
    assert(!qjs_intl_collation_match_init_overlay(&a, &snapshot->root.keys, text, length, &mutable, &snapshot->allocator));
    assert(!qjs_intl_collation_match_init_overlay(&b, &snapshot->root.keys, text, length, frozen, &snapshot->allocator));
    for (;;) {
        QJSIntlCollationMatchResult x, y;
        int xp, yp;
        assert(++steps < 256);
        assert(!qjs_intl_collation_match_next(&a, &x, NULL, &xp));
        assert(!qjs_intl_collation_match_next(&b, &y, NULL, &yp));
        assert(xp == yp);
        if (!xp)
            break;
        assert(x.kind == y.kind);
        if (x.kind == QJS_INTL_COLLATION_MATCH_ROOT) {
            assert(x.mapping_id == y.mapping_id);
        } else if (x.kind == QJS_INTL_COLLATION_MATCH_SCALAR) {
            assert(x.scalar == y.scalar);
        } else {
            QJSIntlRelationMapping mapping;
            uint32_t at = qjs_intl_read_u32(snapshot->rows + (size_t)y.sequence * 16 + 8);
            uint32_t count = qjs_intl_read_u32(snapshot->rows + (size_t)y.sequence * 16 + 12), i;
            assert(x.kind == QJS_INTL_COLLATION_MATCH_OVERLAY);
            assert(!qjs_intl_collation_map_sequence(&program->builder.map, x.sequence, &mapping));
            assert(count == mapping.count);
            for (i = 0; i < count; i++) {
                unsigned int axis;
                for (axis = 0; axis < 4; axis++) {
                    uint32_t rank;
                    assert(!qjs_intl_weight_resolve(program->builder.relations.axes + axis, mapping.elements[i].weights[axis], &rank));
                    assert(snapshot->elements[at + i].ranks[axis] == rank);
                }
            }
        }
    }
    qjs_intl_collation_match_clear(&a);
    qjs_intl_collation_match_clear(&b);
}

static void mutations(const QJSIntlCompiledTrie *data, const QJSIntlCollationSnapshot *snapshot, State *state)
{
    const QJSIntlAllocator a = allocator(state);
    QJSIntlCollationTrie view;
    QJSIntlSection sections[] = {data->nodes, data->edges, data->suppressed};
    uint8_t *copies[3];
    size_t baseline = state->live;
    unsigned int i;
    for (i = 0; i < 3; i++) {
        copies[i] = malloc(sections[i].length + 1);
        assert(copies[i]);
        memcpy(copies[i] + 1, sections[i].bytes, sections[i].length);
        sections[i].bytes = copies[i] + 1;
    }
    assert(!qjs_intl_collation_trie_open(sections, sections + 1, sections + 2, &snapshot->keys, &a, &view));
    assert(state->live == baseline && view.nodes == copies[0] + 1);
    /* Each mutation starts from a complete valid graph, so later guards are
       exercised independently rather than through an earlier corrupted field. */
    {
        struct Mutation { unsigned int section; size_t offset; uint32_t word; } cases[] = {
            {0, 0, 1}, {0, 4, UINT32_MAX}, {0, 8, 0}, {0, 12, 0},
            {1, 0, 0xd800}, {1, 4, 0}, {1, 4, data->nodes.count},
            {2, 0, 0xdfff}, {2, 4, 'a'}
        };
        for (i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
            unsigned int section = cases[i].section;
            uint8_t *p = copies[section] + 1 + cases[i].offset;
            uint32_t old = qjs_intl_read_u32(p);
            put32(p, cases[i].word);
            assert(qjs_intl_collation_trie_open(sections, sections + 1, sections + 2, &snapshot->keys, &a, &view) == QJS_INTL_DATA_ERROR);
            assert(!view.nodes && !view.node_count && state->live == baseline);
            put32(p, old);
        }
    }
    {
        uint8_t *first = copies[1] + 1, *second = first + 8;
        uint32_t code = qjs_intl_read_u32(second), target = qjs_intl_read_u32(second + 4);
        assert(qjs_intl_read_u32(copies[0] + 5) >= 2);
        put32(second, qjs_intl_read_u32(first));
        assert(qjs_intl_collation_trie_open(sections, sections + 1, sections + 2, &snapshot->keys, &a, &view) == QJS_INTL_DATA_ERROR);
        put32(second, code);
        put32(second + 4, qjs_intl_read_u32(first + 4));
        assert(qjs_intl_collation_trie_open(sections, sections + 1, sections + 2, &snapshot->keys, &a, &view) == QJS_INTL_DATA_ERROR);
        put32(second + 4, target);
        assert(!view.nodes && state->live == baseline);
    }
    /* A permutation of terminal IDs still passes uniqueness/range checks but
       must fail exact agreement with the independently validated key index. */
    {
        uint8_t *first = NULL, *second = NULL;
        uint32_t x, y;
        for (i = 0; i < data->nodes.count; i++) {
            uint8_t *p = copies[0] + 1 + (size_t)i * 16 + 12;
            if (qjs_intl_read_u32(p) != UINT32_MAX) {
                if (!first)
                    first = p;
                else {
                    second = p;
                    break;
                }
            }
        }
        assert(first && second);
        x = qjs_intl_read_u32(first);
        y = qjs_intl_read_u32(second);
        put32(second, x);
        assert(qjs_intl_collation_trie_open(sections, sections + 1, sections + 2, &snapshot->keys, &a, &view) == QJS_INTL_DATA_ERROR);
        put32(second, y);
        put32(first, y);
        put32(second, x);
        assert(qjs_intl_collation_trie_open(sections, sections + 1, sections + 2, &snapshot->keys, &a, &view) == QJS_INTL_DATA_ERROR);
        assert(!view.nodes && state->live == baseline);
        put32(first, x);
        put32(second, y);
    }
    state->fail_at = state->calls + 1;
    assert(qjs_intl_collation_trie_open(sections, sections + 1, sections + 2, &snapshot->keys, &a, &view) == QJS_INTL_NO_MEMORY);
    assert(!view.nodes && state->live == baseline);
    state->fail_at = 0;
    for (i = 0; i < 3; i++)
        free(copies[i]);
}

static void test_trie(const QJSIntlRootInput *root)
{
    State state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationProgram program = {0};
    QJSIntlCollationSnapshot snapshot;
    QJSIntlCompiledTrie data;
    QJSIntlCollationTrie view;
    QJSIntlCollationOverlay overlay;
    const QJSIntlIRProgram identity = {{(const uint8_t *)"en", 2}, {(const uint8_t *)"standard", 8}, {0}, 0};
    const uint32_t xy[] = {'x','y'}, ba[] = {'b','a'}, abc[] = {'a','b','c'}, nul[] = {0,0x1f600};
    const uint16_t texts[][8] = {{'x'}, {'x','y'}, {'a','x','y'}, {'b','a','x','y'}, {'a','b','c'},
                                  {0,0xd83d,0xde00}, {'e'}, {'m','n'}, {'a','x',0x327,0x30a}};
    const size_t lengths[] = {1,2,3,4,3,3,1,2,4};
    size_t baseline, first, calls, i;
    assert(!qjs_intl_collation_builder_init(&program.builder, root, &a));
    assert(!qjs_intl_collation_reorder_init(&program.reorder, root, &a));
    qjs_intl_collation_settings_init(&program.policy);
    assert(!qjs_intl_collation_map_suppress(&program.builder.map, 'a'));
    assert(!qjs_intl_collation_map_suppress(&program.builder.map, 'm'));
    put(&program, NULL, 0, xy, 1, 0);
    put(&program, NULL, 0, xy, 2, 0);
    put(&program, ba + 1, 1, xy, 1, 0);
    put(&program, ba + 1, 1, xy, 2, 0);
    put(&program, ba, 2, xy, 2, 0);
    put(&program, ba + 1, 1, (const uint32_t[]){'x',0x30a}, 2, 0);
    put(&program, NULL, 0, abc, 3, 0);
    put(&program, abc, 1, abc + 1, 2, 0);
    put(&program, abc, 2, abc + 2, 1, 0);
    put(&program, NULL, 0, nul, 2, 0);
    put(&program, nul, 1, nul + 1, 1, 0);
    put(&program, NULL, 0, (const uint32_t[]){'e'}, 1, 1);
    assert(!qjs_intl_collation_snapshot_create(&program, &identity, &snapshot));
    baseline = state.live;
    first = state.calls;
    assert(!qjs_intl_compiled_trie_create(&snapshot, &data));
    calls = state.calls - first;
    assert(!qjs_intl_collation_trie_open(&data.nodes, &data.edges, &data.suppressed, &snapshot.keys, &a, &view));
    assert(!qjs_intl_collation_trie_overlay(&view, &overlay));
    assert(overlay.root_context_suppressed(overlay.opaque, 'a'));
    assert(overlay.root_context_suppressed(overlay.opaque, 'm'));
    assert(!overlay.root_context_suppressed(overlay.opaque, 'x'));
    {
        const uint32_t text[] = {'b', 'a', 'x' | UINT32_C(0xE6000000)};
        assert(overlay.longest_prefix(overlay.opaque, text, 2, UINT32_MAX) == 2);
        assert(overlay.longest_prefix(overlay.opaque, text, 2, 2) == 1);
        assert(!overlay.longest_prefix(overlay.opaque, text, 2, 1));
    }
    for (i = 0; i < sizeof(lengths) / sizeof(*lengths); i++)
        compare_matches(&program, &snapshot, &overlay, texts[i], lengths[i]);
    mutations(&data, &snapshot, &state);
    qjs_intl_compiled_trie_clear(&data);
    assert(state.live == baseline);
    for (i = 1; i <= calls; i++) {
        state.fail_at = state.calls + i;
        assert(qjs_intl_compiled_trie_create(&snapshot, &data) == QJS_INTL_NO_MEMORY);
        assert(!data.node_bytes && !data.nodes.bytes && state.live == baseline);
    }
    state.fail_at = 0;
    assert(!qjs_intl_compiled_trie_create(&snapshot, &data));
    assert(!qjs_intl_collation_trie_open(&data.nodes, &data.edges, &data.suppressed, &snapshot.keys, &a, &view));
    assert(!qjs_intl_collation_trie_overlay(&view, &overlay));
    qjs_intl_collation_snapshot_clear(&snapshot);
    qjs_intl_collation_builder_clear(&program.builder);
    qjs_intl_collation_reorder_clear(&program.reorder);
    /* Queries retain neither snapshot nor compiler arrays after opening. */
    assert(overlay.begin(overlay.opaque, 'x', ba, 2) != UINT32_MAX);
    assert(overlay.root_context_suppressed(overlay.opaque, 'm'));
    qjs_intl_compiled_trie_clear(&data);
    assert(!state.live);
}

static void test_empty(const QJSIntlRootInput *root)
{
    State state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationProgram program = {0};
    QJSIntlCollationSnapshot snapshot;
    QJSIntlCompiledTrie data;
    QJSIntlCollationTrie view;
    QJSIntlCollationOverlay overlay;
    const QJSIntlIRProgram identity = {{(const uint8_t *)"en", 2}, {(const uint8_t *)"standard", 8}, {0}, 0};
    assert(!qjs_intl_collation_builder_init(&program.builder, root, &a));
    assert(!qjs_intl_collation_reorder_init(&program.reorder, root, &a));
    qjs_intl_collation_settings_init(&program.policy);
    assert(!qjs_intl_collation_map_suppress(&program.builder.map, 'a'));
    assert(!qjs_intl_collation_snapshot_create(&program, &identity, &snapshot));
    assert(!qjs_intl_compiled_trie_create(&snapshot, &data));
    assert(data.nodes.count == 1 && !data.edges.count && !data.edge_bytes);
    assert(!qjs_intl_collation_trie_open(&data.nodes, &data.edges, &data.suppressed, &snapshot.keys, &a, &view));
    assert(!qjs_intl_collation_trie_overlay(&view, &overlay));
    assert(overlay.begin(overlay.opaque, 'a', NULL, 0) == UINT32_MAX);
    assert(overlay.root_context_suppressed(overlay.opaque, 'a'));
    qjs_intl_compiled_trie_clear(&data);
    qjs_intl_collation_snapshot_clear(&snapshot);
    qjs_intl_collation_builder_clear(&program.builder);
    qjs_intl_collation_reorder_clear(&program.reorder);
    assert(!state.live);
}

int main(void)
{
    State state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRootInput root;
    assert(!qjs_intl_root_input_open(intl_root_fixture, intl_root_fixture_size, &a, &root));
    assert(!state.live);
    test_trie(&root);
    test_empty(&root);
    return 0;
}
