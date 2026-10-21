/* Semantic producer/reader roundtrip, rank boundaries and untrusted bytes.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../tools/intl/collation-archive.h"
#include "../src/intl/collation/compiled-data.h"
#include "../src/intl/collation/ranked-elements.h"
#include "../src/intl/collation/ranked-compare.h"
#include "intl_collation_ordered.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern const uint8_t intl_root_fixture[], intl_program_success[];
extern const size_t intl_root_fixture_size, intl_program_success_size;

typedef struct Fixture {
    size_t calls, live, fail_at;
    unsigned int completed;
    QJSIntlCollationArchive archive;
    QJSIntlCollationSnapshot snapshots[3];
    QJSIntlCompiledTrie tries[3];
    QJSIntlCompiledCollation image;
} Fixture;

static void *allocate(void *opaque, size_t size)
{
    Fixture *f = opaque;
    void *p;

    assert(size);
    if (++f->calls == f->fail_at)
        return NULL;

    p = malloc(size);
    assert(p);
    f->live++;
    return p;
}

static void *resize(void *opaque, void *p, size_t size)
{
    Fixture *f = opaque;
    void *next;

    assert(p && size);
    if (++f->calls == f->fail_at)
        return NULL;

    next = realloc(p, size);
    assert(next);
    return next;
}

static void release(void *opaque, void *p)
{
    Fixture *f = opaque;

    assert(p && f->live);
    f->live--;
    free(p);
}

static QJSIntlAllocator allocator(Fixture *f)
{
    return (QJSIntlAllocator){f, allocate, resize, release};
}

static QJSIntlStatus retain(void *opaque, const QJSIntlIRProgram *identity, QJSIntlCollationProgram *program)
{
    Fixture *f = opaque;
    unsigned int slot = f->completed;
    QJSIntlStatus status;

    assert(slot < 3);
    status = qjs_intl_collation_snapshot_create(program, identity, f->snapshots + slot);
    if (!status)
        status = qjs_intl_compiled_trie_create(f->snapshots + slot, f->tries + slot);
    if (!status)
        status = qjs_intl_collation_archive_add(&f->archive, f->snapshots + slot, f->tries + slot);
    if (!status)
        f->completed++;

    return status;
}

static void init_program(QJSIntlCollationProgram *program, const QJSIntlRootInput *root, Fixture *f)
{
    QJSIntlAllocator a = allocator(f);

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

static void make(Fixture *f, QJSIntlRootInput *root)
{
    QJSIntlAllocator a = allocator(f);
    QJSIntlIRView ir;
    QJSIntlCollationProgram program;
    const QJSIntlIRProgram identity = {{(const uint8_t *)"zz", 2}, {(const uint8_t *)"standard", 8}, {0}, 4};
    const uint32_t q = 'q', empty = 0xe6, digit = 0xff17, marks[] = {'w', 0x327, 0x301};
    uint32_t unused;

    assert(!qjs_intl_root_input_open(intl_root_fixture, intl_root_fixture_size, &a, root));
    assert(!qjs_intl_ir_open(intl_program_success, intl_program_success_size, root->fingerprint, &ir, NULL));
    assert(!qjs_intl_collation_archive_init(&f->archive, root, &a));
    assert(!qjs_intl_collation_program_execute(root, &ir, &a, retain, f, NULL));
    assert(f->completed == 2);

    init_program(&program, root, f);
    assert(!qjs_intl_collation_builder_logical(&program.builder, QJS_INTL_ROOT_LAST_TERTIARY_IGNORABLE, 0));
    assert(!qjs_intl_collation_builder_relate(&program.builder, 4, NULL, 0, &q, 1, NULL, 0));
    assert(!qjs_intl_collation_builder_relate(&program.builder, 0, NULL, 0, marks, 3, NULL, 0));
    assert(!qjs_intl_collation_builder_relate(&program.builder, 0, NULL, 0, &digit, 1, NULL, 0));
    assert(!qjs_intl_collation_map_put(&program.builder.map, NULL, 0, &empty, 1, NULL, 0, &unused));

    /* Established positions can remain unused by live CEs. Preserve both gap0
       and the final root's own gap, without expanding the virtual root domain. */
    assert(!qjs_intl_weight_insert(program.builder.relations.axes, 0, 0, NULL, &unused));
    assert(!qjs_intl_weight_insert(program.builder.relations.axes, root->metadata.limits.primary_max, 0, NULL, &unused));
    assert(!retain(f, &identity, &program));
    clear_program(&program);

    assert(!qjs_intl_collation_archive_finish(&f->archive, &f->image));
    qjs_intl_collation_archive_clear(&f->archive);
}

static void clear_sources(Fixture *f)
{
    unsigned int i;

    for (i = 0; i < 3; i++) {
        qjs_intl_compiled_trie_clear(f->tries + i);
        qjs_intl_collation_snapshot_clear(f->snapshots + i);
    }
}

static void equal_ce(const QJSIntlCollationRankedElement *a, const QJSIntlCollationRankedElement *b)
{
    unsigned int axis;

    for (axis = 0; axis < 4; axis++)
        assert(a->ranks[axis] == b->ranks[axis]);
    assert(a->root_primary == b->root_primary && a->case_value == b->case_value);
    assert(a->variable == b->variable && a->implicit == b->implicit);
}

static void equal_bytes(const void *a, const void *b, size_t count)
{
    if (count)
        assert(!memcmp(a, b, count));
}

static void compare_program(const QJSIntlCollationSnapshot *source, const QJSIntlCompiledCollationProgram *p)
{
    uint32_t i;
    unsigned int axis;

    assert(p->locale.length == source->identity.locale.length && p->collation.length == source->identity.collation.length);
    assert(p->alternate.length == source->identity.alternate.length);
    equal_bytes(p->locale.data, source->identity.locale.bytes, p->locale.length);
    equal_bytes(p->collation.data, source->identity.collation.bytes, p->collation.length);
    equal_bytes(p->alternate.data, source->identity.alternate.bytes, p->alternate.length);

    assert(p->policy.strength == source->policy.strength && p->policy.case_first == source->policy.case_first);
    assert(p->policy.max_variable == source->policy.max_variable && p->policy.shifted == source->policy.shifted);
    assert(p->policy.backwards_secondary == source->policy.backwards_secondary && p->policy.numeric == source->policy.numeric);
    assert(p->policy.normalization == source->policy.normalization && p->policy.case_level == source->policy.case_level);

    assert(p->keys.row_count == source->keys.row_count && p->keys.key_count == source->keys.key_count);
    assert(p->keys.context_count == source->keys.context_count && p->elements.count == source->element_count);
    equal_bytes(p->keys.rows, source->keys.rows, (size_t)p->keys.row_count * 16);
    equal_bytes(p->keys.keys, source->keys.keys, (size_t)p->keys.key_count * 4);
    equal_bytes(p->keys.contexts, source->keys.contexts, (size_t)p->keys.context_count * 12);

    for (i = 0; i < p->elements.count; i++) {
        QJSIntlCollationRankedElement ce;
        assert(!qjs_intl_compiled_collation_element(p, i, &ce));
        equal_ce(&ce, source->elements + i);
    }

    assert(p->suppressed.count == source->suppressed_count && p->group_ranks.count == source->group_count);
    for (i = 0; i < p->suppressed.count; i++)
        assert(qjs_intl_read_u32(p->suppressed.bytes + (size_t)i * 4) == source->suppressed[i]);
    for (i = 0; i < p->group_ranks.count; i++)
        assert(qjs_intl_read_u32(p->group_ranks.bytes + (size_t)i * 4) == source->group_ranks[i]);

    for (axis = 0; axis < 4; axis++) {
        uint32_t maximum = source->root_maxima[axis], root, expected, actual;
        assert(p->offsets[axis].count == source->offset_counts[axis]);
        for (i = 0; i < source->offset_counts[axis]; i++) {
            const QJSIntlCollationRootOffset *offset = source->offsets[axis] + i;
            assert(qjs_intl_read_u32(p->offsets[axis].bytes + (size_t)i * 8) == offset->root_gap);
            assert(qjs_intl_read_u32(p->offsets[axis].bytes + (size_t)i * 8 + 4) == offset->added);

            for (root = offset->root_gap; root <= offset->root_gap + 1 && root <= maximum; root++) {
                assert(!qjs_intl_collation_snapshot_root_rank(source, axis, root, &expected));
                assert(!qjs_intl_compiled_collation_root_rank(p, axis, root, &actual) && actual == expected);
            }
        }
        assert(!qjs_intl_collation_snapshot_root_rank(source, axis, maximum, &expected));
        assert(!qjs_intl_compiled_collation_root_rank(p, axis, maximum, &actual) && actual == expected);
        assert(p->final_maxima[axis] == maximum + (source->offset_counts[axis] ?
            source->offsets[axis][source->offset_counts[axis] - 1].added : 0));
    }

    for (i = 0; i < source->root.base.elements.count; i++) {
        QJSIntlCollationCoordinate original;
        QJSIntlCollationRankedElement expected = {0}, actual;
        assert(!qjs_intl_collation_base_element(&source->root.base, i, &original));
        for (axis = 0; axis < 4; axis++)
            assert(!qjs_intl_collation_snapshot_root_rank(source, axis, original.weights[axis], expected.ranks + axis));
        expected.root_primary = original.root_primary;
        expected.case_value = original.case_value;
        expected.variable = original.variable;
        expected.implicit = original.implicit;
        assert(!qjs_intl_compiled_collation_root_element(p, i, &actual));
        equal_ce(&expected, &actual);
    }

    {
        const uint32_t codes[] = {0, 0x4e00, 0xd800, 0x20000, 0x10ffff};
        for (i = 0; i < sizeof(codes) / sizeof(*codes); i++) {
            QJSIntlCollationCoordinate original;
            QJSIntlCollationRankedElement actual;
            assert(!qjs_intl_root_input_implicit(&source->root, codes[i], &original));
            assert(!qjs_intl_compiled_collation_implicit(p, codes[i], &actual));
            assert(actual.root_primary == original.root_primary && actual.implicit && !actual.variable);
            for (axis = 0; axis < 4; axis++) {
                uint32_t expected;
                assert(!qjs_intl_collation_snapshot_root_rank(source, axis, original.weights[axis], &expected));
                assert(actual.ranks[axis] == expected);
            }
        }
    }
}

