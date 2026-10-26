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
#include "prepared.h"
#include <string.h>

void qjs_intl_segment_input_clear(QJSIntlSegmentInput *input)
{
    if (!input)
        return;
    if (input->state) {
        QJSIntlPreparedSegments *state = input->state;

        state->allocator.free(state->allocator.opaque, state);
    }
    memset(input, 0, sizeof(*input));
}

static QJSIntlStatus segment_at(const QJSIntlSegmentInput *input, size_t index, int exact,
                                QJSIntlSegment *out)
{
    const QJSIntlPreparedSegments *state;
    size_t low = 0, high, start;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !input || (unsigned int)input->granularity > QJS_INTL_SEGMENT_SENTENCE ||
        (input->text.length && (!input->text.data || !input->state)))
        return QJS_INTL_INVALID_ARGUMENT;
    if (exact && index > input->text.length)
        return QJS_INTL_INVALID_ARGUMENT;
    if (index >= input->text.length)
        return QJS_INTL_OK;
    state = input->state;
    high = state->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;

        if ((size_t)state->boundaries[middle].end <= index)
            low = middle + 1;
        else
            high = middle;
    }
    if (low == state->count)
        return QJS_INTL_DATA_ERROR;
    start = low ? (size_t)state->boundaries[low - 1].end : 0;
    if (exact && start != index)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = (QJSIntlSegment){start, (size_t)state->boundaries[low].end, 1,
                            state->boundaries[low].word_like};
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_next(const QJSIntlSegmentInput *input, size_t start,
                                    QJSIntlSegment *out)
{
    return segment_at(input, start, 1, out);
}

QJSIntlStatus qjs_intl_segment_containing(const QJSIntlSegmentInput *input, size_t index,
                                          QJSIntlSegment *out)
{
    return segment_at(input, index, 0, out);
}
