/* Localized zone source queries, hostile records and allocation failure.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/timezone-name-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t timezone_name_empty_bytes[];
extern const size_t timezone_name_empty_length;

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

static uint32_t head_for(const QJSIntlTimeZoneNameData *data, const char *locale,
                         QJSIntlTimeZoneNameFamily family, const char *entity)
{
    QJSIntlTimeZoneNameGroup group = {0};
    uint32_t index, head;
    group.family = family;
    group.entity = bytes(entity);
    group.width = family >= QJS_INTL_TIMEZONE_NAME_ZONE ? QJS_INTL_TIMEZONE_NAME_LONG : QJS_INTL_TIMEZONE_NAME_NO_WIDTH;
    assert(!qjs_intl_timezone_name_find_group(data, &group, &index));
    assert(!qjs_intl_timezone_name_head(data, locale_id(&data->data, locale), index, &head));
    return head;
}

static void check_queries(const QJSIntlTimeZoneNameData *data)
{
    QJSIntlTimeZoneNameEntry entry;
    QJSIntlTimeZoneNameNode node;
    QJSIntlTimeZoneNameGroup group;
    uint32_t i, j, found, head = head_for(data, "de", QJS_INTL_TIMEZONE_NAME_METAZONE, "British");
    assert(head);
    assert(!qjs_intl_timezone_name_node(data, head, &node));
    assert(equal(node.origin_locale, "en-1901-1996"));
    assert(!qjs_intl_timezone_name_group_at(data, node.group, &group));
    assert(group.family == QJS_INTL_TIMEZONE_NAME_ZONE && equal(group.entity, "Europe/London"));
    assert(!qjs_intl_timezone_name_lookup(data, head, bytes("standard"), QJS_INTL_TIMEZONE_NAME_NO_TYPE, bytes(""), &entry));
    assert(equal(entry.text, "English London"));
    head = head_for(data, "en-GB", QJS_INTL_TIMEZONE_NAME_FORMAT, "");
    assert(!qjs_intl_timezone_name_lookup(data, head, bytes("gmtFormat"), QJS_INTL_TIMEZONE_NAME_NO_TYPE, bytes(""), &entry));
    assert(!entry.blocked && !entry.text.length);
    assert(qjs_intl_timezone_name_lookup(data, head, bytes("gmtFormat"), QJS_INTL_TIMEZONE_NAME_NO_TYPE, bytes("utc"), &entry) == QJS_INTL_UNSUPPORTED);
    assert(!entry.text.data && !entry.code.data);
    assert(!qjs_intl_timezone_name_node(data, head, &node) && node.parent);
    assert(!qjs_intl_timezone_name_lookup(data, node.parent, bytes("regionFormat"), QJS_INTL_TIMEZONE_NAME_STANDARD, bytes(""), &entry));
    assert(equal(entry.text, "{0} Standard"));
    assert(!qjs_intl_timezone_name_resolve(data, head, bytes("regionFormat"),
                                          QJS_INTL_TIMEZONE_NAME_STANDARD, bytes(""), &entry, &node));
    assert(equal(entry.text, "{0} Standard") && equal(node.origin_locale, "root"));
    assert(!qjs_intl_timezone_name_resolve(data, head, bytes("gmtFormat"),
                                          QJS_INTL_TIMEZONE_NAME_NO_TYPE, bytes(""), &entry, &node));
    assert(!entry.blocked && !entry.text.length && equal(node.origin_locale, "en-1901-1996"));
    assert(qjs_intl_timezone_name_resolve(data, 0, bytes("gmtFormat"),
                                         QJS_INTL_TIMEZONE_NAME_NO_TYPE, bytes(""), &entry, &node) == QJS_INTL_UNSUPPORTED);
    assert(!entry.text.data && !node.origin_locale.data);

    head = head_for(data, "en-GB", QJS_INTL_TIMEZONE_NAME_CITY, "Europe/London");
    assert(!qjs_intl_timezone_name_lookup(data, head, bytes("exemplarCity"), QJS_INTL_TIMEZONE_NAME_NO_TYPE, bytes("secondary"), &entry));
    assert(entry.blocked && !entry.text.length);
    for (i = 0; i < data->group_count; i++) {
        assert(!qjs_intl_timezone_name_group_at(data, i, &group));
        assert(!qjs_intl_timezone_name_find_group(data, &group, &found) && found == i);
        for (j = 0; j < data->section.count; j++) {
            assert(!qjs_intl_timezone_name_head(data, j, i, &head));
            if (head) {
                QJSIntlTimeZoneNameGroup origin;
                assert(!qjs_intl_timezone_name_node(data, head, &node));
                assert(!qjs_intl_timezone_name_group_at(data, node.group, &origin));
                assert(origin.family == group.family ||
                       (origin.family >= QJS_INTL_TIMEZONE_NAME_ZONE && group.family >= QJS_INTL_TIMEZONE_NAME_ZONE));
            }
        }
    }
    /* Every leaf is exactly queryable, including all independent alternates. */
    for (i = 1; i <= data->node_count; i++) {
        uint32_t start, count;
        assert(!qjs_intl_timezone_name_node(data, i, &node));
        start = qjs_intl_read_u32(data->maps + (size_t)node.map * 8);
        count = qjs_intl_read_u32(data->maps + (size_t)node.map * 8 + 4);
        for (j = 0; j < count; j++) {
            const uint8_t *field = data->entries + ((size_t)start + j) * 16;
            QJSIntlBytes code, alt, text;
            uint32_t type = qjs_intl_read_u32(field + 4), value = qjs_intl_read_u32(field + 12);
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field), &code));
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field + 8), &alt));
            assert(!qjs_intl_timezone_name_lookup(data, i, code, type, alt, &entry));
            assert(entry.type == type && entry.code.length == code.length && entry.alt.length == alt.length);
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
    assert(qjs_intl_timezone_name_node(data, 0, &node) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_timezone_name_group_at(data, data->group_count, &group) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_timezone_name_head(data, data->section.count, 0, &head) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_timezone_name_lookup(data, 1, bytes(""), QJS_INTL_TIMEZONE_NAME_TYPE_COUNT, bytes(""), &entry) == QJS_INTL_INVALID_ARGUMENT);
    group = (QJSIntlTimeZoneNameGroup){0};
    group.entity = (QJSIntlBytes){NULL, 1};
    assert(qjs_intl_timezone_name_find_group(data, &group, &found) == QJS_INTL_INVALID_ARGUMENT && !found);
    group.entity = bytes("missing");
    assert(qjs_intl_timezone_name_find_group(data, &group, &found) == QJS_INTL_UNSUPPORTED && !found);
    group.family = (QJSIntlTimeZoneNameFamily)-1;
    assert(qjs_intl_timezone_name_find_group(data, &group, &found) == QJS_INTL_INVALID_ARGUMENT);
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
    QJSIntlTimeZoneNameData result, zero = {0};
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_timezone_name_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