static void test_roundtrip_oom_and_lifetime(Fixture *f, const QJSIntlRootInput *root)
{
    QJSIntlAllocator a = allocator(f);
    QJSIntlCompiledCollationData data;
    QJSIntlCompiledCollationProgram retained;
    size_t before = f->calls, baseline = f->live, opening_calls, i;
    uint32_t rank;

    assert(!qjs_intl_compiled_collation_open(f->image.bytes, f->image.length, root->fingerprint, &a, &data));
    opening_calls = f->calls - before;
    assert(data.ready && data.program_count == 3 && f->live == baseline);

    before = f->calls;
    for (i = 0; i < 3; i++) {
        QJSIntlCompiledCollationData local = data;
        QJSIntlCompiledCollationProgram p;
        assert(!qjs_intl_compiled_collation_program(&local, i, &p));
        memset(&local, 0, sizeof(local)); /* The child's descriptors own no parent pointer. */
        compare_program(f->snapshots + i, &p);
    }
    assert(f->calls == before && f->live == baseline);

    assert(!qjs_intl_compiled_collation_program(&data, 2, &retained));
    assert(retained.final_maxima[3] == 1);
    assert(!qjs_intl_compiled_collation_root_rank(&retained, 3, 0, &rank) && !rank);
    assert(!qjs_intl_compiled_collation_root_rank(&retained, 0, root->metadata.limits.primary_max, &rank));
    assert(rank < retained.final_maxima[0]);

    {
        QJSIntlCompiledCollationProgram bad;
        QJSIntlCollationRankedElement ce;

        assert(qjs_intl_compiled_collation_program(&data, data.program_count, &bad) == QJS_INTL_INVALID_ARGUMENT && !bad.ready);
        assert(qjs_intl_compiled_collation_element(&retained, retained.elements.count, &ce) == QJS_INTL_INVALID_ARGUMENT && !ce.ranks[0]);
        assert(qjs_intl_compiled_collation_root_element(&retained, retained.root.elements.count, &ce) == QJS_INTL_INVALID_ARGUMENT && !ce.ranks[0]);
        assert(qjs_intl_compiled_collation_implicit(&retained, 0x110000, &ce) == QJS_INTL_INVALID_ARGUMENT && !ce.ranks[0]);
        assert(qjs_intl_compiled_collation_root_rank(&retained, 4, 0, &rank) == QJS_INTL_INVALID_ARGUMENT && !rank);
    }

    for (i = 1; i <= opening_calls; i++) {
        QJSIntlCompiledCollationData failed;
        f->fail_at = f->calls + i;
        assert(qjs_intl_compiled_collation_open(f->image.bytes, f->image.length, root->fingerprint, &a, &failed) == QJS_INTL_NO_MEMORY);
        assert(!failed.ready && !failed.fingerprint && !failed.root.ready && f->live == baseline);
    }
    f->fail_at = 0;

    clear_sources(f);
    assert(f->live == 1); /* Only the archive image remains. */
    memset(&data, 0, sizeof(data));
    assert(!qjs_intl_compiled_collation_implicit(&retained, 0x4e00, &(QJSIntlCollationRankedElement){0}));
    assert(!qjs_intl_compiled_collation_root_rank(&retained, 3, 0, &rank) && !rank);
}

static uint32_t word(const uint8_t *p, unsigned int index)
{
    return qjs_intl_read_u32(p + index * 4);
}

static void put32(uint8_t *p, uint32_t value)
{
    unsigned int i;

    for (i = 0; i < 4; i++)
        p[i] = value >> (i * 8);
}

static void rejected(Fixture *f, const uint8_t *bytes, size_t length)
{
    QJSIntlAllocator a = allocator(f);
    QJSIntlCompiledCollationData data;
    size_t baseline = f->live;

    memset(&data, 0xff, sizeof(data));
    assert(qjs_intl_compiled_collation_open(bytes, length, f->image.bytes + 32, &a, &data) == QJS_INTL_DATA_ERROR);
    assert(!data.ready && !data.fingerprint && !data.root.ready && f->live == baseline);
}

static uint8_t *edge_word(const QJSIntlCompiledCollationProgram *p, uint32_t parent, uint32_t code)
{
    const uint8_t *node = p->trie.nodes + (size_t)parent * 16;
    uint32_t at = word(node, 0), count = word(node, 1), i;

    for (i = 0; i < count; i++) {
        const uint8_t *edge = p->trie.edges + (size_t)(at + i) * 8;
        if (word(edge, 0) == code)
            return (uint8_t *)edge;
    }
    abort();
}

