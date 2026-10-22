/* Independent wire corruption, no-allocation queries and copied view lifetime.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/segment/program.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE(name) \
    extern const uint8_t qjs_intl_segment_program_##name##_bytes[]; \
    extern const size_t qjs_intl_segment_program_##name##_length
FIXTURE(golden);
FIXTURE(absent);
FIXTURE(empty);
FIXTURE(sentence);
FIXTURE(bos);
FIXTURE(wide);
FIXTURE(two);
FIXTURE(routes);
FIXTURE(rejecting);
FIXTURE(lexical);
FIXTURE(nullable);
FIXTURE(noncanonical_machines);
FIXTURE(noncanonical_states);

typedef struct Allocation { size_t calls, live; } Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    state->calls++;
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

static void open_data(const uint8_t *bytes, size_t length, const QJSIntlAllocator *allocator,
                      QJSIntlSegmentProgramData *out)
{
    QJSIntlData shared;
    size_t calls;
    Allocation *state = allocator->opaque;
    assert(!qjs_intl_data_open(bytes, length, allocator, &shared));
    calls = state->calls;
    assert(!qjs_intl_segment_program_data_open(&shared, out));
    assert(state->calls == calls && !state->live);
    /* Returning the fixed view must not retain this temporary descriptor. */
    memset(&shared, 0, sizeof(shared));
}

static void reject(const uint8_t *bytes, size_t length, const QJSIntlAllocator *allocator,
                   QJSIntlStatus expected)
{
    QJSIntlData shared;
    QJSIntlSegmentProgramData result, zero;
    size_t calls;
    Allocation *state = allocator->opaque;
    assert(!qjs_intl_data_open(bytes, length, allocator, &shared));
    calls = state->calls;
    memset(&zero, 0, sizeof(zero));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_segment_program_data_open(&shared, &result) == expected);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(state->calls == calls && !state->live);
}

static void corrupt(const uint8_t *bytes, size_t length, const QJSIntlAllocator *allocator,
                    size_t offset, uint32_t value, size_t width, QJSIntlStatus expected)
{
    QJSIntlPack pack;
    QJSIntlSection section;
    uint8_t *copy = malloc(length);
    size_t i, start;
    assert(copy && !qjs_intl_pack_open(bytes, length, &pack));
    assert(!qjs_intl_pack_section(&pack, QJS_INTL_SECTION_SEGMENT_PROGRAMS, &section));
    assert(offset <= section.length && width <= section.length - offset);
    start = (size_t)(section.bytes - pack.bytes);
    memcpy(copy, bytes, length);
    for (i = 0; i < width; i++)
        copy[start + offset + i] = value >> (8 * i);
    /* The outer envelope/shared data remain valid: this probes the C owner,
       independently of Python's refusal to encode malformed inner records. */
    reject(copy, length, allocator, expected);
    free(copy);
}

