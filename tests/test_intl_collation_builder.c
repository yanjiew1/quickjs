/* Ordered rule execution and ownership across current/root mapping views.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../tools/intl/collation-builder.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t intl_root_fixture[];
extern const size_t intl_root_fixture_size;

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

static void relate(QJSIntlCollationBuilder *builder, unsigned int strength,
                    const uint32_t *prefix, uint32_t prefix_count,
                    const uint32_t *text, uint32_t count,
                    const uint32_t *extension, uint32_t extension_count)
{
    assert(!qjs_intl_collation_builder_relate(builder, strength, prefix, prefix_count,
                                              text, count, extension, extension_count));
}

static QJSIntlRelationElement one(QJSIntlCollationBuilder *builder, uint32_t code)
{
    QJSIntlRelationMapping out = {0};
    QJSIntlRelationElement result;
    assert(!qjs_intl_collation_builder_lookup(builder, &code, 1, &out));
    assert(out.count == 1);
    result = out.elements[0];
    qjs_intl_relation_mapping_clear(&builder->allocator, &out);
    return result;
}

static uint32_t rank(QJSIntlCollationBuilder *builder, unsigned int axis, uint32_t id)
{
    uint32_t result;
    assert(!qjs_intl_weight_resolve(&builder->relations.axes[axis], id, &result));
    return result;
}

static void test_order(const QJSIntlRootInput *data)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationBuilder builder;
    uint32_t a_code = 'a', g_code = 'g', h_code = 'h', k_code = 'k', u_code = 'u';
    QJSIntlRelationElement g, h, k, u;
    unsigned int i;
    uint32_t base;
    assert(!qjs_intl_collation_builder_init(&builder, data, &a));
    base = one(&builder, a_code).weights[0];
    assert(!qjs_intl_collation_builder_reset(&builder, &a_code, 1, 0));
    relate(&builder, 1, NULL, 0, &g_code, 1, NULL, 0);
    g = one(&builder, g_code);
    assert(!qjs_intl_collation_builder_reset(&builder, &a_code, 1, 0));
    relate(&builder, 1, NULL, 0, &h_code, 1, NULL, 0);
    relate(&builder, 1, NULL, 0, &k_code, 1, NULL, 0);
    h = one(&builder, h_code);
    k = one(&builder, k_code);
    assert(!qjs_intl_collation_builder_reset(&builder, &a_code, 1, 2));
    relate(&builder, 2, NULL, 0, &u_code, 1, NULL, 0);
    u = one(&builder, u_code);
    assert(!builder.before);
    for (i = 0; i < 4; i++)
        assert(!qjs_intl_weight_freeze(&builder.relations.axes[i]));
    assert(rank(&builder, 0, base) < rank(&builder, 0, h.weights[0]));
    assert(rank(&builder, 0, h.weights[0]) < rank(&builder, 0, k.weights[0]));
    assert(rank(&builder, 0, k.weights[0]) < rank(&builder, 0, g.weights[0]));
    assert(rank(&builder, 0, g.weights[0]) < rank(&builder, 0, base + 1));
    assert(u.weights[0] == base && rank(&builder, 1, u.weights[1]) < rank(&builder, 1, 32));
    qjs_intl_collation_builder_clear(&builder);
    assert(!state.live);
}

static void test_case_extensions_and_normalization(const QJSIntlRootInput *data)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationBuilder builder;
    QJSIntlRelationMapping mapping = {0};
    QJSIntlRelationElement ce, saved;
    const uint32_t a_code = 'a', upper = 'A', lower = 'w', x = 'x', pair[] = {'A','w'};
    const uint32_t l = 'l', colon = ':', contextual[] = {'l',':'}, composed = 0xe5;
    const uint32_t zero = 0, supplementary = 0x1f600, decomposed[] = {'a',0x30a};
    uint32_t base, l_base;
    assert(!qjs_intl_collation_builder_init(&builder, data, &a));
    base = one(&builder, a_code).weights[0];
    l_base = one(&builder, l).weights[0];
    assert(!qjs_intl_collation_builder_reset(&builder, &a_code, 1, 0));
    relate(&builder, 0, NULL, 0, &upper, 1, NULL, 0);
    ce = one(&builder, upper);
    assert(ce.weights[2] == data->metadata.limits.tertiary_common && ce.case_value == QJS_INTL_RELATION_UPPER);
    assert(!qjs_intl_collation_builder_reset(&builder, &upper, 1, 0));
    relate(&builder, 0, NULL, 0, &lower, 1, NULL, 0);
    ce = one(&builder, lower);
    assert(ce.weights[2] == data->metadata.limits.tertiary_common && ce.case_value == QJS_INTL_RELATION_LOWER);
    /* Immutable root Aw remains upper/lower after both current mappings change.
       The explicit extension A retains its CURRENT upper-case CE verbatim. */
    relate(&builder, 0, NULL, 0, pair, 2, &upper, 1);
    assert(!qjs_intl_collation_builder_lookup(&builder, pair, 2, &mapping));
    assert(mapping.count == 2 && mapping.elements[0].case_value == QJS_INTL_RELATION_MIXED);
    assert(mapping.elements[1].case_value == QJS_INTL_RELATION_UPPER);
    qjs_intl_relation_mapping_clear(&a, &mapping);
    relate(&builder, 1, NULL, 0, &x, 1, NULL, 0);
    saved = one(&builder, x); /* Previous extension does not enter the cursor. */
    relate(&builder, 0, &l, 1, &colon, 1, NULL, 0);
    assert(!qjs_intl_collation_builder_lookup(&builder, contextual, 2, &mapping));
    assert(mapping.count == 2 && mapping.elements[0].weights[0] == l_base);
    assert(mapping.elements[1].weights[0] == saved.weights[0]);
    qjs_intl_relation_mapping_clear(&a, &mapping);
    ce = one(&builder, colon);
    assert(ce.weights[0] == data->metadata.generic_base + colon + 1);
    assert(!qjs_intl_collation_builder_reset(&builder, &a_code, 1, 0));
    relate(&builder, 0, NULL, 0, &composed, 1, NULL, 0);
    ce = one(&builder, composed);
    assert(ce.weights[0] == base);
    assert(!qjs_intl_collation_builder_lookup(&builder, decomposed, 2, &mapping));
    assert(mapping.count == 1 && mapping.elements[0].weights[0] == ce.weights[0]);
    qjs_intl_relation_mapping_clear(&a, &mapping);
    relate(&builder, 0, NULL, 0, &zero, 1, NULL, 0);
    ce = one(&builder, zero);
    assert(ce.weights[0] == base); /* Counted NUL is not an empty key. */
    ce = one(&builder, supplementary);
    assert(ce.weights[0] < data->metadata.han_base && ce.weights[0] != base);
    qjs_intl_collation_builder_clear(&builder);
    assert(!state.live);
}

