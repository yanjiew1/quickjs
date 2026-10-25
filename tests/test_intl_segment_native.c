/* Native provider admission, exact locale routes and independent lifetimes.
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
#include "../src/intl/segment/segment.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

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

#if QJS_INTL_TEST_SEGMENT_CASE == 0
static void check_routes_and_lifetimes(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlSegmentService *service;
    QJSIntlSegmenter *en, *zh, *word, *sentence;
    QJSIntlSegmentInput input = {0}, other = {0};
    QJSIntlSegment found, first;
    const QJSIntlBytes *available;
    uint16_t units[] = {'a', 'b', 0xd840, 0xdc00, 0, 0xd800};
    const uint16_t words[] = {0x7532, 0x4e59, 0x4e19};
    QJSIntlUTF16 text = {units, 6};
    size_t count, calls;

    assert(!qjs_intl_segment_open(allocator, NULL, &service));
    assert(!qjs_intl_segment_available(service, &available, &count) && count == 2);
    assert(available[0].length == 2 && !memcmp(available[0].data, "en", 2));
    assert(available[1].length == 2 && !memcmp(available[1].data, "zh", 2));
    assert(!qjs_intl_segmenter_open(service, 0, QJS_INTL_SEGMENT_GRAPHEME, &en));
    assert(!qjs_intl_segmenter_open(service, 1, QJS_INTL_SEGMENT_GRAPHEME, &zh));
    assert(!qjs_intl_segmenter_open(service, 0, QJS_INTL_SEGMENT_WORD, &word));
    assert(!qjs_intl_segmenter_open(service, 1, QJS_INTL_SEGMENT_SENTENCE, &sentence));
    /* Root is in the middle of the shared registry. Removing it from the
       public inventory must preserve zh's distinct grapheme route. */
    assert(!qjs_intl_segment_input_prepare(en, &text, &input));
    assert(!qjs_intl_segment_input_prepare(zh, &text, &other));
    assert(!qjs_intl_segment_next(&input, 0, &found) && found.end == 1);
    assert(!qjs_intl_segment_next(&other, 0, &found) && found.end == 2);
    qjs_intl_segment_input_clear(&input);
    qjs_intl_segment_input_clear(&other);
    qjs_intl_segment_close(service);
    qjs_intl_segmenter_close(en);
    qjs_intl_segmenter_close(zh);

    text = (QJSIntlUTF16){words, 3};
    assert(!qjs_intl_segment_input_prepare(word, &text, &input));
    qjs_intl_segmenter_close(word);
    assert(!qjs_intl_segment_next(&input, 0, &found) && found.end == 1 && found.is_word_like);
    assert(!qjs_intl_segment_next(&input, 1, &first) && first.end == 3 && first.is_word_like);
    qjs_intl_segment_input_clear(&input);
    text = (QJSIntlUTF16){units, 6};
    assert(!qjs_intl_segment_input_prepare(sentence, &text, &input));
    qjs_intl_segmenter_close(sentence);
    /* No shared owner, routing cache or handle survives. Queries need only
       the final vector; even the original text is no longer inspected. */
    assert(state->live == 1);
    memset(units, 0xff, sizeof(units));
    calls = state->calls;
    assert(!qjs_intl_segment_next(&input, 0, &first) && first.end == 2);
    assert(!qjs_intl_segment_containing(&input, 3, &found) && found.start == 2 && found.end == 4);
    assert(!qjs_intl_segment_next(&input, first.end, &first) && first.start == 2 && first.end == 4);
    assert(!qjs_intl_segment_containing(&input, 0, &found) && found.start == 0 && found.end == 2);
    assert(!qjs_intl_segment_containing(&input, 1, &found) && found.start == 0 && found.end == 2);
    assert(!qjs_intl_segment_next(&input, 6, &found) && !found.present);
    assert(!qjs_intl_segment_containing(&input, SIZE_MAX, &found) && !found.present);
    assert(qjs_intl_segment_next(&input, 3, &found) == QJS_INTL_INVALID_ARGUMENT && !found.present);
    assert(state->calls == calls);
    qjs_intl_segment_input_clear(&input);
    qjs_intl_segment_input_clear(&input);
    assert(!state->live);
}

