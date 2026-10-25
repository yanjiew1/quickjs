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
#include "dictionary-refine.h"
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include <string.h>

typedef struct Cluster {
    size_t start, next, legal;
    uint64_t cost;
    unsigned int family;
    uint8_t mutable, hard, known;
} Cluster;

/* Normalize Unicode evidence to dictionary families before intersection:
   Han/kana mixtures and Japanese shared letters remain CJK-compatible. A
   mark-only cluster is not a core; generic marks may attach to a letter core.
   Non-Letter/Mark scalars are conservative barriers, never transparent input. */
static unsigned int family_of(const QJSIntlSegmentRouting *routing,
                               const QJSIntlSegmentText *text, size_t start, size_t end)
{
    unsigned int mask = (1u << QJS_INTL_DICTIONARY_FAMILY_COUNT) - 1, family;
    int letter = 0;
    size_t i;
    for (i = start; i < end; i++) {
        uint32_t scalar = text->scalars[i];
        unsigned int candidates;
        int is_letter = lre_is_letter(scalar);
        if (!is_letter && !lre_is_mark(scalar))
            return QJS_INTL_DICTIONARY_FAMILY_COUNT;
        candidates = qjs_intl_segment_routing_mask(routing, scalar, 0);
        if (!candidates)
            candidates = qjs_intl_segment_routing_mask(routing, scalar, 1);
        if (!candidates && (is_letter || !qjs_intl_segment_routing_inherited(routing, scalar)))
            return QJS_INTL_DICTIONARY_FAMILY_COUNT;
        if (candidates)
            mask &= candidates;
        letter |= is_letter;
        if (!mask)
            return QJS_INTL_DICTIONARY_FAMILY_COUNT;
    }
    if (!letter || (mask & (mask - 1)))
        return QJS_INTL_DICTIONARY_FAMILY_COUNT;
    for (family = 0; !(mask & (1u << family)); family++)
        continue;
    return family;
}

/* A legal complete terminal is required even at a leaf. Raw scalar traversal
   is bounded by key length independently of a possibly huge grapheme tail. */
