/* Mutable mapping ownership, stable IDs, suppression and bounded failure sweeps.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../tools/intl/collation-map.h"
#include "../src/intl/collation/match.h"
#include "../src/intl/collation/contexts.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define NONE QJS_INTL_COLLATION_NODE_NONE

extern const uint8_t qjs_intl_collation_fixture_bytes[];
extern const size_t qjs_intl_collation_fixture_length;

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

static uint32_t lookup(const QJSIntlCollationMap *map, const uint32_t *prefix, uint32_t prefix_count,
                        const uint32_t *suffix, uint32_t suffix_count)
{
    QJSIntlCollationOverlay view;
    uint32_t node, i;
    assert(suffix_count && !qjs_intl_collation_map_overlay(map, &view));
    node = view.begin(view.opaque, suffix[0], prefix, prefix_count);
    for (i = 0; node != NONE && i < suffix_count; i++)
        node = view.step(view.opaque, node, suffix[i]);
    return node == NONE ? NONE : view.terminal(view.opaque, node);
}

static uint32_t put(QJSIntlCollationMap *map, const uint32_t *prefix, uint32_t prefix_count,
                     const uint32_t *suffix, uint32_t suffix_count,
                     const QJSIntlRelationElement *ce, size_t count)
{
    uint32_t sequence;
    assert(!qjs_intl_collation_map_put(map, prefix, prefix_count, suffix, suffix_count,
                                      ce, count, &sequence));
    assert(sequence != NONE);
    return sequence;
}

static void test_copies_and_contexts(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationMap map;
    QJSIntlCollationOverlay view;
    QJSIntlRelationMapping old, replaced, empty;
    QJSIntlRelationElement ce = {.weights = {100, 32, 2, 0}};
    const uint32_t prefix[] = {'l', 0x301}, packed[] = {'x', 'l', 0xe6000301, 0xb7};
    const uint32_t suffix[] = {0xb7, 'x'}, zero = 0;
    uint32_t first, second, ignored;
    assert(!qjs_intl_collation_map_init(&map, &a));
    first = put(&map, prefix, 2, suffix, 1, &ce, 1);
    assert(first == 0);
    assert(!qjs_intl_collation_map_sequence(&map, first, &old));
    ce.weights[0] = 200;
    second = put(&map, prefix, 2, suffix, 1, &ce, 1);
    assert(second != first && lookup(&map, prefix, 2, suffix, 1) == second);
    assert(!qjs_intl_collation_map_sequence(&map, second, &replaced));
    assert(old.elements[0].weights[0] == 100 && replaced.elements[0].weights[0] == 200);
    /* New sequences can copy borrowed old CEs without invalidating either. */
    ignored = put(&map, NULL, 0, suffix, 2, old.elements, old.count);
    assert(!qjs_intl_collation_map_sequence(&map, ignored, &replaced));
    assert(replaced.elements != old.elements && replaced.elements[0].weights[0] == 100);
    ignored = put(&map, NULL, 0, &zero, 1, NULL, 0);
    assert(lookup(&map, NULL, 0, &zero, 1) == ignored);
    assert(!qjs_intl_collation_map_sequence(&map, ignored, &empty));
    assert(!empty.count && !empty.elements);
    assert(!qjs_intl_collation_map_overlay(&map, &view));
    assert(view.longest_prefix(view.opaque, packed, 3, 4) == 2);
    assert(view.longest_prefix(view.opaque, packed, 3, 2) == 0);
    assert(view.begin(view.opaque, 0xb7, packed + 1, 2) != NONE);
    assert(qjs_intl_collation_map_put(&map, NULL, 0, suffix, 0, &ce, 1, &ignored) ==
           QJS_INTL_INVALID_ARGUMENT && ignored == NONE);
    assert(qjs_intl_collation_map_sequence(&map, NONE, &empty) == QJS_INTL_INVALID_ARGUMENT);
    assert(!empty.count && !empty.elements);
    qjs_intl_collation_map_clear(&map);
    qjs_intl_collation_map_clear(&map);
    assert(!state.live);
}