static void test_mutations(Fixture *f)
{
    QJSIntlAllocator a = allocator(f);
    QJSIntlCompiledCollationData data;
    QJSIntlCompiledCollationProgram q;
    QJSIntlCollationOverlay overlay;
    uint8_t *allocation = malloc(f->image.length + 1), *bytes, *block;
    const uint8_t *record;
    size_t baseline = f->live, i;

    assert(allocation);
    bytes = allocation + 1;
    memcpy(bytes, f->image.bytes, f->image.length);
    assert(!qjs_intl_compiled_collation_open(bytes, f->image.length, f->image.bytes + 32, &a, &data));
    assert(data.fingerprint == bytes + 32 && f->live == baseline);
    assert(!qjs_intl_compiled_collation_program(&data, 2, &q));
    assert(q.offsets[0].count == 2 && q.offsets[3].count == 1);
    assert(!qjs_intl_collation_trie_overlay(&q.trie, &overlay));

    record = data.sections[QJS_INTL_COMPILED_PROGRAMS].bytes + 2 * 32;
    block = (uint8_t *)data.sections[QJS_INTL_COMPILED_BLOCKS].bytes + word(record, 6);

    {
        struct Mutation { uint8_t *at; uint32_t value; } cases[] = {
            {bytes, 0}, {bytes + 4, QJS_INTL_COMPILED_COLLATION_VERSION + 1}, {bytes + 8, 17u << 16},
            {bytes + 16, (uint32_t)f->image.length + 4}, {bytes + 20, 11}, {bytes + 24, 4}, {bytes + 28, 1},
            {bytes + 64, 2}, {bytes + 68, 255}, {bytes + 76, 0},
            {(uint8_t *)data.sections[QJS_INTL_COMPILED_META].bytes, UINT32_C(0x80000000)},
            {(uint8_t *)data.sections[QJS_INTL_COMPILED_META].bytes + 60, q.groups.count},
            {(uint8_t *)q.groups.bytes, 0},
            {(uint8_t *)q.groups.bytes + (size_t)(q.groups.count - 1) * 8 + 4, 1},
            {(uint8_t *)data.sections[QJS_INTL_COMPILED_PROGRAMS].bytes, 1},
            {(uint8_t *)data.sections[QJS_INTL_COMPILED_PROGRAMS].bytes + 4, 0},
            {(uint8_t *)data.sections[QJS_INTL_COMPILED_LABELS].bytes, 0xff},
            {block, 0}, {block + 4, 3}, {block + 8, 4}, {block + 12, 32}, {block + 16, UINT32_MAX},
            {(uint8_t *)q.group_ranks.bytes + 4, word(q.group_ranks.bytes, 0)},
            {(uint8_t *)q.offsets[0].bytes + 8, word(q.offsets[0].bytes, 0)},
            {(uint8_t *)q.offsets[0].bytes + 12, UINT32_MAX},
            {(uint8_t *)q.offsets[3].bytes, 1}, {(uint8_t *)q.offsets[3].bytes + 4, 0},
            {(uint8_t *)q.offsets[3].bytes + 4, UINT32_MAX},
            {(uint8_t *)q.elements.bytes + 12, 2},
            {(uint8_t *)q.elements.bytes + 16, q.root.metadata.field_separator},
            {(uint8_t *)q.elements.bytes + 20, 3}, {(uint8_t *)q.elements.bytes + 20, 4},
            {(uint8_t *)q.elements.bytes + 20, 8}, {(uint8_t *)q.elements.bytes + 20, 16},
            {(uint8_t *)q.trie.nodes + 8, 0}
        };

        for (i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
            uint32_t old = word(cases[i].at, 0);
            put32(cases[i].at, cases[i].value);
            rejected(f, bytes, f->image.length);
            put32(cases[i].at, old);
        }
    }

    /* Numeric tokens own one reserved final primary. An ordinary CE cannot
       occupy it even if all levels and its separate origin are in range. */
    {
        uint8_t *ce = (uint8_t *)q.elements.bytes;
        uint32_t old[3] = {word(ce, 0), word(ce, 1), word(ce, 2)}, rank;

        assert(!qjs_intl_compiled_collation_root_rank(&q, 0, q.root.metadata.numeric, &rank));
        put32(ce, rank);
        assert(!qjs_intl_compiled_collation_root_rank(&q, 1, q.root.metadata.limits.secondary_common, &rank));
        put32(ce + 4, rank);
        assert(!qjs_intl_compiled_collation_root_rank(&q, 2, q.root.metadata.limits.tertiary_common, &rank));
        put32(ce + 8, rank);
        rejected(f, bytes, f->image.length);

        /* P>0 with origin0 is legitimate for an insertion after root zero. */
        put32(ce, 1);
        assert(!qjs_intl_compiled_collation_open(bytes, f->image.length, NULL, &a, &(QJSIntlCompiledCollationData){0}));
        for (i = 0; i < 3; i++)
            put32(ce + i * 4, old[i]);
    }

    /* Keep the trie and key index in agreement so QC/CCC checks, rather than a
       later key/trie mismatch, are the reason these Unicode mutations fail. */
    {
        QJSIntlCollationKeySpan key;
        uint32_t suffix = overlay.begin(overlay.opaque, 'w', NULL, 0);
        uint32_t w_node = overlay.step(overlay.opaque, suffix, 'w');
        uint32_t cedilla_node = overlay.step(overlay.opaque, w_node, 0x327);
        uint8_t *root_w = edge_word(&q, 0, 'w'), *suffix_w = edge_word(&q, suffix, 'w');
        uint8_t *cedilla = edge_word(&q, w_node, 0x327), *acute = edge_word(&q, cedilla_node, 0x301);
        uint8_t *words;

        assert(!qjs_intl_collation_keys_at(&q.keys, 1, &key) && key.count == 3 && word(key.keys, 0) == 'w');
        words = (uint8_t *)key.keys;
        put32(words + 4, 0x301);
        put32(words + 8, 0x327);
        put32(cedilla, 0x301);
        put32(acute, 0x327);
        rejected(f, bytes, f->image.length);
        put32(words + 4, 0x327);
        put32(words + 8, 0x301);
        put32(cedilla, 0x327);
        put32(acute, 0x301);

        put32(words, 0xc0);
        put32(root_w, 0xc0);
        put32(suffix_w, 0xc0);
        rejected(f, bytes, f->image.length);
        put32(words, 'w');
        put32(root_w, 'w');
        put32(suffix_w, 'w');
    }

    /* High bits belong to the final rank, not a symbolic-ID subtag. Q's zero
       root still resolves to zero while its own gap has large positive ranks. */
    {
        QJSIntlCompiledCollationData large;
        QJSIntlCompiledCollationProgram p;
        QJSIntlCollationRankedElement ce;
        uint32_t value, high = UINT32_C(0x80000000);

        put32((uint8_t *)q.offsets[3].bytes + 4, high);
        for (i = 0; i < q.elements.count; i++)
            put32((uint8_t *)q.elements.bytes + i * 24 + 12, high);
        /* The final fullwidth digit retains a small Q rank as a comparison
           control against the high-bit q/w ranks. */
        put32((uint8_t *)q.elements.bytes + (q.elements.count - 1) * 24 + 12, 1);

        assert(!qjs_intl_compiled_collation_open(bytes, f->image.length, NULL, &a, &large));
        assert(!qjs_intl_compiled_collation_program(&large, 2, &p) && p.final_maxima[3] == high);
        assert(!qjs_intl_compiled_collation_element(&p, 0, &ce) && ce.ranks[3] == high);
        {
            const uint16_t large_q = 'q', small_q = 0xff17;
            QJSIntlCollationPolicy policy = p.policy;
            int order;

            policy.numeric = 0;
            policy.strength = QJS_INTL_COLLATION_STRENGTH_QUATERNARY;
            assert(!qjs_intl_collation_compare_ranked(&p, &large_q, 1, &small_q, 1, &a, &policy, &order));
            assert(order == 1);
        }
        assert(!qjs_intl_compiled_collation_root_rank(&p, 3, 0, &value) && !value);

        put32((uint8_t *)q.offsets[3].bytes + 4, 1);
        for (i = 0; i < q.elements.count; i++)
            put32((uint8_t *)q.elements.bytes + i * 24 + 12, 1);
    }

    rejected(f, bytes, f->image.length - 1);
    bytes[32] ^= 1;
    i = f->calls;
    rejected(f, bytes, f->image.length);
    assert(f->calls == i); /* Pairing fails before temporary audit allocations. */
    bytes[32] ^= 1;
    assert(f->live == baseline);
    free(allocation);
}