static QJSIntlStatus known_edges(const QJSIntlSegmentDictionary *dictionary,
                                 const QJSIntlSegmentText *text, Cluster *nodes,
                                 size_t start, size_t end, int weighted)
{
    size_t scalar = nodes[start].start, limit = nodes[end].start - scalar;
    size_t step, endpoint = start + 1;
    uint32_t state = dictionary->graph.root;
    if (limit > dictionary->graph.max_scalars)
        limit = dictionary->graph.max_scalars;
    for (step = 0; step < limit; step++) {
        uint32_t next, value;
        QJSIntlStatus status = qjs_intl_segment_dictionary_step(dictionary, state,
                                                                text->scalars[scalar + step], &next);
        if (status == QJS_INTL_UNSUPPORTED)
            break;
        if (status)
            return status;
        state = next;
        if (scalar + step + 1 != nodes[endpoint].start)
            continue;
        if (endpoint == end || nodes[endpoint].mutable) {
            status = qjs_intl_segment_dictionary_value(dictionary, state, &value);
            if (!status) {
                uint64_t cost = value;
                if (weighted) {
                    /* An overflowing known candidate cannot beat the finite
                       unknown path already available at this vertex. */
                    if (nodes[endpoint].cost > UINT64_MAX - cost)
                        goto next_endpoint;
                    cost += nodes[endpoint].cost;
                }
                if (!weighted || cost < nodes[start].cost ||
                    (cost == nodes[start].cost && (!nodes[start].known || endpoint > nodes[start].next))) {
                    nodes[start].next = endpoint;
                    nodes[start].cost = cost;
                    nodes[start].known = 1;
                }
            } else if (status != QJS_INTL_UNSUPPORTED) {
                return status;
            }
        }
next_endpoint:
        endpoint++;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus refine_run(const QJSIntlSegmentDictionary *dictionary,
                                const QJSIntlSegmentText *text, Cluster *nodes,
                                size_t start, size_t end, uint8_t *breaks)
{
    size_t i, legal = end;
    QJSIntlStatus status;
    /* Next legal endpoint is computed once, avoiding repeated walks through
       arbitrarily long protected no-break chains on every unknown step. */
    for (i = end; i-- > start;) {
        nodes[i].legal = legal;
        if (nodes[i].mutable)
            legal = i;
    }
    nodes[end].cost = 0;
    if (dictionary->valued) {
        uint64_t run_length = end - start;
        if (run_length > UINT64_MAX / 255)
            return QJS_INTL_OVERFLOW;
        for (i = end; i-- > start;) {
            size_t next = nodes[i].legal;
            uint64_t unknown = (uint64_t)(next - i) * 255;
            if (i != start && !nodes[i].mutable)
                continue;
            if (nodes[next].cost > UINT64_MAX - unknown)
                return QJS_INTL_OVERFLOW;
            nodes[i].cost = unknown + nodes[next].cost;
            nodes[i].next = next;
            nodes[i].known = 0;
            status = known_edges(dictionary, text, nodes, i, end, 1);
            if (status)
                return status;
        }
    } else {
        for (i = start; i < end; i = nodes[i].next) {
            nodes[i].next = nodes[i].legal;
            nodes[i].known = 0;
            status = known_edges(dictionary, text, nodes, i, end, 0);
            if (status)
                return status;
        }
    }
    for (i = start; i < end; i = nodes[i].next) {
        size_t j, next = nodes[i].next;
        if (!nodes[i].known)
            continue; /* Unknown-to-unknown endpoints preserve the base bits. */
        if (i > start)
            breaks[nodes[i].start] = 1;
        if (next < end)
            breaks[nodes[next].start] = 1;
        for (j = i + 1; j < next; j++)
            if (nodes[j].mutable)
                breaks[nodes[j].start] = 0;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_segment_dictionary_refine(const QJSIntlAllocator *allocator,
                                                 const QJSIntlSegmentProgram *program,
                                                 const QJSIntlSegmentText *text,
                                                 const QJSIntlSegmentScan *graphemes,
                                                 const QJSIntlSegmentDictionaryData *dictionaries,
                                                 const QJSIntlSegmentRouting *routing,
                                                 QJSIntlSegmentScan *words)
{
    Cluster *nodes;
    uint8_t *breaks;
    size_t bytes, count, clusters = 0, i, start;
    QJSIntlStatus status = QJS_INTL_OK;
    void *scratch;
    if (!allocator || !allocator->malloc || !allocator->free || !program ||
        program->kind != QJS_INTL_SEGMENT_WORD || !text || !text->offsets ||
        !graphemes || !graphemes->breaks || !words || !words->breaks || !words->rules ||
        !dictionaries || !routing || text->count != graphemes->count || text->count != words->count ||
        (text->count && !text->scalars))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!graphemes->breaks[0] || !graphemes->breaks[text->count] ||
        !words->breaks[0] || !words->breaks[text->count])
        return QJS_INTL_INVALID_ARGUMENT;
    if (!text->count)
        return QJS_INTL_OK;
    if (text->count == SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    count = text->count + 1;
    if (count > SIZE_MAX / (sizeof(*nodes) + 1))
        return QJS_INTL_OVERFLOW;
    bytes = count * (sizeof(*nodes) + 1);
    scratch = allocator->malloc(allocator->opaque, bytes);
    if (!scratch)
        return QJS_INTL_NO_MEMORY;
    nodes = scratch;
    memset(nodes, 0, count * sizeof(*nodes));
    breaks = (uint8_t *)(nodes + count);
    memcpy(breaks, words->breaks, count);
    for (i = 0; i < text->count; i++)
        if (graphemes->breaks[i])
            nodes[clusters++].start = i;
    nodes[clusters].start = text->count;
    for (i = 0; i < clusters; i++) {
        size_t position = nodes[i].start;
        nodes[i].family = family_of(routing, text, position, nodes[i + 1].start);
        if (!i)
            continue;
        if (words->rules[position] == QJS_INTL_SEGMENT_DEFAULT_RULE) {
            nodes[i].mutable = words->breaks[position] != 0;
        } else {
            QJSIntlSegmentRule rule;
            status = qjs_intl_segment_program_rule(program, words->rules[position], &rule);
            if (status)
                goto done;
            nodes[i].mutable = rule.role == QJS_INTL_SEGMENT_LEXICAL && !rule.breaks;
        }
        nodes[i].hard = words->breaks[position] && !nodes[i].mutable;
    }
    for (start = 0; start < clusters;) {
        size_t end = start + 1;
        const QJSIntlSegmentDictionary *dictionary;
        if (nodes[start].family == QJS_INTL_DICTIONARY_FAMILY_COUNT) {
            start++;
            continue;
        }
        while (end < clusters && nodes[end].family == nodes[start].family && !nodes[end].hard)
            end++;
        status = qjs_intl_segment_dictionary_at(dictionaries,
                                                (QJSIntlSegmentDictionaryFamily)nodes[start].family, &dictionary);
        if (!status)
            status = refine_run(dictionary, text, nodes, start, end, breaks);
        if (status)
            goto done;
        start = end;
    }
    memcpy(words->breaks, breaks, count);
done:
    allocator->free(allocator->opaque, scratch);
    return status;
}
