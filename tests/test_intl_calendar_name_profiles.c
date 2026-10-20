/* Prepared calendar-name data and its exact, allocation-free query interface.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/name-data.h"
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

static void check_english(const QJSIntlCalendarNameData *data)
{
    uint32_t calendar, locale, i, parent, group, head;
    QJSIntlCalendarNameGroup key = {0};
    for (calendar = 0; calendar < data->calendar_count; calendar++) {
        QJSIntlBytes name;
        assert(!qjs_intl_calendar_name_calendar_at(data, calendar, &name));
        if (equal(name, "gregorian"))
            break;
    }
    assert(calendar < data->calendar_count);
    for (locale = 0; locale < data->section.count; locale++) {
        QJSIntlBytes name;
        assert(!qjs_intl_data_locale(&data->data, locale, &name, &parent));
        if (equal(name, "en"))
            break;
    }
    assert(locale < data->section.count);
    key.calendar = calendar;
    key.context = QJS_INTL_CAL_NAME_FORMAT;
    key.width = QJS_INTL_CAL_NAME_WIDE;
    for (i = 0; i < 2; i++) {
        QJSIntlCalendarNameEntry entry;
        key.family = i ? QJS_INTL_CAL_NAME_DAY : QJS_INTL_CAL_NAME_MONTH;
        assert(!qjs_intl_calendar_name_find_group(data, &key, &group));
        assert(!qjs_intl_calendar_name_head(data, locale, group, &head));
        while (head) {
            QJSIntlCalendarNameNode node;
            QJSIntlStatus status = qjs_intl_calendar_name_lookup(data, head, bytes(i ? "sun" : "1"),
                                                                QJS_INTL_CAL_NAME_NO_YEAR, bytes(""), &entry);
            assert(!qjs_intl_calendar_name_node(data, head, &node));
            if (status == QJS_INTL_OK) {
                assert(!entry.blocked && equal(entry.text, i ? "Sunday" : "January"));
                assert(equal(node.origin_locale, "en"));
                break;
            }
            assert(status == QJS_INTL_UNSUPPORTED);
            head = node.parent;
        }
        assert(head);
    }
}

static void check_entries(const QJSIntlCalendarNameData *data)
{
    uint32_t i, j;
    for (i = 0; i < data->group_count; i++) {
        QJSIntlCalendarNameGroup group;
        uint32_t found;
        assert(!qjs_intl_calendar_name_group_at(data, i, &group));
        assert(!qjs_intl_calendar_name_find_group(data, &group, &found) && found == i);
    }
    /* Exercise every raw selector in the prepared source layers, including
       alternate month years and historic numeric era identities. */
    for (i = 1; i <= data->node_count; i++) {
        QJSIntlCalendarNameNode node;
        uint32_t start, count;
        assert(!qjs_intl_calendar_name_node(data, i, &node));
        start = qjs_intl_read_u32(data->maps + (size_t)node.map * 8);
        count = qjs_intl_read_u32(data->maps + (size_t)node.map * 8 + 4);
        for (j = 0; j < count; j++) {
            const uint8_t *field = data->entries + ((size_t)start + j) * 16;
            QJSIntlBytes code, alt, text;
            QJSIntlCalendarNameEntry entry;
            QJSIntlCalendarNameYear year = qjs_intl_read_u32(field + 4);
            uint32_t value = qjs_intl_read_u32(field + 12);
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field), &code));
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field + 8), &alt));
            assert(!qjs_intl_calendar_name_lookup(data, i, code, year, alt, &entry));
            assert(entry.year == year && entry.code.length == code.length && entry.alt.length == alt.length);
            assert(!memcmp(entry.code.data, code.data, code.length));
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
    QJSIntlCalendarNameData owner, result, zero = {0};
    size_t calls, successful, i;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_calendar_name_data_open(&data, &allocator, &owner) && !state.live);
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
        assert(qjs_intl_calendar_name_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
        state.calls = state.fail_at = 0;
        assert(!qjs_intl_calendar_name_data_open(&data, &allocator, &result) && !state.live);
    }
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_calendar_name_data_open(&data, &allocator, &result) && !state.live);
    check_english(&result);
    free(unaligned);
    return 0;
}