static void test_unsafe_context(const QJSIntlRootInput *root, int mapped_start)
{
    Fixture f = {0};
    QJSIntlAllocator a = allocator(&f);
    QJSIntlCollationProgram program;
    QJSIntlCollationCoordinate zero;
    const QJSIntlIRProgram identity = {{(const uint8_t *)"en", 2}, {(const uint8_t *)"standard", 8}, {0}, 0};
    uint32_t prefix = mapped_start ? 'a' : 0x1161, suffix = mapped_start ? 0x1161 : 'a', sequence;

    init_program(&program, root, &f);
    assert(!qjs_intl_root_input_element_at(root, 0, &zero));
    /* The low-level map can frame NFD keys that the higher-level rule builder
       would reject. This isolates the runtime's original-boundary guard. */
    assert(!qjs_intl_collation_map_put(&program.builder.map, &prefix, 1, &suffix, 1, &zero, 1, &sequence));
    assert(!qjs_intl_collation_archive_init(&f.archive, root, &a));
    assert(!retain(&f, &identity, &program));
    clear_program(&program);
    assert(!qjs_intl_collation_archive_finish(&f.archive, &f.image));
    qjs_intl_collation_archive_clear(&f.archive);

    rejected(&f, f.image.bytes, f.image.length);
    clear_sources(&f);
    qjs_intl_compiled_collation_clear(&f.image);
    assert(!f.live);
}

static void test_root_only(const QJSIntlRootInput *root)
{
    Fixture f = {0};
    QJSIntlAllocator a = allocator(&f);
    QJSIntlCompiledCollationData data;
    QJSIntlCompiledCollationProgram program;

    assert(!qjs_intl_collation_archive_init(&f.archive, root, &a));
    assert(!qjs_intl_collation_archive_finish(&f.archive, &f.image));
    qjs_intl_collation_archive_clear(&f.archive);

    assert(!qjs_intl_compiled_collation_open(f.image.bytes, f.image.length, root->fingerprint, &a, &data));
    assert(data.ready && !data.program_count && f.live == 1);
    assert(qjs_intl_compiled_collation_program(&data, 0, &program) == QJS_INTL_INVALID_ARGUMENT && !program.ready);

    qjs_intl_compiled_collation_clear(&f.image);
    assert(!f.live);
}

/* Construct the expected payload independently of matcher selection. The
   reader's byte/rank equivalence with host snapshots is checked above. */
static size_t expected_mapping(const QJSIntlCompiledCollationProgram *p, int overlay,
                               const uint32_t *key, size_t length, uint32_t prefix,
                               QJSIntlCollationRankedElement *out)
{
    const QJSIntlCollationKeyIndex *keys = overlay ? &p->keys : &p->root.keys;
    uint32_t i;

    for (i = 0; i < keys->row_count; i++) {
        QJSIntlCollationKeySpan span;
        uint32_t j, start, count;

        assert(!qjs_intl_collation_keys_at(keys, i, &span));
        if (span.count != length || qjs_intl_collation_keys_prefix(keys, i) != prefix)
            continue;

        for (j = 0; j < length && qjs_intl_read_u32(span.keys + (size_t)j * 4) == key[j]; j++)
            continue;
        if (j != length)
            continue;

        if (overlay)
            assert(!qjs_intl_compiled_collation_mapping(p, i, &start, &count));
        else
            assert(!qjs_intl_collation_base_mapping(&p->root, i, &start, &count));
        assert(count < 8);

        for (j = 0; j < count; j++) {
            if (overlay)
                assert(!qjs_intl_compiled_collation_element(p, start + j, out + j));
            else
                assert(!qjs_intl_compiled_collation_root_element(p, start + j, out + j));
        }
        return count;
    }

    assert(!"missing authored expected mapping");
    return 0;
}

static void check_stream(Fixture *f, const QJSIntlCompiledCollationProgram *p,
                         const uint16_t *input, size_t length,
                         const QJSIntlCollationRankedElement *expected, size_t count)
{
    QJSIntlAllocator a = allocator(f);
    QJSIntlCompiledCollationProgram local = *p;
    QJSIntlCollationRankedElements it;
    QJSIntlCollationRankedToken token;
    uint16_t copy[16];
    size_t baseline = f->live, calls, i;
    int present;

    assert(length <= 16);
    if (length)
        memcpy(copy, input, length * sizeof(*copy));
    assert(!qjs_intl_collation_ranked_elements_init(&it, &local, copy, length, 0, &a));
    memset(&local, 0, sizeof(local));
    memset(copy, 0xff, sizeof(copy));
    memset(&a, 0, sizeof(a));
    calls = f->calls;

    for (i = 0; i < count; i++) {
        assert(!qjs_intl_collation_ranked_elements_next(&it, &token, &present) && present);
        assert(!token.number.length);
        equal_ce(&token.element, expected + i);
    }

    assert(!qjs_intl_collation_ranked_elements_next(&it, &token, &present) && !present);
    assert(!qjs_intl_collation_ranked_elements_next(&it, &token, &present) && !present);
    assert(!token.number.length && !token.element.ranks[0] && f->calls == calls);
    qjs_intl_collation_ranked_elements_clear(&it);
    qjs_intl_collation_ranked_elements_clear(&it);
    assert(f->live == baseline);
}

static void test_ranked_streams(Fixture *f, const QJSIntlRootInput *root)
{
    QJSIntlAllocator a = allocator(f);
    QJSIntlCompiledCollationData data;
    QJSIntlCompiledCollationProgram p;
    QJSIntlCollationRankedElement expected[16];
    const uint32_t a_key[] = {'a'}, az[] = {'a', 'z'}, ae[] = {0xe6}, j_key[] = {'j'};
    const uint32_t abc[] = {'a', 'b', 'c'}, q[] = {'q'}, zero[] = {0};
    const uint16_t mixed[] = {'a', 'z', 0xe6, 'j'}, contraction[] = {'a', 'b', 'c'};
    const uint16_t empty_mixed[] = {0xe6, 'q', 0xe6, 'w', 0x327, 0x301, 0xe6, 0, 0x4e00, 0xd800};
    const uint16_t empty_only[] = {0xe6, 0xe6, 0xe6};
    size_t count;

    assert(!qjs_intl_compiled_collation_open(f->image.bytes, f->image.length, root->fingerprint, &a, &data));

    assert(!qjs_intl_compiled_collation_program(&data, 0, &p));
    count = expected_mapping(&p, 0, a_key, 1, 0, expected);
    count += expected_mapping(&p, 1, az, 2, 1, expected + count);
    count += expected_mapping(&p, 0, ae, 1, 0, expected + count);
    count += expected_mapping(&p, 1, j_key, 1, 0, expected + count);
    check_stream(f, &p, mixed, 4, expected, count);

    assert(!qjs_intl_compiled_collation_program(&data, 1, &p));
    count = expected_mapping(&p, 0, abc, 3, 0, expected);
    check_stream(f, &p, contraction, 3, expected, count);

    assert(!qjs_intl_compiled_collation_program(&data, 2, &p));
    count = expected_mapping(&p, 1, q, 1, 0, expected);
    assert(count == 1 && expected[0].ranks[3] == 1);
    expected[count++] = expected[0]; /* Equal tailored marks carry the same CE. */
    count += expected_mapping(&p, 0, zero, 1, 0, expected + count);
    assert(!qjs_intl_compiled_collation_implicit(&p, 0x4e00, expected + count++));
    assert(!qjs_intl_compiled_collation_implicit(&p, 0xd800, expected + count++));
    check_stream(f, &p, empty_mixed, 10, expected, count);
    check_stream(f, &p, empty_only, 3, NULL, 0);
    check_stream(f, &p, NULL, 0, NULL, 0);

    /* Init owns the normalized input and can fail at every allocation stage;
       failure must not leak or leave an uninitializable matcher behind. */
    {
        QJSIntlCollationRankedElements it;
        size_t baseline = f->live, calls = f->calls, allocations, i;

        assert(!qjs_intl_collation_ranked_elements_init(&it, &p, mixed, 4, 0, &a));
        allocations = f->calls - calls;
        qjs_intl_collation_ranked_elements_clear(&it);

        for (i = 1; i <= allocations; i++) {
            f->fail_at = f->calls + i;
            assert(qjs_intl_collation_ranked_elements_init(&it, &p, mixed, 4, 0, &a) == QJS_INTL_NO_MEMORY);
            qjs_intl_collation_ranked_elements_clear(&it);
            assert(f->live == baseline);
        }
        f->fail_at = 0;
    }
}

