/* Rule-derived CLDR relation vectors, independent of XML and mapping lookup.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../tools/intl/collation-relations.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
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
    if (++state->calls == state->fail_at)
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

static QJSIntlCollationLogical logical(uint32_t p, uint16_t s, uint16_t t)
{
    return (QJSIntlCollationLogical){p, s, t, 0, 0};
}

static QJSIntlRelationElement root(uint32_t p, uint16_t s, uint16_t t)
{
    QJSIntlCollationLogical ce = logical(p, s, t);
    QJSIntlRelationElement result;
    assert(!qjs_intl_relation_root(&ce, &result));
    return result;
}

static QJSIntlRelationMapping apply(QJSIntlRelations *owner, unsigned int strength, int before,
                                    const QJSIntlCollationLogical *cases, size_t case_count,
                                    const QJSIntlRelationElement *extension, size_t extension_count)
{
    QJSIntlRelationMapping result = {0};
    assert(!qjs_intl_relations_apply(owner, strength, before, cases, case_count,
                                    extension, extension_count, &result));
    return result;
}

static void freeze(QJSIntlRelations *owner)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        assert(!qjs_intl_weight_freeze(&owner->axes[i]));
}

static uint32_t rank(QJSIntlRelations *owner, unsigned int axis, uint32_t id)
{
    uint32_t result;
    assert(!qjs_intl_weight_resolve(&owner->axes[axis], id, &result));
    return result;
}

static void test_repeated_resets_and_expansions(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRelations owner;
    QJSIntlRelationElement reset[] = {root(100, 32, 2), root(200, 32, 8), root(0, 40, 2)};
    QJSIntlCollationLogical lower = logical(1, 32, 2);
    QJSIntlRelationMapping x, y, z, kept, equal;
    uint32_t first;
    assert(!qjs_intl_relations_init(&owner, &a));
    /* &ae<g: change the last primary and discard weaker trailing CEs. */
    assert(!qjs_intl_relations_reset(&owner, reset, 3));
    x = apply(&owner, 1, 0, &lower, 1, NULL, 0);
    assert(x.count == 2 && x.elements[0].weights[0] == 100);
    first = x.elements[1].weights[0];
    assert(x.elements[1].weights[1] == 32 && x.elements[1].weights[2] == 2);
    assert(x.elements[1].root_primary == 200);
    /* &ae<h<k inserts h,k before the previous g, without mutating its copy. */
    assert(!qjs_intl_relations_reset(&owner, reset, 2));
    y = apply(&owner, 1, 0, &lower, 1, NULL, 0);
    z = apply(&owner, 1, 0, &lower, 1, NULL, 0);
    assert(x.elements[1].weights[0] == first);
    /* An explicit extension is copied, but does not become the chain cursor. */
    kept = apply(&owner, 0, 0, &lower, 1, reset + 1, 2);
    assert(kept.count == 4 && owner.cursor.count == 2);
    assert(!memcmp(kept.elements + 2, reset + 1, 2 * sizeof(*reset)));
    equal = apply(&owner, 0, 0, &lower, 1, NULL, 0);
    assert(equal.count == 2 && equal.elements[1].weights[0] == z.elements[1].weights[0]);
    freeze(&owner);
    assert(rank(&owner, 0, 200) < rank(&owner, 0, y.elements[1].weights[0]));
    assert(rank(&owner, 0, y.elements[1].weights[0]) < rank(&owner, 0, z.elements[1].weights[0]));
    assert(rank(&owner, 0, z.elements[1].weights[0]) < rank(&owner, 0, first));
    assert(rank(&owner, 0, first) < rank(&owner, 0, 201));
    qjs_intl_relation_mapping_clear(&a, &x);
    qjs_intl_relation_mapping_clear(&a, &y);
    qjs_intl_relation_mapping_clear(&a, &z);
    qjs_intl_relation_mapping_clear(&a, &kept);
    qjs_intl_relation_mapping_clear(&a, &equal);
    qjs_intl_relations_clear(&owner);
    assert(!state.live);
}