static void test_logical_positions_retain_rule_history(const QJSIntlRootInput *data)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationBuilder builder;
    const uint32_t a_code = 'a', x = 'x', y = 'y', z = 'z', prefix = 'p';
    QJSIntlRelationElement old_x, y_ce, z_ce;
    assert(!qjs_intl_collation_builder_init(&builder, data, &a));
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_LAST_PRIMARY_IGNORABLE, 0));
    relate(&builder, 2, NULL, 0, &y, 1, NULL, 0);
    relate(&builder, 2, NULL, 0, &x, 1, NULL, 0);
    old_x = one(&builder, x);
    y_ce = one(&builder, y);
    assert(!qjs_intl_collation_builder_reset(&builder, &a_code, 1, 0));
    relate(&builder, 0, NULL, 0, &x, 1, NULL, 0);
    assert(one(&builder, x).weights[0] == one(&builder, a_code).weights[0]);
    /* &[last PI]<<y<<x &a=x &[before 2][last PI]<<z: the established
       x position persists. A live-only scan would incorrectly put z before y. */
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_LAST_PRIMARY_IGNORABLE, 2));
    relate(&builder, 2, NULL, 0, &z, 1, NULL, 0);
    z_ce = one(&builder, z);
    assert(!qjs_intl_weight_freeze(&builder.relations.axes[1]));
    assert(rank(&builder, 1, y_ce.weights[1]) < rank(&builder, 1, z_ce.weights[1]));
    assert(rank(&builder, 1, z_ce.weights[1]) < rank(&builder, 1, old_x.weights[1]));
    qjs_intl_collation_builder_clear(&builder);
    assert(!qjs_intl_collation_builder_init(&builder, data, &a));
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_LAST_PRIMARY_IGNORABLE, 0));
    relate(&builder, 2, &prefix, 1, &x, 1, NULL, 0);
    old_x = builder.relations.cursor.elements[0];
    assert(!qjs_intl_collation_map_suppress(&builder.map, x));
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_LAST_PRIMARY_IGNORABLE, 2));
    relate(&builder, 2, NULL, 0, &z, 1, NULL, 0);
    z_ce = one(&builder, z);
    assert(!qjs_intl_weight_freeze(&builder.relations.axes[1]));
    assert(rank(&builder, 1, z_ce.weights[1]) < rank(&builder, 1, old_x.weights[1]));
    qjs_intl_collation_builder_clear(&builder);
    assert(!qjs_intl_collation_builder_init(&builder, data, &a));
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_FIRST_PRIMARY_IGNORABLE, 2));
    assert(builder.relations.cursor.count == 1);
    relate(&builder, 2, NULL, 0, &x, 1, NULL, 0);
    old_x = one(&builder, x);
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_FIRST_PRIMARY_IGNORABLE, 0));
    assert(builder.relations.cursor.elements[0].weights[1] == old_x.weights[1]);
    qjs_intl_collation_builder_clear(&builder);
    assert(!state.live);
}

