/* Native preparation to final UTF-16 vectors, lifetimes and independent queries.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/segment/native-input.h"
#include "../src/intl/segment/suppression-data.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t qjs_intl_segment_program_priority_bytes[], qjs_intl_suppression_runtime_bytes[];
extern const size_t qjs_intl_segment_program_priority_length, qjs_intl_suppression_runtime_length;
extern const uint8_t qjs_intl_dictionary_refine_programs_bytes[], qjs_intl_dictionary_refine_weighted_bytes[];
extern const size_t qjs_intl_dictionary_refine_programs_length, qjs_intl_dictionary_refine_weighted_length;

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

static void *resize(void *opaque, void *ptr, size_t size)
{
    Allocation *state = opaque;
    int fresh = ptr == NULL;
    void *result;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    result = realloc(ptr, size);
    if (result && fresh)
        state->live++;
    return result;
}

static QJSIntlStatus prepare_input(const QJSIntlAllocator *allocator,
                                   const QJSIntlSegmentProgram *selected,
                                   const QJSIntlSegmentProgram *grapheme,
                                   const QJSIntlSegmentLiteralGraph *list,
                                   const QJSIntlUTF16 *source,
                                   QJSIntlSegmentNativeInput **out)
{
    QJSIntlSegmentNativeRules rules = {selected, grapheme, list, NULL, NULL};
    return qjs_intl_segment_native_input_prepare(allocator, &rules, source, out);
}

static void open_views(const QJSIntlAllocator *allocator, QJSIntlSegmentProgram *grapheme,
                       QJSIntlSegmentProgram *sentence, QJSIntlSegmentLiteralGraph *list)
{
    QJSIntlData shared;
    QJSIntlSegmentProgramData programs;
    QJSIntlSegmentSuppressionData lists;
    assert(!qjs_intl_data_open(qjs_intl_segment_program_priority_bytes,
                              qjs_intl_segment_program_priority_length, allocator, &shared));
    assert(!qjs_intl_segment_program_data_open(&shared, &programs));
    assert(!qjs_intl_segment_program_at(&programs, 0, grapheme));
    assert(!qjs_intl_data_open(qjs_intl_suppression_runtime_bytes,
                              qjs_intl_suppression_runtime_length, allocator, &shared));
    assert(!qjs_intl_segment_program_data_open(&shared, &programs));
    assert(!qjs_intl_segment_program_at(&programs, 0, sentence));
    assert(!qjs_intl_segment_suppression_data_open(&shared, allocator, &lists));
    assert(!qjs_intl_segment_suppression_at(&lists, 0, list));
}

static void check_graphemes(const QJSIntlAllocator *allocator, const QJSIntlSegmentProgram *grapheme)
{
    Allocation *state = allocator->opaque;
    uint16_t units[] = {'a', 'b', 0xd840, 0xdc00, 0, 'c'};
    QJSIntlUTF16 source = {units, 6};
    QJSIntlSegmentNativeInput *input;
    QJSIntlSegment found, next;
    size_t calls;
    assert(!prepare_input(allocator, grapheme, NULL, NULL, &source, &input));
    assert(state->live == 1);
    calls = state->calls;
    memset(units, 0xff, sizeof(units));
    assert(!qjs_intl_segment_native_input_next(input, 0, &next) && next.start == 0 && next.end == 2);
    assert(!qjs_intl_segment_native_input_containing(input, 3, &found) && found.start == 2 && found.end == 4);
    assert(qjs_intl_segment_native_input_next(input, 1, &found) == QJS_INTL_INVALID_ARGUMENT && !found.present);
    assert(!qjs_intl_segment_native_input_next(input, next.end, &next) && next.start == 2 && next.end == 4);
    assert(!qjs_intl_segment_native_input_containing(input, 1, &found) && found.start == 0 && found.end == 2);
    assert(!qjs_intl_segment_native_input_next(input, 4, &next) && next.end == 5);
    assert(!qjs_intl_segment_native_input_next(input, 5, &next) && next.end == 6);
    assert(!qjs_intl_segment_native_input_next(input, 6, &next) && !next.present);
    assert(!qjs_intl_segment_native_input_containing(input, SIZE_MAX, &found) && !found.present);
    assert(qjs_intl_segment_native_input_next(input, 7, &next) == QJS_INTL_INVALID_ARGUMENT);
    assert(state->calls == calls);
    qjs_intl_segment_native_input_close(input);
    source = (QJSIntlUTF16){NULL, 0};
    assert(!prepare_input(allocator, grapheme, NULL, NULL, &source, &input));
    assert(!qjs_intl_segment_native_input_next(input, 0, &next) && !next.present);
    assert(!qjs_intl_segment_native_input_containing(input, 0, &found) && !found.present);
    qjs_intl_segment_native_input_close(input);
    assert(!state->live);
}

static void check_words(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlData shared;
    QJSIntlPack pack;
    QJSIntlSegmentProgramData programs;
    QJSIntlSegmentProgram grapheme, word;
    QJSIntlSegmentDictionaryData dictionaries;
    QJSIntlSegmentRouting *routing;
    QJSIntlSegmentNativeRules rules;
    QJSIntlSegmentNativeInput *input;
    QJSIntlSegment segment;
    uint16_t units[] = {0x7532, 0x4e59, 0x4e19, ' ', '4', '2', '!'};
    QJSIntlUTF16 source = {units, 7};
    size_t i, calls, live;
    assert(!qjs_intl_data_open(qjs_intl_dictionary_refine_programs_bytes,
                              qjs_intl_dictionary_refine_programs_length, allocator, &shared));
    assert(!qjs_intl_segment_program_data_open(&shared, &programs));
    assert(!qjs_intl_segment_program_at(&programs, 0, &grapheme));
    assert(!qjs_intl_segment_program_at(&programs, 1, &word));
    assert(!qjs_intl_pack_open(qjs_intl_dictionary_refine_weighted_bytes,
                              qjs_intl_dictionary_refine_weighted_length, &pack));
    assert(!qjs_intl_segment_dictionary_data_open(&pack, allocator, &dictionaries));
    assert(!qjs_intl_segment_routing_open(allocator, &routing));
    rules = (QJSIntlSegmentNativeRules){&word, &grapheme, NULL, &dictionaries, routing};
    live = state->live;
    state->calls = 0;
    assert(!qjs_intl_segment_native_input_prepare(allocator, &rules, &source, &input));
    calls = state->calls;
    assert(state->live == live + 1);
    assert(!qjs_intl_segment_native_input_next(input, 0, &segment));
    assert(segment.start == 0 && segment.end == 1 && segment.is_word_like);
    assert(!qjs_intl_segment_native_input_containing(input, 2, &segment));
    assert(segment.start == 1 && segment.end == 3 && segment.is_word_like);
    assert(!qjs_intl_segment_native_input_containing(input, 3, &segment) && !segment.is_word_like);
    assert(!qjs_intl_segment_native_input_containing(input, 5, &segment) && segment.is_word_like);
    assert(!qjs_intl_segment_native_input_next(input, 6, &segment) && !segment.is_word_like);
    qjs_intl_segment_native_input_close(input);
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        input = (void *)1;
        assert(qjs_intl_segment_native_input_prepare(allocator, &rules, &source, &input) == QJS_INTL_NO_MEMORY);
        assert(!input && state->live == live);
    }
    state->fail_at = 0;
    {
        const uint16_t pair[] = {0xd840, 0xdc00};
        QJSIntlUTF16 other = {NULL, 0};
        assert(!qjs_intl_segment_native_input_prepare(allocator, &rules, &other, &input));
        assert(!qjs_intl_segment_native_input_next(input, 0, &segment) && !segment.present);
        qjs_intl_segment_native_input_close(input);
        other = (QJSIntlUTF16){pair, 2};
        assert(!qjs_intl_segment_native_input_prepare(allocator, &rules, &other, &input));
        assert(!qjs_intl_segment_native_input_containing(input, 1, &segment));
        assert(segment.start == 0 && segment.end == 2 && segment.is_word_like);
        qjs_intl_segment_native_input_close(input);
        assert(state->live == live);
    }
    assert(!qjs_intl_segment_native_input_prepare(allocator, &rules, &source, &input));
    qjs_intl_segment_routing_close(routing);
    memset(&rules, 0, sizeof(rules));
    memset(&dictionaries, 0, sizeof(dictionaries));
    memset(&word, 0, sizeof(word));
    memset(&grapheme, 0, sizeof(grapheme));
    memset(units, 0, sizeof(units));
    calls = state->calls;
    assert(!qjs_intl_segment_native_input_containing(input, 2, &segment));
    assert(segment.start == 1 && segment.end == 3 && segment.is_word_like);
    assert(state->calls == calls && state->live == 1);
    qjs_intl_segment_native_input_close(input);
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlSegmentProgram grapheme, sentence, word;
    QJSIntlSegmentLiteralGraph list;
    QJSIntlSegmentNativeInput *input;
    QJSIntlSegment segment;
    uint16_t units[] = {'M', 'r', '.', 'a', 'b'};
    QJSIntlUTF16 source = {units, 5};
    size_t calls, i;
    open_views(&allocator, &grapheme, &sentence, &list);
    assert(!state.live);
    check_graphemes(&allocator, &grapheme);
    state.calls = 0;
    assert(!prepare_input(&allocator, &sentence, &grapheme, &list, &source, &input));
    calls = state.calls;
    assert(state.live == 1);
    /* Suppression removes the first three soft cuts; grapheme protection
       removes a|b. Neither stage alone would produce this final span. */
    assert(!qjs_intl_segment_native_input_next(input, 0, &segment));
    assert(segment.start == 0 && segment.end == 5 && segment.present);
    qjs_intl_segment_native_input_close(input);
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        input = (void *)1;
        assert(prepare_input(&allocator, &sentence, &grapheme, &list,
                                                     &source, &input) == QJS_INTL_NO_MEMORY);
        assert(!input && !state.live);
    }
    state.fail_at = 0;
    assert(!prepare_input(&allocator, &sentence, &grapheme, &list, &source, &input));
    memset(&sentence, 0, sizeof(sentence));
    memset(&list, 0, sizeof(list));
    memset(units, 0, sizeof(units));
    assert(!qjs_intl_segment_native_input_containing(input, 4, &segment) && segment.end == 5);
    qjs_intl_segment_native_input_close(input);
    word = grapheme;
    word.kind = QJS_INTL_SEGMENT_WORD;
    calls = state.calls;
    assert(prepare_input(&allocator, &word, NULL, NULL,
                          &source, &input) == QJS_INTL_INVALID_ARGUMENT);
    assert(!input && state.calls == calls);
    check_words(&allocator);
    qjs_intl_segment_native_input_close(NULL);
    assert(!state.live);
    return 0;
}
