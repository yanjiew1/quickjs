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
#include "suppression-data.h"
#include "../native/wire.h"
#include <string.h>

enum {
    HEADER = 72, DESCRIPTOR = 48, BINDING = 8, MAX_SIZE = 32 * 1024 * 1024,
    MAX_LISTS = 256, MAX_BINDINGS = 65536,
    MAX_STATES = 262144, MAX_EDGES = 524288, MAX_WORDS = 65536, MAX_SCALARS = 256,
    MAX_TOTAL_STATES = 1000000, MAX_TOTAL_EDGES = 2000000, MAX_TOTAL_WORDS = 1000000
};

static uint32_t word(const uint8_t *bytes, uint32_t index)
{
    return qjs_intl_read_u32(bytes + (size_t)index * 4);
}

static void metadata(const uint8_t *descriptor, QJSIntlSegmentLiteralGraph *out)
{
    memset(out, 0, sizeof(*out));
    out->root = word(descriptor, 0);
    out->words = word(descriptor, 1);
    out->max_scalars = word(descriptor, 2);
    out->max_utf16 = word(descriptor, 3);
    out->alphabet_count = word(descriptor, 4);
    out->value_count = word(descriptor, 5);
    out->state_count = word(descriptor, 6);
    out->edge_count = word(descriptor, 7);
}

/* Descriptor extents have already passed the section-wide preflight. Binding
   repeats only bounded array checks; neither graph scratch nor shared state
   is needed when a caller later copies one validated list. */
static QJSIntlStatus list_view(const QJSIntlSection *section, const uint8_t *descriptor,
                               QJSIntlSegmentLiteralGraph *out)
{
    QJSIntlSegmentLiteralGraph shape;
    metadata(descriptor, &shape);
    memset(out, 0, sizeof(*out));
    if (!shape.words)
        return QJS_INTL_OK;
    return qjs_intl_segment_literal_graph_bind(section->bytes + word(descriptor, 8),
                                              word(descriptor, 9), &shape, out);
}

