/* Counted logical root ownership, malformed data, pinned prefixes and fallback.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../tools/intl/collation-root-input.h"
#include "../src/intl/collation/match.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t intl_root_fixture[];
extern const size_t intl_root_fixture_size;
extern const uint8_t intl_root_unsafe_prefix[], intl_root_unsafe_text[];
extern const size_t intl_root_unsafe_prefix_size, intl_root_unsafe_text_size;
extern const uint8_t intl_root_multirange[];
extern const size_t intl_root_multirange_size;

typedef struct Allocation {
    size_t calls, live;
    int fail;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *p;
    assert(size);
    state->calls++;
    if (state->fail)
        return NULL;
    p = malloc(size);
    assert(p);
    state->live++;
    return p;
}

static void *resize(void *opaque, void *p, size_t size)
{
    Allocation *state = opaque;
    assert(p && size);
    state->calls++;
    if (state->fail)
        return NULL;
    p = realloc(p, size);
    assert(p);
    return p;
}

static void release(void *opaque, void *p)
{
    Allocation *state = opaque;
    assert(p && state->live);
    state->live--;
    free(p);
}

static QJSIntlAllocator allocator(Allocation *state)
{
    return (QJSIntlAllocator){state, allocate, resize, release};
}

static void put32(uint8_t *p, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        p[i] = value >> (i * 8);
}

static size_t section_offset(unsigned int kind)
{
    return qjs_intl_read_u32(intl_root_fixture + 64 + kind * 16 + 4);
}

static void empty(const QJSIntlRootInput *view)
{
    const unsigned char *bytes = (const unsigned char *)view;
    size_t i;
    for (i = 0; i < sizeof(*view); i++)
        assert(!bytes[i]);
}

static void bad(const uint8_t *bytes, size_t length)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRootInput view;
    memset(&view, 0xff, sizeof(view));
    assert(qjs_intl_root_input_open(bytes, length, &a, &view) == QJS_INTL_DATA_ERROR);
    assert(!state.live);
    empty(&view);
}

static void test_borrowed_and_fallback(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRootInput view;
    QJSIntlCollationCoordinate ce;
    uint8_t *bytes = malloc(intl_root_fixture_size + 1);
    uint32_t i, start, count;
    unsigned int role;
    const uint32_t generic[] = {0, 1, 0xd800, 0xe000, 0x10ffff};
    assert(bytes);
    memcpy(bytes + 1, intl_root_fixture, intl_root_fixture_size);
    /* All tables and returned slices must work without native alignment. */
    assert(!qjs_intl_root_input_open(bytes + 1, intl_root_fixture_size, &a, &view));
    assert(state.calls == 1 && !state.live && view.fingerprint == bytes + 33);
    for (i = 0; i < 32; i++)
        assert(view.fingerprint[i] == i);
    assert(view.metadata.implicit_last_marker == view.metadata.generic_base);
    assert(!qjs_intl_root_input_reset(&view, QJS_INTL_ROOT_FIRST_IMPLICIT, &role, &ce));
    assert(role == QJS_INTL_ROOT_ARTIFICIAL_BOUNDARY &&
           ce.weights[0] == view.metadata.implicit_first_marker && !ce.implicit);
    assert(!qjs_intl_root_input_reset(&view, QJS_INTL_ROOT_LAST_REGULAR, &role, &ce));
    assert(role == QJS_INTL_ROOT_DERIVED_INITIAL && ce.weights[0] == view.metadata.han_base);
    assert(qjs_intl_root_input_reset(&view, 0, &role, &ce) == QJS_INTL_INVALID_ARGUMENT);
    assert(!role && !ce.weights[0]);
    assert(!qjs_intl_root_input_mapping(&view, 0, &start, &count) && count == 1);
    assert(!qjs_intl_root_input_element_at(&view, start, &ce));
    assert(!ce.weights[0] && !ce.weights[1] && !ce.weights[2] && !ce.weights[3]);
    for (i = 0; i < sizeof(generic) / sizeof(*generic); i++) {
        assert(!qjs_intl_root_input_implicit(&view, generic[i], &ce));
        assert(ce.weights[0] == view.metadata.generic_base + generic[i] + 1);
        assert(ce.weights[0] < view.metadata.generic_limit && ce.root_primary == ce.weights[0]);
        assert(ce.weights[1] == view.metadata.limits.secondary_common &&
               ce.weights[2] == view.metadata.limits.tertiary_common && !ce.weights[3]);
        assert(ce.implicit && !ce.variable && ce.case_value == QJS_INTL_COLLATION_LOWER);
    }
    /* Radical order 丂,一,丁 differs from code point order 一,丁,丂. */
    for (i = 0x4e00; i <= 0x4e02; i++) {
        assert(!qjs_intl_root_input_implicit(&view, i, &ce));
        assert(ce.weights[0] == view.metadata.han_base + (i == 0x4e02 ? 1 : i - 0x4e00 + 2));
    }
    assert(state.calls == 1); /* Lookup/fallback retains no allocating owner. */
    assert(qjs_intl_root_input_implicit(&view, 0x110000, &ce) == QJS_INTL_INVALID_ARGUMENT);
    assert(!ce.weights[0] && !ce.implicit);
    assert(qjs_intl_root_input_mapping(&view, view.keys.row_count, &start, &count) == QJS_INTL_INVALID_ARGUMENT);
    assert(!start && !count);
    assert(qjs_intl_root_input_element_at(&view, view.sections[QJS_INTL_ROOT_CES].count, &ce) ==
           QJS_INTL_INVALID_ARGUMENT);
    free(bytes);
}