static void test_suppression_and_long_keys(void)
{
    enum { LENGTH = 6000 };
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationMap map;
    QJSIntlCollationOverlay view;
    QJSIntlRelationMapping retained;
    QJSIntlRelationElement ce = {.weights = {100, 32, 2, 0}};
    uint32_t *key = malloc((LENGTH + 1) * sizeof(*key));
    const uint32_t prefix[] = {'l'}, suffix[] = {0xb7, 'x'};
    uint32_t i, plain, context, contraction;
    assert(key && !qjs_intl_collation_map_init(&map, &a));
    for (i = 0; i < LENGTH; i++)
        key[i] = 'a';
    key[LENGTH] = suffix[0];
    plain = put(&map, NULL, 0, key, 1, &ce, 1);
    contraction = put(&map, NULL, 0, key, LENGTH, &ce, 1);
    context = put(&map, prefix, 1, key, LENGTH, &ce, 1);
    put(&map, prefix, 1, suffix, 2, &ce, 1);
    put(&map, key, LENGTH, suffix, 2, &ce, 1);
    assert(!qjs_intl_collation_map_sequence(&map, contraction, &retained));
    assert(!qjs_intl_collation_map_suppress(&map, 'a'));
    assert(lookup(&map, NULL, 0, key, 1) == plain);
    assert(lookup(&map, NULL, 0, key, LENGTH) == NONE);
    assert(lookup(&map, prefix, 1, key, LENGTH) == NONE);
    assert(lookup(&map, prefix, 1, suffix, 2) != NONE);
    assert(retained.elements[0].weights[0] == 100);
    assert(!qjs_intl_collation_map_sequence(&map, context, &retained));
    assert(retained.elements[0].weights[0] == 100);
    context = put(&map, prefix, 1, key, LENGTH, &ce, 1);
    assert(lookup(&map, prefix, 1, key, LENGTH) == context);
    assert(!qjs_intl_collation_map_overlay(&map, &view));
    assert(view.longest_prefix(view.opaque, key, LENGTH, LENGTH + 1) == LENGTH);
    assert(view.root_context_suppressed(view.opaque, 'a'));
    assert(!view.root_context_suppressed(view.opaque, 0xb7));
    assert(!qjs_intl_collation_map_suppress(&map, 0xb7));
    assert(lookup(&map, prefix, 1, suffix, 2) == NONE);
    assert(lookup(&map, key, LENGTH, suffix, 2) == NONE);
    /* A prefix's ordinary singleton is still contextual and must be removed. */
    put(&map, prefix, 1, suffix, 1, &ce, 1);
    assert(!qjs_intl_collation_map_suppress(&map, 0xb7));
    assert(lookup(&map, prefix, 1, suffix, 1) == NONE);
    qjs_intl_collation_map_clear(&map);
    free(key);
    assert(!state.live);
}

static void test_suppression_failure(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationMap map;
    QJSIntlCollationOverlay view;
    QJSIntlRelationElement ce = {.weights = {100, 32, 2, 0}};
    const uint32_t key = 'a';
    uint32_t code = 0xe000, id;
    assert(!qjs_intl_collation_map_init(&map, &a));
    id = put(&map, NULL, 0, &key, 1, &ce, 1);
    /* Fill the node arena exactly. A previously unseen suppression record must
       then allocate, rather than accidentally testing an already reserved slot. */
    while (map.node_count < map.node_capacity)
        assert(!qjs_intl_collation_map_suppress(&map, code++));
    assert(!qjs_intl_collation_map_overlay(&map, &view));
    state.fail_at = state.calls + 1;
    assert(qjs_intl_collation_map_suppress(&map, code) == QJS_INTL_NO_MEMORY);
    assert(!view.root_context_suppressed(view.opaque, code));
    assert(lookup(&map, NULL, 0, &key, 1) == id);
    state.fail_at = 0;
    assert(!qjs_intl_collation_map_suppress(&map, code));
    assert(view.root_context_suppressed(view.opaque, code));
    assert(lookup(&map, NULL, 0, &key, 1) == id);
    assert(!qjs_intl_collation_map_suppress(&map, 0));
    assert(view.root_context_suppressed(view.opaque, 0));
    qjs_intl_collation_map_clear(&map);
    assert(!state.live);
}

static size_t allocation_case(size_t fail_at)
{
    Allocation state = {.fail_at = fail_at};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationMap map;
    QJSIntlStatus status;
    QJSIntlRelationElement ce = {.weights = {100, 32, 2, 0}};
    uint32_t i, id, prefix[] = {'l', 0x301}, key[180], previous = 0;
    status = qjs_intl_collation_map_init(&map, &a);
    if (status) {
        assert(status == QJS_INTL_NO_MEMORY && state.failures == 1);
        qjs_intl_collation_map_clear(&map);
        state.fail_at = 0;
        assert(!qjs_intl_collation_map_init(&map, &a));
    }
    for (i = 0; i < 180; i++)
        key[i] = 'a';
    for (i = 0; i < 96; i++) {
        uint32_t code = 0xe000 + i, old_count = map.sequence_count;
        status = qjs_intl_collation_map_put(&map, prefix, 2, &code, 1, &ce, 1, &id);
        if (status) {
            assert(status == QJS_INTL_NO_MEMORY && state.failures == 1 && id == NONE);
            assert(map.sequence_count == old_count && lookup(&map, prefix, 2, &code, 1) == NONE);
            state.fail_at = 0;
            assert(!qjs_intl_collation_map_put(&map, prefix, 2, &code, 1, &ce, 1, &id));
        }
        assert(id == i && lookup(&map, prefix, 2, &code, 1) == id);
        previous = id;
    }
    status = qjs_intl_collation_map_put(&map, prefix, 2, key, 180, &ce, 1, &id);
    if (status) {
        uint32_t last = 0xe000 + 95;
        assert(status == QJS_INTL_NO_MEMORY && state.failures == 1 && id == NONE);
        assert(map.sequence_count == 96 && lookup(&map, prefix, 2, &last, 1) == previous);
        assert(lookup(&map, prefix, 2, key, 180) == NONE);
        state.fail_at = 0;
        assert(!qjs_intl_collation_map_put(&map, prefix, 2, key, 180, &ce, 1, &id));
    }
    assert(id == 96);
    /* Replacing a terminal can fail too; the previous ID must survive. */
    status = qjs_intl_collation_map_put(&map, prefix, 2, key, 180, &ce, 1, &id);
    if (status) {
        assert(status == QJS_INTL_NO_MEMORY && state.failures == 1 && id == NONE);
        assert(lookup(&map, prefix, 2, key, 180) == 96);
        state.fail_at = 0;
        assert(!qjs_intl_collation_map_put(&map, prefix, 2, key, 180, &ce, 1, &id));
    }
    assert(id == 97 && (!fail_at || state.failures == 1));
    previous = state.calls;
    qjs_intl_collation_map_clear(&map);
    assert(!state.live);
    return previous;
}