static void test_golden(const QJSIntlAllocator *allocator)
{
    QJSIntlSegmentProgramData data, copied;
    QJSIntlSegmentProgram program, zero_program;
    QJSIntlSegmentMachine left, right, zero_machine;
    QJSIntlSegmentRule rule, zero_rule;
    Allocation *state = allocator->opaque;
    size_t calls, i;
    uint32_t character_class, next;
    int accepted;
    uint8_t *unaligned = malloc(qjs_intl_segment_program_golden_length + 1);
    memset(&zero_program, 0, sizeof(zero_program));
    memset(&zero_machine, 0, sizeof(zero_machine));
    memset(&zero_rule, 0, sizeof(zero_rule));
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_segment_program_golden_bytes, qjs_intl_segment_program_golden_length);
    open_data(unaligned + 1, qjs_intl_segment_program_golden_length, allocator, &data);
    assert(data.program_count == 1 && data.binding_count == 1 && data.locale_count == 2);
    for (i = 0; i < 32; i++)
        assert(data.fingerprint[i] == i);
    copied = data;
    memset(&data, 0, sizeof(data));
    calls = state->calls;
    assert(!qjs_intl_segment_program_find(&copied, 0, QJS_INTL_SEGMENT_WORD, &program));
    assert(program.requirements == 13 && program.rule_count == 1 && program.class_count == 3);
    assert(!qjs_intl_segment_program_rule(&program, 0, &rule));
    assert(rule.left == 0 && rule.right == 1 && !rule.breaks && !rule.role);
    assert(!qjs_intl_segment_program_machine(&program, rule.left, &left));
    assert(!qjs_intl_segment_program_machine(&program, rule.right, &right));
    assert(left.mode == QJS_INTL_SEGMENT_LEFT_RESTART && right.mode == QJS_INTL_SEGMENT_RIGHT_REVERSED_RESTART);
    assert(!qjs_intl_segment_program_classify(&program, 'a', &character_class) && character_class == 1);
    assert(!qjs_intl_segment_machine_step(&left, 0, character_class, &next) && next == 1);
    assert(!qjs_intl_segment_machine_accepts(&left, next, &accepted) && accepted);
    assert(!qjs_intl_segment_program_classify(&program, 'b', &character_class) && character_class == 2);
    assert(!qjs_intl_segment_machine_step(&right, 0, character_class, &next) && next == 1);
    assert(!qjs_intl_segment_machine_step(&left, 1, character_class, &next) && next == 0);
    assert(!qjs_intl_segment_machine_accepts(&left, next, &accepted) && !accepted);
    assert(!qjs_intl_segment_program_classify(&program, 0xd800, &character_class) && !character_class);
    assert(!qjs_intl_segment_program_classify(&program, 0x10ffff, &character_class) && !character_class);
    assert(qjs_intl_segment_program_classify(&program, 0x110000, &character_class) == QJS_INTL_INVALID_ARGUMENT && !character_class);
    assert(qjs_intl_segment_machine_step(&left, 2, 0, &next) == QJS_INTL_INVALID_ARGUMENT && !next);
    assert(qjs_intl_segment_machine_step(&left, 0, 3, &next) == QJS_INTL_INVALID_ARGUMENT && !next);
    assert(qjs_intl_segment_machine_accepts(&left, 2, &accepted) == QJS_INTL_INVALID_ARGUMENT && !accepted);
    assert(qjs_intl_segment_program_rule(&program, 1, &rule) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&rule, &zero_rule, sizeof(rule)));
    assert(qjs_intl_segment_program_machine(&program, 2, &right) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&right, &zero_machine, sizeof(right)));
    assert(qjs_intl_segment_program_find(&copied, 1, QJS_INTL_SEGMENT_WORD, &program) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&program, &zero_program, sizeof(program)));
    assert(qjs_intl_segment_program_find(&copied, 0, (QJSIntlGranularity)-1, &program) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_segment_program_at(&copied, 1, &program) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_segment_program_at(&copied, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(state->calls == calls && !state->live);
    free(unaligned);
}

static void test_profiles(const QJSIntlAllocator *allocator)
{
    QJSIntlSegmentProgramData data;
    QJSIntlSegmentProgram program;
    QJSIntlSegmentMachine machine;
    QJSIntlSegmentRule rule;
    uint32_t next, character_class;
    int accepted;
    open_data(qjs_intl_segment_program_empty_bytes, qjs_intl_segment_program_empty_length, allocator, &data);
    assert(!qjs_intl_segment_program_at(&data, 0, &program));
    assert(!program.rule_count && !program.machine_count && !program.requirements);
    assert(!qjs_intl_segment_program_classify(&program, 0, &next) && !next);
    open_data(qjs_intl_segment_program_bos_bytes, qjs_intl_segment_program_bos_length, allocator, &data);
    assert(!qjs_intl_segment_program_at(&data, 0, &program));
    assert(!qjs_intl_segment_program_machine(&program, 0, &machine));
    assert(machine.mode == QJS_INTL_SEGMENT_LEFT_BOS_ONLY);
    open_data(qjs_intl_segment_program_sentence_bytes, qjs_intl_segment_program_sentence_length, allocator, &data);
    assert(!qjs_intl_segment_program_at(&data, 0, &program));
    assert(program.requirements == 10);
    assert(!qjs_intl_segment_program_rule(&program, 0, &rule) && rule.breaks && rule.role == 2);
    assert(!qjs_intl_segment_program_classify(&program, 'a', &character_class) && program.flags[character_class] == 1);
    assert(!qjs_intl_segment_program_classify(&program, 'b', &character_class) && program.flags[character_class] == 16);
    open_data(qjs_intl_segment_program_wide_bytes, qjs_intl_segment_program_wide_length, allocator, &data);
    assert(!qjs_intl_segment_program_at(&data, 0, &program));
    assert(!qjs_intl_segment_program_machine(&program, 0, &machine) && machine.state_count == 257);
    assert(!qjs_intl_segment_machine_step(&machine, 255, 0, &next) && next == 256);
    assert(!qjs_intl_segment_machine_step(&machine, 256, 0, &next) && next == 256);
    assert(!qjs_intl_segment_machine_accepts(&machine, 7, &accepted) && accepted);
    assert(!qjs_intl_segment_machine_accepts(&machine, 8, &accepted) && accepted);
    assert(!qjs_intl_segment_machine_accepts(&machine, 255, &accepted) && accepted);
    assert(!qjs_intl_segment_machine_accepts(&machine, 256, &accepted) && accepted);
    open_data(qjs_intl_segment_program_two_bytes, qjs_intl_segment_program_two_length, allocator, &data);
    assert(data.program_count == 2 && data.binding_count == 2);
    assert(!qjs_intl_segment_program_find(&data, 0, QJS_INTL_SEGMENT_GRAPHEME, &program) && program.kind == 0);
    assert(!qjs_intl_segment_program_find(&data, 0, QJS_INTL_SEGMENT_WORD, &program) && program.kind == 1);
    open_data(qjs_intl_segment_program_rejecting_bytes, qjs_intl_segment_program_rejecting_length, allocator, &data);
    assert(!qjs_intl_segment_program_at(&data, 0, &program));
    assert(!qjs_intl_segment_program_machine(&program, 0, &machine));
    assert(!qjs_intl_segment_machine_accepts(&machine, 0, &accepted) && !accepted);
    assert(!qjs_intl_segment_machine_accepts(&machine, 1, &accepted) && !accepted);
    open_data(qjs_intl_segment_program_lexical_bytes, qjs_intl_segment_program_lexical_length, allocator, &data);
    assert(!qjs_intl_segment_program_at(&data, 0, &program));
    assert(!qjs_intl_segment_program_rule(&program, 0, &rule) && rule.role == QJS_INTL_SEGMENT_LEXICAL);
    open_data(qjs_intl_segment_program_nullable_bytes, qjs_intl_segment_program_nullable_length, allocator, &data);
    assert(!qjs_intl_segment_program_at(&data, 0, &program));
    assert(!qjs_intl_segment_program_machine(&program, 0, &machine));
    assert(!qjs_intl_segment_machine_accepts(&machine, 0, &accepted) && accepted);
}

int main(void)
{
    static const struct Patch { size_t offset; uint32_t value; size_t width; } bad[] = {
        {0, 0, 1}, {20, 2, 4}, {24, 0, 4}, {24, 65537, 4}, {28, 223, 4}, {32, 1, 4}, {36, 1, 4},
        {72, 3, 4}, {76, 0, 4}, {80, 0, 4}, {80, 16385, 4}, {84, UINT32_MAX, 4},
        {88, 1025, 4}, {92, 2049, 4}, {96, 125, 4}, {100, 99, 4}, {104, 1, 4}, {108, 1, 4},
        {112, 2, 4}, {116, 2, 4}, {120, 1, 4}, {124, 0, 4}, {128, 96, 4}, {136, 0x110001, 4},
        {140, 1, 2}, {142, 0, 2}, {144, 3, 2}, {148, 1, 1}, {151, 1, 1},
        {152, 1, 2}, {154, 0, 2}, {156, 2, 1}, {157, 2, 1}, {158, 1, 2},
        {160, 0, 4}, {160, UINT32_MAX, 4}, {164, 3, 4}, {168, 69, 4}, {172, 81, 4},
        {180, 0, 4}, {194, 2, 2}, {194, 0, 2}, {204, 0x82, 1}, {205, 1, 1}, {223, 1, 1}
    };
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlSegmentProgramData output, zero;
    size_t i;
    test_golden(&allocator);
    test_profiles(&allocator);
    reject(qjs_intl_segment_program_absent_bytes, qjs_intl_segment_program_absent_length,
           &allocator, QJS_INTL_UNSUPPORTED);
    reject(qjs_intl_segment_program_noncanonical_machines_bytes, qjs_intl_segment_program_noncanonical_machines_length,
           &allocator, QJS_INTL_DATA_ERROR);
    reject(qjs_intl_segment_program_noncanonical_states_bytes, qjs_intl_segment_program_noncanonical_states_length,
           &allocator, QJS_INTL_DATA_ERROR);
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        corrupt(qjs_intl_segment_program_golden_bytes, qjs_intl_segment_program_golden_length,
                &allocator, bad[i].offset, bad[i].value, bad[i].width, QJS_INTL_DATA_ERROR);
    for (i = 8; i <= 16; i += 4)
        corrupt(qjs_intl_segment_program_golden_bytes, qjs_intl_segment_program_golden_length,
                &allocator, i, 99, 4, QJS_INTL_UNSUPPORTED);
    /* Both target programs have word kind, so these failures independently
       exercise first-use numbering and the unbound-program rejection. */
    corrupt(qjs_intl_segment_program_routes_bytes, qjs_intl_segment_program_routes_length,
            &allocator, 160, 1, 4, QJS_INTL_DATA_ERROR);
    corrupt(qjs_intl_segment_program_routes_bytes, qjs_intl_segment_program_routes_length,
            &allocator, 172, 0, 4, QJS_INTL_DATA_ERROR);
    corrupt(qjs_intl_segment_program_sentence_bytes, qjs_intl_segment_program_sentence_length,
            &allocator, 149, 3, 1, QJS_INTL_DATA_ERROR);
    memset(&output, 0xff, sizeof(output));
    memset(&zero, 0, sizeof(zero));
    assert(qjs_intl_segment_program_data_open(NULL, &output) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&output, &zero, sizeof(output)));
    assert(qjs_intl_segment_program_data_open(NULL, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!state.live);
    return 0;
}
