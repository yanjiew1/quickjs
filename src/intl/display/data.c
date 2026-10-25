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
#include "data.h"
#include "../common/source-locale.h"
#include "../native/wire.h"
#include <string.h>

#define HEADER_WORDS 6
#define ENTRY_WIDTH 6
#define NODE_WIDTH 4
#define BLOCKED UINT32_C(0xfffffffe)

static uint32_t word(const uint8_t *bytes, size_t index)
{
    return qjs_intl_read_u32(bytes + index * 4);
}

static int take(const QJSIntlSection *section, size_t *at, uint32_t count, uint32_t width,
                const uint8_t **out)
{
    if (count > (section->length / 4 - *at) / width)
        return 0;
    *out = section->bytes + *at * 4;
    *at += (size_t)count * width;
    return 1;
}

static int compare_words(const uint8_t *left, const uint8_t *right, size_t count)
{
    size_t i;
    for (i = 0; i < count; i++) {
        uint32_t a = word(left, i), b = word(right, i);
        if (a != b)
            return a < b ? -1 : 1;
    }
    return 0;
}

static int ordered(const uint8_t *records, uint32_t count, unsigned int width)
{
    size_t i;
    for (i = 1; i < count; i++) {
        if (compare_words(records + (i - 1) * width * 4, records + i * width * 4, width) >= 0)
            return 0;
    }
    return 1;
}

static int equal(QJSIntlBytes text, const char *value)
{
    size_t length = strlen(value);
    return text.length == length && (!length || !memcmp(text.data, value, length));
}

static int unresolved(QJSIntlBytes text)
{
    return equal(text, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91") ||
           equal(text, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85");
}

static int normalized(QJSIntlBytes raw, QJSIntlBytes norm, unsigned int group)
{
    size_t i;
    unsigned int separator = 1;
    if (!raw.length || raw.length != norm.length)
        return 0;
    if (group == QJS_INTL_NAME_PATTERN)
        return (!memcmp(raw.data, norm.data, raw.length) &&
                (equal(raw, "localePattern") || equal(raw, "localeSeparator") ||
                 equal(raw, "localeKeyTypePattern")));
    if (group == QJS_INTL_NAME_BRACKET)
        return !memcmp(raw.data, norm.data, raw.length);
    for (i = 0; i < raw.length; i++) {
        unsigned int c = (uint8_t)raw.data[i], expected = c;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            separator = 0;
            if (c >= 'A' && c <= 'Z')
                expected = c + ('a' - 'A');
        } else if ((c == '-' || c == '_') && !separator) {
            separator = 1;
            expected = '-';
        } else {
            return 0;
        }
        if ((uint8_t)norm.data[i] != expected)
            return 0;
    }
    return !separator;
}

static uint32_t key_find(const QJSIntlNameData *data, uint32_t group, uint32_t code)
{
    uint32_t low = 0, high = data->key_count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        uint32_t a = word(data->keys, (size_t)mid * 2), b = word(data->keys, (size_t)mid * 2 + 1);
        if (a < group || (a == group && b < code))
            low = mid + 1;
        else
            high = mid;
    }
    if (low < data->key_count && word(data->keys, (size_t)low * 2) == group &&
        word(data->keys, (size_t)low * 2 + 1) == code)
        return low;
    return data->key_count;
}