static void check_failures(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlSegmentService *service = NULL;
    QJSIntlSegmenter *segmenter = NULL, *bad = NULL;
    QJSIntlSegmentInput input;
    QJSIntlSegment found;
    const QJSIntlBytes *available;
    uint16_t units[] = {0x7532, 0x4e59, 0x4e19};
    QJSIntlUTF16 text = {units, 3};
    size_t calls, at, live;

    state->calls = 0;
    assert(!qjs_intl_segment_open(allocator, NULL, &service));
    calls = state->calls;
    qjs_intl_segment_close(service);
    assert(!state->live);
    /* Sweep physical allocations, including routing reallocations. Each
       failed open must unpublish its output and release every partial owner. */
    for (at = 1; at <= calls; at++) {
        state->calls = 0;
        state->fail_at = at;
        service = (void *)1;
        assert(qjs_intl_segment_open(allocator, NULL, &service) == QJS_INTL_NO_MEMORY);
        assert(!service && !state->live);
    }
    state->fail_at = 0;
    assert(!qjs_intl_segment_open(allocator, NULL, &service));
    live = state->live;
    state->fail_at = state->calls + 1;
    assert(qjs_intl_segmenter_open(service, 0, QJS_INTL_SEGMENT_WORD, &segmenter) == QJS_INTL_NO_MEMORY);
    assert(!segmenter && state->live == live);
    state->fail_at = 0;
    assert(!qjs_intl_segmenter_open(service, 0, QJS_INTL_SEGMENT_WORD, &segmenter));
    live = state->live;
    calls = state->calls;
    assert(!qjs_intl_segment_input_prepare(segmenter, &text, &input));
    calls = state->calls - calls;
    qjs_intl_segment_input_clear(&input);
    assert(state->live == live);
    for (at = 1; at <= calls; at++) {
        state->fail_at = state->calls + at;
        memset(&input, 0xff, sizeof(input));
        assert(qjs_intl_segment_input_prepare(segmenter, &text, &input) == QJS_INTL_NO_MEMORY);
        assert(!input.state && !input.text.data && !input.text.length && state->live == live);
    }
    state->fail_at = 0;
    text = (QJSIntlUTF16){NULL, 0};
    assert(!qjs_intl_segment_input_prepare(segmenter, &text, &input));
    assert(!qjs_intl_segment_next(&input, 0, &found) && !found.present);
    qjs_intl_segment_input_clear(&input);
    text = (QJSIntlUTF16){NULL, 1};
    assert(qjs_intl_segment_input_prepare(segmenter, &text, &input) == QJS_INTL_INVALID_ARGUMENT);
    assert(!input.state);
    assert(qjs_intl_segment_input_prepare(segmenter, NULL, &input) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_segmenter_open(service, 2, QJS_INTL_SEGMENT_WORD, &bad) == QJS_INTL_INVALID_ARGUMENT);
    /* The failed open resets a fresh output; keep the existing handle separate. */
    assert(!bad);
    assert(qjs_intl_segmenter_open(service, 0, (QJSIntlGranularity)-1, &bad) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_segment_available(NULL, &available, &calls) == QJS_INTL_INVALID_ARGUMENT);
    assert(!available && !calls);
    qjs_intl_segmenter_close(segmenter);
    qjs_intl_segment_close(service);
    assert(!state->live);
}
#endif

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlSegmentService *service = (void *)1;
    QJSIntlAllocator invalid = allocator;

    invalid.realloc = NULL;
    assert(qjs_intl_segment_open(&invalid, NULL, &service) == QJS_INTL_INVALID_ARGUMENT && !service);
    assert(qjs_intl_segment_open(&allocator, NULL, NULL) == QJS_INTL_INVALID_ARGUMENT);
#if QJS_INTL_TEST_SEGMENT_CASE == 0
    check_routes_and_lifetimes(&allocator);
    check_failures(&allocator);
#else
    assert(qjs_intl_segment_open(&allocator, NULL, &service) ==
           (QJS_INTL_TEST_SEGMENT_CASE == 1 || QJS_INTL_TEST_SEGMENT_CASE == 4 ?
            QJS_INTL_DATA_ERROR : QJS_INTL_UNSUPPORTED));
    assert(!service);
#endif
    qjs_intl_segment_close(NULL);
    qjs_intl_segmenter_close(NULL);
    qjs_intl_segment_input_clear(NULL);
    assert(!state.live);
    return 0;
}
