/* Authored weighted/greedy paths, protected cuts, routing and atomic failure.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/segment/dictionary-refine.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE(name) \
    extern const uint8_t qjs_intl_dictionary_refine_##name##_bytes[]; \
    extern const size_t qjs_intl_dictionary_refine_##name##_length
FIXTURE(programs);
FIXTURE(weighted);
FIXTURE(zero);
FIXTURE(huge);
FIXTURE(tie);
FIXTURE(prefix);
FIXTURE(suffix);
FIXTURE(pass);
FIXTURE(mixed);
FIXTURE(sea);
FIXTURE(sea_prefix);
#undef FIXTURE

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;
typedef struct Fixture { const uint8_t *bytes; size_t length; } Fixture;

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

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static void open_dictionary(const QJSIntlAllocator *allocator, Fixture fixture,
                            QJSIntlSegmentDictionaryData *out)
{
    QJSIntlPack pack;
    assert(!qjs_intl_pack_open(fixture.bytes, fixture.length, &pack));
    assert(!qjs_intl_segment_dictionary_data_open(&pack, allocator, out));
}

static void prepare(const QJSIntlAllocator *allocator, const QJSIntlSegmentProgram *grapheme,
                    const QJSIntlSegmentProgram *word, const uint16_t *units, size_t length,
                    QJSIntlSegmentText *text, QJSIntlSegmentScan *clusters, QJSIntlSegmentScan *words)
{
    QJSIntlUTF16 input = {units, length};
    assert(!qjs_intl_segment_text_prepare(allocator, &input, text));
    assert(!qjs_intl_segment_scan_prepare(allocator, grapheme, text, clusters));
    assert(!qjs_intl_segment_scan_prepare(allocator, word, text, words));
}

static void check(const QJSIntlAllocator *allocator, const QJSIntlSegmentRouting *routing,
                  const QJSIntlSegmentProgram *grapheme, const QJSIntlSegmentProgram *word,
                  Fixture fixture, const uint16_t *units, size_t length, const char *expected)
{
    Allocation *state = allocator->opaque;
    QJSIntlSegmentDictionaryData dictionaries;
    QJSIntlSegmentText text;
    QJSIntlSegmentScan clusters, words;
    size_t i, live = state->live;
    open_dictionary(allocator, fixture, &dictionaries);
    prepare(allocator, grapheme, word, units, length, &text, &clusters, &words);
    assert(!qjs_intl_segment_dictionary_refine(allocator, word, &text, &clusters, &dictionaries, routing, &words));
    assert(strlen(expected) == text.count + 1);
    for (i = 0; i <= text.count; i++)
        assert(words.breaks[i] == (uint8_t)(expected[i] - '0'));
    qjs_intl_segment_scan_clear(&words);
    qjs_intl_segment_scan_clear(&clusters);
    qjs_intl_segment_text_clear(&text);
    assert(state->live == live);
}

static void check_failure(const QJSIntlAllocator *allocator, const QJSIntlSegmentRouting *routing,
                          const QJSIntlSegmentProgram *grapheme, const QJSIntlSegmentProgram *word,
                          Fixture fixture)
{
    Allocation *state = allocator->opaque;
    QJSIntlSegmentDictionaryData dictionaries;
    QJSIntlSegmentText text;
    QJSIntlSegmentScan clusters, words;
    const uint16_t units[] = {0x7532, 0x4e59, ' ', 0x1000};
    uint8_t original[5];
    size_t live;
    open_dictionary(allocator, fixture, &dictionaries);
    prepare(allocator, grapheme, word, units, 4, &text, &clusters, &words);
    memcpy(original, words.breaks, sizeof(original));
    live = state->live;
    state->fail_at = state->calls + 1;
    assert(qjs_intl_segment_dictionary_refine(allocator, word, &text, &clusters, &dictionaries,
                                              routing, &words) == QJS_INTL_NO_MEMORY);
    assert(state->live == live && !memcmp(original, words.breaks, sizeof(original)));
    state->fail_at = 0;
    /* The earlier Han run can choose a known word, but the later Myanmar run
       has no dictionary in this fixture. No earlier change may be published. */
    assert(qjs_intl_segment_dictionary_refine(allocator, word, &text, &clusters, &dictionaries,
                                              routing, &words) == QJS_INTL_UNSUPPORTED);
    assert(state->live == live && !memcmp(original, words.breaks, sizeof(original)));
    qjs_intl_segment_scan_clear(&words);
    qjs_intl_segment_scan_clear(&clusters);
    qjs_intl_segment_text_clear(&text);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlData shared;
    QJSIntlSegmentProgramData programs;
    QJSIntlSegmentProgram grapheme, defaults, lexical, blocked, hard;
    QJSIntlSegmentRouting *routing;
    const uint16_t abc[] = {0x7532, 0x4e59, 0x4e19}, abcd[] = {0x7532, 0x4e59, 0x4e19, 0x4e01};
    const uint16_t thai[] = {0xe01, 0xe02, 0xe04, 0xe07};
    const uint16_t generic[] = {0xe01, 0x34f, 0xe02}, conflicting[] = {0xe01, 0x1037, 0xe02};
    const uint16_t foreign[] = {0xe01, 0x93e, 0xe02}, restricted[] = {0xe01, 0x1cd0, 0xe02};
    const uint16_t mark_only[] = {0x34f, 0xe01, 0xe02};
    const uint16_t mixed[] = {0x7532, 0x30ab}, shared_letter[] = {0x30ab, 0x30fc, 0x30ab};
    const uint16_t punctuation[] = {0x30ab, 0x30fb, 0x30ab};
    const uint16_t supplementary[] = {0xd840, 0xdc00, 0x4e59};
