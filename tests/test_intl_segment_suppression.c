/* Suppression list ownership, hostile extents and physical allocation failure.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "../src/intl/segment/suppression-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE(name) \
    extern const uint8_t qjs_intl_suppression_##name##_bytes[]; \
    extern const size_t qjs_intl_suppression_##name##_length
FIXTURE(golden);
FIXTURE(absent);
FIXTURE(empty);
FIXTURE(literal);
FIXTURE(two);
FIXTURE(unreachable);
FIXTURE(aggregate);
#undef FIXTURE

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

static QJSIntlStatus open_lists(const QJSIntlAllocator *allocator, const uint8_t *bytes, size_t length,
                                size_t fail_at, QJSIntlSegmentSuppressionData *out)
{
    Allocation *state = allocator->opaque;
    QJSIntlData shared;
    state->fail_at = 0;
    assert(!qjs_intl_data_open(bytes, length, allocator, &shared));
    assert(!state->live);
    state->calls = 0;
    state->fail_at = fail_at;
    return qjs_intl_segment_suppression_data_open(&shared, allocator, out);
}

static void reject(const QJSIntlAllocator *allocator, const uint8_t *bytes, size_t length,
                   size_t fail_at, QJSIntlStatus expected)
{
    QJSIntlSegmentSuppressionData data, zero = {0};
    memset(&data, 0xff, sizeof(data));
    assert(open_lists(allocator, bytes, length, fail_at, &data) == expected);
    assert(!memcmp(&data, &zero, sizeof(data)));
    assert(!((Allocation *)allocator->opaque)->live);
}

static void corrupt(const QJSIntlAllocator *allocator, size_t base, size_t offset, uint32_t value,
                    size_t fail_at, QJSIntlStatus expected)
{
    uint8_t *raw = malloc(qjs_intl_suppression_golden_length);
    assert(raw && base + offset <= qjs_intl_suppression_golden_length - 4);
    memcpy(raw, qjs_intl_suppression_golden_bytes, qjs_intl_suppression_golden_length);
    put32(raw + base + offset, value);
    reject(allocator, raw, qjs_intl_suppression_golden_length, fail_at, expected);
    free(raw);
}

static void check_literal(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlSegmentSuppressionData data;
    QJSIntlSegmentLiteralGraph graph, copy;
    uint32_t at, next, value;
    size_t i, calls;
    const char key[] = "n. Chr.";
    assert(!open_lists(allocator, qjs_intl_suppression_literal_bytes,
                       qjs_intl_suppression_literal_length, 0, &data));
    calls = state->calls;
    assert(calls == 1);
    assert(!qjs_intl_segment_suppression_find(&data, 0, &graph));
    assert(!qjs_intl_segment_suppression_find(&data, 1, &copy));
    assert(graph.starts == copy.starts && graph.words == 2);
    memset(&data, 0, sizeof(data));
    at = graph.root;
    for (i = 0; i < sizeof(key) - 1; i++) {
        assert(!qjs_intl_segment_literal_graph_step(&graph, at, (uint8_t)key[i], &next));
        at = next;
    }
    assert(!qjs_intl_segment_literal_graph_value(&graph, at, &value) && !value);
    assert(qjs_intl_segment_literal_graph_step(&copy, copy.root, 'N', &next) == QJS_INTL_UNSUPPORTED);
    assert(!qjs_intl_segment_literal_graph_step(&copy, copy.root, 0x20000, &at));
    assert(!qjs_intl_segment_literal_graph_step(&copy, at, '.', &next));
    assert(!qjs_intl_segment_literal_graph_value(&copy, next, &value) && !value);
    assert(qjs_intl_segment_literal_graph_step(&copy, copy.root, 0xd800, &at) == QJS_INTL_UNSUPPORTED);
    assert(state->calls == calls && !state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlSegmentSuppressionData data, saved;
    QJSIntlSegmentLiteralGraph graph, zero = {0};
    QJSIntlPack pack;
    QJSIntlSection section;
    uint32_t at, value;
    size_t i, base, calls;
    uint8_t *unaligned = malloc(qjs_intl_suppression_golden_length + 1);
    const size_t bad[][2] = {
        {0, 0}, {20, 3}, {24, 0}, {24, 65537}, {28, 231}, {32, 1}, {36, 1},
        {72, 1}, {76, 1}, {104, 183}, {108, 4}, {112, 1}, {120, 1}, {124, 65537},
        {128, 257}, {132, 1}, {136, 65537}, {140, 2}, {144, 262145}, {148, 524289},
        {152, 185}, {156, 47}, {160, 1}, {164, 1},
        {168, 3}, {172, 1}, {176, 0}, {180, 0}, {180, 2},
        {124, 3}, {128, 1}, {184, 0xd800}, {188, 97}, {192, 7}, {196, 1},
        {208, 3}, {212, 2}, {216, 1}, {220, 2}, {224, 1}, {228, 2}
    };
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_suppression_golden_bytes, qjs_intl_suppression_golden_length);
    assert(!open_lists(&allocator, unaligned + 1, qjs_intl_suppression_golden_length, 0, &data));
    assert(state.calls == 1 && !state.live && data.list_count == 2 && data.binding_count == 2);
    calls = state.calls;
    saved = data;
    memset(&data, 0, sizeof(data));
    assert(!qjs_intl_segment_suppression_find(&saved, 0, &graph));
    assert(!memcmp(&graph, &zero, sizeof(graph))); /* Present empty, not missing. */
    assert(!qjs_intl_segment_suppression_find(&saved, 1, &graph));
    assert(graph.words == 2 && graph.max_scalars == 2 && graph.max_utf16 == 2);
    assert(!qjs_intl_segment_literal_graph_step(&graph, graph.root, 'a', &at));
    assert(!qjs_intl_segment_literal_graph_value(&graph, at, &value) && !value);
    assert(!qjs_intl_segment_literal_graph_step(&graph, at, 'b', &at));
    assert(!qjs_intl_segment_literal_graph_value(&graph, at, &value));
    assert(qjs_intl_segment_suppression_find(&saved, 2, &graph) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&graph, &zero, sizeof(graph)));
    assert(qjs_intl_segment_suppression_find(&saved, 3, &graph) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_segment_suppression_at(&saved, 2, &graph) == QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls);
    free(unaligned);
    assert(!qjs_intl_pack_open(qjs_intl_suppression_golden_bytes, qjs_intl_suppression_golden_length, &pack));
    assert(!qjs_intl_pack_section(&pack, QJS_INTL_SECTION_SEGMENT_SUPPRESSIONS, &section));
    base = (size_t)(section.bytes - qjs_intl_suppression_golden_bytes);
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        corrupt(&allocator, base, bad[i][0], bad[i][1], 0, QJS_INTL_DATA_ERROR);
    for (i = 8; i <= 16; i += 4)
        corrupt(&allocator, base, i, 2, 0, QJS_INTL_UNSUPPORTED);
    /* Even a failing allocator cannot turn invalid membership or a later bad
       descriptor/route into OOM during an earlier graph's validation. */
    for (i = 0; i < 3; i++) {
        const size_t offsets[] = {192, 160, 180};
        corrupt(&allocator, base, offsets[i], 7, 1, QJS_INTL_DATA_ERROR);
        assert(!state.calls);
    }
    reject(&allocator, qjs_intl_suppression_aggregate_bytes, qjs_intl_suppression_aggregate_length,
           1, QJS_INTL_DATA_ERROR);
    assert(!state.calls);
    reject(&allocator, qjs_intl_suppression_unreachable_bytes, qjs_intl_suppression_unreachable_length,
           0, QJS_INTL_DATA_ERROR);
    reject(&allocator, qjs_intl_suppression_absent_bytes, qjs_intl_suppression_absent_length,
           0, QJS_INTL_UNSUPPORTED);
    assert(!open_lists(&allocator, qjs_intl_suppression_empty_bytes, qjs_intl_suppression_empty_length, 1, &data));
    assert(!state.calls);
    assert(!qjs_intl_segment_suppression_at(&data, 1, &graph) && !graph.words);
    for (i = 1; i <= 2; i++)
        reject(&allocator, qjs_intl_suppression_two_bytes, qjs_intl_suppression_two_length,
               i, QJS_INTL_NO_MEMORY);
    assert(!open_lists(&allocator, qjs_intl_suppression_two_bytes, qjs_intl_suppression_two_length, 0, &data));
    assert(state.calls == 2 && !state.live);
    {
        uint8_t *raw = malloc(qjs_intl_suppression_two_length);
        const uint8_t *second = data.descriptors + 48;
        size_t section_start = (size_t)(data.section.bytes - qjs_intl_suppression_two_bytes);
        uint32_t start = qjs_intl_read_u32(second + 32);
        uint32_t alphabet = qjs_intl_read_u32(second + 16);
        assert(raw);
        /* Both lists are nonempty. Bind/policy errors in the second must not
           be masked by physical OOM while validating the first. */
        memcpy(raw, qjs_intl_suppression_two_bytes, qjs_intl_suppression_two_length);
        put32(raw + section_start + start + (size_t)alphabet * 4, 7);
        reject(&allocator, raw, qjs_intl_suppression_two_length, 1, QJS_INTL_DATA_ERROR);
        assert(!state.calls);
        memcpy(raw, qjs_intl_suppression_two_bytes, qjs_intl_suppression_two_length);
        put32(raw + section_start + 72 + 48 + 16, alphabet + 1);
        reject(&allocator, raw, qjs_intl_suppression_two_length, 1, QJS_INTL_DATA_ERROR);
        assert(!state.calls);
        free(raw);
    }
    check_literal(&allocator);
    return 0;
}
