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
#include "root-data.h"
#include "../native/wire.h"
#include <string.h>

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)((unsigned int)p[0] | (unsigned int)p[1] << 8);
}

static uint32_t word(const uint8_t *p, size_t index)
{
    return qjs_intl_read_u32(p + index * 4);
}

static int take(const QJSIntlSection *section, size_t *at, uint32_t count,
                size_t width, const uint8_t **out)
{
    if (*at > section->length || count > (section->length - *at) / width)
        return 0;
    *out = section->bytes + *at;
    *at += (size_t)count * width;
    return 1;
}

static void entry_at(const QJSIntlCollationRoot *root, uint32_t index,
                     QJSIntlCollationEntry *out)
{
    const uint8_t *row = root->mappings + (size_t)index * 16;
    out->keys = root->keys + (size_t)word(row, 0) * 4;
    out->key_count = word(row, 1);
    out->elements = root->elements + (size_t)word(row, 2) * 8;
    out->element_count = word(row, 3);
}

static int compare(const uint8_t *a, size_t a_length, const uint8_t *b, size_t b_length)
{
    size_t i, length = a_length < b_length ? a_length : b_length;
    for (i = 0; i < length; i++) {
        uint32_t x = word(a, i), y = word(b, i);
        if (x != y)
            return x < y ? -1 : 1;
    }
    return a_length == b_length ? 0 : a_length < b_length ? -1 : 1;
}

QJSIntlStatus qjs_intl_collation_root_open(const QJSIntlPack *pack, QJSIntlCollationRoot *out)
{
    QJSIntlCollationRoot result = {0};
    QJSIntlCollationEntry previous = {0};
    QJSIntlStatus status;
    size_t at = 16;
    uint32_t i, key_end = 0, element_end = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!pack || !pack->bytes || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_pack_section(pack, QJS_INTL_SECTION_COLLATION_ROOT, &result.section);
    if (status)
        return status;
    if (!result.section.count || result.section.length < at ||
        result.section.length % 4 || word(result.section.bytes, 0) != 1 ||
        word(result.section.bytes, 1) != (18U << 16))
        return QJS_INTL_DATA_ERROR;
    result.key_count = word(result.section.bytes, 2);
    result.element_count = word(result.section.bytes, 3);
    if (!take(&result.section, &at, result.section.count, 16, &result.mappings) ||
        !take(&result.section, &at, result.key_count, 4, &result.keys) ||
        !take(&result.section, &at, result.element_count, 8, &result.elements) ||
        at != result.section.length)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < result.key_count; i++) {
        uint32_t scalar = word(result.keys, i);
        if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff))
            return QJS_INTL_DATA_ERROR;
    }
    for (i = 0; i < result.element_count; i++) {
        const uint8_t *element = result.elements + (size_t)i * 8;
        uint16_t variable = read_u16(element + 6);
        if (variable > 1 || (variable && !read_u16(element)))
            return QJS_INTL_DATA_ERROR;
    }
    for (i = 0; i < result.section.count; i++) {
        const uint8_t *row = result.mappings + (size_t)i * 16;
        uint32_t key_start = word(row, 0), key_length = word(row, 1);
        uint32_t element_start = word(row, 2), element_length = word(row, 3);
        QJSIntlCollationEntry entry;
        /* Pools are canonical contiguous partitions, not arbitrary references.
           Establish the bounds before decoding this row's borrowed views. */
        if (key_start != key_end || !key_length || key_length > result.key_count - key_end ||
            element_start != element_end || !element_length ||
            element_length > result.element_count - element_end)
            return QJS_INTL_DATA_ERROR;
        key_end += key_length;
        element_end += element_length;
        entry_at(&result, i, &entry);
        if (i && compare(previous.keys, previous.key_count, entry.keys, entry.key_count) >= 0)
            return QJS_INTL_DATA_ERROR;
        previous = entry;
    }
    if (key_end != result.key_count || element_end != result.element_count)
        return QJS_INTL_DATA_ERROR;
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_root_entry_at(const QJSIntlCollationRoot *root, uint32_t index,
                                              QJSIntlCollationEntry *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!root || !root->section.bytes || !out || index >= root->section.count)
        return QJS_INTL_INVALID_ARGUMENT;
    entry_at(root, index, out);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_key_at(const QJSIntlCollationEntry *entry, uint32_t index,
                                     uint32_t *out)
{
    if (out)
        *out = 0;
    if (!entry || !entry->keys || !out || index >= entry->key_count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = word(entry->keys, index);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_element_at(const QJSIntlCollationEntry *entry, uint32_t index,
                                            QJSIntlCollationElement *out)
{
    const uint8_t *p;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!entry || !entry->elements || !out || index >= entry->element_count)
        return QJS_INTL_INVALID_ARGUMENT;
    p = entry->elements + (size_t)index * 8;
    out->primary = read_u16(p);
    out->secondary = read_u16(p + 2);
    out->tertiary = read_u16(p + 4);
    out->variable = read_u16(p + 6);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_root_find(const QJSIntlCollationRoot *root,
                                          const uint32_t *key, size_t length,
                                          QJSIntlCollationEntry *out)
{
    uint32_t low = 0, high;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!root || !root->section.bytes || !out || (!key && length))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!length)
        return QJS_INTL_UNSUPPORTED;
    high = root->section.count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        QJSIntlCollationEntry entry;
        size_t i, common;
        int order = 0;
        entry_at(root, middle, &entry);
        common = length < entry.key_count ? length : entry.key_count;
        for (i = 0; i < common; i++) {
            uint32_t scalar = word(entry.keys, i);
            if (scalar != key[i]) {
                order = scalar < key[i] ? -1 : 1;
                break;
            }
        }
        if (!order)
            order = entry.key_count == length ? 0 : entry.key_count < length ? -1 : 1;
        if (order < 0)
            low = middle + 1;
        else if (order > 0)
            high = middle;
        else {
            *out = entry;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_collation_root_prefix(const QJSIntlCollationRoot *root,
                                            const uint32_t *input, size_t length,
                                            QJSIntlCollationEntry *out)
{
    uint32_t low = 0, high, found = UINT32_MAX;
    size_t depth = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!root || !root->section.bytes || !out || (!input && length))
        return QJS_INTL_INVALID_ARGUMENT;
    high = root->section.count;
    while (low < high) {
        QJSIntlCollationEntry entry;
        uint32_t first, end;
        entry_at(root, low, &entry);
        /* Within this interval every key shares the prefix. Its exact key,
           if present, precedes its extensions and is the fallback on a later
           mismatch (a predecessor search alone would lose that fallback). */
        if (entry.key_count == depth) {
            found = low++;
            if (low == high)
                break;
        }
        if (depth == length)
            break;
        first = low;
        end = high;
        while (first < end) {
            uint32_t middle = first + (end - first) / 2;
            entry_at(root, middle, &entry);
            if (word(entry.keys, depth) < input[depth])
                first = middle + 1;
            else
                end = middle;
        }
        low = first;
        end = high;
        while (first < end) {
            uint32_t middle = first + (end - first) / 2;
            entry_at(root, middle, &entry);
            if (word(entry.keys, depth) <= input[depth])
                first = middle + 1;
            else
                end = middle;
        }
        high = first;
        depth++;
    }
    if (found == UINT32_MAX)
        return QJS_INTL_UNSUPPORTED;
    entry_at(root, found, out);
    return QJS_INTL_OK;
}
