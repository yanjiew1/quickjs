/* Prepared timezone presentation source data and its exact, allocation-free query interface.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/timezone-name-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static void check_english(const QJSIntlTimeZoneNameData *data)
{
    uint32_t locale, parent, group, head;
    QJSIntlTimeZoneNameGroup key = {0};
    for (locale = 0; locale < data->section.count; locale++) {
        QJSIntlBytes name;
        assert(!qjs_intl_data_locale(&data->data, locale, &name, &parent));
        if (equal(name, "en"))
            break;
    }
    assert(locale < data->section.count);
    key.family = QJS_INTL_TIMEZONE_NAME_METAZONE;
    key.entity = bytes("America_Pacific");
    key.width = QJS_INTL_TIMEZONE_NAME_LONG;
    assert(!qjs_intl_timezone_name_find_group(data, &key, &group));
    assert(!qjs_intl_timezone_name_head(data, locale, group, &head));
    while (head) {
        QJSIntlTimeZoneNameNode node;
        QJSIntlTimeZoneNameEntry entry;
        QJSIntlStatus status = qjs_intl_timezone_name_lookup(data, head, bytes("generic"),
                                                            QJS_INTL_TIMEZONE_NAME_NO_TYPE, bytes(""), &entry);
        assert(!qjs_intl_timezone_name_node(data, head, &node));
        if (status == QJS_INTL_OK) {
            assert(!entry.blocked && equal(entry.text, "Pacific Time"));
            assert(equal(node.origin_locale, "en"));
            assert(!qjs_intl_timezone_name_lookup(data, head, bytes("standard"),
                                                  QJS_INTL_TIMEZONE_NAME_NO_TYPE, bytes(""), &entry));
            assert(equal(entry.text, "Pacific Standard Time"));
            break;
        }
        assert(status == QJS_INTL_UNSUPPORTED);
        head = node.parent;
    }
    assert(head);
}

static void check_entries(const QJSIntlTimeZoneNameData *data)
{
    uint32_t i, j;
    for (i = 0; i < data->group_count; i++) {
        QJSIntlTimeZoneNameGroup group;
        uint32_t found;
        assert(!qjs_intl_timezone_name_group_at(data, i, &group));
        assert(!qjs_intl_timezone_name_find_group(data, &group, &found) && found == i);
    }
    /* Check each locale's family-row address without expanding the implicit
       locale/group cube. Opening already validates every physical head. */
    for (i = 0; i < data->section.count; i++) {
        for (j = 0; j < QJS_INTL_TIMEZONE_NAME_FAMILY_COUNT; j++) {
            const QJSIntlTimeZoneNameRows *rows = &data->families[j];
            uint32_t column, head, row;
            assert(rows->group_count);
            column = (i + j) % rows->group_count;
            row = qjs_intl_read_u32(rows->locale_rows + (size_t)i * 4);
            assert(!qjs_intl_timezone_name_head(data, i, rows->group_start + column, &head));
            assert(head == qjs_intl_read_u32(rows->rows + ((size_t)row * rows->group_count + column) * 4));
        }
    }
    for (i = 1; i <= data->node_count; i++) {
        QJSIntlTimeZoneNameNode node;
        uint32_t start, count;
        assert(!qjs_intl_timezone_name_node(data, i, &node));
        start = qjs_intl_read_u32(data->maps + (size_t)node.map * 8);
        count = qjs_intl_read_u32(data->maps + (size_t)node.map * 8 + 4);
        for (j = 0; j < count; j++) {
            const uint8_t *field = data->entries + ((size_t)start + j) * 16;
            QJSIntlBytes code, alt, text;
            QJSIntlTimeZoneNameEntry entry;
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
                assert(!text.length || !memcmp(entry.text.data, text.data, text.length));
            }
        }
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlTimeZoneNameData owner, result, zero = {0};
    size_t calls, successful, i;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_timezone_name_data_open(&data, &allocator, &owner) && !state.live);
    successful = state.calls;
    assert(successful);
    calls = state.calls;
    check_english(&owner);
    check_entries(&owner);
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
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_timezone_name_data_open(&data, &allocator, &result) && !state.live);
    check_english(&result);
    free(unaligned);
    return 0;
}
