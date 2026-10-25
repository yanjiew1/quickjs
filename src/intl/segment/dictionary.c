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
#include "dictionary.h"
#include "../native/wire.h"
#include <string.h>

enum { HEADER = 16, RECORD = 48, MAX_STATES = 1000000, MAX_EDGES = 2000000 };

static uint32_t word(const uint8_t *array, uint32_t index)
{
    return qjs_intl_read_u32(array + (size_t)index * 4);
}

QJSIntlStatus qjs_intl_segment_dictionary_data_open(const QJSIntlPack *pack, const QJSIntlAllocator *allocator,
                                                    QJSIntlSegmentDictionaryData *out)
{
    QJSIntlSegmentDictionaryData result = {0};
    QJSIntlSection *section = &result.section;
    uint32_t index, previous = 0, total_states = 0, total_edges = 0;
    size_t expected;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !pack || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_pack_section(pack, QJS_INTL_SECTION_SEGMENT_DICTIONARIES, section);
    if (status)
        return status;
    if (section->length < HEADER || memcmp(section->bytes, "QJSDICT\0", 8) ||
        !section->count || section->count > QJS_INTL_DICTIONARY_FAMILY_COUNT ||
        word(section->bytes, 3) != section->count)
        return QJS_INTL_DATA_ERROR;
    if (word(section->bytes, 2) != 1)
        return QJS_INTL_UNSUPPORTED;
    expected = HEADER + (size_t)section->count * RECORD;
    if (expected > section->length)
        return QJS_INTL_DATA_ERROR;
    for (index = 0; index < section->count; index++) {
        const uint8_t *record = section->bytes + HEADER + (size_t)index * RECORD;
        uint32_t family = word(record, 0), valued = word(record, 1);
        size_t start = word(record, 10), length = word(record, 11);
        QJSIntlSegmentDictionary *dictionary;
        if (family >= QJS_INTL_DICTIONARY_FAMILY_COUNT || (index && family <= previous) ||
            valued != (family == QJS_INTL_DICTIONARY_CJK) || start != expected ||
            start > section->length || length > section->length - start)
            return QJS_INTL_DATA_ERROR;
        dictionary = &result.dictionaries[family];
        dictionary->graph.root = word(record, 2);
        dictionary->graph.words = word(record, 3);
        dictionary->graph.max_scalars = word(record, 4);
        dictionary->graph.max_utf16 = word(record, 5);
        dictionary->graph.alphabet_count = word(record, 6);
        dictionary->graph.value_count = word(record, 7);
        dictionary->graph.state_count = word(record, 8);
        dictionary->graph.edge_count = word(record, 9);
        dictionary->valued = valued;
        if (!dictionary->graph.words || !dictionary->graph.max_scalars || !dictionary->graph.max_utf16 ||
            !dictionary->graph.alphabet_count || dictionary->graph.alphabet_count > 65536 ||
            !dictionary->graph.value_count || dictionary->graph.value_count > 65535 ||
            dictionary->graph.state_count < 2 || dictionary->graph.state_count > MAX_STATES - total_states ||
            !dictionary->graph.edge_count || dictionary->graph.edge_count > MAX_EDGES - total_edges ||
            dictionary->graph.root != dictionary->graph.state_count - 1)
            return QJS_INTL_DATA_ERROR;
        total_states += dictionary->graph.state_count;
        total_edges += dictionary->graph.edge_count;
        {
            QJSIntlSegmentLiteralGraph metadata = dictionary->graph;
            status = qjs_intl_segment_literal_graph_bind(section->bytes + start, length,
                                                        &metadata, &dictionary->graph);
        }
        /* A family owns its payload policy. Reject it before graph scratch can
           fail, so malformed membership data is never reported as OOM. */
        if (!status && !valued && (dictionary->graph.value_count != 1 ||
                                  word(dictionary->graph.values, 0)))
            status = QJS_INTL_DATA_ERROR;
        if (!status)
            status = qjs_intl_segment_literal_graph_validate(&dictionary->graph, allocator);
        if (status)
            return status;
        result.present |= 1U << family;
        previous = family;
        expected = start + length;
    }
    if (expected != section->length)
        return QJS_INTL_DATA_ERROR;
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_dictionary_at(const QJSIntlSegmentDictionaryData *data,
                                             QJSIntlSegmentDictionaryFamily family,
                                             const QJSIntlSegmentDictionary **out)
{
    if (out)
        *out = NULL;
    if (!out || !data || !data->section.bytes || (unsigned int)family >= QJS_INTL_DICTIONARY_FAMILY_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!(data->present & (1U << family)))
        return QJS_INTL_UNSUPPORTED;
    *out = &data->dictionaries[family];
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_dictionary_step(const QJSIntlSegmentDictionary *data, uint32_t state,
                                               uint32_t scalar, uint32_t *out)
{
    return qjs_intl_segment_literal_graph_step(data ? &data->graph : NULL, state, scalar, out);
}

QJSIntlStatus qjs_intl_segment_dictionary_value(const QJSIntlSegmentDictionary *data, uint32_t state,
                                                uint32_t *out)
{
    return qjs_intl_segment_literal_graph_value(data ? &data->graph : NULL, state, out);
}
