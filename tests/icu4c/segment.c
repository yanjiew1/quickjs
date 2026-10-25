/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "helpers.h"
#include "../../src/intl/segment/segment.h"

static void test_segment(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlSegmentService *service;
    QJSIntlSegmenter *segmenter;
    QJSIntlSegmentInput input, empty, failed;
    QJSIntlSegment segment, other;
    const QJSIntlBytes *available;
    size_t count, locale, i, before;
    static const uint16_t text[] = {'A', 0x301, ' ', 0xd800, 0, 0xd83d, 0xde00};
    static const size_t ends[] = {2, 3, 4, 5, 7};
    QJSIntlUTF16 slice = {text, sizeof(text) / sizeof(*text)};

    assert(!qjs_intl_segment_open(allocator, NULL, &service));
    assert(!qjs_intl_segment_available(service, &available, &count));
    locale = find(available, count, "en");
    assert(!qjs_intl_segmenter_open(service, locale, QJS_INTL_SEGMENT_GRAPHEME, &segmenter));
    qjs_intl_segment_close(service);
    state->fail_at = state->calls + 1;
    assert(qjs_intl_segment_input_prepare(segmenter, &slice, &failed) == QJS_INTL_NO_MEMORY);
    assert(!failed.state && !failed.text.data && !failed.text.length);
    state->fail_at = 0;
    assert(!qjs_intl_segment_input_prepare(segmenter, &slice, &input));
    assert(!qjs_intl_segment_input_prepare(segmenter, &(QJSIntlUTF16){0}, &empty));
    qjs_intl_segmenter_close(segmenter);
    before = state->calls;
    for (i = 0; i < sizeof(ends) / sizeof(ends[0]); i++) {
        size_t start = i ? ends[i - 1] : 0;

        assert(!qjs_intl_segment_next(&input, start, &segment));
        assert(segment.present && segment.start == start && segment.end == ends[i]);
        assert(!segment.is_word_like);
        assert(!qjs_intl_segment_containing(&input, ends[i] - 1, &other));
        assert(other.present && other.start == start && other.end == ends[i]);
        /* Interleaving an unrelated query must not advance either traversal. */
        assert(!qjs_intl_segment_next(&input, 0, &other) && other.end == 2);
    }
    assert(!qjs_intl_segment_next(&input, slice.length, &segment) && !segment.present);
    assert(qjs_intl_segment_next(&input, slice.length + 1, &segment) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_segment_next(&input, 1, &segment) == QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_segment_containing(&input, SIZE_MAX, &segment) && !segment.present);
    assert(!qjs_intl_segment_next(&empty, 0, &segment) && !segment.present);
    assert(state->calls == before);
    qjs_intl_segment_input_clear(&empty);
    qjs_intl_segment_input_clear(&input);
    qjs_intl_segment_input_clear(&input);
    assert(!state->live);

    /* Dictionary and sentence traversal follow ICU's installed rules; both
       paths must publish complete immutable boundaries and word statuses. */
    assert(!qjs_intl_segment_open(allocator, NULL, &service));
    assert(!qjs_intl_segment_available(service, &available, &count));
    {
        static const uint16_t thai[] = {
            0xe20, 0xe32, 0xe29, 0xe32, 0xe44, 0xe17, 0xe22,
            0xe20, 0xe32, 0xe29, 0xe32, 0xe44, 0xe17, 0xe22,
        };
        static const size_t thai_ends[] = {4, 7, 11, 14};
        QJSIntlUTF16 words = {thai, 14};
        size_t start = 0;

        assert(!qjs_intl_segmenter_open(service, find(available, count, "th"),
                                        QJS_INTL_SEGMENT_WORD, &segmenter));
        assert(!qjs_intl_segment_input_prepare(segmenter, &words, &input));
        qjs_intl_segmenter_close(segmenter);
        for (i = 0; i < sizeof(thai_ends) / sizeof(*thai_ends); i++) {
            assert(!qjs_intl_segment_next(&input, start, &segment));
            assert(segment.present && segment.is_word_like && segment.end == thai_ends[i]);
            start = segment.end;
        }
        qjs_intl_segment_input_clear(&input);
    }
    {
        static const uint16_t sentences[] = {'H', 'e', 'l', 'l', 'o', '!', ' ', 'B', 'y', 'e', '.'};
        QJSIntlUTF16 text = {sentences, 11};

        assert(!qjs_intl_segmenter_open(service, find(available, count, "en"),
                                        QJS_INTL_SEGMENT_SENTENCE, &segmenter));
        assert(!qjs_intl_segment_input_prepare(segmenter, &text, &input));
        qjs_intl_segmenter_close(segmenter);
        assert(!qjs_intl_segment_next(&input, 0, &segment) && segment.present && segment.end == 7);
        assert(!qjs_intl_segment_next(&input, 7, &segment) && segment.present && segment.end == 11);
        qjs_intl_segment_input_clear(&input);
    }
    qjs_intl_segment_close(service);
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);

    test_segment(&allocator);
    assert(!state.live);
    return 0;
}