static void test_ranked_numbers(Fixture *f, const QJSIntlRootInput *root)
{
    QJSIntlAllocator a = allocator(f);
    QJSIntlCompiledCollationData data;
    QJSIntlCompiledCollationProgram p;
    QJSIntlCollationRankedElements it;
    QJSIntlCollationRankedToken token;
    QJSIntlCollationNumber first_number;
    const uint16_t input[] = {0xe6, '0', 0x660, '2', 0, '3', 0xe6};
    size_t calls;
    int present, i;

    assert(!qjs_intl_compiled_collation_open(f->image.bytes, f->image.length, root->fingerprint, &a, &data));
    assert(!qjs_intl_compiled_collation_program(&data, 1, &p));
    assert(!qjs_intl_collation_ranked_elements_init(&it, &p, input, 7, 1, &a));
    calls = f->calls;

    /* A rejected output argument must not consume text. Drain the entire
       initial expansion before returning the following numeric token. */
    assert(qjs_intl_collation_ranked_elements_next(&it, NULL, &present) == QJS_INTL_INVALID_ARGUMENT && !present);
    for (i = 0; i < 2; i++)
        assert(!qjs_intl_collation_ranked_elements_next(&it, &token, &present) && present && !token.number.length);
    assert(!qjs_intl_collation_ranked_elements_next(&it, &token, &present) && present);
    assert(token.number.length == 1 && (token.number.digits[0] & QJS_INTL_COLLATION_SCALAR_MASK) == '2');
    assert(!token.element.ranks[0] && !token.element.ranks[1] && !token.element.ranks[2]);
    first_number = token.number;

    /* Even a completely ignorable non-digit is a numeric-run boundary. */
    assert(!qjs_intl_collation_ranked_elements_next(&it, &token, &present) && present && !token.number.length);
    assert(!token.element.ranks[0] && !token.element.ranks[1] && !token.element.ranks[2]);
    assert(!qjs_intl_collation_ranked_elements_next(&it, &token, &present) && present);
    assert(token.number.length == 1 && (token.number.digits[0] & QJS_INTL_COLLATION_SCALAR_MASK) == '3');

    for (i = 0; i < 2; i++)
        assert(!qjs_intl_collation_ranked_elements_next(&it, &token, &present) && present && !token.number.length);
    assert(!qjs_intl_collation_ranked_elements_next(&it, &token, &present) && !present);
    assert((first_number.digits[0] & QJS_INTL_COLLATION_SCALAR_MASK) == '2' && f->calls == calls);
    qjs_intl_collation_ranked_elements_clear(&it);

    /* Only numeric mode bypasses a digit's explicit tailoring. The same
       fullwidth digit is otherwise the authored Q-only relation. */
    assert(!qjs_intl_compiled_collation_program(&data, 2, &p));
    for (i = 0; i < 2; i++) {
        const uint16_t digit = 0xff17;

        assert(!qjs_intl_collation_ranked_elements_init(&it, &p, &digit, 1, i, &a));
        assert(!qjs_intl_collation_ranked_elements_next(&it, &token, &present) && present);
        if (i)
            assert(token.number.length == 1 && !token.element.ranks[3]);
        else
            assert(!token.number.length && token.element.ranks[3] == 1);
        assert(!qjs_intl_collation_ranked_elements_next(&it, &token, &present) && !present);
        qjs_intl_collation_ranked_elements_clear(&it);
    }
}

static void test_binding_views(Fixture *f)
{
    static const char suffix[] = "enenemojifr";
    QJSIntlAllocator a = allocator(f);
    QJSIntlCompiledCollationData source, data;
    QJSIntlCollationBinding binding;
    uint8_t *bytes, *rows;
    uint32_t i, at = 64 + 12 * 16, labels, label_start = 0;
    size_t size, calls;

    assert(!qjs_intl_compiled_collation_open(f->image.bytes, f->image.length, NULL, &a, &source));
    labels = source.sections[QJS_INTL_COMPILED_LABELS].count;
    size = f->image.length - source.sections[QJS_INTL_COMPILED_LABELS].length +
        ((labels + sizeof(suffix) - 1 + 3) & ~(size_t)3) + 3 * 24;
    bytes = calloc(size, 1);
    assert(bytes);
    memcpy(bytes, f->image.bytes, 64);
    put32(bytes + 16, (uint32_t)size);

    /* Authored populated envelope: preserve source sections and append a
       canonical label suffix and three binding rows, with distinct usage IDs. */
    for (i = 0; i < 12; i++) {
        const QJSIntlSection *s = source.sections + i;
        uint8_t *directory = bytes + 64 + i * 16;
        uint32_t length = (uint32_t)s->length, count = s->count;

        if (i == QJS_INTL_COMPILED_LABELS) {
            label_start = at;
            count = labels + sizeof(suffix) - 1;
            length = (count + 3) & ~3u;
            memcpy(bytes + at, s->bytes, labels);
            memcpy(bytes + at + labels, suffix, sizeof(suffix) - 1);
        } else if (i == QJS_INTL_COMPILED_BINDINGS) {
            count = 3;
            length = 3 * 24;
        } else if (length) {
            memcpy(bytes + at, s->bytes, length);
        }

        put32(directory, i + 1);
        put32(directory + 4, at);
        put32(directory + 8, length);
        put32(directory + 12, count);
        at += length;
    }
    assert(at == size);

    rows = bytes + size - 3 * 24;
    {
        const uint32_t values[][6] = {
            {labels, 2, labels + 2, 0, 0, 1},
            {labels + 2, 2, labels + 4, 5, 0, UINT32_MAX},
            {labels + 9, 2, labels + 11, 0, 1, 0},
        };
        unsigned int j;

        for (i = 0; i < 3; i++) {
            for (j = 0; j < 6; j++)
                put32(rows + (size_t)i * 24 + j * 4, values[i][j]);
        }
    }

    assert(!qjs_intl_compiled_collation_open(bytes, size, source.fingerprint, &a, &data));
    calls = f->calls;
    assert(!qjs_intl_compiled_collation_binding(&data, 0, &binding));
    assert(binding.locale.length == 2 && !memcmp(binding.locale.data, "en", 2));
    assert(!binding.collation.length && binding.sort_program == 0 && binding.search_program == 1);
    assert(!qjs_intl_compiled_collation_binding(&data, 1, &binding));
    assert(binding.collation.length == 5 && !memcmp(binding.collation.data, "emoji", 5));
    assert(binding.sort_program == 0 && binding.search_program == UINT32_MAX);
    assert(!qjs_intl_compiled_collation_binding(&data, 2, &binding));
    assert(binding.locale.length == 2 && !memcmp(binding.locale.data, "fr", 2));
    assert(f->calls == calls);
    assert(qjs_intl_compiled_collation_binding(&data, 3, &binding) == QJS_INTL_INVALID_ARGUMENT);
    assert(!binding.locale.data && !binding.collation.data);

    /* Grammar/casing is checked at open, not deferred to public locale lookup. */
    bytes[label_start + labels] = 'E';
    rejected(f, bytes, size);
    bytes[label_start + labels] = 'e';
    bytes[label_start + labels + 4] = 'E';
    rejected(f, bytes, size);
    bytes[label_start + labels + 4] = 'e';

    /* Source identities are opaque diagnostics, but public binding targets
       must exclude both private prefixes and nonempty alternates. */
    {
        uint8_t *record = bytes + word(bytes + 64 + QJS_INTL_COMPILED_PROGRAMS * 16, 1);
        uint32_t kind_at = word(record, 2);
        uint8_t saved[8];

        assert(word(record, 3) == sizeof(saved));
        memcpy(saved, bytes + label_start + kind_at, sizeof(saved));
        memcpy(bytes + label_start + kind_at, "private-", sizeof(saved));
        rejected(f, bytes, size);
        memcpy(bytes + label_start + kind_at, saved, sizeof(saved));

        /* Repartition the same counted labels into a nonempty alternate,
           keeping the program/label pools and following records unchanged. */
        put32(record + 4, 1);
        put32(record + 8, 1);
        put32(record + 16, 9);
        put32(record + 20, 1);
        rejected(f, bytes, size);
    }
    free(bytes);
}

