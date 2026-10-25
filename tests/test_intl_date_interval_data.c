/* Immutable interval-layer queries, malformed input and validation allocation.
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
#include "../src/intl/datetime/interval-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t date_interval_empty_bytes[];
extern const size_t date_interval_empty_length;

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

static QJSIntlBytes bytes(const char *text)
{
    QJSIntlBytes result = {text, strlen(text)};
    return result;
}

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) && (!text.length || !memcmp(text.data, expected, text.length));
}

static uint32_t locale_id(const QJSIntlData *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_locale(data, i, &text, &parent));
        if (equal(text, name))
            return i;
    }
    abort();
}

static uint32_t string_id(const QJSIntlData *data, const char *name)
{
    uint32_t i;
    for (i = 0; i < data->strings.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_string(data, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}

static uint32_t calendar_id(const QJSIntlDateIntervalData *data, const char *name)
{
    uint32_t i;
    for (i = 0; i < data->calendar_count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_date_interval_calendar_at(data, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}

static uint32_t item_head(const QJSIntlDateIntervalData *data, const char *locale, const char *calendar)
{
    QJSIntlDateIntervalGroup group = {0};
    uint32_t index, head;
    group.calendar = calendar_id(data, calendar);
    group.family = QJS_INTL_DATE_INTERVAL_ITEM;
    group.skeleton = bytes("yMd");
    assert(!qjs_intl_date_interval_find_group(data, &group, &index));
    assert(!qjs_intl_date_interval_head(data, locale_id(&data->data, locale), index, &head));
    return head;
}

static void check_queries(const QJSIntlDateIntervalData *data)
{
    QJSIntlDateIntervalEntry entry;
    QJSIntlDateIntervalNode node;
    QJSIntlDateIntervalGroup group;
    uint32_t i, j, found, head = item_head(data, "en-GB", "generic");
    assert(head);
    assert(qjs_intl_date_interval_lookup(data, head, bytes("d"), bytes(""), &entry) == QJS_INTL_UNSUPPORTED);
    {
        QJSIntlDateIntervalEntry zero = {0};
        assert(!memcmp(&entry, &zero, sizeof(entry)));
    }
    assert(!qjs_intl_date_interval_lookup(data, head, bytes("M"), bytes("variant"), &entry));
    assert(equal(entry.text, "'own'") && !entry.blocked);
    assert(!qjs_intl_date_interval_node(data, head, &node) && equal(node.origin_locale, "en-GB"));
    head = node.parent;
    assert(!qjs_intl_date_interval_lookup(data, head, bytes("d"), bytes("variant"), &entry));
    assert(equal(entry.text, "'child'"));
    assert(!qjs_intl_date_interval_node(data, head, &node) && equal(node.origin_locale, "en"));
    head = node.parent;
    assert(!qjs_intl_date_interval_node(data, head, &node) && equal(node.origin_locale, "root"));
    assert(!qjs_intl_date_interval_group_at(data, node.group, &group));
    assert(group.calendar == calendar_id(data, "gregorian") && !group.alt.length && !group.item_alt.length);
    assert(equal(group.skeleton, "yMd") && group.family == QJS_INTL_DATE_INTERVAL_ITEM);
    assert(!qjs_intl_date_interval_lookup(data, head, bytes("M"), bytes(""), &entry));
    assert(equal(entry.text, "latestFirst:M d – M d"));
    assert(!qjs_intl_date_interval_lookup(data, head, bytes("y"), bytes(""), &entry));
    assert(!entry.blocked && !entry.text.length);
    assert(!qjs_intl_date_interval_lookup(data, head, bytes("d"), bytes(""), &entry));
    assert(entry.blocked && !entry.text.length);
    head = item_head(data, "de", "generic");
    assert(!qjs_intl_date_interval_node(data, head, &node) && equal(node.origin_locale, "en-1901-1996"));
    assert(!qjs_intl_date_interval_lookup(data, head, bytes("d"), bytes(""), &entry));
    assert(equal(entry.text, "'external'"));
    group.calendar = calendar_id(data, "gregorian");
    group.alt = bytes("variant");
    group.item_alt = bytes("ascii");
    assert(!qjs_intl_date_interval_find_group(data, &group, &found));
    assert(!qjs_intl_date_interval_head(data, locale_id(&data->data, "en-GB"), found, &head));
    assert(!qjs_intl_date_interval_lookup(data, head, bytes("d"), bytes("variant"), &entry));
    assert(equal(entry.text, "dd–dd"));
    group.alt = group.item_alt = group.skeleton = bytes("");
    group.family = QJS_INTL_DATE_INTERVAL_FALLBACK;
    assert(!qjs_intl_date_interval_find_group(data, &group, &found));
    assert(!qjs_intl_date_interval_head(data, locale_id(&data->data, "en-GB"), found, &head));
    assert(!qjs_intl_date_interval_lookup(data, head, bytes(""), bytes(""), &entry));
    assert(equal(entry.text, "{0}–{1}"));
    group.family = QJS_INTL_DATE_INTERVAL_RANGE;
    assert(!qjs_intl_date_interval_find_group(data, &group, &found));
    assert(!qjs_intl_date_interval_head(data, locale_id(&data->data, "en-GB"), found, &head));
    assert(!qjs_intl_date_interval_lookup(data, head, bytes("mixed"), bytes(""), &entry));
    assert(equal(entry.text, "{1} to {0}"));
    for (i = 0; i < data->group_count; i++) {
        assert(!qjs_intl_date_interval_group_at(data, i, &group));
        assert(!qjs_intl_date_interval_find_group(data, &group, &found) && found == i);
        for (j = 0; j < data->section.count; j++) {
            assert(!qjs_intl_date_interval_head(data, j, i, &head));
            if (head) {
                QJSIntlDateIntervalGroup origin;
                assert(!qjs_intl_date_interval_node(data, head, &node));
                assert(!qjs_intl_date_interval_group_at(data, node.group, &origin));
                assert(origin.family == group.family);
            }
        }
    }
    /* Every leaf is exactly queryable, independently of its source family. */
    for (i = 1; i <= data->node_count; i++) {
        uint32_t start, count;
        assert(!qjs_intl_date_interval_node(data, i, &node));
        start = qjs_intl_read_u32(data->maps + (size_t)node.map * 8);
        count = qjs_intl_read_u32(data->maps + (size_t)node.map * 8 + 4);
        for (j = 0; j < count; j++) {
            const uint8_t *field = data->entries + ((size_t)start + j) * 12;
            QJSIntlBytes code, alt, text;
            uint32_t value = qjs_intl_read_u32(field + 8);
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field), &code));
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field + 4), &alt));
            assert(!qjs_intl_date_interval_lookup(data, i, code, alt, &entry));
            assert(entry.code.length == code.length && entry.alt.length == alt.length);
            assert(!code.length || !memcmp(entry.code.data, code.data, code.length));
            assert(!alt.length || !memcmp(entry.alt.data, alt.data, alt.length));
            if (value == UINT32_C(0xfffffffe)) {
                assert(entry.blocked);
            } else {
                assert(!qjs_intl_data_string(&data->data, value, &text));
                assert(!entry.blocked && entry.text.length == text.length);
                assert(!text.length || !memcmp(text.data, entry.text.data, text.length));
            }
        }
    }
    {
        uint32_t index = 99;
        group.calendar = calendar_id(data, "gregorian");
        group.family = QJS_INTL_DATE_INTERVAL_ITEM;
        group.alt = group.item_alt = (QJSIntlBytes){NULL, 0};
        group.skeleton = bytes("yMd");
        assert(!qjs_intl_date_interval_find_group(data, &group, &index));
        group.alt.length = 1;
        assert(qjs_intl_date_interval_find_group(data, &group, &index) == QJS_INTL_INVALID_ARGUMENT && !index);
        group.alt.length = 0;
        group.skeleton = bytes("missing");
        assert(qjs_intl_date_interval_find_group(data, &group, &index) == QJS_INTL_UNSUPPORTED && !index);
    }
    assert(qjs_intl_date_interval_node(data, 0, &node) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_interval_group_at(data, data->group_count, &group) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_interval_head(data, data->section.count, 0, &head) == QJS_INTL_INVALID_ARGUMENT);
    {
        QJSIntlBytes empty = {NULL, 0}, invalid = {NULL, 1};
        assert(qjs_intl_date_interval_lookup(data, 1, empty, empty, &entry) == QJS_INTL_UNSUPPORTED);
        assert(qjs_intl_date_interval_lookup(data, 1, invalid, empty, &entry) == QJS_INTL_INVALID_ARGUMENT);
    }
}