static void test_multiple_han_ranges(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRootInput view;
    QJSIntlCollationCoordinate ce;
    assert(!qjs_intl_root_input_open(intl_root_multirange, intl_root_multirange_size, &a, &view));
    assert(!qjs_intl_root_input_implicit(&view, 0x4e02, &ce));
    assert(ce.weights[0] == view.metadata.han_base + 1);
    assert(!qjs_intl_root_input_implicit(&view, 0x20000, &ce));
    assert(ce.weights[0] == view.metadata.han_base + 5);
    assert(!qjs_intl_root_input_implicit(&view, 0x20001, &ce));
    assert(ce.weights[0] == view.metadata.han_base + 4);
    assert(!qjs_intl_root_input_implicit(&view, 0x1ffff, &ce));
    assert(ce.weights[0] == view.metadata.generic_base + 0x20000);
    assert(!qjs_intl_root_input_implicit(&view, 0x20002, &ce));
    assert(ce.weights[0] == view.metadata.generic_base + 0x20003);
    assert(!state.live);
}

static void test_matcher_uses_logical_mapping_ids(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRootInput view;
    QJSIntlCollationMatch match = {0};
    QJSIntlCollationMatchResult result;
    QJSIntlCollationCoordinate ce;
    uint32_t start, count;
    int present;
    const uint16_t text[] = {'a', 'b', 'c'};
    assert(!qjs_intl_root_input_open(intl_root_fixture, intl_root_fixture_size, &a, &view));
    assert(!qjs_intl_collation_match_init(&match, &view.keys, text, 3, &a));
    assert(!qjs_intl_collation_match_next(&match, &result, NULL, &present));
    assert(present && result.kind == QJS_INTL_COLLATION_MATCH_ROOT && result.suffix_length == 3);
    assert(!qjs_intl_root_input_mapping(&view, result.mapping_id, &start, &count));
    assert(count == 1 && !qjs_intl_root_input_element_at(&view, start, &ce));
    assert(ce.weights[1] == view.metadata.limits.secondary_common);
    assert(!qjs_intl_collation_match_next(&match, &result, NULL, &present) && !present);
    qjs_intl_collation_match_clear(&match);
    {
        const uint16_t contextual[] = {'l', 0xb7};
        assert(!qjs_intl_collation_match_init(&match, &view.keys, contextual, 2, &a));
        assert(!qjs_intl_collation_match_next(&match, &result, NULL, &present));
        assert(present && result.kind == QJS_INTL_COLLATION_MATCH_SCALAR && result.scalar == 'l');
        assert(!qjs_intl_collation_match_next(&match, &result, NULL, &present));
        assert(present && result.kind == QJS_INTL_COLLATION_MATCH_ROOT && result.suffix_length == 1);
        assert(!qjs_intl_root_input_mapping(&view, result.mapping_id, &start, &count));
        assert(count == 1 && !qjs_intl_root_input_element_at(&view, start, &ce));
        assert(!ce.weights[0] && ce.weights[1] > view.metadata.limits.secondary_boundary);
        qjs_intl_collation_match_clear(&match);
    }
    assert(!state.live);
}