static void test_logical_seeds_and_ignorable_classes(const QJSIntlRootInput *data)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationBuilder builder;
    QJSIntlRelationElement x_ce, y_ce, z_ce, w_ce, t_ce, q_ce, r_ce;
    const uint32_t x = 'x', y = 'y', z = 'z', w = 'w', t = 't', q = 'q', r = 'r';
    uint32_t seed;
    assert(!qjs_intl_collation_builder_init(&builder, data, &a));
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_LAST_SECONDARY_IGNORABLE, 0));
    seed = builder.relations.cursor.elements[0].weights[2];
    relate(&builder, 3, NULL, 0, &x, 1, NULL, 0);
    x_ce = one(&builder, x);
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_FIRST_SECONDARY_IGNORABLE, 0));
    relate(&builder, 3, NULL, 0, &y, 1, NULL, 0);
    y_ce = one(&builder, y);
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_FIRST_SECONDARY_IGNORABLE, 3));
    relate(&builder, 3, NULL, 0, &z, 1, NULL, 0);
    z_ce = one(&builder, z);
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_FIRST_SECONDARY_IGNORABLE, 0));
    relate(&builder, 3, NULL, 0, &w, 1, NULL, 0);
    w_ce = one(&builder, w);
    /* A new T-only position from zero enters before the constructed SI seed. */
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_LAST_TERTIARY_IGNORABLE, 0));
    relate(&builder, 3, NULL, 0, &t, 1, NULL, 0);
    t_ce = one(&builder, t);
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_FIRST_SECONDARY_IGNORABLE, 0));
    assert(builder.relations.cursor.elements[0].weights[2] == t_ce.weights[2]);
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_LAST_TERTIARY_IGNORABLE, 0));
    relate(&builder, 4, NULL, 0, &q, 1, NULL, 0);
    q_ce = one(&builder, q);
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_LAST_TERTIARY_IGNORABLE, 0));
    relate(&builder, 4, NULL, 0, &r, 1, NULL, 0);
    r_ce = one(&builder, r);
    assert(!qjs_intl_weight_freeze(&builder.relations.axes[2]));
    assert(!qjs_intl_weight_freeze(&builder.relations.axes[3]));
    assert(rank(&builder, 2, t_ce.weights[2]) < rank(&builder, 2, z_ce.weights[2]));
    assert(rank(&builder, 2, z_ce.weights[2]) < rank(&builder, 2, w_ce.weights[2]));
    assert(rank(&builder, 2, w_ce.weights[2]) < rank(&builder, 2, seed));
    assert(rank(&builder, 2, seed) < rank(&builder, 2, y_ce.weights[2]));
    assert(rank(&builder, 2, y_ce.weights[2]) < rank(&builder, 2, x_ce.weights[2]));
    assert(!q_ce.weights[0] && !q_ce.weights[1] && !q_ce.weights[2]);
    assert(rank(&builder, 3, q_ce.weights[3]) < rank(&builder, 3, r_ce.weights[3]));
    qjs_intl_collation_builder_clear(&builder);
    assert(!state.live);
}

