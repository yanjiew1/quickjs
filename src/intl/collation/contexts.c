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
#include "contexts.h"
#include "../native/wire.h"
#include <string.h>

static uint32_t word(const uint8_t *p, size_t index)
{
    return qjs_intl_read_u32(p + index * 4);
}

static void context_at(const QJSIntlCollationContexts *view, uint32_t index,
                       QJSIntlCollationContext *out)
{
    const uint8_t *row = view->section.bytes + 8 + (size_t)index * 12;
    uint32_t length = word(row, 1), skip = word(row, 2);
    out->source_index = word(row, 0);
    qjs_intl_collation_root_entry_at(&view->root, out->source_index, &out->before);
    out->mapping = out->before;
    out->before.key_count = length;
    out->before.element_count = skip;
    out->mapping.keys += (size_t)length * 4;
    out->mapping.key_count -= length;
    out->mapping.elements += (size_t)skip * 8;
    out->mapping.element_count -= skip;
}

/* Compare wire keys directly, without allocating a host-endian key buffer. */
static int compare(const QJSIntlCollationEntry *a, const QJSIntlCollationEntry *b)
{
    uint32_t i, count = a->key_count < b->key_count ? a->key_count : b->key_count;
    for (i = 0; i < count; i++) {
        uint32_t x = word(a->keys, i), y = word(b->keys, i);
        if (x != y)
            return x < y ? -1 : 1;
    }
    return a->key_count == b->key_count ? 0 : a->key_count < b->key_count ? -1 : 1;
}

static int valid_prefix(const QJSIntlCollationContexts *view,
                        const QJSIntlCollationEntry *before)
{
    uint32_t lo = 0, hi = view->root.section.count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        QJSIntlCollationEntry entry;
        int order;
        qjs_intl_collation_root_entry_at(&view->root, mid, &entry);
        order = compare(&entry, before);
        if (order < 0)
            lo = mid + 1;
        else if (order > 0)
            hi = mid;
        else
            return entry.element_count == before->element_count &&
                   !memcmp(entry.elements, before->elements, (size_t)entry.element_count * 8);
    }
    return 0;
}

QJSIntlStatus qjs_intl_collation_contexts_open(const QJSIntlPack *pack,
                                             QJSIntlCollationContexts *out)
{
    QJSIntlCollationContexts view = {0};
    QJSIntlStatus status;
    uint32_t i, previous = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!pack || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_pack_section(pack, QJS_INTL_SECTION_COLLATION_CONTEXTS, &view.section);
    if (status)
        return status;
    status = qjs_intl_collation_root_open(pack, &view.root);
    if (status)
        return status == QJS_INTL_UNSUPPORTED ? QJS_INTL_DATA_ERROR : status;
    if (view.section.length < 8 || word(view.section.bytes, 0) != 1 ||
        word(view.section.bytes, 1) != (18U << 16) ||
        view.section.count != (view.section.length - 8) / 12 ||
        (view.section.length - 8) % 12)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < view.section.count; i++) {
        const uint8_t *row = view.section.bytes + 8 + (size_t)i * 12;
        uint32_t source = word(row, 0), length = word(row, 1), skip = word(row, 2);
        QJSIntlCollationEntry entry;
        QJSIntlCollationContext context;
        if ((i && source <= previous) ||
            qjs_intl_collation_root_entry_at(&view.root, source, &entry) ||
            !length || length >= entry.key_count || !skip || skip >= entry.element_count)
            return QJS_INTL_DATA_ERROR;
        context_at(&view, i, &context);
        if (!valid_prefix(&view, &context.before))
            return QJS_INTL_DATA_ERROR;
        previous = source;
    }
    *out = view;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_context_at(const QJSIntlCollationContexts *view,
                                           uint32_t index, QJSIntlCollationContext *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!view || !view->section.bytes || !out || index >= view->section.count)
        return QJS_INTL_INVALID_ARGUMENT;
    context_at(view, index, out);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_contexts_keys(const QJSIntlCollationContexts *view,
                                               QJSIntlCollationKeyIndex *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!view || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!view->section.bytes || !view->root.section.bytes)
        return QJS_INTL_UNSUPPORTED;
    *out = (QJSIntlCollationKeyIndex){view->root.mappings, view->root.keys,
        view->section.bytes + 8, view->root.section.count, view->root.key_count, view->section.count};
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_contexts_mapping(const QJSIntlCollationContexts *view, uint32_t source,
                                                  QJSIntlCollationEntry *out)
{
    uint32_t low = 0, high;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!view || !view->section.bytes || !out || source >= view->root.section.count)
        return QJS_INTL_INVALID_ARGUMENT;
    high = view->section.count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        uint32_t id = word(view->section.bytes + 8 + (size_t)middle * 12, 0);
        if (id < source)
            low = middle + 1;
        else if (id > source)
            high = middle;
        else {
            QJSIntlCollationContext context;
            context_at(view, middle, &context);
            *out = context.mapping;
            return QJS_INTL_OK;
        }
    }
    return qjs_intl_collation_root_entry_at(&view->root, source, out);
}
