/* Sparse symbolic insertion, stable root aliases and allocator recovery.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../tools/intl/collation-weights.h"
#include <assert.h>
#include <stdlib.h>

typedef struct Allocation {
    size_t calls, fail_at, live, failures;
} Allocation;

static int fail(Allocation *state)
{
    if (++state->calls == state->fail_at) {
        state->failures++;
        return 1;
    }
    return 0;
}

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (fail(state))
        return NULL;
    ptr = malloc(size);
    assert(ptr);
    state->live++;
    return ptr;
}

static void *resize(void *opaque, void *ptr, size_t size)
{
    Allocation *state = opaque;
    void *next;
    assert(ptr && size);
    if (fail(state))
        return NULL;
    next = realloc(ptr, size);
    assert(next);
    return next;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static QJSIntlAllocator allocator(Allocation *state)
{
    return (QJSIntlAllocator){state, allocate, resize, release};
}

static uint32_t insert(QJSIntlWeightGraph *graph, uint32_t reference, int before)
{
    uint32_t id;
    assert(!qjs_intl_weight_insert(graph, reference, before, NULL, &id));
    assert(id != QJS_INTL_WEIGHT_NONE);
    return id;
}

static uint32_t resolve(const QJSIntlWeightGraph *graph, uint32_t id)
{
    uint32_t rank;
    assert(!qjs_intl_weight_resolve(graph, id, &rank));
    return rank;
}

static void test_order_and_aliases(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlWeightGraph graph;
    QJSIntlWeightParents parents = {42, 77}, copied;
    uint32_t x, y, z, end, before, after_before, end2, old_x, rank;
    assert(!qjs_intl_weight_graph_init(&graph, &a, 10));
    assert(!qjs_intl_weight_insert(&graph, 2, 0, &parents, &x));
    old_x = x; /* A copied mapping retains identity when another is replaced. */
    y = insert(&graph, 2, 0);
    z = insert(&graph, x, 1);
    end = insert(&graph, 3, 1);
    before = insert(&graph, 2, 1);
    after_before = insert(&graph, before, 0);
    end2 = insert(&graph, 3, 1);
    assert(qjs_intl_weight_resolve(&graph, x, &rank) == QJS_INTL_INVALID_ARGUMENT && !rank);
    assert(!qjs_intl_weight_parents(&graph, x, &copied));
    assert(copied.primary == 42 && copied.secondary == 77);
    assert(!qjs_intl_weight_freeze(&graph));
    assert(!qjs_intl_weight_freeze(&graph));
    assert(resolve(&graph, 0) == 0 && resolve(&graph, 1) == 1);
    assert(resolve(&graph, before) == 2 && resolve(&graph, after_before) == 3);
    assert(resolve(&graph, 2) == 4);
    assert(resolve(&graph, y) == 5 && resolve(&graph, z) == 6);
    assert(resolve(&graph, x) == 7 && resolve(&graph, old_x) == 7);
    assert(resolve(&graph, end) == 8 && resolve(&graph, end2) == 9);
    assert(resolve(&graph, 3) == 10 && resolve(&graph, 10) == 17);
    assert(qjs_intl_weight_insert(&graph, x, 0, NULL, &rank) == QJS_INTL_INVALID_ARGUMENT);
    assert(rank == QJS_INTL_WEIGHT_NONE);
    qjs_intl_weight_graph_clear(&graph);
    qjs_intl_weight_graph_clear(&graph);
    assert(!state.live);
}

