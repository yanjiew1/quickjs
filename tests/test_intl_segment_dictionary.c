/* Dictionary reader validates hostile bytes and retains no allocation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/segment/dictionary.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t qjs_intl_dictionary_fixture_bytes[], qjs_intl_dictionary_unreachable_bytes[];
extern const uint8_t qjs_intl_dictionary_dead_bytes[], qjs_intl_dictionary_absent_bytes[];
extern const size_t qjs_intl_dictionary_fixture_length, qjs_intl_dictionary_unreachable_length;
extern const size_t qjs_intl_dictionary_dead_length, qjs_intl_dictionary_absent_length;

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        state->live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static void put32(uint8_t *p, uint32_t value)
{
    size_t i;
    for (i = 0; i < 4; i++)
        p[i] = value >> (8 * i);
}

static void reject(const QJSIntlAllocator *allocator, const uint8_t *bytes, size_t length,
                   QJSIntlStatus expected)
{
    QJSIntlPack pack;
    QJSIntlSegmentDictionaryData data, zero;
    memset(&zero, 0, sizeof(zero));
    assert(!qjs_intl_pack_open(bytes, length, &pack));
    memset(&data, 0xff, sizeof(data));
    assert(qjs_intl_segment_dictionary_data_open(&pack, allocator, &data) == expected);
    assert(!memcmp(&data, &zero, sizeof(data)));
    assert(!((Allocation *)allocator->opaque)->live);
}

static void corrupt(const QJSIntlAllocator *allocator, size_t offset, uint32_t value,
                    QJSIntlStatus expected)
{
    uint8_t *raw = malloc(qjs_intl_dictionary_fixture_length);
    assert(raw && offset <= qjs_intl_dictionary_fixture_length - 4);
    memcpy(raw, qjs_intl_dictionary_fixture_bytes, qjs_intl_dictionary_fixture_length);
    put32(raw + offset, value);
    reject(allocator, raw, qjs_intl_dictionary_fixture_length, expected);
    free(raw);
}

static void corrupt16(const QJSIntlAllocator *allocator, size_t offset, uint32_t value)
{
    uint8_t *raw = malloc(qjs_intl_dictionary_fixture_length);
    assert(raw && offset <= qjs_intl_dictionary_fixture_length - 2);
    memcpy(raw, qjs_intl_dictionary_fixture_bytes, qjs_intl_dictionary_fixture_length);
    raw[offset] = value;
    raw[offset + 1] = value >> 8;
    reject(allocator, raw, qjs_intl_dictionary_fixture_length, QJS_INTL_DATA_ERROR);
    free(raw);
}

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlPack pack;
    QJSIntlSection section;
    QJSIntlSegmentDictionaryData data;
    const QJSIntlSegmentDictionary *cjk, *thai, *missing;
    QJSIntlSegmentDictionary saved;
    uint32_t at, next, value;
    size_t calls, i, base, record, padding, alphabet, values, starts, terminals, labels, targets;
    uint32_t states, edges, root, first, first_label;
    uint8_t *unaligned = malloc(qjs_intl_dictionary_fixture_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_dictionary_fixture_bytes, qjs_intl_dictionary_fixture_length);
    assert(!qjs_intl_pack_open(unaligned + 1, qjs_intl_dictionary_fixture_length, &pack));
    assert(!qjs_intl_segment_dictionary_data_open(&pack, &allocator, &data));
    assert(state.calls == 2 && !state.live && data.present == 17);
    calls = state.calls;
    assert(!qjs_intl_segment_dictionary_at(&data, QJS_INTL_DICTIONARY_CJK, &cjk));
    assert(!qjs_intl_segment_dictionary_at(&data, QJS_INTL_DICTIONARY_THAI, &thai));
    assert(cjk->valued && cjk->words == 5 && cjk->max_scalars == 4 && cjk->max_utf16 == 6);
    assert(!thai->valued && thai->words == 3);
    assert(qjs_intl_segment_dictionary_at(&data, QJS_INTL_DICTIONARY_KHMER, &missing) == QJS_INTL_UNSUPPORTED);
    assert(!missing);
    assert(!qjs_intl_segment_dictionary_step(cjk, cjk->root, 'a', &at));
    assert(!qjs_intl_segment_dictionary_value(cjk, at, &value) && value == 0);
    assert(!qjs_intl_segment_dictionary_step(cjk, at, 'b', &next));
    assert(!qjs_intl_segment_dictionary_value(cjk, next, &value) && value == UINT32_MAX);
    assert(!qjs_intl_segment_dictionary_step(cjk, next, 'c', &at));
    assert(!qjs_intl_segment_dictionary_step(cjk, at, 'd', &next));
    assert(!qjs_intl_segment_dictionary_value(cjk, next, &value) && value == 20);
    at = cjk->root;
    for (i = 0; i < 3; i++) {
        assert(!qjs_intl_segment_dictionary_step(cjk, at, 0x20000 + i, &next));
        at = next;
    }
    assert(!qjs_intl_segment_dictionary_value(cjk, at, &value) && value == 30);
    assert(qjs_intl_segment_dictionary_step(cjk, at, 'a', &next) == QJS_INTL_UNSUPPORTED && !next);
    assert(qjs_intl_segment_dictionary_step(cjk, cjk->root, 0xd800, &next) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_segment_dictionary_value(cjk, cjk->root, &value) == QJS_INTL_UNSUPPORTED && !value);
    assert(qjs_intl_segment_dictionary_step(cjk, cjk->state_count, 'a', &next) == QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls && !state.live); /* Traversal allocates nothing. */
    assert(!qjs_intl_pack_section(&pack, QJS_INTL_SECTION_SEGMENT_DICTIONARIES, &section));
    base = (size_t)(section.bytes - (unaligned + 1));
    record = base + 16;
    padding = (size_t)(thai->terminals + thai->state_count * 2 - (unaligned + 1));
    assert(thai->labels > thai->terminals + thai->state_count * 2);
    alphabet = (size_t)(cjk->alphabet - (unaligned + 1));
    values = (size_t)(cjk->values - (unaligned + 1));
    starts = (size_t)(cjk->starts - (unaligned + 1));
    terminals = (size_t)(cjk->terminals - (unaligned + 1));
    labels = (size_t)(cjk->labels - (unaligned + 1));
    targets = (size_t)(cjk->targets - (unaligned + 1));
    states = cjk->state_count;
    edges = cjk->edge_count;
    root = cjk->root;
    first = read32(cjk->starts + root * 4);
    assert(read32(cjk->starts + (root + 1) * 4) >= first + 2);
    first_label = (uint32_t)cjk->labels[first * 2] | (uint32_t)cjk->labels[first * 2 + 1] << 8;
    saved = *thai;
    memset(&data, 0, sizeof(data));
    assert(!qjs_intl_segment_dictionary_step(&saved, saved.root, 0xe01, &at));
    assert(!qjs_intl_segment_dictionary_value(&saved, at, &value) && !value);
    free(unaligned);

    /* Fail each temporary validation allocation, then prove retry succeeds. */
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        reject(&allocator, qjs_intl_dictionary_fixture_bytes, qjs_intl_dictionary_fixture_length,
               QJS_INTL_NO_MEMORY);
    }
    state.calls = state.fail_at = 0;
    assert(!qjs_intl_pack_open(qjs_intl_dictionary_fixture_bytes, qjs_intl_dictionary_fixture_length, &pack));
    assert(!qjs_intl_segment_dictionary_data_open(&pack, &allocator, &data) && !state.live);
    reject(&allocator, qjs_intl_dictionary_unreachable_bytes, qjs_intl_dictionary_unreachable_length, QJS_INTL_DATA_ERROR);
    reject(&allocator, qjs_intl_dictionary_dead_bytes, qjs_intl_dictionary_dead_length, QJS_INTL_DATA_ERROR);
    reject(&allocator, qjs_intl_dictionary_absent_bytes, qjs_intl_dictionary_absent_length, QJS_INTL_UNSUPPORTED);
    corrupt(&allocator, base, 0, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, base + 8, 2, QJS_INTL_UNSUPPORTED);
    corrupt(&allocator, base + 12, 1, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record, 5, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 4, 0, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 8, 0, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 12, 99, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 16, 99, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 20, 99, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 24, 65537, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 28, 65536, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 32, 1000001, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 36, 2000001, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 40, 0, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, padding, 1, QJS_INTL_DATA_ERROR);
    /* Exercise array checks directly, not only descriptor rejection. */
    corrupt(&allocator, alphabet, 0x110000, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, alphabet, 0xd800, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, values, UINT32_MAX, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, starts, 1, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, starts + states * 4, edges + 1, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, starts + root * 4, edges + 1, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, targets, UINT32_MAX, QJS_INTL_DATA_ERROR);
    corrupt16(&allocator, terminals, 65535);
    corrupt16(&allocator, labels, 65535);
    corrupt16(&allocator, labels + (first + 1) * 2, first_label);
    corrupt(&allocator, record + 12, 1, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 16, 1, QJS_INTL_DATA_ERROR);
    corrupt(&allocator, record + 20, 1, QJS_INTL_DATA_ERROR);
    assert(!state.live);
    return 0;
}
