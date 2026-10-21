/* Common root semantics need no compiler envelope, reset facts or token tables.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/collation/base.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t intl_root_fixture[], intl_root_multirange[], intl_root_unsafe_prefix[], intl_root_unsafe_text[];
extern const size_t intl_root_fixture_size, intl_root_multirange_size;

typedef struct State { unsigned int calls, live, fail; } State;

static void *allocate(void *opaque, size_t size)
{
    State *state = opaque;
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

static void release(void *opaque, void *p)
{
    State *state = opaque;
    assert(p && state->live);
    state->live--;
    free(p);
}

static QJSIntlAllocator allocator(State *state)
{
    return (QJSIntlAllocator){state, allocate, NULL, release};
}

static uint32_t word(const uint8_t *bytes, unsigned int i)
{
    return qjs_intl_read_u32(bytes + (size_t)i * 4);
}

static QJSIntlSection table(const uint8_t *bytes, unsigned int kind)
{
    const uint8_t *row = bytes + 64 + (kind - 1) * 16;
    return (QJSIntlSection){bytes + word(row, 1), word(row, 2), word(row, 3)};
}

/* The authored container is just fixture storage. Only these six spans and
   decoded metadata reach base_open; no host reader is linked into this test. */
static QJSIntlCollationBaseSource source(const uint8_t *bytes)
{
    QJSIntlSection meta = table(bytes, 1);
    const uint8_t *p = meta.bytes;
    QJSIntlCollationBaseSource out = {0};
    out.metadata.limits = (QJSIntlCollationRootLimits){word(p,0),word(p,1),word(p,2),
        word(p,3),word(p,4),word(p,5),word(p,6)};
    out.metadata.han_base = word(p,7);
    out.metadata.han_count = word(p,8);
    out.metadata.generic_base = word(p,9);
    out.metadata.generic_count = word(p,10);
    out.metadata.generic_limit = word(p,11);
    out.metadata.variable_top = word(p,14);
    out.metadata.numeric = word(p,15);
    out.metadata.field_separator = word(p,16);
    out.mappings = table(bytes, 2);
    out.keys = table(bytes, 3);
    out.contexts = table(bytes, 4);
    out.elements = table(bytes, 5);
    out.han_ranges = table(bytes, 6);
    out.han_ranks = table(bytes, 7);
    return out;
}

static void put32(uint8_t *bytes, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        bytes[i] = value >> (8 * i);
}

static void test_independent_borrowed_base(void)
{
    State state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationBaseSource input;
    QJSIntlCollationBase view;
    QJSIntlCollationCoordinate ce;
    uint8_t *copy = malloc(intl_root_fixture_size + 1);
    unsigned int i, calls;
    uint32_t start, count;
    assert(copy);
    memcpy(copy + 1, intl_root_fixture, intl_root_fixture_size);
    input = source(copy + 1);
    /* Remove every compiler-only table and its envelope AFTER projecting spans.
       The logical base is still complete and all byte views remain unaligned. */
    for (i = 8; i <= 12; i++) {
        QJSIntlSection ignored = table(copy + 1, i);
        memset((void *)ignored.bytes, 0, ignored.length);
    }
    memset(copy + 1, 0, (size_t)(input.mappings.bytes - (copy + 1)));
    assert(!qjs_intl_collation_base_open(&input, &a, &view));
    assert(view.ready && !state.live && view.keys.rows == input.mappings.bytes);
    calls = state.calls;
    assert(!qjs_intl_collation_base_mapping(&view, 0, &start, &count) && count == 1);
    assert(!qjs_intl_collation_base_element(&view, start, &ce));
    assert(!ce.weights[0] && !ce.weights[1] && !ce.weights[2] && !ce.weights[3]);
    for (i = 0x4e00; i <= 0x4e02; i++) {
        assert(!qjs_intl_collation_base_implicit(&view, i, &ce));
        assert(ce.weights[0] == input.metadata.han_base + (i == 0x4e02 ? 1 : i - 0x4e00 + 2));
    }
    for (i = 0; i < 3; i++) {
        uint32_t code = i == 0 ? 0 : i == 1 ? 0xd800 : 0x10ffff;
        assert(!qjs_intl_collation_base_implicit(&view, code, &ce));
        assert(ce.weights[0] == input.metadata.generic_base + code + 1 && ce.implicit);
        assert(ce.weights[1] == input.metadata.limits.secondary_common && ce.weights[2] == input.metadata.limits.tertiary_common);
    }
    assert(calls == state.calls); /* Queries retain no allocating owner. */
    assert(qjs_intl_collation_base_mapping(&view, view.keys.row_count, &start, &count) == QJS_INTL_INVALID_ARGUMENT);
    assert(!start && !count);
    assert(qjs_intl_collation_base_element(&view, view.elements.count, &ce) == QJS_INTL_INVALID_ARGUMENT && !ce.weights[0]);
    assert(qjs_intl_collation_base_implicit(&view, 0x110000, &ce) == QJS_INTL_INVALID_ARGUMENT && !ce.weights[0]);
    free(copy);
}

