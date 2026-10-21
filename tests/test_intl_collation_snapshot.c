/* Frozen copies outlive compiler state; virtual-root ranks keep inserted gaps.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../tools/intl/collation-snapshot.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t intl_root_fixture[];
extern const size_t intl_root_fixture_size;
extern const uint8_t intl_program_success[];
extern const size_t intl_program_success_size;

typedef struct State {
    size_t calls, fail_at, live;
    unsigned int ends;
    QJSIntlCollationSnapshot snapshots[2];
} State;

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

static void init(QJSIntlCollationProgram *program, const QJSIntlRootInput *root, State *state)
{
    QJSIntlAllocator a = allocator(state);
    *program = (QJSIntlCollationProgram){0};
    assert(!qjs_intl_collation_builder_init(&program->builder, root, &a));
    assert(!qjs_intl_collation_reorder_init(&program->reorder, root, &a));
    qjs_intl_collation_settings_init(&program->policy);
}

static void clear_program(QJSIntlCollationProgram *program)
{
    qjs_intl_collation_builder_clear(&program->builder);
    qjs_intl_collation_reorder_clear(&program->reorder);
}

static void put(QJSIntlCollationMap *map, const uint32_t *prefix, uint32_t prefix_count,
                  const uint32_t *suffix, uint32_t count, const QJSIntlRelationElement *ce, size_t ces)
{
    uint32_t sequence;
    assert(!qjs_intl_collation_map_put(map, prefix, prefix_count, suffix, count, ce, ces, &sequence));
}

static void assert_root_ranks(const QJSIntlCollationProgram *program, const QJSIntlCollationSnapshot *snapshot)
{
    unsigned int axis;
    for (axis = 0; axis < 4; axis++) {
        const QJSIntlWeightGraph *graph = program->builder.relations.axes + axis;
        uint32_t tests[] = {0, 1, graph->root_max, graph->root_max ? graph->root_max - 1 : 0};
        uint32_t i;
        for (i = 0; i < graph->gap_count; i++) {
            uint32_t gap, added, expected, actual;
            assert(!qjs_intl_weight_offset_at(graph, i, &gap, &added));
            assert(snapshot->offsets[axis][i].root_gap == gap && snapshot->offsets[axis][i].added == added);
            assert(!qjs_intl_weight_resolve(graph, gap, &expected));
            assert(!qjs_intl_collation_snapshot_root_rank(snapshot, axis, gap, &actual) && expected == actual);
            if (gap < graph->root_max) {
                assert(!qjs_intl_weight_resolve(graph, gap + 1, &expected));
                assert(!qjs_intl_collation_snapshot_root_rank(snapshot, axis, gap + 1, &actual) && expected == actual);
            }
        }
        for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
            uint32_t expected, actual;
            if (tests[i] > graph->root_max)
                continue;
            assert(!qjs_intl_weight_resolve(graph, tests[i], &expected));
            assert(!qjs_intl_collation_snapshot_root_rank(snapshot, axis, tests[i], &actual) && expected == actual);
        }
    }
}

static void test_copy_and_root_gaps(const QJSIntlRootInput *root)
{
    State state = {0};
    QJSIntlCollationProgram program;
    QJSIntlCollationSnapshot snapshot;
    uint8_t locale[] = {'e', 'n'};
    const QJSIntlIRProgram identity = {{locale, 2}, {(const uint8_t *)"standard", 8}, {0}, 1};
    const uint32_t abc[] = {'a', 'b', 'c'}, zero = 0, supplementary = 0x1f600, n = 'n';
    uint32_t deep[64], tail_ids[4], i, inserted, base_rank, next_rank, contexts = 0;
    QJSIntlRelationElement ce, zero_ce;
    init(&program, root, &state);
    assert(!qjs_intl_root_input_implicit(root, 0x4e00, &ce));
    /* Populate several root gaps, including zero, before-first-Han and generic
       fallback. Snapshot shifts must work beyond explicitly mapped characters. */
    for (i = 0; i < 4; i++) {
        QJSIntlWeightGraph *graph = program.builder.relations.axes + i;
        assert(!qjs_intl_weight_insert(graph, 0, 0, NULL, &inserted));
        assert(!qjs_intl_weight_insert(graph, graph->root_max, 0, NULL, &inserted));
        tail_ids[i] = inserted;
    }
    zero_ce = ce;
    for (i = 0; i < 4; i++)
        zero_ce.weights[i] = tail_ids[i];
    zero_ce.case_value = QJS_INTL_COLLATION_MIXED;
    zero_ce.variable = 1;
    zero_ce.implicit = 0;
    assert(!qjs_intl_weight_insert(program.builder.relations.axes, root->metadata.han_base + 1, 1, NULL, &inserted));
    ce.weights[0] = inserted;
    assert(!qjs_intl_weight_insert(program.builder.relations.axes, root->metadata.generic_base + 100, 1, NULL, &inserted));
    put(&program.builder.map, NULL, 0, abc, 3, &ce, 1);
    put(&program.builder.map, abc, 1, abc + 1, 2, &ce, 1);
    put(&program.builder.map, abc, 2, abc + 2, 1, &ce, 1);
    put(&program.builder.map, NULL, 0, &zero, 1, &zero_ce, 1);
    put(&program.builder.map, NULL, 0, &supplementary, 1, &ce, 1);
    /* Replacement's old CE vector remains in compiler history, not output. */
    put(&program.builder.map, NULL, 0, &supplementary, 1, NULL, 0);
    for (i = 0; i < sizeof(deep) / sizeof(*deep); i++)
        deep[i] = 'q';
    put(&program.builder.map, NULL, 0, deep, sizeof(deep) / sizeof(*deep), &ce, 1);
    assert(!qjs_intl_collation_map_suppress(&program.builder.map, 'z'));
    assert(!qjs_intl_collation_map_suppress(&program.builder.map, n));
    assert(!qjs_intl_collation_map_suppress(&program.builder.map, n));
    /* Reintroduction preserves suppression of inherited root contexts. */
    put(&program.builder.map, abc, 1, &n, 1, &ce, 1);
    assert(!qjs_intl_collation_snapshot_create(&program, &identity, &snapshot));
    assert(snapshot.ready && snapshot.keys.row_count == 7 && snapshot.element_count == 6);
    assert(snapshot.suppressed_count == 2 && snapshot.suppressed[0] == 'n' && snapshot.suppressed[1] == 'z');
    assert(snapshot.keys.context_count == 3 && snapshot.group_count == program.reorder.count);
    assert_root_ranks(&program, &snapshot);
    assert(!qjs_intl_collation_snapshot_root_rank(&snapshot, 0, root->metadata.han_base, &base_rank));
    assert(!qjs_intl_collation_snapshot_root_rank(&snapshot, 0, root->metadata.han_base + 1, &next_rank));
    assert(next_rank == base_rank + 2);
    for (i = 0; i < snapshot.keys.row_count; i++) {
        uint32_t at = qjs_intl_read_u32(snapshot.rows + (size_t)i * 16 + 8);
        uint32_t count = qjs_intl_read_u32(snapshot.rows + (size_t)i * 16 + 12);
        QJSIntlCollationKeySpan key;
        assert(!qjs_intl_collation_keys_at(&snapshot.keys, i, &key));
        if (key.count == 3 && qjs_intl_read_u32(key.keys) == 'a' && qjs_intl_read_u32(key.keys + 4) == 'b') {
            assert(qjs_intl_collation_keys_prefix(&snapshot.keys, i) == contexts++);
        }
        if (count) {
            const QJSIntlRelationElement *expected = key.count == 1 && !qjs_intl_read_u32(key.keys) ? &zero_ce : &ce;
            unsigned int axis;
            assert(at < snapshot.element_count);
            assert(snapshot.elements[at].root_primary == expected->root_primary);
            assert(snapshot.elements[at].case_value == expected->case_value);
            assert(snapshot.elements[at].implicit == expected->implicit && snapshot.elements[at].variable == expected->variable);
            for (axis = 0; axis < 4; axis++) {
                uint32_t rank;
                assert(!qjs_intl_weight_resolve(program.builder.relations.axes + axis, expected->weights[axis], &rank));
                assert(snapshot.elements[at].ranks[axis] == rank);
            }
        }
    }
    assert(contexts == 3);
    locale[0] = 'x';
    clear_program(&program);
    assert(snapshot.identity.locale.bytes[0] == 'e' && snapshot.identity.locale.length == 2);
    assert(snapshot.keys.row_count == 7 && !snapshot.elements[0].implicit);
    assert(!qjs_intl_collation_snapshot_root_rank(&snapshot, 0, root->metadata.han_base + 1, &next_rank));
    qjs_intl_collation_snapshot_clear(&snapshot);
    qjs_intl_collation_snapshot_clear(&snapshot);
    assert(!state.live);
}