static void put_u32(uint8_t *at, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        at[i] = value >> (i * 8);
}

static void reject_copy(uint8_t *copy, const QJSIntlAllocator *allocator)
{
    QJSIntlData data;
    QJSIntlDateIntervalData result, zero = {0};
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_interval_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

static void corrupt(const QJSIntlDateIntervalData *owner, const QJSIntlAllocator *allocator,
                    const uint8_t *at, uint32_t value)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    size_t offset = at - owner->data.pack.bytes;
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    reject_copy(copy, allocator);
    free(copy);
}

static void duplicate(const QJSIntlDateIntervalData *owner, const QJSIntlAllocator *allocator,
                      const uint8_t *at, size_t length)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    size_t offset = at - owner->data.pack.bytes;
    assert(copy && offset >= length && offset <= qjs_intl_native_length - length);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    memcpy(copy + offset, copy + offset - length, length);
    reject_copy(copy, allocator);
    free(copy);
}

static void check_corruption(const QJSIntlDateIntervalData *data, const QJSIntlAllocator *allocator)
{
    size_t i;
    uint32_t child = 0, range_node = 0, item_suffix = UINT32_MAX;
    for (i = 0; i < 7; i++) {
        corrupt(data, allocator, data->section.bytes + i * 4, UINT32_MAX);
        corrupt(data, allocator, data->section.bytes + i * 4, 0);
    }
    corrupt(data, allocator, data->calendars, data->data.strings.count);
    corrupt(data, allocator, data->calendars, string_id(&data->data, "Generic"));
    corrupt(data, allocator, data->calendars + 4, qjs_intl_read_u32(data->calendars));
    corrupt(data, allocator, data->suffixes, data->data.strings.count);
    corrupt(data, allocator, data->suffixes + 4, QJS_INTL_DATE_INTERVAL_FAMILY_COUNT);
    corrupt(data, allocator, data->suffixes + 8, data->data.strings.count);
    corrupt(data, allocator, data->suffixes + 12, data->data.strings.count);
    corrupt(data, allocator, data->locale_rows, data->row_count);
    corrupt(data, allocator, data->rows, data->node_count + 1);
    corrupt(data, allocator, data->maps, 1);
    corrupt(data, allocator, data->maps + 4, 0);
    corrupt(data, allocator, data->nodes, data->data.strings.count);
    corrupt(data, allocator, data->nodes + 4, data->group_count);
    corrupt(data, allocator, data->nodes + 8, data->map_count);
    corrupt(data, allocator, data->nodes + 12, 1);
    for (i = 0; i < data->node_count; i++) {
        QJSIntlDateIntervalNode node;
        QJSIntlDateIntervalGroup group;
        assert(!qjs_intl_date_interval_node(data, (uint32_t)i + 1, &node));
        assert(!qjs_intl_date_interval_group_at(data, node.group, &group));
        if (node.parent && group.family == QJS_INTL_DATE_INTERVAL_ITEM)
            child = (uint32_t)i;
        if (!node.parent && group.family == QJS_INTL_DATE_INTERVAL_RANGE)
            range_node = (uint32_t)i + 1;
        if (group.family == QJS_INTL_DATE_INTERVAL_ITEM)
            item_suffix = node.group % data->suffix_count;
    }
    assert(child && range_node && range_node <= child && item_suffix != UINT32_MAX);
    corrupt(data, allocator, data->nodes + (size_t)child * 16 + 12, range_node);
    corrupt(data, allocator, data->rows + (size_t)item_suffix * 4, range_node);
    corrupt(data, allocator, data->suffixes + (size_t)item_suffix * 16 + 8, string_id(&data->data, "bad!"));
    for (i = 0; i < 5; i++) {
        static const char *const bad[] = {"", "En", "en-us", "bad_tag", "en-1901-1901"};
        corrupt(data, allocator, data->nodes, string_id(&data->data, bad[i]));
    }
    corrupt(data, allocator, data->entries, data->data.strings.count);
    corrupt(data, allocator, data->entries + 4, data->data.strings.count);
    corrupt(data, allocator, data->entries + 8, data->data.strings.count);
    corrupt(data, allocator, data->entries + 8, string_id(&data->data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91"));
    corrupt(data, allocator, data->entries + 8, string_id(&data->data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"));
    corrupt(data, allocator, data->entries, string_id(&data->data, "Unknown"));
    duplicate(data, allocator, data->suffixes + 16, 16);
    duplicate(data, allocator, data->nodes + 16, 16);
    duplicate(data, allocator, data->rows + (size_t)data->suffix_count * 4, (size_t)data->suffix_count * 4);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlDateIntervalData owner, result, zero = {0};
    size_t i, calls, successful;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_date_interval_data_open(&data, &allocator, &owner) && !state.live);
    successful = state.calls;
    assert(successful > 1); /* Multivariant source-locale validation allocates. */
    calls = state.calls;
    check_queries(&owner);
    assert(state.calls == calls && !state.live);
    for (i = 1; i <= successful; i++) {
        state.calls = 0;
        state.fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_interval_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
        state.calls = state.fail_at = 0;
        assert(!qjs_intl_date_interval_data_open(&data, &allocator, &result) && !state.live);
    }
    check_corruption(&owner, &allocator);
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_date_interval_data_open(&data, &allocator, &result));
    check_queries(&result);
    free(unaligned);
    assert(!qjs_intl_data_open(date_interval_empty_bytes, date_interval_empty_length, &allocator, &data));
    assert(!qjs_intl_date_interval_data_open(&data, &allocator, &result));
    assert(!result.map_count && !result.node_count && !result.entry_count && result.row_count == 1);
    for (i = 0; i < result.group_count; i++) {
        uint32_t head = 123;
        assert(!qjs_intl_date_interval_head(&result, 0, (uint32_t)i, &head) && !head);
    }
    {
        QJSIntlDateIntervalGroup group;
        uint32_t index = 123;
        assert(result.group_count == 1);
        assert(!qjs_intl_date_interval_group_at(&result, 0, &group));
        group.alt = bytes("missing");
        assert(qjs_intl_date_interval_find_group(&result, &group, &index) == QJS_INTL_UNSUPPORTED && !index);
        group.family = (QJSIntlDateIntervalFamily)-1;
        assert(qjs_intl_date_interval_find_group(&result, &group, &index) == QJS_INTL_INVALID_ARGUMENT);
    }
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_interval_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
    return 0;
}