static void rejected(const QJSIntlCollationBaseSource *input, const QJSIntlAllocator *a)
{
    QJSIntlCollationBase view;
    assert(qjs_intl_collation_base_open(input, a, &view) == QJS_INTL_DATA_ERROR);
    assert(!view.ready && !view.keys.rows && !view.elements.bytes);
}

static void test_spans_metadata_unicode_and_oom(void)
{
    State state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationBaseSource input = source(intl_root_fixture), bad;
    QJSIntlCollationBase view;
    uint8_t *ranks = malloc(input.han_ranks.length);
    assert(ranks);
    bad = input;
    bad.keys.bytes = NULL;
    rejected(&bad, &a);
    bad = input;
    bad.elements.length--;
    rejected(&bad, &a);
    bad = input;
    bad.mappings.count++;
    rejected(&bad, &a);
    bad = input;
    bad.metadata.generic_limit--;
    rejected(&bad, &a);
    bad = input;
    bad.metadata.limits.secondary_max = UINT32_C(0x80000000);
    rejected(&bad, &a);
    bad = input;
    memcpy(ranks, input.han_ranks.bytes, input.han_ranks.length);
    bad.han_ranks.bytes = ranks;
    put32(ranks, 0);
    rejected(&bad, &a);
    put32(ranks, word(input.han_ranks.bytes, 1));
    rejected(&bad, &a);
    bad = source(intl_root_unsafe_prefix);
    rejected(&bad, &a);
    bad = source(intl_root_unsafe_text);
    rejected(&bad, &a);
    state.fail = 1;
    assert(qjs_intl_collation_base_open(&input, &a, &view) == QJS_INTL_NO_MEMORY);
    assert(!view.ready && !view.keys.rows && !state.live);
    state.fail = 0;
    assert(!qjs_intl_collation_base_open(&input, &a, &view));
    assert(!state.live);
    free(ranks);
}

static void test_multi_range_and_records(void)
{
    State state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationBaseSource input = source(intl_root_multirange);
    QJSIntlCollationBase view;
    QJSIntlCollationCoordinate ce;
    uint8_t record[24];
    assert(intl_root_multirange_size);
    assert(!qjs_intl_collation_base_open(&input, &a, &view) && !state.live);
    assert(!qjs_intl_collation_base_implicit(&view, 0x20001, &ce));
    assert(ce.weights[0] == input.metadata.han_base + 4);
    assert(!qjs_intl_collation_base_implicit(&view, 0x20000, &ce));
    assert(ce.weights[0] == input.metadata.han_base + 5);
    memcpy(record, input.elements.bytes, sizeof(record));
    assert(qjs_intl_collation_base_check_element(&input.metadata, record));
    qjs_intl_collation_base_read_element(record, &ce);
    assert(!ce.weights[0] && !ce.root_primary);
    put32(record + 20, 3);
    assert(!qjs_intl_collation_base_check_element(&input.metadata, record));
    put32(record + 20, 0);
    put32(record + 12, 1);
    assert(!qjs_intl_collation_base_check_element(&input.metadata, record));
}

int main(void)
{
    test_independent_borrowed_base();
    test_spans_metadata_unicode_and_oom();
    test_multi_range_and_records();
    return 0;
}