static void check_order(Fixture *f, const QJSIntlCompiledCollationProgram *p,
                        const QJSIntlCollationPolicy *policy,
                        const uint16_t *left, size_t left_length,
                        const uint16_t *right, size_t right_length, int expected)
{
    QJSIntlAllocator a = allocator(f);
    size_t baseline = f->live;
    int order = 99;

    assert(!qjs_intl_collation_compare_ranked(p, left, left_length, right, right_length, &a, policy, &order));
    if (order != expected) {
        size_t i;

        fprintf(stderr, "ranked order %d, expected %d; left:", order, expected);
        for (i = 0; i < left_length; i++)
            fprintf(stderr, " %04x", left[i]);
        fprintf(stderr, "; right:");
        for (i = 0; i < right_length; i++)
            fprintf(stderr, " %04x", right[i]);
        fprintf(stderr, "\n");
    }
    assert(order == expected && f->live == baseline);
    assert(!qjs_intl_collation_compare_ranked(p, right, right_length, left, left_length, &a, policy, &order));
    assert(order == -expected && f->live == baseline);
}

static void check_ascii_order(Fixture *f, const QJSIntlCompiledCollationProgram *p,
                              const QJSIntlCollationPolicy *policy,
                              const char *left, const char *right, int expected)
{
    uint16_t a[16], b[16];
    size_t i, an = strlen(left), bn = strlen(right);

    assert(an <= 16 && bn <= 16);
    for (i = 0; i < an; i++)
        a[i] = (unsigned char)left[i];
    for (i = 0; i < bn; i++)
        b[i] = (unsigned char)right[i];

    check_order(f, p, policy, a, an, b, bn, expected);
}

static void test_ranked_case_levels(const QJSIntlRootInput *root)
{
    Fixture f = {0};
    QJSIntlAllocator a = allocator(&f);
    QJSIntlCollationProgram builder;
    const QJSIntlIRProgram identity = {{(const uint8_t *)"xx", 2}, {(const uint8_t *)"standard", 8}, {0}, 0};
    QJSIntlCompiledCollationData data;
    QJSIntlCompiledCollationProgram p;
    QJSIntlCollationPolicy policy;
    const uint32_t lower = 'a', t_only = 't', s_only = 's', upper_alias = 'c';
    const uint32_t separator = 0xfffe, separator_alias = 'x';
    const uint32_t accent[] = {'a', 0x301}, variant[] = {'a', 0x2bc};
    const uint16_t accented[] = {'a', 0x301}, plain = 'a';
    const uint16_t variant_upper[] = {'a', 0x2bc, 'A'}, plain_pair[] = {'a', 'a'};

    init_program(&builder, root, &f);
    assert(!qjs_intl_collation_builder_reset(&builder.builder, &lower, 1, 0));
    assert(!qjs_intl_collation_builder_relate(&builder.builder, 2, NULL, 0, accent, 2, NULL, 0));
    assert(!qjs_intl_collation_builder_reset(&builder.builder, &lower, 1, 0));
    assert(!qjs_intl_collation_builder_relate(&builder.builder, 3, NULL, 0, variant, 2, NULL, 0));
    assert(!qjs_intl_collation_builder_logical(&builder.builder, QJS_INTL_ROOT_FIRST_SECONDARY_IGNORABLE, 0));
    assert(!qjs_intl_collation_builder_relate(&builder.builder, 3, NULL, 0, &t_only, 1, NULL, 0));
    assert(!qjs_intl_collation_builder_logical(&builder.builder, QJS_INTL_ROOT_FIRST_PRIMARY_IGNORABLE, 0));
    assert(!qjs_intl_collation_builder_relate(&builder.builder, 2, NULL, 0, &s_only, 1, NULL, 0));
    assert(!qjs_intl_collation_builder_relate(&builder.builder, 0, NULL, 0, &upper_alias, 1, NULL, 0));
    assert(!qjs_intl_collation_builder_reset(&builder.builder, &separator, 1, 0));
    assert(!qjs_intl_collation_builder_relate(&builder.builder, 2, NULL, 0, &separator_alias, 1, NULL, 0));
    assert(!qjs_intl_collation_archive_init(&f.archive, root, &a));
    assert(!retain(&f, &identity, &builder));
    clear_program(&builder);
    assert(!qjs_intl_collation_archive_finish(&f.archive, &f.image));
    qjs_intl_collation_archive_clear(&f.archive);
    assert(!qjs_intl_compiled_collation_open(f.image.bytes, f.image.length, NULL, &a, &data));
    assert(!qjs_intl_compiled_collation_program(&data, 0, &p));

    policy = p.policy;
    policy.case_first = QJS_INTL_COLLATION_UPPER_FIRST;
    check_ascii_order(&f, &p, &policy, "ata", "aat", 1);
    check_order(&f, &p, &policy, variant_upper, 3, plain_pair, 2, 1);
    policy.case_level = 1;
    check_order(&f, &p, &policy, variant_upper, 3, plain_pair, 2, -1);

    policy.case_first = QJS_INTL_COLLATION_CASE_OFF;
    policy.strength = QJS_INTL_COLLATION_STRENGTH_PRIMARY;
    check_order(&f, &p, &policy, &plain, 1, accented, 2, 0);
    check_ascii_order(&f, &p, &policy, "ata", "aat", 0);
    check_ascii_order(&f, &p, &policy, "s", "c", 0);
    policy.strength = QJS_INTL_COLLATION_STRENGTH_SECONDARY;
    check_order(&f, &p, &policy, &plain, 1, accented, 2, -1);
    /* Tailored secondary-only CEs clear case bits even when their original
       root text had uppercase metadata (TR35 Tailored Strings, step 5). */
    check_ascii_order(&f, &p, &policy, "s", "c", 0);

    policy.case_level = 0;
    check_ascii_order(&f, &p, &policy, "ab", "ba", -1);
    policy.backwards_secondary = 1;
    check_ascii_order(&f, &p, &policy, "ab", "ba", 1);
    {
        const uint16_t left[] = {'a', 'b', 0xfffe, 'b', 'a'};
        const uint16_t right[] = {'b', 'a', 0xfffe, 'a', 'b'};
        const uint16_t prefix[] = {'s', 'a', 0xfffe}, suffix[] = {'a', 0xfffe, 's'};
        const uint16_t alias[] = {'a', 'x', 'a'}, literal[] = {'a', 0xfffe, 'a'};
        const uint16_t empty_segments[] = {0xfffe, 0xfffe}, with_zero[] = {0, 0xfffe, 0xfffe};

        /* Global reversal chooses the wrong segment; concatenating reversed
           segments loses a segment-prefix length difference. */
        check_order(&f, &p, &policy, left, 5, right, 5, 1);
        check_order(&f, &p, &policy, prefix, 3, suffix, 3, 1);
        check_order(&f, &p, &policy, empty_segments, 2, with_zero, 3, 0);
        check_order(&f, &p, &policy, alias, 3, literal, 3, 1);
    }

    policy.strength = QJS_INTL_COLLATION_STRENGTH_IDENTICAL;
    policy.numeric = 1;
    check_ascii_order(&f, &p, &policy, "02", "2", -1);
    {
        const uint16_t first[] = {0xfffe, 0}, second[] = {0, 0xfffe};
        const uint16_t zero = 0, composed = 0xe5, decomposed[] = {'a', 0x30a};

        /* Both strings have the same separator CE. The last tie-breaker must
           put FFFE below NUL rather than use ordinary code point order. */
        check_order(&f, &p, &policy, first, 2, second, 2, -1);
        check_order(&f, &p, &policy, &zero, 1, NULL, 0, 1);
        check_order(&f, &p, &policy, &composed, 1, decomposed, 2, 0);
    }
    clear_sources(&f);
    qjs_intl_compiled_collation_clear(&f.image);
    assert(!f.live);
}

