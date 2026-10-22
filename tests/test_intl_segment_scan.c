/* UTF-16 ownership, directional base decisions and physical allocation failures.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/segment/scan.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE(name) \
    extern const uint8_t qjs_intl_segment_program_##name##_bytes[]; \
    extern const size_t qjs_intl_segment_program_##name##_length
FIXTURE(golden);
FIXTURE(empty);
FIXTURE(bos);
FIXTURE(directional);
FIXTURE(priority);
FIXTURE(bits);
FIXTURE(parity);

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

static void open_program(const QJSIntlAllocator *allocator, const uint8_t *bytes, size_t length,
                         QJSIntlSegmentProgram *out)
{
    QJSIntlData shared;
    QJSIntlSegmentProgramData programs;
    assert(!qjs_intl_data_open(bytes, length, allocator, &shared));
    assert(!qjs_intl_segment_program_data_open(&shared, &programs));
    assert(!qjs_intl_segment_program_at(&programs, 0, out));
}

static void decode_and_scan(const QJSIntlAllocator *allocator, const QJSIntlSegmentProgram *program,
                            const uint16_t *units, size_t length, QJSIntlSegmentScan *out)
{
    QJSIntlSegmentText text;
    QJSIntlUTF16 input = {units, length};
    assert(!qjs_intl_segment_text_prepare(allocator, &input, &text));
    assert(!qjs_intl_segment_scan_prepare(allocator, program, &text, out));
    /* Scan vectors retain no pointer to this separately owned decoded text. */
    qjs_intl_segment_text_clear(&text);
}

static void test_text(const QJSIntlAllocator *allocator)
{
    uint16_t units[] = {'a', 0xd83d, 0xde00, 0, 0xd800, 'b', 0xdc00};
    static const uint32_t scalars[] = {'a', 0x1f600, 0, 0xd800, 'b', 0xdc00};
    static const size_t offsets[] = {0, 1, 3, 4, 5, 6, 7};
    QJSIntlUTF16 input = {units, sizeof(units) / sizeof(units[0])};
    QJSIntlSegmentText text, zero;
    memset(&zero, 0, sizeof(zero));
    assert(!qjs_intl_segment_text_prepare(allocator, &input, &text));
    memset(units, 0xff, sizeof(units));
    assert(text.count == 6 && text.length == 7);
    assert(!memcmp(text.scalars, scalars, sizeof(scalars)));
    assert(!memcmp(text.offsets, offsets, sizeof(offsets)));
    qjs_intl_segment_text_clear(&text);
    qjs_intl_segment_text_clear(&text);
    assert(!memcmp(&text, &zero, sizeof(text)));
    input = (QJSIntlUTF16){NULL, 0};
    assert(!qjs_intl_segment_text_prepare(allocator, &input, &text));
    assert(!text.count && !text.length && text.offsets[0] == 0);
    qjs_intl_segment_text_clear(&text);
    input.length = 1;
    assert(qjs_intl_segment_text_prepare(allocator, &input, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&text, &zero, sizeof(text)));
}

static void test_decisions(const QJSIntlAllocator *allocator)
{
    QJSIntlSegmentProgram program;
    QJSIntlSegmentScan scan;
    static const uint16_t cabd[] = {'c', 'a', 'b', 'd'}, aab[] = {'a', 'a', 'b'};
    static const uint16_t abc[] = {'a', 'b', 'c'}, ab[] = {'a', 'b'}, a9[] = {'a', '9'};
    static const uint16_t x9[] = {'x', '9'};
    size_t i;
    open_program(allocator, qjs_intl_segment_program_golden_bytes, qjs_intl_segment_program_golden_length, &program);
    decode_and_scan(allocator, &program, cabd, 4, &scan);
    for (i = 0; i <= 4; i++) {
        assert(scan.breaks[i] == (i != 2));
        assert(scan.rules[i] == (i == 2 ? 0 : QJS_INTL_SEGMENT_DEFAULT_RULE));
    }
    qjs_intl_segment_scan_clear(&scan);
    decode_and_scan(allocator, &program, aab, 3, &scan);
    assert(!scan.breaks[2] && scan.rules[2] == 0);
    qjs_intl_segment_scan_clear(&scan);
    decode_and_scan(allocator, &program, NULL, 0, &scan);
    assert(!scan.count && scan.breaks[0] && scan.rules[0] == QJS_INTL_SEGMENT_DEFAULT_RULE);
    qjs_intl_segment_scan_clear(&scan);
    open_program(allocator, qjs_intl_segment_program_bos_bytes, qjs_intl_segment_program_bos_length, &program);
    decode_and_scan(allocator, &program, aab, 3, &scan);
    assert(scan.breaks[2] && scan.rules[2] == QJS_INTL_SEGMENT_DEFAULT_RULE);
    qjs_intl_segment_scan_clear(&scan);
    open_program(allocator, qjs_intl_segment_program_directional_bytes, qjs_intl_segment_program_directional_length, &program);
    decode_and_scan(allocator, &program, abc, 3, &scan);
    assert(!scan.breaks[1] && scan.rules[1] == 0 && scan.breaks[2]);
    qjs_intl_segment_scan_clear(&scan);
    open_program(allocator, qjs_intl_segment_program_priority_bytes, qjs_intl_segment_program_priority_length, &program);
    decode_and_scan(allocator, &program, cabd, 4, &scan);
    assert(scan.rules[1] == 1 && scan.rules[2] == 0 && scan.rules[3] == 1);
    assert(!scan.breaks[2]);
    qjs_intl_segment_scan_clear(&scan);
    open_program(allocator, qjs_intl_segment_program_bits_bytes, qjs_intl_segment_program_bits_length, &program);
    decode_and_scan(allocator, &program, a9, 2, &scan);
    assert(!scan.breaks[1] && scan.rules[1] == 9);
    qjs_intl_segment_scan_clear(&scan);
    decode_and_scan(allocator, &program, x9, 2, &scan);
    assert(!scan.breaks[1] && scan.rules[1] == 10);
    qjs_intl_segment_scan_clear(&scan);
    open_program(allocator, qjs_intl_segment_program_empty_bytes, qjs_intl_segment_program_empty_length, &program);
    decode_and_scan(allocator, &program, ab, 2, &scan);
    assert(scan.breaks[0] && scan.breaks[1] && scan.breaks[2]);
    assert(scan.rules[1] == QJS_INTL_SEGMENT_DEFAULT_RULE);
    qjs_intl_segment_scan_clear(&scan);
}

