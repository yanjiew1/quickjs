/* Minimal Unicode segmentation coverage, indexing and handle lifetime.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/segment/segment.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;
static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    assert(ptr);
    state->live++;
    return ptr;
}
static void *resize(void *opaque, void *ptr, size_t size)
{
    (void)opaque;
    (void)ptr;
    (void)size;
    abort(); /* This backend never grows a per-input buffer. */
}
static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static void boundaries(const QJSIntlSegmenter *segmenter, const uint16_t *text, size_t length,
                       const size_t *ends, const int *words, size_t count)
{
    QJSIntlUTF16 input = {text, length};
    QJSIntlSegment segment;
    size_t i, start = 0, index;
    for (i = 0; i < count; i++) {
        assert(!qjs_intl_segment_next(segmenter, &input, start, &segment));
        assert(segment.present && segment.start == start && segment.end == ends[i]);
        assert(segment.is_word_like == (words ? words[i] : 0));
        for (index = start; index < segment.end; index++) {
            QJSIntlSegment found;
            assert(!qjs_intl_segment_containing(segmenter, &input, index, &found));
            assert(found.present && found.start == start && found.end == ends[i]);
            assert(found.is_word_like == segment.is_word_like);
        }
        start = segment.end;
    }
    assert(start == length);
    for (i = 0; i < 2; i++) {
        assert(!qjs_intl_segment_next(segmenter, &input, length, &segment));
        assert(!segment.present && !segment.start && !segment.end);
    }
    assert(!qjs_intl_segment_containing(segmenter, &input, SIZE_MAX, &segment));
    assert(!segment.present);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlSegmentService *service;
    QJSIntlSegmenter *segmenters[3], *out = (void *)1;
    const QJSIntlBytes *locales;
    size_t count, calls;
    int i;
    assert(!qjs_intl_segment_open(&allocator, &service));
    assert(!qjs_intl_segment_available(service, &locales, &count) && count == 2);
    assert(locales[0].length == 2 && !memcmp(locales[0].data, "en", 2));
    assert(locales[1].length == 5 && !memcmp(locales[1].data, "en-US", 5));
    for (i = 0; i < 3; i++)
        assert(!qjs_intl_segmenter_open(service, 1, i, &segmenters[i]));
    assert(qjs_intl_segmenter_open(service, 2, 0, &out) == QJS_INTL_INVALID_ARGUMENT && !out);
    assert(qjs_intl_segmenter_open(service, 0, (QJSIntlGranularity)-1, &out) ==
           QJS_INTL_INVALID_ARGUMENT && !out);
    state.fail_at = state.calls + 1;
    assert(qjs_intl_segmenter_open(service, 0, 0, &out) == QJS_INTL_NO_MEMORY && !out);
    state.fail_at = 0;
    qjs_intl_segment_close(service); /* Segmenters own their own state. */
    calls = state.calls;
    {
        const uint16_t text[] = {'a',0x301,'b',0,'c',0xd83d,0xde00,0xd800};
        const size_t ends[] = {2,3,4,5,7,8};
        boundaries(segmenters[0], text, 8, ends, NULL, 6);
    }
    {
        /* Unicode18 GB9c joins a leading linker to its consonant; the pinned
           CLDR beta root's older consonant-led rule is a different profile. */
        const uint16_t text[] = {0x94d,0x915};
        const size_t ends[] = {2};
        boundaries(segmenters[0], text, 2, ends, NULL, 1);
    }
    {
        const uint16_t text[] = {'a','b',' ','1','2','.','3',' ','_',' ',0xd83d,0xde00};
        const size_t ends[] = {2,3,7,8,9,10,12};
        const int words[] = {1,0,1,0,0,0,0};
        boundaries(segmenters[1], text, 12, ends, words, 7);
    }
    {
        const uint16_t text[] = {'H','i','.',' ','B','y','e','!'};
        const size_t ends[] = {4,8};
        boundaries(segmenters[2], text, 8, ends, NULL, 2);
    }
    {
        const uint16_t points[] = {0x4e00,0x3042,0xe01,'7','_',0x301,0xd800};
        for (i = 0; i < 7; i++) {
            QJSIntlUTF16 input = {points + i, 1};
            QJSIntlSegment segment;
            assert(!qjs_intl_segment_next(segmenters[1], &input, 0, &segment));
            assert(segment.present && segment.is_word_like == (i < 4));
        }
    }
    for (i = 0; i < 3; i++) {
        QJSIntlUTF16 empty = {NULL, 0}, invalid = {NULL, 1};
        QJSIntlSegment segment;
        boundaries(segmenters[i], NULL, 0, NULL, NULL, 0);
        assert(qjs_intl_segment_next(segmenters[i], &empty, 1, &segment) ==
               QJS_INTL_INVALID_ARGUMENT && !segment.present);
        assert(qjs_intl_segment_containing(segmenters[i], &invalid, 0, &segment) ==
               QJS_INTL_INVALID_ARGUMENT && !segment.present);
        qjs_intl_segmenter_close(segmenters[i]);
    }
    assert(state.calls == calls && !state.live);
    state.fail_at = state.calls + 1;
    service = (void *)1;
    assert(qjs_intl_segment_open(&allocator, &service) == QJS_INTL_NO_MEMORY && !service);
    qjs_intl_segmenter_close(NULL);
    qjs_intl_segment_close(NULL);
    return 0;
}