static void test_zero_empty_and_invalid(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlWeightGraph graph;
    QJSIntlWeightParents parents;
    uint32_t id, first, rank;
    assert(qjs_intl_weight_graph_init(&graph, &a, UINT32_MAX) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_weight_graph_clear(&graph);
    assert(!qjs_intl_weight_graph_init(&graph, &a, 7));
    assert(!qjs_intl_weight_freeze(&graph) && !state.calls);
    assert(resolve(&graph, 7) == 7);
    qjs_intl_weight_graph_clear(&graph);
    assert(!qjs_intl_weight_graph_init(&graph, &a, 7));
    assert(qjs_intl_weight_insert(&graph, 0, 1, NULL, &id) == QJS_INTL_INVALID_ARGUMENT);
    assert(id == QJS_INTL_WEIGHT_NONE);
    assert(qjs_intl_weight_insert(&graph, 8, 0, NULL, &id) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_weight_insert(&graph, UINT32_MAX, 0, NULL, &id) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_weight_insert(&graph, 0, 0, NULL, NULL) == QJS_INTL_INVALID_ARGUMENT);
    first = insert(&graph, 0, 0);
    id = insert(&graph, first, 1);
    assert(qjs_intl_weight_parents(&graph, 0, &parents) == QJS_INTL_INVALID_ARGUMENT);
    assert(!parents.primary && !parents.secondary);
    assert(!qjs_intl_weight_freeze(&graph));
    assert(resolve(&graph, 0) == 0 && resolve(&graph, id) == 1 && resolve(&graph, first) == 2);
    assert(resolve(&graph, 1) == 3);
    assert(qjs_intl_weight_resolve(&graph, UINT32_MAX, &rank) == QJS_INTL_INVALID_ARGUMENT && !rank);
    qjs_intl_weight_graph_clear(&graph);
    qjs_intl_weight_graph_clear(NULL);
    assert(!state.live);
}

static void test_compare_before_and_after_freeze(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlWeightGraph graph;
    uint32_t ids[40], i, j, count = 8;
    int orders[40][40], order;
    size_t calls;
    assert(!qjs_intl_weight_graph_init(&graph, &a, 7));
    for (i = 0; i < count; i++)
        ids[i] = i;
    for (i = 0; i < 24; i++) {
        /* Alternate insertion beside roots and older tailored IDs, including
           before-insertion in the previous gap and after the final root. */
        uint32_t reference = ids[(i * 7 + 3) % count];
        ids[count++] = insert(&graph, reference, (i & 1) && reference != 0);
        calls = state.calls;
        assert(!qjs_intl_weight_compare(&graph, reference, ids[count - 1], &order));
        assert(order == ((i & 1) && reference != 0 ? 1 : -1));
        assert(state.calls == calls && !graph.frozen);
    }
    calls = state.calls;
    for (i = 0; i < count; i++) {
        for (j = 0; j < count; j++)
            assert(!qjs_intl_weight_compare(&graph, ids[i], ids[j], &orders[i][j]));
    }
    assert(state.calls == calls && !graph.frozen);
    assert(qjs_intl_weight_compare(&graph, UINT32_MAX, 0, &order) == QJS_INTL_INVALID_ARGUMENT && !order);
    assert(qjs_intl_weight_compare(&graph, 0, 8, &order) == QJS_INTL_INVALID_ARGUMENT && !order);
    assert(!qjs_intl_weight_freeze(&graph));
    calls = state.calls;
    for (i = 0; i < count; i++) {
        for (j = 0; j < count; j++) {
            uint32_t left = resolve(&graph, ids[i]), right = resolve(&graph, ids[j]);
            int expected = left < right ? -1 : left > right ? 1 : 0;
            assert(orders[i][j] == expected);
            assert(!qjs_intl_weight_compare(&graph, ids[i], ids[j], &order) && order == expected);
        }
    }
    assert(state.calls == calls);
    qjs_intl_weight_graph_clear(&graph);
    assert(qjs_intl_weight_compare(&graph, 0, 0, &order) == QJS_INTL_INVALID_ARGUMENT && !order);
    assert(qjs_intl_weight_compare(NULL, 0, 0, &order) == QJS_INTL_INVALID_ARGUMENT && !order);
    assert(!state.live);
}

static size_t allocation_case(size_t fail_at, uint32_t expected[160])
{
    Allocation state = {.fail_at = fail_at};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlWeightGraph graph;
    uint32_t ids[160], i;
    QJSIntlStatus status;
    assert(!qjs_intl_weight_graph_init(&graph, &a, 100));
    for (i = 0; i < 160; i++) {
        status = qjs_intl_weight_insert(&graph, i % 40, i & 1, NULL, &ids[i]);
        if (status) {
            assert(status == QJS_INTL_NO_MEMORY && state.failures == 1);
            assert(ids[i] == QJS_INTL_WEIGHT_NONE && graph.node_count == i);
            state.fail_at = 0;
            assert(!qjs_intl_weight_insert(&graph, i % 40, i & 1, NULL, &ids[i]));
        }
    }
    status = qjs_intl_weight_freeze(&graph);
    if (status) {
        assert(status == QJS_INTL_NO_MEMORY && state.failures == 1);
        state.fail_at = 0;
        assert(!qjs_intl_weight_freeze(&graph));
    }
    for (i = 0; i < 160; i++) {
        if (fail_at)
            assert(resolve(&graph, ids[i]) == expected[i]);
        else
            expected[i] = resolve(&graph, ids[i]);
    }
    assert(!fail_at || state.failures == 1);
    qjs_intl_weight_graph_clear(&graph);
    assert(!state.live);
    return state.calls;
}

static void test_large_chain(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlWeightGraph graph;
    uint32_t i, id = 5;
    assert(!qjs_intl_weight_graph_init(&graph, &a, 7471103));
    for (i = 0; i < 102038; i++)
        id = insert(&graph, id, 0);
    assert(graph.gap_count == 1); /* Virtual roots consume no per-root storage. */
    assert(!qjs_intl_weight_freeze(&graph));
    assert(resolve(&graph, 5) == 5 && resolve(&graph, id) == 102043);
    assert(resolve(&graph, 6) == 102044);
    assert(resolve(&graph, 7471103) == 7573141);
    qjs_intl_weight_graph_clear(&graph);
    assert(!state.live);
}

static void test_high_ranks_are_not_ids(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlWeightGraph graph;
    uint32_t first, second, last;
    assert(!qjs_intl_weight_graph_init(&graph, &a, UINT32_C(0x7fffffff)));
    first = insert(&graph, 0, 0);
    second = insert(&graph, 0, 0);
    last = insert(&graph, UINT32_C(0x7fffffff), 0);
    assert(!qjs_intl_weight_freeze(&graph));
    assert(resolve(&graph, second) == 1 && resolve(&graph, first) == 2);
    assert(resolve(&graph, UINT32_C(0x7fffffff)) == UINT32_C(0x80000001));
    assert(resolve(&graph, last) == UINT32_C(0x80000002));
    qjs_intl_weight_graph_clear(&graph);
    assert(!state.live);
}

int main(void)
{
    uint32_t expected[160];
    size_t calls, failure;
    test_order_and_aliases();
    test_zero_empty_and_invalid();
    test_compare_before_and_after_freeze();
    calls = allocation_case(0, expected);
    for (failure = 1; failure <= calls; failure++)
        allocation_case(failure, expected);
    test_large_chain();
    test_high_ranks_are_not_ids();
    return 0;
}