static void test_bands_before_and_parents(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRelations owner;
    QJSIntlRelationElement ordinary = root(100, 32, 2), zero = root(0, 0, 0);
    QJSIntlRelationElement accent = root(0, 33, 2);
    QJSIntlRelationMapping before[3], secondary, tertiary, primary, quaternary, bad = {0};
    QJSIntlWeightParents parents;
    unsigned int i;
    assert(!qjs_intl_relations_init(&owner, &a));
    for (i = 0; i < 3; i++) {
        assert(!qjs_intl_relations_reset(&owner, &ordinary, 1));
        before[i] = apply(&owner, i + 1, 1, NULL, 0, NULL, 0);
        assert(before[i].elements[0].weights[i] != ordinary.weights[i]);
    }
    assert(!qjs_intl_weight_parents(&owner.axes[2], before[2].elements[0].weights[2], &parents));
    assert(parents.primary == 100 && parents.secondary == 32);
    assert(!qjs_intl_relations_reset(&owner, &zero, 1));
    assert(qjs_intl_relations_apply(&owner, 2, 1, NULL, 0, NULL, 0, &bad) == QJS_INTL_DATA_ERROR);
    assert(!bad.elements && !bad.count && !owner.cursor.elements[0].weights[1]);
    secondary = apply(&owner, 2, 0, NULL, 0, NULL, 0);
    assert(!secondary.elements[0].weights[0] && secondary.elements[0].weights[2] == 2);
    assert(!qjs_intl_relations_reset(&owner, &zero, 1));
    tertiary = apply(&owner, 3, 0, NULL, 0, NULL, 0);
    assert(tertiary.elements[0].case_value == QJS_INTL_RELATION_UPPER);
    primary = apply(&owner, 1, 0, NULL, 0, NULL, 0);
    assert(primary.elements[0].weights[1] == 32 && primary.elements[0].weights[2] == 2);
    quaternary = apply(&owner, 4, 0, NULL, 0, NULL, 0);
    assert(quaternary.elements[0].weights[0] == primary.elements[0].weights[0]);
    freeze(&owner);
    for (i = 0; i < 3; i++) {
        uint32_t before_rank = rank(&owner, i, before[i].elements[0].weights[i]);
        assert(before_rank && before_rank < rank(&owner, i, ordinary.weights[i]));
        qjs_intl_relation_mapping_clear(&a, &before[i]);
    }
    /* Nonzero ignorable weights must follow every ordinary weak weight. */
    assert(rank(&owner, 1, 0xffff) < rank(&owner, 1, secondary.elements[0].weights[1]));
    assert(rank(&owner, 1, secondary.elements[0].weights[1]) < rank(&owner, 1, accent.weights[1]));
    assert(rank(&owner, 2, 0xffff) < rank(&owner, 2, tertiary.elements[0].weights[2]));
    assert(rank(&owner, 3, quaternary.elements[0].weights[3]) > 0);
    assert(qjs_intl_relations_reset(&owner, &zero, 1) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_relation_mapping_clear(&a, &secondary);
    qjs_intl_relation_mapping_clear(&a, &tertiary);
    qjs_intl_relation_mapping_clear(&a, &primary);
    qjs_intl_relation_mapping_clear(&a, &quaternary);
    qjs_intl_relations_clear(&owner);
    assert(!state.live);
}

static void test_case_and_equality(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRelations owner;
    QJSIntlRelationElement reset[] = {root(100, 32, 2), root(200, 32, 8), root(0, 40, 8)};
    QJSIntlCollationLogical cases[] = {logical(1, 32, 8), logical(0, 40, 8), logical(2, 32, 2)};
    QJSIntlRelationMapping result;
    assert(!qjs_intl_relations_init(&owner, &a));
    /* &a=A keeps ordinary T=2 but independently becomes uppercase. */
    assert(!qjs_intl_relations_reset(&owner, reset, 1));
    result = apply(&owner, 0, 0, cases, 1, NULL, 0);
    assert(result.elements[0].weights[2] == 2);
    assert(result.elements[0].case_value == QJS_INTL_RELATION_UPPER);
    qjs_intl_relation_mapping_clear(&a, &result);
    /* &A=a keeps ordinary T=8 but becomes lower/uncased. */
    assert(!qjs_intl_relations_reset(&owner, reset + 1, 1));
    result = apply(&owner, 0, 0, cases + 2, 1, NULL, 0);
    assert(result.elements[0].weights[2] == 8);
    assert(result.elements[0].case_value == QJS_INTL_RELATION_LOWER);
    qjs_intl_relation_mapping_clear(&a, &result);
    /* &aE=Ch distributes upper/lower; &a=Ch/E folds Ch to MIXED and copies E. */
    assert(!qjs_intl_relations_reset(&owner, reset, 3));
    result = apply(&owner, 0, 0, cases, 3, NULL, 0);
    assert(result.count == 3 && result.elements[0].case_value == QJS_INTL_RELATION_UPPER);
    assert(result.elements[1].case_value == QJS_INTL_RELATION_LOWER);
    assert(result.elements[2].case_value == QJS_INTL_RELATION_LOWER);
    qjs_intl_relation_mapping_clear(&a, &result);
    assert(!qjs_intl_relations_reset(&owner, reset, 1));
    result = apply(&owner, 0, 0, cases, 3, reset + 1, 1);
    assert(result.count == 2 && result.elements[0].case_value == QJS_INTL_RELATION_MIXED);
    assert(result.elements[1].case_value == QJS_INTL_RELATION_UPPER);
    qjs_intl_relation_mapping_clear(&a, &result);
    /* Fewer root primaries clear the remaining primary cases. */
    assert(!qjs_intl_relations_reset(&owner, reset, 2));
    result = apply(&owner, 0, 0, cases, 1, NULL, 0);
    assert(result.elements[0].case_value == QJS_INTL_RELATION_UPPER);
    assert(result.elements[1].case_value == QJS_INTL_RELATION_LOWER);
    qjs_intl_relation_mapping_clear(&a, &result);
    /* Homogeneous upper/lower expansions do not acquire a mixed case. */
    assert(!qjs_intl_relations_reset(&owner, reset, 1));
    cases[2].tertiary = 8;
    result = apply(&owner, 0, 0, cases, 3, NULL, 0);
    assert(result.elements[0].case_value == QJS_INTL_RELATION_UPPER);
    qjs_intl_relation_mapping_clear(&a, &result);
    cases[0].tertiary = cases[2].tertiary = 2;
    result = apply(&owner, 0, 0, cases, 3, NULL, 0);
    assert(result.elements[0].case_value == QJS_INTL_RELATION_LOWER);
    qjs_intl_relation_mapping_clear(&a, &result);
    qjs_intl_relations_clear(&owner);
    assert(!state.live);
}

static void test_root_provenance_and_empty_reset(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRelations owner;
    QJSIntlCollationLogical source = logical(QJS_INTL_COLLATION_PRIMARY_ROOT_MAX - 1, 32, 2);
    QJSIntlRelationElement ce;
    QJSIntlRelationMapping result, bad = {0};
    source.implicit = 1;
    assert(!qjs_intl_relation_root(&source, &ce));
    assert(ce.root_primary == source.primary && ce.implicit && !ce.variable);
    assert(!qjs_intl_relations_init(&owner, &a));
    assert(qjs_intl_relations_apply(&owner, 1, 0, NULL, 0, NULL, 0, &bad) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!bad.count && !bad.elements);
    assert(!qjs_intl_relations_reset(&owner, &ce, 1));
    result = apply(&owner, 1, 0, NULL, 0, NULL, 0);
    assert(result.elements[0].root_primary == source.primary && result.elements[0].implicit);
    qjs_intl_relation_mapping_clear(&a, &result);
    assert(!qjs_intl_relations_reset(&owner, NULL, 0));
    assert(owner.cursor.count == 1 && !owner.cursor.elements[0].weights[0]);
    result = apply(&owner, 0, 0, NULL, 0, NULL, 0);
    assert(result.count == 1 && !result.elements[0].weights[2]);
    assert(result.elements[0].case_value == QJS_INTL_RELATION_LOWER);
    qjs_intl_relation_mapping_clear(&a, &result);
    assert(qjs_intl_relations_apply(&owner, 4, 1, NULL, 0, NULL, 0, &bad) ==
           QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_relations_clear(&owner);
    assert(!state.live);
}

static void test_interleaved_weak_parents(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRelations owner;
    QJSIntlRelationElement roots[] = {root(100, 32, 2), root(200, 32, 2)};
    QJSIntlRelationMapping s[2], t[2], copied;
    QJSIntlWeightParents parents;
    unsigned int i;
    assert(!qjs_intl_relations_init(&owner, &a));
    for (i = 0; i < 2; i++) {
        assert(!qjs_intl_relations_reset(&owner, roots + i, 1));
        s[i] = apply(&owner, 2, 0, NULL, 0, NULL, 0);
    }
    for (i = 0; i < 2; i++) {
        assert(!qjs_intl_relations_reset(&owner, s[i].elements, s[i].count));
        t[i] = apply(&owner, 3, 0, NULL, 0, NULL, 0);
        assert(!qjs_intl_weight_parents(&owner.axes[2], t[i].elements[0].weights[2], &parents));
        assert(parents.primary == roots[i].weights[0]);
        assert(parents.secondary == s[i].elements[0].weights[1]);
    }
    /* Both inputs can borrow the old cursor; commit releases it only after
       all source CEs have been copied to independent output/cursor storage. */
    copied = apply(&owner, 1, 0, NULL, 0, owner.cursor.elements, owner.cursor.count);
    assert(copied.count == 2 && owner.cursor.count == 1);
    assert(!memcmp(copied.elements + 1, t[1].elements, sizeof(*copied.elements)));
    freeze(&owner);
    for (i = 0; i < 2; i++) {
        assert(rank(&owner, 1, s[i].elements[0].weights[1]) > rank(&owner, 1, 32));
        assert(rank(&owner, 1, s[i].elements[0].weights[1]) < rank(&owner, 1, 33));
        assert(t[i].elements[0].weights[0] == roots[i].weights[0]);
        assert(t[i].elements[0].weights[1] == s[i].elements[0].weights[1]);
        assert(rank(&owner, 2, t[i].elements[0].weights[2]) > rank(&owner, 2, 2));
        assert(rank(&owner, 2, t[i].elements[0].weights[2]) < rank(&owner, 2, 3));
        qjs_intl_relation_mapping_clear(&a, &s[i]);
        qjs_intl_relation_mapping_clear(&a, &t[i]);
    }
    qjs_intl_relation_mapping_clear(&a, &copied);
    qjs_intl_relations_clear(&owner);
    assert(!state.live);
}

static size_t allocation_case(size_t fail_at)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRelations owner;
    QJSIntlRelationElement reset = root(100, 32, 2), saved;
    QJSIntlRelationMapping result = {0};
    QJSIntlStatus status;
    size_t calls;
    assert(!qjs_intl_relations_init(&owner, &a));
    assert(!qjs_intl_relations_reset(&owner, &reset, 1));
    saved = owner.cursor.elements[0];
    state.calls = 0;
    state.fail_at = fail_at;
    status = qjs_intl_relations_apply(&owner, 1, 0, NULL, 0, &reset, 1, &result);
    calls = state.calls;
    if (fail_at) {
        assert(status == QJS_INTL_NO_MEMORY && !result.count && !result.elements);
        assert(owner.cursor.count == 1 && !memcmp(&saved, owner.cursor.elements, sizeof(saved)));
        assert(owner.axes[0].node_count == 0);
        state.fail_at = 0;
        assert(!qjs_intl_relations_apply(&owner, 1, 0, NULL, 0, &reset, 1, &result));
    } else {
        assert(!status);
    }
    freeze(&owner);
    assert(rank(&owner, 0, result.elements[0].weights[0]) == 101);
    assert(result.count == 2 && result.elements[1].weights[0] == 100);
    qjs_intl_relation_mapping_clear(&a, &result);
    /* Reset can borrow its own cursor; a failed copy must not release it. */
    state.calls = 0;
    state.fail_at = 1;
    qjs_intl_relations_clear(&owner);
    assert(!qjs_intl_relations_init(&owner, &a));
    state.fail_at = 0;
    assert(!qjs_intl_relations_reset(&owner, &reset, 1));
    state.fail_at = state.calls + 1;
    assert(qjs_intl_relations_reset(&owner, owner.cursor.elements, owner.cursor.count) ==
           QJS_INTL_NO_MEMORY);
    assert(!memcmp(&reset, owner.cursor.elements, sizeof(reset)));
    state.fail_at = 0;
    assert(!qjs_intl_relations_reset(&owner, owner.cursor.elements, owner.cursor.count));
    assert(!memcmp(&reset, owner.cursor.elements, sizeof(reset)));
    qjs_intl_relations_clear(&owner);
    assert(!state.live);
    return calls;
}

int main(void)
{
    size_t calls, failure;
    test_repeated_resets_and_expansions();
    test_bands_before_and_parents();
    test_case_and_equality();
    test_root_provenance_and_empty_reset();
    test_interleaved_weak_parents();
    calls = allocation_case(0);
    for (failure = 1; failure <= calls; failure++)
        allocation_case(failure);
    return 0;
}