static void test_large_starred_shape(void)
{
    enum { COUNT = 102038 };
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationMap map;
    QJSIntlRelationElement ce = {.weights = {100, 32, 2, 0}};
    QJSIntlRelationMapping retained;
    uint32_t i, code;
    assert(!qjs_intl_collation_map_init(&map, &a));
    for (i = 0; i < COUNT; i++) {
        code = 0xf0000 + i;
        assert(put(&map, NULL, 0, &code, 1, &ce, 1) == i);
        if (!i)
            assert(!qjs_intl_collation_map_sequence(&map, 0, &retained));
    }
    /* Every arena/hash has grown repeatedly; neither sequence IDs nor their
       independently allocated CE arrays may have changed identity/content. */
    for (i = 0; i < COUNT; i += 17) {
        code = 0xf0000 + i;
        assert(lookup(&map, NULL, 0, &code, 1) == i);
    }
    assert(retained.count == 1 && retained.elements[0].weights[0] == 100);
    assert(map.sequence_count == COUNT);
    qjs_intl_collation_map_clear(&map);
    assert(!state.live);
}

static void scan(const QJSIntlCollationContexts *data, QJSIntlCollationMap *map,
                  const uint32_t *expected, size_t count)
{
    static const uint16_t input[] = {'a', 'b', 'c'};
    QJSIntlCollationOverlay view;
    QJSIntlCollationMatch match;
    QJSIntlCollationKeyIndex keys;
    QJSIntlCollationMatchResult result;
    size_t i;
    int present;
    assert(!qjs_intl_collation_map_overlay(map, &view));
    assert(!qjs_intl_collation_contexts_keys(data, &keys));
    assert(!qjs_intl_collation_match_init_overlay(&match, &keys, input, 3, &view, &map->allocator));
    for (i = 0; i < count; i++) {
        uint32_t value;
        assert(!qjs_intl_collation_match_next(&match, &result, NULL, &present) && present);
        if (result.kind == QJS_INTL_COLLATION_MATCH_OVERLAY) {
            value = UINT32_C(0x80000000) | result.sequence;
        } else {
            QJSIntlCollationElement ce;
            assert(result.kind == QJS_INTL_COLLATION_MATCH_ROOT);
            QJSIntlCollationEntry entry;
            assert(!qjs_intl_collation_contexts_mapping(data, result.mapping_id, &entry));
            assert(!qjs_intl_collation_element_at(&entry, 0, &ce));
            value = ce.primary;
        }
        assert(value == expected[i]);
    }
    assert(!qjs_intl_collation_match_next(&match, &result, NULL, &present) && !present);
    qjs_intl_collation_match_clear(&match);
}

static void test_real_matcher_adapter(void)
{
    static const uint32_t abc[] = {'a', 'b', 'c'};
    static const uint32_t root[] = {0x2000}, replaced[] = {0x80000001};
    static const uint32_t suppressed[] = {0x80000000, 0x1300, 0x1400}, restored[] = {0x80000002};
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationMap map;
    QJSIntlCollationContexts data;
    QJSIntlPack pack;
    QJSIntlRelationElement ce = {.weights = {100, 32, 2, 0}};
    assert(!qjs_intl_pack_open(qjs_intl_collation_fixture_bytes,
                              qjs_intl_collation_fixture_length, &pack));
    assert(!qjs_intl_collation_contexts_open(&pack, &data));
    assert(!qjs_intl_collation_map_init(&map, &a));
    put(&map, NULL, 0, abc, 1, &ce, 1);
    scan(&data, &map, root, 1);
    put(&map, NULL, 0, abc, 3, &ce, 1);
    scan(&data, &map, replaced, 1);
    assert(!qjs_intl_collation_map_suppress(&map, 'a'));
    scan(&data, &map, suppressed, 3);
    put(&map, NULL, 0, abc, 3, &ce, 1);
    scan(&data, &map, restored, 1);
    qjs_intl_collation_map_clear(&map);
    assert(!state.live);
}

int main(void)
{
    size_t calls, failure;
    test_copies_and_contexts();
    test_suppression_and_long_keys();
    test_suppression_failure();
    calls = allocation_case(0);
    for (failure = 1; failure <= calls; failure++)
        allocation_case(failure);
    test_large_starred_shape();
    test_real_matcher_adapter();
    return 0;
}