QJSIntlStatus qjs_intl_name_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                      QJSIntlNameData *out)
{
    QJSIntlNameData result = {0};
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    QJSIntlBytes empty;
    size_t at = HEADER_WORDS, i, scratch;
    uint32_t *depths;
    uint8_t *used_rows, *used_nodes, *map_groups, *used_keys, *sources;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->pack.bytes || !data->strings.bytes ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_DISPLAY_NAMES, &result.section) ||
        !result.section.count || result.section.count != data->locales.count ||
        result.section.length % 4 || result.section.length < HEADER_WORDS * 4 ||
        word(result.section.bytes, 0) != 1)
        return status;
    result.row_count = word(result.section.bytes, 1);
    result.map_count = word(result.section.bytes, 2);
    result.node_count = word(result.section.bytes, 3);
    result.entry_count = word(result.section.bytes, 4);
    result.key_count = word(result.section.bytes, 5);
    if (!result.row_count || result.row_count > result.section.count ||
        result.map_count > result.node_count || result.entry_count < result.map_count ||
        result.key_count > (uint64_t)result.entry_count * QJS_INTL_NAME_GROUP_COUNT ||
        qjs_intl_data_string(data, 0, &empty) || empty.length ||
        !take(&result.section, &at, result.section.count, 1, &result.locale_rows) ||
        !take(&result.section, &at, result.row_count, QJS_INTL_NAME_GROUP_COUNT, &result.rows) ||
        !take(&result.section, &at, result.map_count, 2, &result.maps) ||
        !take(&result.section, &at, result.node_count, NODE_WIDTH, &result.nodes) ||
        !take(&result.section, &at, result.entry_count, ENTRY_WIDTH, &result.entries) ||
        !take(&result.section, &at, result.key_count, 2, &result.keys) ||
        at != result.section.length / 4 ||
        !ordered(result.rows, result.row_count, QJS_INTL_NAME_GROUP_COUNT) ||
        !ordered(result.keys, result.key_count, 2))
        return status;
    at = 0;
    for (i = 0; i < result.map_count; i++) {
        uint32_t start = word(result.maps, i * 2), count = word(result.maps, i * 2 + 1);
        size_t j;
        if (start != at || !count || count > result.entry_count - at)
            return status;
        for (j = 1; j < count; j++) {
            if (compare_words(result.entries + (at + j - 1) * ENTRY_WIDTH * 4,
                              result.entries + (at + j) * ENTRY_WIDTH * 4, 4) >= 0)
                return status;
        }
        if (i) {
            uint32_t previous = word(result.maps, (i - 1) * 2);
            uint32_t previous_count = word(result.maps, (i - 1) * 2 + 1);
            size_t common = count < previous_count ? count : previous_count;
            int order = compare_words(result.entries + (size_t)previous * ENTRY_WIDTH * 4,
                                      result.entries + at * ENTRY_WIDTH * 4, common * ENTRY_WIDTH);
            if (order > 0 || (!order && previous_count >= count))
                return status;
        }
        at += count;
    }
    if (at != result.entry_count)
        return status;
    for (i = 0; i < result.entry_count; i++) {
        const uint8_t *entry = result.entries + i * ENTRY_WIDTH * 4;
        unsigned int column;
        QJSIntlBytes text;
        for (column = 0; column < 5; column++) {
            if (word(entry, column) >= data->strings.count)
                return status;
        }
        if (word(entry, 5) != BLOCKED &&
            (qjs_intl_data_string(data, word(entry, 5), &text) || unresolved(text)))
            return status;
    }
    for (i = 0; i < result.key_count; i++) {
        if (word(result.keys, i * 2) >= QJS_INTL_NAME_GROUP_COUNT ||
            word(result.keys, i * 2 + 1) >= data->strings.count)
            return status;
    }
    /* Counts occupy disjoint spans in the bounded pack, including the shared
       string table. This coverage/depth scratch is also bounded on 32-bit. */
    scratch = ((size_t)result.node_count + 1) * sizeof(*depths) + result.row_count +
              result.node_count + result.map_count + result.key_count + data->strings.count;
    depths = allocator->malloc(allocator->opaque, scratch);
    if (!depths)
        return QJS_INTL_NO_MEMORY;
    memset(depths, 0, scratch);
    used_rows = (uint8_t *)(depths + (size_t)result.node_count + 1);
    used_nodes = used_rows + result.row_count;
    map_groups = used_nodes + result.node_count;
    used_keys = map_groups + result.map_count;
    sources = used_keys + result.key_count;
    for (i = 0; i < result.section.count; i++) {
        uint32_t index = word(result.locale_rows, i);
        if (index >= result.row_count)
            goto done;
        used_rows[index] = 1;
    }
    for (i = 0; i < result.row_count; i++) {
        unsigned int group;
        if (!used_rows[i])
            goto done;
        for (group = 0; group < QJS_INTL_NAME_GROUP_COUNT; group++) {
            uint32_t head = word(result.rows, i * QJS_INTL_NAME_GROUP_COUNT + group);
            if (head > result.node_count ||
                (head && word(result.nodes, ((size_t)head - 1) * NODE_WIDTH + 1) != group))
                goto done;
            if (head)
                used_nodes[head - 1] = 1;
        }
    }
    for (i = 0; i < result.node_count; i++) {
        const uint8_t *node = result.nodes + i * NODE_WIDTH * 4;
        uint32_t origin = word(node, 0), group = word(node, 1), map = word(node, 2);
        uint32_t parent = word(node, 3), start, count;
        size_t j;
        if (origin >= data->strings.count || group >= QJS_INTL_NAME_GROUP_COUNT ||
            map >= result.map_count || parent > i ||
            (parent && word(result.nodes, ((size_t)parent - 1) * NODE_WIDTH + 1) != group))
            goto done;
        depths[i + 1] = depths[parent] + 1;
        if (i && (depths[i + 1] < depths[i] ||
                  (depths[i + 1] == depths[i] && compare_words(node - NODE_WIDTH * 4, node, NODE_WIDTH) >= 0)))
            goto done;
        if (!sources[origin]) {
            QJSIntlBytes text;
            QJSIntlStatus check;
            qjs_intl_data_string(data, origin, &text);
            check = qjs_intl_source_locale_validate(allocator, text);
            if (check) {
                if (check == QJS_INTL_NO_MEMORY || check == QJS_INTL_OVERFLOW)
                    status = check;
                goto done;
            }
            sources[origin] = 1;
        }
        if (map_groups[map] & (1u << group))
            continue;
        map_groups[map] |= 1u << group;
        start = word(result.maps, (size_t)map * 2);
        count = word(result.maps, (size_t)map * 2 + 1);
        for (j = start; j < (size_t)start + count; j++) {
            const uint8_t *entry = result.entries + j * ENTRY_WIDTH * 4;
            QJSIntlBytes norm, raw;
            uint32_t key = key_find(&result, group, word(entry, 0));
            qjs_intl_data_string(data, word(entry, 0), &norm);
            qjs_intl_data_string(data, word(entry, 4), &raw);
            if (!normalized(raw, norm, group) ||
                (word(entry, 2) && group != QJS_INTL_NAME_LANGUAGE) ||
                (word(entry, 3) && group != QJS_INTL_NAME_CALENDAR) || key == result.key_count)
                goto done;
            used_keys[key] = 1;
        }
    }
    for (i = result.node_count; i > 0; i--) {
        uint32_t parent = word(result.nodes, (i - 1) * NODE_WIDTH + 3);
        if (!used_nodes[i - 1])
            goto done;
        if (parent)
            used_nodes[parent - 1] = 1;
    }
    for (i = 0; i < (size_t)result.map_count + result.key_count; i++) {
        if (!map_groups[i])
            goto done;
    }
    *out = result;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, depths);
    return status;
}