static void test_last_regular_gap_and_unsupported(const QJSIntlRootInput *data)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationBuilder builder;
    QJSIntlRelationElement x_ce, y_ce;
    const uint32_t first_han = 0x4e02, x = 'x', y = 'y';
    unsigned int kind;
    assert(!qjs_intl_collation_builder_init(&builder, data, &a));
    assert(!qjs_intl_collation_builder_reset(&builder, &first_han, 1, 1));
    relate(&builder, 1, NULL, 0, &x, 1, NULL, 0);
    x_ce = one(&builder, x);
    assert(x_ce.root_primary == data->metadata.han_base + 1);
    assert(!qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_LAST_REGULAR, 0));
    relate(&builder, 1, NULL, 0, &y, 1, NULL, 0);
    y_ce = one(&builder, y);
    assert(!qjs_intl_weight_freeze(&builder.relations.axes[0]));
    assert(rank(&builder, 0, data->metadata.han_base) < rank(&builder, 0, x_ce.weights[0]));
    assert(rank(&builder, 0, x_ce.weights[0]) < rank(&builder, 0, y_ce.weights[0]));
    assert(rank(&builder, 0, y_ce.weights[0]) < rank(&builder, 0, data->metadata.han_base + 1));
    qjs_intl_collation_builder_clear(&builder);
    for (kind = QJS_INTL_ROOT_FIRST_TERTIARY_IGNORABLE; kind <= QJS_INTL_ROOT_LAST_TRAILING; kind++) {
        if (kind == QJS_INTL_ROOT_LAST_TERTIARY_IGNORABLE ||
            kind == QJS_INTL_ROOT_FIRST_SECONDARY_IGNORABLE || kind == QJS_INTL_ROOT_LAST_SECONDARY_IGNORABLE ||
            kind == QJS_INTL_ROOT_FIRST_PRIMARY_IGNORABLE || kind == QJS_INTL_ROOT_LAST_PRIMARY_IGNORABLE ||
            kind == QJS_INTL_ROOT_LAST_REGULAR)
            continue;
        assert(!qjs_intl_collation_builder_init(&builder, data, &a));
        assert(qjs_intl_collation_builder_logical(&builder, kind, 0) == QJS_INTL_UNSUPPORTED && builder.failed);
        qjs_intl_collation_builder_clear(&builder);
    }
    assert(!state.live);
}

