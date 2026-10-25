/* One owner for logical root records, independent of either wire envelope.
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
#include "base.h"
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include <string.h>

static uint32_t word(const uint8_t *p, size_t index)
{
    return qjs_intl_read_u32(p + index * 4);
}

static int scalar(uint32_t code)
{
    return code <= 0x10ffff && (code < 0xd800 || code > 0xdfff);
}

int qjs_intl_collation_base_check_element(const QJSIntlCollationBaseMetadata *m, const uint8_t *bytes)
{
    const QJSIntlCollationRootLimits *r = &m->limits;
    uint32_t p = word(bytes, 0), s = word(bytes, 1), t = word(bytes, 2);
    uint32_t flags = word(bytes, 5), case_value = flags & 3;
    int implicit = (m->han_base < p && (uint64_t)p <= (uint64_t)m->han_base + m->han_count) ||
        (m->generic_base < p && p < m->generic_limit);
    if (p > r->primary_max || s > r->secondary_max || t > r->tertiary_max ||
        word(bytes, 3) || word(bytes, 4) != p || flags & ~15u || case_value == 3 ||
        p == m->numeric || !!(flags & 8) != implicit ||
        !!(flags & 4) != (m->field_separator < p && p <= m->variable_top))
        return 0;
    if (p)
        return s && s < r->secondary_boundary && t && t < r->tertiary_boundary;
    if (s)
        return s > r->secondary_boundary && t && t < r->tertiary_boundary;
    if (t)
        return t > r->tertiary_boundary && case_value == QJS_INTL_COLLATION_UPPER;
    return case_value == QJS_INTL_COLLATION_LOWER;
}

static int mappings(const QJSIntlCollationBase *view)
{
    const QJSIntlCollationKeyIndex *keys = &view->keys;
    uint32_t i, end = 0, context = 0, previous_prefix = 0, previous_count = 0;
    const uint8_t *previous = NULL;
    for (i = 0; i < keys->key_count; i++) {
        if (!scalar(word(keys->keys, i)))
            return 0;
    }
    for (i = 0; i < keys->context_count; i++) {
        const uint8_t *row = keys->contexts + (size_t)i * 12;
        uint32_t id = word(row, 0), prefix = word(row, 1);
        if (id >= keys->row_count || (i && id <= word(row - 12, 0)) || !prefix ||
            prefix >= word(keys->rows + (size_t)id * 16, 1) || word(row, 2))
            return 0;
    }
    for (i = 0; i < keys->row_count; i++) {
        const uint8_t *row = keys->rows + (size_t)i * 16, *key;
        uint32_t start = word(row, 0), count = word(row, 1), prefix = 0;
        uint32_t ce_start = word(row, 2), ce_count = word(row, 3), j;
        uint32_t pool_count = view->elements.count;
        if (start != end || !count || count > keys->key_count - end || !ce_count ||
            ce_start > pool_count || ce_count > pool_count - ce_start)
            return 0;
        key = keys->keys + (size_t)start * 4;
        end += count;
        if (context < keys->context_count && word(keys->contexts + (size_t)context * 12, 0) == i) {
            prefix = word(keys->contexts + (size_t)context++ * 12, 1);
            /* Source keys remain original. Check both starts with the same
               pinned boundary predicate used for new tailoring contexts. */
            if (!unicode_nfc_boundary_before(word(key, 0)) ||
                !unicode_nfc_boundary_before(word(key, prefix)))
                return 0;
        }
        if (word(key, prefix) == 0xfdd0)
            return 0;
        if (previous) {
            uint32_t limit = previous_count < count ? previous_count : count;
            int order = 0;
            for (j = 0; j < limit; j++) {
                uint32_t a = word(previous, j), b = word(key, j);
                if (a != b) {
                    order = a < b ? -1 : 1;
                    break;
                }
            }
            if (!order)
                order = previous_count < count ? -1 : previous_count > count ? 1 :
                    previous_prefix < prefix ? -1 : previous_prefix > prefix ? 1 : 0;
            if (order >= 0)
                return 0;
        }
        previous = key;
        previous_count = count;
        previous_prefix = prefix;
    }
    return keys->row_count && end == keys->key_count;
}

static int han(const QJSIntlCollationBase *view, uint8_t *seen)
{
    const QJSIntlSection *ranges = &view->han_ranges;
    const QJSIntlSection *ranks = &view->han_ranks;
    uint32_t i, end = 0, previous = 0;
    memset(seen, 0, (view->metadata.han_count + 7u) / 8u);
    for (i = 0; i < ranges->count; i++) {
        const uint8_t *row = ranges->bytes + (size_t)i * 12;
        uint32_t first = word(row, 0), last = word(row, 1), start = word(row, 2), count;
        if (first > last || !scalar(first) || !scalar(last) ||
            (first <= 0xdfff && last >= 0xd800) || (i && first <= previous) || start != end)
            return 0;
        count = last - first + 1;
        if (count > ranks->count - end)
            return 0;
        end += count;
        previous = last;
    }
    if (end != ranks->count || end != view->metadata.han_count)
        return 0;
    for (i = 0; i < ranks->count; i++) {
        uint32_t rank = word(ranks->bytes, i);
        if (!rank || rank > end || seen[(rank - 1) / 8] & (1u << ((rank - 1) % 8)))
            return 0;
        seen[(rank - 1) / 8] |= 1u << ((rank - 1) % 8);
    }
    return 1;
}