static void test_ranked_comparison(Fixture *f)
{
    QJSIntlAllocator a = allocator(f);
    QJSIntlCompiledCollationData data;
    QJSIntlCompiledCollationProgram en, fr;
    QJSIntlCollationPolicy policy;
    const uint16_t latin = 'a', han = 0x4e00;

    assert(!qjs_intl_compiled_collation_open(f->image.bytes, f->image.length, NULL, &a, &data));
    assert(!qjs_intl_compiled_collation_program(&data, 0, &en));
    assert(!qjs_intl_compiled_collation_program(&data, 1, &fr));
    policy = fr.policy;

    check_ascii_order(f, &fr, NULL, "2", "10", -1);
    check_ascii_order(f, &fr, NULL, "002", "2", 0);
    policy.numeric = 0;
    check_ascii_order(f, &fr, &policy, "2", "10", 1);
    check_ascii_order(f, &en, &policy, "g", "h", -1);
    check_ascii_order(f, &en, &policy, "h", "i", -1);
    check_ascii_order(f, &en, &policy, "g", "i", -1);
    check_ascii_order(f, &en, &policy, "j", "k", 0);

    /* A weaker early case difference cannot beat the later secondary b/a
       difference. Ordinary T remains distinct from an explicit case level. */
    check_ascii_order(f, &fr, &policy, "a", "A", -1);
    check_ascii_order(f, &fr, &policy, "ab", "Aa", 1);
    policy.case_first = QJS_INTL_COLLATION_UPPER_FIRST;
    check_ascii_order(f, &fr, &policy, "a", "A", 1);
    policy.strength = QJS_INTL_COLLATION_STRENGTH_PRIMARY;
    check_ascii_order(f, &fr, &policy, "a", "b", 0);
    policy.case_level = 1;
    check_ascii_order(f, &fr, &policy, "a", "A", 1);
    policy.case_first = QJS_INTL_COLLATION_CASE_OFF;
    check_ascii_order(f, &fr, &policy, "ab", "Aa", -1);
    policy = fr.policy;

    check_order(f, &en, &policy, &latin, 1, &han, 1, 1);
    check_order(f, &fr, &policy, &latin, 1, &han, 1, -1);
    {
        const uint16_t composed = 0xe5, decomposed[] = {'a', 0x30a};
        const uint16_t zero = 0;

        check_order(f, &en, &policy, &composed, 1, decomposed, 2, 0);
        check_order(f, &en, &policy, NULL, 0, &zero, 1, 0);
    }

    /* A primary-equal long prefix exercises every buffer-growth allocation;
       failed init/growth leaves a zero result and no retained allocation. */
    {
        uint16_t lower[128], upper[128];
        size_t i, calls, allocations, baseline = f->live;
        int order;

        for (i = 0; i < 128; i++) {
            lower[i] = 'a';
            upper[i] = 'A';
        }
        calls = f->calls;
        assert(!qjs_intl_collation_compare_ranked(&fr, lower, 128, upper, 128, &a, &policy, &order) && order == -1);
        allocations = f->calls - calls;
        assert(f->live == baseline);

        for (i = 1; i <= allocations; i++) {
            f->fail_at = f->calls + i;
            order = 99;
            assert(qjs_intl_collation_compare_ranked(&fr, lower, 128, upper, 128, &a, &policy, &order) == QJS_INTL_NO_MEMORY);
            assert(!order && f->live == baseline);
        }
        f->fail_at = 0;
    }

    /* A first-primary mismatch must not allocate a full weak sort key. Both
       sides pay only the same initialization cost as standalone iterators. */
    {
        QJSIntlCollationRankedElements it[2];
        uint16_t left[512], right[512];
        size_t i, calls, init_calls;
        int order;

        for (i = 0; i < 512; i++) {
            left[i] = latin;
            right[i] = han;
        }
        calls = f->calls;
        assert(!qjs_intl_collation_ranked_elements_init(it, &fr, left, 512, policy.numeric, &a));
        assert(!qjs_intl_collation_ranked_elements_init(it + 1, &fr, right, 512, policy.numeric, &a));
        init_calls = f->calls - calls;
        qjs_intl_collation_ranked_elements_clear(it);
        qjs_intl_collation_ranked_elements_clear(it + 1);

        calls = f->calls;
        assert(!qjs_intl_collation_compare_ranked(&fr, left, 512, right, 512, &a, &policy, &order) && order == -1);
        assert(f->calls - calls == init_calls);

        /* Pure primary equality also needs no weak buffer. Wholly ignorable
           long input must not create one even with tertiary strength. */
        for (i = 0; i < 512; i++)
            right[i] = 'A';
        policy.strength = QJS_INTL_COLLATION_STRENGTH_PRIMARY;
        calls = f->calls;
        assert(!qjs_intl_collation_compare_ranked(&fr, left, 512, right, 512, &a, &policy, &order) && !order);
        assert(f->calls - calls == init_calls);

        memset(left, 0, sizeof(left));
        calls = f->calls;
        assert(!qjs_intl_collation_ranked_elements_init(it, &fr, left, 512, policy.numeric, &a));
        init_calls = f->calls - calls;
        qjs_intl_collation_ranked_elements_clear(it);

        policy = fr.policy;
        calls = f->calls;
        assert(!qjs_intl_collation_compare_ranked(&fr, left, 512, NULL, 0, &a, &policy, &order) && !order);
        assert(f->calls - calls == init_calls);
    }

    /* Invalid policy values fail before scanning or publishing an order. */
    {
        int order = 99;

        policy.strength = (QJSIntlCollationStrength)0;
        assert(qjs_intl_collation_compare_ranked(&fr, &latin, 1, &han, 1, &a, &policy, &order) == QJS_INTL_INVALID_ARGUMENT && !order);
        policy = fr.policy;
        policy.case_first = (QJSIntlCollationCaseFirst)99;
        order = 99;
        assert(qjs_intl_collation_compare_ranked(&fr, &latin, 1, &han, 1, &a, &policy, &order) == QJS_INTL_INVALID_ARGUMENT && !order);
    }
}