static void test_invalid_commands(const QJSIntlRootInput *data)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationBuilder builder;
    QJSIntlRelationMapping out = {0};
    const uint32_t code = 'a', acute = 0x301;
    assert(!qjs_intl_collation_builder_init(&builder, data, &a));
    assert(!qjs_intl_collation_builder_reset(&builder, &code, 1, 2));
    assert(qjs_intl_collation_builder_relate(&builder, 1, NULL, 0, &code, 1, NULL, 0) ==
           QJS_INTL_INVALID_ARGUMENT && builder.failed);
    assert(qjs_intl_collation_builder_lookup(&builder, &code, 1, &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(!out.count && !out.elements);
    qjs_intl_collation_builder_clear(&builder);
    assert(!qjs_intl_collation_builder_init(&builder, data, &a));
    assert(!qjs_intl_collation_builder_reset(&builder, &code, 1, 0));
    assert(qjs_intl_collation_builder_relate(&builder, 0, &code, 1, &acute, 1, NULL, 0) ==
           QJS_INTL_DATA_ERROR && builder.failed);
    qjs_intl_collation_builder_clear(&builder);
    assert(!qjs_intl_collation_builder_init(&builder, data, &a));
    assert(qjs_intl_collation_builder_lookup(&builder, &code, UINT32_MAX, &out) ==
           QJS_INTL_OVERFLOW && builder.failed);
    qjs_intl_collation_builder_clear(&builder);
    assert(!qjs_intl_collation_builder_init(&builder, data, &a));
    assert(qjs_intl_collation_builder_lookup(&builder, &code, 1, NULL) ==
           QJS_INTL_INVALID_ARGUMENT && builder.failed);
    qjs_intl_collation_builder_clear(&builder);
    assert(!state.live);
}

static size_t allocation_case(const QJSIntlRootInput *data, size_t fail_at)
{
    Allocation state = {.fail_at = fail_at};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationBuilder builder;
    QJSIntlRelationMapping out = {0};
    QJSIntlStatus status;
    const uint32_t a_code = 'a', x = 'x', y[] = {'A','w'};
    status = qjs_intl_collation_builder_init(&builder, data, &a);
    if (!status)
        status = qjs_intl_collation_builder_reset(&builder, &a_code, 1, 0);
    if (!status)
        status = qjs_intl_collation_builder_relate(&builder, 1, NULL, 0, &x, 1, NULL, 0);
    if (!status)
        status = qjs_intl_collation_builder_reset(&builder, &x, 1, 0);
    if (!status)
        status = qjs_intl_collation_builder_logical(&builder, QJS_INTL_ROOT_LAST_SECONDARY_IGNORABLE, 0);
    if (!status)
        status = qjs_intl_collation_builder_relate(&builder, 0, NULL, 0, y, 2, &x, 1);
    if (!status)
        status = qjs_intl_collation_builder_lookup(&builder, y, 2, &out);
    if (status) {
        assert(fail_at && status == QJS_INTL_NO_MEMORY && state.failures == 1);
        assert(!builder.ready || builder.failed);
        assert(qjs_intl_collation_builder_reset(&builder, &a_code, 1, 0) == QJS_INTL_INVALID_ARGUMENT);
    } else {
        assert(!fail_at && out.count == 2);
    }
    qjs_intl_relation_mapping_clear(&a, &out);
    qjs_intl_collation_builder_clear(&builder);
    assert(!state.live);
    return state.calls;
}

int main(void)
{
    QJSIntlRootInput data;
    size_t calls, failure;
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    assert(!qjs_intl_root_input_open(intl_root_fixture, intl_root_fixture_size, &a, &data));
    assert(!state.live);
    test_order(&data);
    test_case_extensions_and_normalization(&data);
    test_invalid_commands(&data);
    test_logical_positions_retain_rule_history(&data);
    test_logical_seeds_and_ignorable_classes(&data);
    test_last_regular_gap_and_unsupported(&data);
    calls = allocation_case(&data, 0);
    for (failure = 1; failure <= calls; failure++)
        allocation_case(&data, failure);
    return 0;
}