static QJSIntlStatus retain(void *opaque, const QJSIntlIRProgram *identity, QJSIntlCollationProgram *program)
{
    State *state = opaque;
    QJSIntlStatus status;
    assert(state->ends < 2);
    status = qjs_intl_collation_snapshot_create(program, identity, state->snapshots + state->ends);
    if (!status) {
        assert_root_ranks(program, state->snapshots + state->ends);
        state->ends++;
    }
    return status;
}

static void clear_retained(State *state)
{
    qjs_intl_collation_snapshot_clear(state->snapshots);
    qjs_intl_collation_snapshot_clear(state->snapshots + 1);
    assert(!state->live);
}

static void test_executor_ownership_and_oom(const QJSIntlRootInput *root)
{
    State state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlIRView view;
    QJSIntlIRDiagnostic error;
    size_t count, i;
    uint8_t *bytes = malloc(intl_program_success_size);
    assert(bytes);
    memcpy(bytes, intl_program_success, intl_program_success_size);
    assert(!qjs_intl_ir_open(bytes, intl_program_success_size, root->fingerprint, &view, NULL));
    assert(!qjs_intl_collation_program_execute(root, &view, &a, retain, &state, &error));
    count = state.calls;
    assert(state.ends == 2 && state.snapshots[0].keys.row_count > state.snapshots[1].keys.row_count);
    clear_retained(&state);
    for (i = 1; i <= count; i++) {
        state = (State){.fail_at = i};
        assert(qjs_intl_collation_program_execute(root, &view, &a, retain, &state, &error) == QJS_INTL_NO_MEMORY);
        assert(state.ends < 2);
        clear_retained(&state);
    }
    state = (State){0};
    assert(!qjs_intl_collation_program_execute(root, &view, &a, retain, &state, &error));
    free(bytes); /* No label slice may borrow the executor input after return. */
    assert(state.snapshots[0].identity.locale.bytes[0] == 'e');
    assert(state.snapshots[1].identity.locale.bytes[0] == 'f');
    assert(state.snapshots[0].policy.strength == 2 && state.snapshots[0].policy.case_first == QJS_INTL_COLLATION_UPPER_FIRST);
    assert(state.snapshots[0].group_ranks[6] < state.snapshots[0].group_ranks[5]);
    assert(state.snapshots[1].policy.numeric && state.snapshots[1].policy.strength == 3);
    clear_retained(&state);
}