QJSIntlStatus qjs_intl_name_head(const QJSIntlNameData *data, uint32_t locale,
                                 QJSIntlNameGroup group, uint32_t *out)
{
    uint32_t row;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        (unsigned int)group >= QJS_INTL_NAME_GROUP_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    row = word(data->locale_rows, locale);
    *out = word(data->rows, (size_t)row * QJS_INTL_NAME_GROUP_COUNT + group);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_name_node(const QJSIntlNameData *data, uint32_t index, QJSIntlNameNode *out)
{
    const uint8_t *node;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !index || index > data->node_count)
        return QJS_INTL_INVALID_ARGUMENT;
    node = data->nodes + ((size_t)index - 1) * NODE_WIDTH * 4;
    qjs_intl_data_string(&data->data, word(node, 0), &out->origin_locale);
    out->group = word(node, 1);
    out->map = word(node, 2);
    out->parent = word(node, 3);
    return QJS_INTL_OK;
}

static int compare_bytes(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t length = a.length < b.length ? a.length : b.length;
    int order = length ? memcmp(a.data, b.data, length) : 0;
    return order ? order : (a.length > b.length) - (a.length < b.length);
}

QJSIntlStatus qjs_intl_name_lookup(const QJSIntlNameData *data, uint32_t node,
                                   QJSIntlBytes code, QJSIntlBytes alt, QJSIntlBytes menu,
                                   QJSIntlBytes scope, QJSIntlNameEntry *out)
{
    QJSIntlBytes selectors[] = {code, alt, menu, scope};
    uint32_t map, start, count, low, high;
    unsigned int column;
    const uint8_t *entry;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !node || node > data->node_count)
        return QJS_INTL_INVALID_ARGUMENT;
    for (column = 0; column < 4; column++) {
        if (!selectors[column].data && selectors[column].length)
            return QJS_INTL_INVALID_ARGUMENT;
    }
    map = word(data->nodes, ((size_t)node - 1) * NODE_WIDTH + 2);
    start = word(data->maps, (size_t)map * 2);
    count = word(data->maps, (size_t)map * 2 + 1);
    low = 0;
    high = count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        int order = 0;
        entry = data->entries + ((size_t)start + mid) * ENTRY_WIDTH * 4;
        for (column = 0; column < 4 && !order; column++) {
            QJSIntlBytes value;
            qjs_intl_data_string(&data->data, word(entry, column), &value);
            order = compare_bytes(value, selectors[column]);
        }
        if (order < 0)
            low = mid + 1;
        else if (order > 0)
            high = mid;
        else {
            qjs_intl_data_string(&data->data, word(entry, 0), &out->normalized);
            qjs_intl_data_string(&data->data, word(entry, 1), &out->alt);
            qjs_intl_data_string(&data->data, word(entry, 2), &out->menu);
            qjs_intl_data_string(&data->data, word(entry, 3), &out->scope);
            qjs_intl_data_string(&data->data, word(entry, 4), &out->raw_code);
            out->blocked = word(entry, 5) == BLOCKED;
            if (!out->blocked)
                qjs_intl_data_string(&data->data, word(entry, 5), &out->text);
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_name_key_at(const QJSIntlNameData *data, uint32_t index,
                                   QJSIntlNameGroup *group, QJSIntlBytes *out)
{
    if (group)
        *group = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !group || !data || !data->section.bytes || index >= data->key_count)
        return QJS_INTL_INVALID_ARGUMENT;
    *group = word(data->keys, (size_t)index * 2);
    return qjs_intl_data_string(&data->data, word(data->keys, (size_t)index * 2 + 1), out);
}