static void test_failure_and_corruption(void)
{
    Allocation state = {.fail = 1};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRootInput view;
    uint8_t *bytes = malloc(intl_root_fixture_size + 4);
    size_t i;
    static const struct { unsigned int kind, index; uint32_t value; } changes[] = {
        {QJS_INTL_ROOT_META, 11, 0}, {QJS_INTL_ROOT_META, 12, 0},
        {QJS_INTL_ROOT_MAPPINGS, 0, 1}, {QJS_INTL_ROOT_MAPPINGS, 3, 0},
        {QJS_INTL_ROOT_KEYS, 0, 0xd800}, {QJS_INTL_ROOT_KEYS, 0, 0x110000},
        {QJS_INTL_ROOT_KEYS, 0, 0xfdd0}, {QJS_INTL_ROOT_CONTEXTS, 0, UINT32_MAX},
        {QJS_INTL_ROOT_CONTEXTS, 1, 0}, {QJS_INTL_ROOT_CONTEXTS, 2, 1},
        {QJS_INTL_ROOT_CES, 3, 1}, {QJS_INTL_ROOT_CES, 4, 1},
        {QJS_INTL_ROOT_CES, 5, 3}, {QJS_INTL_ROOT_CES, 5, 4},
        {QJS_INTL_ROOT_CES, 5, 8}, {QJS_INTL_ROOT_CES, 5, 16},
        {QJS_INTL_ROOT_HAN_RANGES, 2, 1}, {QJS_INTL_ROOT_HAN_RANKS, 0, 0},
        {QJS_INTL_ROOT_HAN_RANKS, 0, 3}, {QJS_INTL_ROOT_RESETS, 1, 3},
        {QJS_INTL_ROOT_WEAK_LIMITS, 1, 2}, {QJS_INTL_ROOT_WEAK_LIMITS, 6, 1},
        {QJS_INTL_ROOT_GROUPS, 0, 0}, {QJS_INTL_ROOT_GROUPS, 1, 3},
        {QJS_INTL_ROOT_TOKENS, 0, 1}, {QJS_INTL_ROOT_TOKEN_BYTES, 0, 0},
    };
    assert(bytes);
    memset(&view, 0xff, sizeof(view));
    assert(qjs_intl_root_input_open(intl_root_fixture, intl_root_fixture_size, &a, &view) == QJS_INTL_NO_MEMORY);
    assert(state.calls == 1 && !state.live);
    empty(&view);
    state.fail = 0;
    assert(!qjs_intl_root_input_open(intl_root_fixture, intl_root_fixture_size, &a, &view));
    assert(state.calls == 2 && !state.live);
    assert(qjs_intl_root_input_open(NULL, 0, &a, &view) == QJS_INTL_INVALID_ARGUMENT);
    empty(&view);
    memcpy(bytes, intl_root_fixture, intl_root_fixture_size);
    for (i = 0; i < 8; i++) {
        bytes[i * 4] ^= 1;
        bad(bytes, intl_root_fixture_size);
        bytes[i * 4] ^= 1;
    }
    for (i = 0; i < 4; i++) {
        bytes[64 + i * 4] ^= 1;
        bad(bytes, intl_root_fixture_size);
        bytes[64 + i * 4] ^= 1;
    }
    bad(bytes, 63);
    bad(bytes, intl_root_fixture_size - 1);
    memset(bytes + intl_root_fixture_size, 0, 4);
    bad(bytes, intl_root_fixture_size + 4);
    for (i = 0; i < sizeof(changes) / sizeof(*changes); i++) {
        memcpy(bytes, intl_root_fixture, intl_root_fixture_size);
        put32(bytes + section_offset(changes[i].kind) + changes[i].index * 4, changes[i].value);
        bad(bytes, intl_root_fixture_size);
    }
    /* Each artifact is Python-structurally valid and sorted. Only the pinned
       NFC predicate rejects its prefix start or mapped-text start. */
    bad(intl_root_unsafe_prefix, intl_root_unsafe_prefix_size);
    bad(intl_root_unsafe_text, intl_root_unsafe_text_size);
    free(bytes);
}

int main(void)
{
    test_borrowed_and_fallback();
    test_multiple_han_ranges();
    test_matcher_uses_logical_mapping_ids();
    test_failure_and_corruption();
    return 0;
}