#define DICT(name) ((Fixture){qjs_intl_dictionary_refine_##name##_bytes, qjs_intl_dictionary_refine_##name##_length})
#define CHECK(mode, dict, units, length, result) \
    check(&allocator, routing, &grapheme, &mode, DICT(dict), units, length, result)
    assert(!qjs_intl_data_open(qjs_intl_dictionary_refine_programs_bytes,
                              qjs_intl_dictionary_refine_programs_length, &allocator, &shared));
    assert(!qjs_intl_segment_program_data_open(&shared, &programs));
    assert(!qjs_intl_segment_program_at(&programs, 0, &grapheme));
    assert(!qjs_intl_segment_program_at(&programs, 1, &defaults));
    assert(!qjs_intl_segment_program_at(&programs, 2, &lexical));
    assert(!qjs_intl_segment_program_at(&programs, 3, &blocked));
    assert(!qjs_intl_segment_program_at(&programs, 4, &hard));
    assert(!qjs_intl_segment_routing_open(&allocator, &routing));
    CHECK(defaults, weighted, abc, 3, "1101"); /* A|BC beats greedy AB|unknown. */
    CHECK(defaults, zero, abc, 3, "1001");
    CHECK(defaults, huge, abc, 3, "1111");
    CHECK(lexical, huge, abc, 3, "1001");
    CHECK(lexical, tie, abc, 3, "1011"); /* Equal cost prefers the longer known edge. */
    CHECK(lexical, prefix, abcd, 4, "11001");
    CHECK(lexical, suffix, abcd, 4, "10111");
    CHECK(blocked, prefix, abc, 3, "1001"); /* Leaf at protected no-break rejected. */
    CHECK(blocked, pass, abc, 3, "1011"); /* Longer key may pass through it. */
    CHECK(hard, zero, abc, 3, "1111"); /* A full key cannot cross the hard cut. */
    CHECK(defaults, mixed, mixed, 2, "101");
    CHECK(defaults, mixed, shared_letter, 3, "1001");
    CHECK(defaults, mixed, punctuation, 3, "1111");
    CHECK(defaults, mixed, supplementary, 3, "101");
    CHECK(lexical, sea, thai, 4, "10011");
    CHECK(lexical, sea_prefix, thai, 3, "1101");
    CHECK(lexical, sea_prefix, thai, 2, "111");
    CHECK(lexical, sea_prefix, generic, 3, "1001");
    /* The synthetic default word program also has a cut inside the cluster.
       Refinement leaves that bit alone; final native-input protection removes it. */
    CHECK(defaults, sea, generic, 3, "1101");
    CHECK(defaults, sea, conflicting, 3, "1111");
    CHECK(defaults, sea, foreign, 3, "1111");
    CHECK(defaults, sea, restricted, 3, "1111");
    CHECK(defaults, sea, mark_only, 3, "1101");
    {
        const size_t length = 4099;
        uint16_t *units = malloc(length * sizeof(*units));
        char *expected = malloc(length + 2);
        size_t i;
        assert(units && expected);
        memset(expected, '0', length + 1);
        expected[0] = expected[length] = '1';
        expected[length + 1] = 0;
        /* One huge cluster must not make raw lookup walk past the key bound. */
        units[0] = 0xe01;
        for (i = 1; i < length - 1; i++)
            units[i] = 0x34f;
        units[length - 1] = 0xe02;
        CHECK(lexical, sea, units, length, expected);
        /* A long protected chain has one virtual unknown edge, not a repeated
           scan for the next legal endpoint at every cluster. */
        for (i = 0; i < length; i++)
            units[i] = 0x7532;
        CHECK(blocked, pass, units, length, expected);
        free(expected);
        free(units);
    }
    check_failure(&allocator, routing, &grapheme, &defaults, DICT(weighted));
    qjs_intl_segment_routing_close(routing);
    assert(!state.live);
    return 0;
}
