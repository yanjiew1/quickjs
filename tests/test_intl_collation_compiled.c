/* Semantic producer/reader roundtrip, rank boundaries and untrusted bytes.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../tools/intl/collation-archive.h"
#include "../src/intl/collation/compiled-data.h"
#include <assert.h>
#include <stdlib.h>
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
    const uint32_t q = 'q', marks[] = {'w', 0x327, 0x301};
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
            {bytes, 0}, {bytes + 4, 2}, {bytes + 8, 17u << 16},
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

        assert(!qjs_intl_compiled_collation_open(bytes, f->image.length, NULL, &a, &large));
        assert(!qjs_intl_compiled_collation_program(&large, 2, &p) && p.final_maxima[3] == high);
        assert(!qjs_intl_compiled_collation_element(&p, 0, &ce) && ce.ranks[3] == high);
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

int main(void)
{
    Fixture f = {0};
    QJSIntlRootInput root;

    make(&f, &root);
    test_mutations(&f);
    test_unsafe_context(&root, 0);
    test_unsafe_context(&root, 1);
    test_root_only(&root);
    test_roundtrip_oom_and_lifetime(&f, &root);
    qjs_intl_compiled_collation_clear(&f.image);
    assert(!f.live);
    return 0;
}