QJSIntlStatus qjs_intl_segment_suppression_data_open(const QJSIntlData *shared,
                                                     const QJSIntlAllocator *allocator,
                                                     QJSIntlSegmentSuppressionData *out)
{
    QJSIntlSegmentSuppressionData result = {0};
    uint32_t i, used = 0, previous_locale = 0;
    uint32_t total_states = 0, total_edges = 0, total_words = 0;
    const uint8_t *header;
    size_t at;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !shared || !shared->pack.bytes || !shared->locales.count ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_pack_section(&shared->pack, QJS_INTL_SECTION_SEGMENT_SUPPRESSIONS, &result.section);
    if (status)
        return status;
    if (result.section.length < HEADER || result.section.length > MAX_SIZE)
        return QJS_INTL_DATA_ERROR;
    header = result.section.bytes;
    if (memcmp(header, "QJSSUPP\0", 8))
        return QJS_INTL_DATA_ERROR;
    if (word(header, 2) != 1 || word(header, 3) != (18u << 16) || word(header, 4) != 1)
        return QJS_INTL_UNSUPPORTED;
    result.list_count = word(header, 5);
    result.binding_count = word(header, 6);
    result.locale_count = shared->locales.count;
    result.fingerprint = header + 40;
    if (!result.list_count || result.list_count > MAX_LISTS ||
        result.list_count != result.section.count || !result.binding_count ||
        result.binding_count > MAX_BINDINGS || word(header, 7) != result.section.length ||
        word(header, 8) || word(header, 9))
        return QJS_INTL_DATA_ERROR;
    at = HEADER + (size_t)result.list_count * DESCRIPTOR;
    if (at > result.section.length || result.binding_count > (result.section.length - at) / BINDING)
        return QJS_INTL_DATA_ERROR;
    result.descriptors = header + HEADER;
    result.bindings = header + at;
    at += (size_t)result.binding_count * BINDING;

    /* Preflight every dimension, extent and aggregate before any graph scratch
       can fail. Empty policy consumes no graph bytes but still owns a route. */
    for (i = 0; i < result.list_count; i++) {
        const uint8_t *descriptor = result.descriptors + (size_t)i * DESCRIPTOR;
        QJSIntlSegmentLiteralGraph shape;
        size_t start = word(descriptor, 8), length = word(descriptor, 9);
        metadata(descriptor, &shape);
        if (start != at || start > result.section.length || length > result.section.length - start ||
            word(descriptor, 10) || word(descriptor, 11))
            return QJS_INTL_DATA_ERROR;
        if (!shape.words) {
            if (shape.root || shape.max_scalars || shape.max_utf16 || shape.alphabet_count ||
                shape.value_count || shape.state_count || shape.edge_count || length)
                return QJS_INTL_DATA_ERROR;
        } else {
            if (shape.words > MAX_WORDS || !shape.max_scalars || shape.max_scalars > MAX_SCALARS ||
                shape.max_utf16 < shape.max_scalars || shape.max_utf16 > 2 * MAX_SCALARS ||
                !shape.alphabet_count || shape.alphabet_count > 65536 || shape.value_count != 1 ||
                shape.state_count < 2 || shape.state_count > MAX_STATES ||
                shape.root != shape.state_count - 1 || !shape.edge_count || shape.edge_count > MAX_EDGES)
                return QJS_INTL_DATA_ERROR;
            if (shape.state_count > MAX_TOTAL_STATES - total_states ||
                shape.edge_count > MAX_TOTAL_EDGES - total_edges || shape.words > MAX_TOTAL_WORDS - total_words)
                return QJS_INTL_DATA_ERROR;
            total_states += shape.state_count;
            total_edges += shape.edge_count;
            total_words += shape.words;
        }
        at = start + length;
    }
    if (at != result.section.length)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < result.binding_count; i++) {
        const uint8_t *binding = result.bindings + (size_t)i * BINDING;
        uint32_t locale = word(binding, 0), index = word(binding, 1);
        if (locale >= result.locale_count || (i && locale <= previous_locale) ||
            index >= result.list_count || index > used)
            return QJS_INTL_DATA_ERROR;
        if (index == used)
            used++;
        previous_locale = locale;
    }
    if (used != result.list_count)
        return QJS_INTL_DATA_ERROR;

    /* Bind every list and check membership before allocating for any list.
       Otherwise a malformed later list could be hidden by an earlier OOM. */
    for (i = 0; i < result.list_count; i++) {
        QJSIntlSegmentLiteralGraph graph;
        status = list_view(&result.section, result.descriptors + (size_t)i * DESCRIPTOR, &graph);
        if (status)
            return status;
        if (!graph.words)
            continue;
        /* Membership is this container's policy, checked before allocation. */
        if (word(graph.values, 0))
            return QJS_INTL_DATA_ERROR;
    }
    for (i = 0; i < result.list_count; i++) {
        QJSIntlSegmentLiteralGraph graph;
        status = list_view(&result.section, result.descriptors + (size_t)i * DESCRIPTOR, &graph);
        if (status)
            return status;
        if (!graph.words)
            continue;
        status = qjs_intl_segment_literal_graph_validate(&graph, allocator);
        if (status)
            return status;
    }
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_suppression_at(const QJSIntlSegmentSuppressionData *data, uint32_t index,
                                              QJSIntlSegmentLiteralGraph *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!data || !data->section.bytes || !out || index >= data->list_count)
        return QJS_INTL_INVALID_ARGUMENT;
    return list_view(&data->section, data->descriptors + (size_t)index * DESCRIPTOR, out);
}

QJSIntlStatus qjs_intl_segment_suppression_find(const QJSIntlSegmentSuppressionData *data, uint32_t locale,
                                                QJSIntlSegmentLiteralGraph *out)
{
    uint32_t low = 0, high;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!data || !data->section.bytes || !out || locale >= data->locale_count)
        return QJS_INTL_INVALID_ARGUMENT;
    high = data->binding_count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t *binding = data->bindings + (size_t)middle * BINDING;
        uint32_t found = word(binding, 0);
        if (found < locale)
            low = middle + 1;
        else if (found == locale)
            return qjs_intl_segment_suppression_at(data, word(binding, 1), out);
        else
            high = middle;
    }
    return QJS_INTL_UNSUPPORTED;
}