static void test_empty_snapshot(const QJSIntlRootInput *root)
{
    State state = {0};
    QJSIntlCollationProgram program;
    QJSIntlCollationSnapshot snapshot;
    const QJSIntlIRProgram identity = {{(const uint8_t *)"en", 2}, {(const uint8_t *)"standard", 8}, {0}, 0};
    uint32_t value = 99;
    init(&program, root, &state);
    assert(!qjs_intl_collation_map_suppress(&program.builder.map, 'a'));
    assert(!qjs_intl_collation_snapshot_create(&program, &identity, &snapshot));
    clear_program(&program);
    assert(snapshot.ready && !snapshot.keys.row_count && !snapshot.keys.rows && !snapshot.elements);
    assert(snapshot.suppressed_count == 1 && snapshot.suppressed[0] == 'a');
    assert(!qjs_intl_collation_snapshot_root_rank(&snapshot, 0, 0, &value) && !value);
    assert(qjs_intl_collation_snapshot_root_rank(&snapshot, 4, 0, &value) == QJS_INTL_INVALID_ARGUMENT && !value);
    assert(qjs_intl_collation_snapshot_root_rank(&snapshot, 0, snapshot.root_maxima[0] + 1, &value) ==
           QJS_INTL_INVALID_ARGUMENT && !value);
    qjs_intl_collation_snapshot_clear(&snapshot);
    assert(!state.live);
}

int main(void)
{
    State state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRootInput root;
    assert(!qjs_intl_root_input_open(intl_root_fixture, intl_root_fixture_size, &a, &root));
    assert(!state.live);
    test_copy_and_root_gaps(&root);
    test_executor_ownership_and_oom(&root);
    test_empty_snapshot(&root);
    return 0;
}