static void corrupt(const QJSIntlTimeZoneNameData *owner, const QJSIntlAllocator *allocator,
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

static void duplicate(const QJSIntlTimeZoneNameData *owner, const QJSIntlAllocator *allocator,
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

static void check_corruption(const QJSIntlTimeZoneNameData *data, const QJSIntlAllocator *allocator)
{
    size_t i;
    uint32_t name_node = 0, format_child = 0;
    for (i = 0; i < 9; i++) {
        corrupt(data, allocator, data->section.bytes + i * 4, UINT32_MAX);
        corrupt(data, allocator, data->section.bytes + i * 4, 0);
    }
    corrupt(data, allocator, data->groups, QJS_INTL_TIMEZONE_NAME_FAMILY_COUNT);
    corrupt(data, allocator, data->groups + 4, data->data.strings.count);
    corrupt(data, allocator, data->groups + 8, data->data.strings.count);
    corrupt(data, allocator, data->groups + 12, QJS_INTL_TIMEZONE_NAME_WIDTH_COUNT);
    corrupt(data, allocator, data->groups + 16, data->data.strings.count);
    for (i = 0; i < data->group_count; i++) {
        QJSIntlTimeZoneNameGroup group;
        assert(!qjs_intl_timezone_name_group_at(data, (uint32_t)i, &group));
        if (group.family != QJS_INTL_TIMEZONE_NAME_FORMAT)
            corrupt(data, allocator, data->groups + i * 20 + 4, string_id(&data->data, "Bad ID"));
    }
    corrupt(data, allocator, data->maps, 1);
    corrupt(data, allocator, data->maps + 4, 0);
    corrupt(data, allocator, data->nodes, data->data.strings.count);
    corrupt(data, allocator, data->nodes, string_id(&data->data, "bad_tag"));
    corrupt(data, allocator, data->nodes, string_id(&data->data, "en-1901-1901"));
    corrupt(data, allocator, data->nodes + 4, data->group_count);
    corrupt(data, allocator, data->nodes + 8, data->map_count);
    corrupt(data, allocator, data->nodes + 12, 1);
    corrupt(data, allocator, data->entries, data->data.strings.count);
    corrupt(data, allocator, data->entries, string_id(&data->data, "Unknown"));
    corrupt(data, allocator, data->entries + 4, QJS_INTL_TIMEZONE_NAME_TYPE_COUNT);
    corrupt(data, allocator, data->entries + 8, data->data.strings.count);
    corrupt(data, allocator, data->entries + 12, data->data.strings.count);
    corrupt(data, allocator, data->entries + 12, string_id(&data->data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91"));
    corrupt(data, allocator, data->entries + 12, string_id(&data->data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"));
    duplicate(data, allocator, data->groups + 20, 20);
    duplicate(data, allocator, data->nodes + 16, 16);
    for (i = 0; i < data->map_count; i++) {
        uint32_t start = qjs_intl_read_u32(data->maps + i * 8);
        uint32_t count = qjs_intl_read_u32(data->maps + i * 8 + 4);
        if (count > 1) {
            uint8_t *copy = malloc(qjs_intl_native_length);
            size_t offset = data->entries - data->data.pack.bytes + (size_t)start * 16;
            assert(copy);
            memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
            memcpy(copy + offset + 16, copy + offset, 12); /* Duplicate key, distinct value allowed. */
            reject_copy(copy, allocator);
            free(copy);
            break;
        }
    }
    assert(i < data->map_count);
    for (i = 1; i <= data->node_count; i++) {
        QJSIntlTimeZoneNameNode node;
        QJSIntlTimeZoneNameGroup group;
        assert(!qjs_intl_timezone_name_node(data, (uint32_t)i, &node));
        assert(!qjs_intl_timezone_name_group_at(data, node.group, &group));
        if (group.family >= QJS_INTL_TIMEZONE_NAME_ZONE && !node.parent)
            name_node = (uint32_t)i;
        if (group.family == QJS_INTL_TIMEZONE_NAME_FORMAT && node.parent)
            format_child = (uint32_t)i;
    }
    assert(name_node && format_child && name_node < format_child);
    corrupt(data, allocator, data->nodes + ((size_t)format_child - 1) * 16 + 12, name_node);
    for (i = 0; i < QJS_INTL_TIMEZONE_NAME_FAMILY_COUNT; i++) {
        const QJSIntlTimeZoneNameRows *rows = &data->families[i];
        assert(rows->group_count && rows->row_count);
        corrupt(data, allocator, rows->locale_rows, rows->row_count);
        corrupt(data, allocator, rows->rows, data->node_count + 1);
        if (i == QJS_INTL_TIMEZONE_NAME_FORMAT)
            corrupt(data, allocator, rows->rows, name_node);
        if (rows->row_count > 1)
            duplicate(data, allocator, rows->rows + (size_t)rows->group_count * 4, (size_t)rows->group_count * 4);
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlTimeZoneNameData owner, result, zero = {0};
    size_t i, calls, successful;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_timezone_name_data_open(&data, &allocator, &owner) && !state.live);
    successful = state.calls;
    assert(successful > 1);
    calls = state.calls;
    check_queries(&owner);
    assert(state.calls == calls && !state.live);
    for (i = 1; i <= successful; i++) {
        state.calls = 0;
        state.fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_timezone_name_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
        state.calls = state.fail_at = 0;
        assert(!qjs_intl_timezone_name_data_open(&data, &allocator, &result) && !state.live);
    }
    check_corruption(&owner, &allocator);
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_timezone_name_data_open(&data, &allocator, &result));
    check_queries(&result);
    free(unaligned);
    assert(!qjs_intl_data_open(timezone_name_empty_bytes, timezone_name_empty_length, &allocator, &data));
    assert(!qjs_intl_timezone_name_data_open(&data, &allocator, &result));
    assert(result.group_count == 1 && !result.map_count && !result.node_count && !result.entry_count);
    for (i = 0; i < QJS_INTL_TIMEZONE_NAME_FAMILY_COUNT; i++) {
        const QJSIntlTimeZoneNameRows *rows = &result.families[i];
        assert(rows->group_count == (i == QJS_INTL_TIMEZONE_NAME_CITY));
        assert(rows->row_count == (i == QJS_INTL_TIMEZONE_NAME_CITY));
    }
    {
        uint32_t head = 123;
        assert(!qjs_intl_timezone_name_head(&result, 0, 0, &head) && !head);
    }
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_timezone_name_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
    return 0;
}