static void test_long_anchored_context(const QJSIntlAllocator *allocator)
{
    QJSIntlSegmentProgram program;
    QJSIntlSegmentScan scan;
    size_t i, length = 4097;
    uint16_t *units = malloc(length * sizeof(*units));
    assert(units);
    for (i = 0; i < length; i++)
        units[i] = 'r';
    open_program(allocator, qjs_intl_segment_program_parity_bytes, qjs_intl_segment_program_parity_length, &program);
    decode_and_scan(allocator, &program, units, length, &scan);
    assert(scan.breaks[0] && scan.breaks[length]);
    for (i = 1; i < length; i++) {
        assert(scan.breaks[i] == !(i & 1));
        assert(scan.rules[i] == ((i & 1) ? 0 : QJS_INTL_SEGMENT_DEFAULT_RULE));
    }
    qjs_intl_segment_scan_clear(&scan);
    free(units);
}

static void test_allocation_failures(QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlSegmentProgram program;
    QJSIntlSegmentText text, zero_text;
    QJSIntlSegmentScan scan, zero_scan;
    static const uint16_t units[] = {'a', 'b'};
    QJSIntlUTF16 input = {units, 2};
    size_t fail;
    memset(&zero_text, 0, sizeof(zero_text));
    memset(&zero_scan, 0, sizeof(zero_scan));
    open_program(allocator, qjs_intl_segment_program_golden_bytes, qjs_intl_segment_program_golden_length, &program);
    assert(!state->live);
    state->fail_at = state->calls + 1;
    memset(&text, 0xff, sizeof(text));
    assert(qjs_intl_segment_text_prepare(allocator, &input, &text) == QJS_INTL_NO_MEMORY);
    assert(!memcmp(&text, &zero_text, sizeof(text)) && !state->live);
    state->fail_at = 0;
    assert(!qjs_intl_segment_text_prepare(allocator, &input, &text));
    assert(state->live == 1);
    {
        QJSIntlSegmentText invalid = text;
        size_t calls = state->calls;
        invalid.length = invalid.count = SIZE_MAX;
        assert(qjs_intl_segment_scan_prepare(allocator, &program, &invalid, &scan) == QJS_INTL_INVALID_ARGUMENT);
        assert(!memcmp(&scan, &zero_scan, sizeof(scan)) && state->calls == calls);
    }
    /* The selected input has exactly one retained output allocation and one
       transient machine/mask allocation. Fail both physical calls in turn. */
    for (fail = 1; fail <= 2; fail++) {
        state->fail_at = state->calls + fail;
        memset(&scan, 0xff, sizeof(scan));
        assert(qjs_intl_segment_scan_prepare(allocator, &program, &text, &scan) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&scan, &zero_scan, sizeof(scan)) && state->live == 1);
    }
    state->fail_at = 0;
    assert(!qjs_intl_segment_scan_prepare(allocator, &program, &text, &scan));
    memset(&program, 0, sizeof(program));
    assert(state->live == 2 && !scan.breaks[1]);
    qjs_intl_segment_scan_clear(&scan);
    qjs_intl_segment_scan_clear(&scan);
    assert(!memcmp(&scan, &zero_scan, sizeof(scan)) && state->live == 1);
    qjs_intl_segment_text_clear(&text);
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    test_text(&allocator);
    test_decisions(&allocator);
    test_long_anchored_context(&allocator);
    test_allocation_failures(&allocator);
    assert(!state.live);
    return 0;
}