static int metadata(const QJSIntlCollationBaseMetadata *m)
{
    const QJSIntlCollationRootLimits *r = &m->limits;
    const uint32_t values[] = {r->primary_max, r->secondary_max, r->tertiary_max,
        r->secondary_common, r->tertiary_common, r->secondary_boundary, r->tertiary_boundary,
        m->han_base, m->han_count, m->generic_base, m->generic_count, m->generic_limit,
        m->variable_top, m->numeric, m->field_separator};
    unsigned int i;
    for (i = 0; i < sizeof(values) / sizeof(*values); i++) {
        if (values[i] >= UINT32_C(0x80000000))
            return 0;
    }
    return r->secondary_common && r->secondary_common < r->secondary_boundary &&
        r->secondary_boundary <= r->secondary_max &&
        r->tertiary_common && r->tertiary_common < r->tertiary_boundary &&
        r->tertiary_boundary <= r->tertiary_max &&
        m->han_count && m->han_count <= 0x110000 && m->generic_count == 0x110000 &&
        m->field_separator && m->field_separator < m->variable_top &&
        m->variable_top < m->numeric && m->numeric < m->han_base &&
        (uint64_t)m->han_base + m->han_count < m->generic_base &&
        (uint64_t)m->generic_base + m->generic_count + 1 == m->generic_limit &&
        m->generic_limit <= r->primary_max;
}

static int span(const QJSIntlSection *s, unsigned int stride)
{
    return (!s->length || s->bytes) && (uint64_t)s->count * stride == s->length;
}

QJSIntlStatus qjs_intl_collation_base_open(const QJSIntlCollationBaseSource *source,
                                           const QJSIntlAllocator *allocator, QJSIntlCollationBase *out)
{
    QJSIntlCollationBase view = {0};
    uint8_t *scratch;
    uint32_t i;
    int valid;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!source || !allocator || !allocator->malloc || !allocator->free || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!metadata(&source->metadata) || !span(&source->mappings, 16) || !span(&source->keys, 4) ||
        !span(&source->contexts, 12) || !span(&source->elements, 24) ||
        !span(&source->han_ranges, 12) || !span(&source->han_ranks, 4))
        return QJS_INTL_DATA_ERROR;
    view.metadata = source->metadata;
    view.keys = (QJSIntlCollationKeyIndex){source->mappings.bytes, source->keys.bytes, source->contexts.bytes,
        source->mappings.count, source->keys.count, source->contexts.count};
    view.elements = source->elements;
    view.han_ranges = source->han_ranges;
    view.han_ranks = source->han_ranks;
    scratch = allocator->malloc(allocator->opaque, (view.metadata.han_count + 7u) / 8u);
    if (!scratch)
        return QJS_INTL_NO_MEMORY;
    valid = mappings(&view) && han(&view, scratch);
    for (i = 0; valid && i < view.elements.count; i++)
        valid = qjs_intl_collation_base_check_element(&view.metadata, view.elements.bytes + (size_t)i * 24);
    allocator->free(allocator->opaque, scratch);
    if (!valid)
        return QJS_INTL_DATA_ERROR;
    view.ready = 1;
    *out = view;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_base_mapping(const QJSIntlCollationBase *view, uint32_t id,
                                              uint32_t *start, uint32_t *count)
{
    const uint8_t *row;
    if (start)
        *start = 0;
    if (count)
        *count = 0;
    if (!view || !view->ready || !start || !count || id >= view->keys.row_count)
        return QJS_INTL_INVALID_ARGUMENT;
    row = view->keys.rows + (size_t)id * 16;
    *start = word(row, 2);
    *count = word(row, 3);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_base_element(const QJSIntlCollationBase *view, uint32_t index,
                                              QJSIntlCollationCoordinate *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!view || !view->ready || !out || index >= view->elements.count)
        return QJS_INTL_INVALID_ARGUMENT;
    qjs_intl_collation_base_read_element(view->elements.bytes + (size_t)index * 24, out);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_base_implicit(const QJSIntlCollationBase *view, uint32_t code,
                                               QJSIntlCollationCoordinate *out)
{
    uint32_t lo = 0, hi, primary;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!view || !view->ready || !out || code > 0x10ffff)
        return QJS_INTL_INVALID_ARGUMENT;
    hi = view->han_ranges.count;
    primary = view->metadata.generic_base + code + 1;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const uint8_t *row = view->han_ranges.bytes + (size_t)mid * 12;
        uint32_t first = word(row, 0), last = word(row, 1);
        if (code < first)
            hi = mid;
        else if (code > last)
            lo = mid + 1;
        else {
            primary = view->metadata.han_base + word(view->han_ranks.bytes,
                                                    word(row, 2) + code - first);
            break;
        }
    }
    *out = (QJSIntlCollationCoordinate){{primary, view->metadata.limits.secondary_common,
        view->metadata.limits.tertiary_common, 0}, primary, QJS_INTL_COLLATION_LOWER, 0, 1};
    return QJS_INTL_OK;
}