static void test_ranked_shifted_and_quaternary(const QJSIntlRootInput *root)
{
    Fixture f = {0};
    QJSIntlAllocator a = allocator(&f);
    QJSIntlCollationProgram builder;
    const QJSIntlIRProgram identity = {{(const uint8_t *)"xx", 2}, {(const uint8_t *)"standard", 8}, {0}, 0};
    const uint32_t a_key = 'a', b_key = 'b', x = 'x', y = 'y', u = 'u', v = 'v', q = 'q';
    const uint32_t xb[] = {'x', 'b'}, ay[] = {'a', 'y'};
    const uint16_t variables[] = {' ', '!', '+', '$'};
    const uint32_t group_keys[] = {0xa0, 0x201c, 0x263a, 0x20ac};
    QJSIntlCompiledCollationData data;
    QJSIntlCompiledCollationProgram p;
    QJSIntlCollationPolicy policy;
    unsigned int i, j;

    init_program(&builder, root, &f);
    for (i = 0; i < 4; i++) {
        QJSIntlRelationMapping value = {0};
        const uint32_t key[] = {0xfdd1, group_keys[i]};
        uint32_t target = variables[i], sequence;

        /* Copy each authenticated source-group boundary onto a visible test
           character, preserving original provenance/default-variable flags. */
        assert(!qjs_intl_collation_builder_lookup(&builder.builder, key, 2, &value));
        assert(!qjs_intl_collation_map_put(&builder.builder.map, NULL, 0, &target, 1,
                                         value.elements, value.count, &sequence));
        qjs_intl_relation_mapping_clear(&a, &value);
    }

    /* Equal stronger-level expansions with Q on different CE positions.
       Dropping common Q=0 markers would incorrectly tie u and v. */
    assert(!qjs_intl_collation_builder_reset(&builder.builder, &a_key, 1, 0));
    assert(!qjs_intl_collation_builder_relate(&builder.builder, 4, NULL, 0, &x, 1, NULL, 0));
    assert(!qjs_intl_collation_builder_reset(&builder.builder, &b_key, 1, 0));
    assert(!qjs_intl_collation_builder_relate(&builder.builder, 4, NULL, 0, &y, 1, NULL, 0));
    assert(!qjs_intl_collation_builder_reset(&builder.builder, xb, 2, 0));
    assert(!qjs_intl_collation_builder_relate(&builder.builder, 0, NULL, 0, &u, 1, NULL, 0));
    assert(!qjs_intl_collation_builder_reset(&builder.builder, ay, 2, 0));
    assert(!qjs_intl_collation_builder_relate(&builder.builder, 0, NULL, 0, &v, 1, NULL, 0));
    assert(!qjs_intl_collation_builder_logical(&builder.builder, QJS_INTL_ROOT_LAST_TERTIARY_IGNORABLE, 0));
    assert(!qjs_intl_collation_builder_relate(&builder.builder, 4, NULL, 0, &q, 1, NULL, 0));

    /* Reorder punctuation before space. Classification must still use the
       source groups, while shifted Q primaries use the saved permutation. */
    {
        uint8_t name[7 * 4], values[12 * 4];
        const char tokens[] = "punct\0space";
        QJSIntlIRInstruction node = {.kind = QJS_INTL_IR_SETTING};

        for (i = 0; i < 7; i++)
            put32(name + i * 4, (uint8_t)"reorder"[i]);
        for (i = 0; i < sizeof(tokens); i++)
            put32(values + i * 4, (uint8_t)tokens[i]);
        node.fields[0] = (QJSIntlIRScalars){name, 7};
        node.fields[1] = (QJSIntlIRScalars){values, sizeof(tokens)};
        assert(!qjs_intl_collation_reorder_apply(&builder.reorder, &node));
    }

    assert(!qjs_intl_collation_archive_init(&f.archive, root, &a));
    assert(!retain(&f, &identity, &builder));
    clear_program(&builder);
    assert(!qjs_intl_collation_archive_finish(&f.archive, &f.image));
    qjs_intl_collation_archive_clear(&f.archive);
    assert(!qjs_intl_compiled_collation_open(f.image.bytes, f.image.length, NULL, &a, &data));
    assert(!qjs_intl_compiled_collation_program(&data, 0, &p));

    policy = p.policy;
    check_ascii_order(&f, &p, &policy, "u", "v", 0);
    policy.strength = QJS_INTL_COLLATION_STRENGTH_QUATERNARY;
    check_ascii_order(&f, &p, &policy, "u", "v", 1);
    check_ascii_order(&f, &p, &policy, "", "q", -1);
    check_ascii_order(&f, &p, &policy, "q", "qq", -1);

    policy.shifted = 1;
    policy.strength = QJS_INTL_COLLATION_STRENGTH_TERTIARY;
    for (i = 0; i < 4; i++) {
        policy.max_variable = (QJSIntlCollationMaxVariable)i;
        for (j = 0; j < 4; j++)
            check_order(&f, &p, &policy, variables + j, 1, NULL, 0, j <= i ? 0 : 1);
    }

    policy.strength = QJS_INTL_COLLATION_STRENGTH_QUATERNARY;
    policy.max_variable = QJS_INTL_COLLATION_VARIABLE_PUNCT;
    check_ascii_order(&f, &p, &policy, " ", "!", 1);
    check_ascii_order(&f, &p, &policy, "!q", "!", 0);
    check_ascii_order(&f, &p, &policy, "!a", "!", 1);
    policy.numeric = 1;
    check_ascii_order(&f, &p, &policy, "!2q", "!2", 1);

    /* A shifted-only primary tie still retains variable positions for Q.
       Sweep failures through that buffer's growth and both init paths. */
    {
        uint16_t input[128];
        size_t baseline = f.live, calls = f.calls, allocations, k;
        int order;

        for (k = 0; k < 128; k++)
            input[k] = '!';
        assert(!qjs_intl_collation_compare_ranked(&p, input, 128, NULL, 0, &a, &policy, &order) && order == 1);
        allocations = f.calls - calls;
        assert(f.live == baseline);
        for (k = 1; k <= allocations; k++) {
            f.fail_at = f.calls + k;
            order = 99;
            assert(qjs_intl_collation_compare_ranked(&p, input, 128, NULL, 0, &a, &policy, &order) == QJS_INTL_NO_MEMORY);
            assert(!order && f.live == baseline);
        }
        f.fail_at = 0;
    }

    clear_sources(&f);
    qjs_intl_compiled_collation_clear(&f.image);
    assert(!f.live);
}

typedef struct OrderedRanked {
    QJSIntlCompiledCollationProgram program;
    QJSIntlCollationPolicy policy;
    QJSIntlAllocator allocator;
} OrderedRanked;

static QJSIntlStatus ordered_ranked_compare(void *opaque, const uint16_t *left, size_t left_length,
                                           const uint16_t *right, size_t right_length, int *order)
{
    OrderedRanked *root = opaque;

    return qjs_intl_collation_compare_ranked(&root->program, left, left_length, right, right_length,
                                            &root->allocator, &root->policy, order);
}

static void ordered_archive(const char *archive, const char *path, int shifted)
{
    Fixture f = {0};
    QJSIntlAllocator a = allocator(&f);
    QJSIntlCompiledCollationData data;
    OrderedRanked root = {.allocator = a};
    uint8_t *bytes;
    long size;
    uint32_t i;
    int found = 0;
    FILE *file = fopen(archive, "rb");

    assert(file && !fseek(file, 0, SEEK_END));
    size = ftell(file);
    assert(size >= 256 && size <= QJS_INTL_COMPILED_COLLATION_MAX_SIZE);
    assert(!fseek(file, 0, SEEK_SET));
    bytes = malloc(size);
    assert(bytes && fread(bytes, 1, size, file) == (size_t)size && !ferror(file));
    assert(!fclose(file));
    assert(!qjs_intl_compiled_collation_open(bytes, size, NULL, &a, &data));

    for (i = 0; i < data.program_count; i++) {
        QJSIntlCompiledCollationProgram p;

        assert(!qjs_intl_compiled_collation_program(&data, i, &p));
        if (p.locale.length == 4 && !memcmp(p.locale.data, "root", 4) &&
            p.collation.length == 8 && !memcmp(p.collation.data, "standard", 8) && !p.alternate.length) {
            assert(!found);
            root.program = p;
            found = 1;
        }
    }
    assert(found);

    root.policy = root.program.policy;
    root.policy.strength = QJS_INTL_COLLATION_STRENGTH_QUATERNARY;
    root.policy.shifted = shifted;
    qjs_test_intl_collation_ordered(path, ordered_ranked_compare, &root);
    assert(!f.live);
    free(bytes);
}

int main(int argc, char **argv)
{
    Fixture f = {0};
    QJSIntlRootInput root;

    if (argc != 1) {
        assert(argc == 5 && !strcmp(argv[1], "--archive") &&
               (!strcmp(argv[3], "--ordered") || !strcmp(argv[3], "--ordered-shifted")));
        ordered_archive(argv[2], argv[4], !strcmp(argv[3], "--ordered-shifted"));
        return 0;
    }

    make(&f, &root);
    test_mutations(&f);
    test_unsafe_context(&root, 0);
    test_unsafe_context(&root, 1);
    test_root_only(&root);
    test_ranked_streams(&f, &root);
    test_ranked_numbers(&f, &root);
    test_binding_views(&f);
    test_ranked_comparison(&f);
    test_ranked_case_levels(&root);
    test_ranked_shifted_and_quaternary(&root);
    test_roundtrip_oom_and_lifetime(&f, &root);
    qjs_intl_compiled_collation_clear(&f.image);
    assert(!f.live);
    return 0;
}
