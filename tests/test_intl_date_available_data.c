/* Immutable available-pattern/template queries, malformed input and validation allocation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/available-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t date_available_empty_bytes[];
extern const size_t date_available_empty_length;

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

static uint32_t calendar_id(const QJSIntlDateAvailableData *data, const char *name)
{
    uint32_t i;
    for (i = 0; i < data->calendar_count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_date_available_calendar_at(data, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}

static uint32_t pattern_head(const QJSIntlDateAvailableData *data, const char *locale, const char *calendar)
{
    QJSIntlDateAvailableGroup group = {0};
    uint32_t index, head;
    group.calendar = calendar_id(data, calendar);
    group.family = QJS_INTL_DATE_AVAILABLE_PATTERN;
    assert(!qjs_intl_date_available_find_group(data, &group, &index));
    assert(!qjs_intl_date_available_head(data, locale_id(&data->data, locale), index, &head));
    return head;
}

static void check_queries(const QJSIntlDateAvailableData *data)
{
    QJSIntlDateAvailableEntry entry;
    QJSIntlDateAvailableNode node;
    QJSIntlDateAvailableGroup group;
    uint32_t i, j, found, head = pattern_head(data, "en-GB", "generic");
    assert(head);
    assert(qjs_intl_date_available_lookup(data, head, bytes("y"), QJS_INTL_DATE_NO_PLURAL,
                                         bytes(""), &entry) == QJS_INTL_UNSUPPORTED);
    {
        QJSIntlDateAvailableEntry zero = {0};
        assert(!memcmp(&entry, &zero, sizeof(entry)));
    }
    assert(!qjs_intl_date_available_lookup(data, head, bytes("d"), QJS_INTL_DATE_NO_PLURAL,
                                          bytes("variant"), &entry));
    assert(equal(entry.text, "'own'") && !entry.blocked);
    assert(!qjs_intl_date_available_node(data, head, &node) && equal(node.origin_locale, "en-GB"));
    head = node.parent;
    assert(!qjs_intl_date_available_lookup(data, head, bytes("y"), QJS_INTL_DATE_NO_PLURAL,
                                          bytes("variant"), &entry));
    assert(equal(entry.text, "'child'"));
    assert(!qjs_intl_date_available_node(data, head, &node) && equal(node.origin_locale, "en"));
    head = node.parent;
    assert(!qjs_intl_date_available_node(data, head, &node) && equal(node.origin_locale, "root"));
    assert(!qjs_intl_date_available_group_at(data, node.group, &group));
    assert(group.calendar == calendar_id(data, "gregorian") && !group.alt.length);
    assert(!qjs_intl_date_available_lookup(data, head, bytes("y"), QJS_INTL_DATE_NO_PLURAL, bytes(""), &entry));
    assert(equal(entry.text, "y"));
    assert(!qjs_intl_date_available_lookup(data, head, bytes("y"), QJS_INTL_DATE_PLURAL_OTHER, bytes(""), &entry));
    assert(equal(entry.text, "yy"));
    assert(!qjs_intl_date_available_lookup(data, head, bytes("y"), QJS_INTL_DATE_PLURAL_ONE,
                                          bytes("variant"), &entry));
    assert(equal(entry.text, "y年😀"));
    assert(!qjs_intl_date_available_lookup(data, head, bytes("j"), QJS_INTL_DATE_NO_PLURAL, bytes(""), &entry));
    assert(equal(entry.text, "h a"));
    assert(!qjs_intl_date_available_lookup(data, head, bytes("M"), QJS_INTL_DATE_NO_PLURAL, bytes(""), &entry));
    assert(!entry.blocked && !entry.text.length);
    assert(!qjs_intl_date_available_lookup(data, head, bytes("d"), QJS_INTL_DATE_NO_PLURAL, bytes(""), &entry));
    assert(entry.blocked && !entry.text.length);
    head = pattern_head(data, "de", "generic");
    assert(!qjs_intl_date_available_node(data, head, &node) && equal(node.origin_locale, "en-1901-1996"));
    assert(!qjs_intl_date_available_lookup(data, head, bytes("y"), QJS_INTL_DATE_NO_PLURAL, bytes(""), &entry));
    assert(equal(entry.text, "'external'"));
    group.calendar = calendar_id(data, "gregorian");
    group.alt = bytes("variant");
    assert(!qjs_intl_date_available_find_group(data, &group, &found));
    assert(!qjs_intl_date_available_head(data, locale_id(&data->data, "en-GB"), found, &head));
    assert(!qjs_intl_date_available_lookup(data, head, bytes("y"), QJS_INTL_DATE_NO_PLURAL, bytes("ascii"), &entry));
    assert(equal(entry.text, "yyyy"));
    group.alt = bytes("narrow variant");
    assert(!qjs_intl_date_available_find_group(data, &group, &found));
    group.family = QJS_INTL_DATE_AVAILABLE_APPEND;
    group.alt = bytes("");
    assert(!qjs_intl_date_available_find_group(data, &group, &found));
    assert(!qjs_intl_date_available_head(data, locale_id(&data->data, "en-GB"), found, &head));
    assert(!qjs_intl_date_available_lookup(data, head, bytes("Time-Day-Of-Week"), QJS_INTL_DATE_NO_PLURAL,
                                          bytes(""), &entry));
    assert(equal(entry.text, "{1} ob {0}"));
    assert(!qjs_intl_date_available_lookup(data, head, bytes("Year"), QJS_INTL_DATE_NO_PLURAL, bytes(""), &entry));
    assert(equal(entry.text, "{0} ({2}: {1})"));
    for (i = 0; i < data->group_count; i++) {
        assert(!qjs_intl_date_available_group_at(data, i, &group));
        assert(!qjs_intl_date_available_find_group(data, &group, &found) && found == i);
        for (j = 0; j < data->section.count; j++) {
            assert(!qjs_intl_date_available_head(data, j, i, &head));
            if (head) {
                QJSIntlDateAvailableGroup origin;
                assert(!qjs_intl_date_available_node(data, head, &node));
                assert(!qjs_intl_date_available_group_at(data, node.group, &origin));
                assert(origin.family == group.family);
            }
        }
    }
    /* Every stored entry, including long raw skeleton IDs, must be found exactly. */
    for (i = 1; i <= data->node_count; i++) {
        uint32_t start, count;
        assert(!qjs_intl_date_available_node(data, i, &node));
        assert(!qjs_intl_date_available_map(data, node.map, &start, &count));
        assert(start == qjs_intl_read_u32(data->maps + (size_t)node.map * 8));
        assert(count == qjs_intl_read_u32(data->maps + (size_t)node.map * 8 + 4));
        for (j = 0; j < count; j++) {
            const uint8_t *field = data->entries + ((size_t)start + j) * 16;
            QJSIntlBytes code, alt, text;
            uint32_t value = qjs_intl_read_u32(field + 12);
            QJSIntlDateAvailablePlural plural = qjs_intl_read_u32(field + 4);
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field), &code));
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field + 8), &alt));
            {
                QJSIntlDateAvailableEntry indexed;
                assert(!qjs_intl_date_available_entry_at(data, start + j, &indexed));
                assert(indexed.plural == plural && indexed.code.length == code.length && indexed.alt.length == alt.length);
                assert(!memcmp(indexed.code.data, code.data, code.length));
                assert(!alt.length || !memcmp(indexed.alt.data, alt.data, alt.length));
                assert(!qjs_intl_date_available_lookup(data, i, code, plural, alt, &entry));
                assert(indexed.blocked == entry.blocked && indexed.text.length == entry.text.length);
                assert(!entry.text.length || !memcmp(indexed.text.data, entry.text.data, entry.text.length));
            }
            assert(entry.plural == plural && entry.code.length == code.length);
            assert(entry.alt.length == alt.length);
            assert(!memcmp(entry.code.data, code.data, code.length));
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
        group.family = QJS_INTL_DATE_AVAILABLE_PATTERN;
        group.alt = (QJSIntlBytes){NULL, 0};
        assert(!qjs_intl_date_available_find_group(data, &group, &index));
        group.alt.length = 1;
        assert(qjs_intl_date_available_find_group(data, &group, &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    }
    {
        uint32_t start = 99, count = 99;
        assert(qjs_intl_date_available_map(data, data->map_count, &start, &count) == QJS_INTL_INVALID_ARGUMENT);
        assert(!start && !count);
        start = count = 99;
        assert(qjs_intl_date_available_map(data, 0, &start, NULL) == QJS_INTL_INVALID_ARGUMENT && !start);
        assert(qjs_intl_date_available_map(NULL, 0, NULL, &count) == QJS_INTL_INVALID_ARGUMENT && !count);
        memset(&entry, 0xff, sizeof(entry));
        assert(qjs_intl_date_available_entry_at(data, data->entry_count, &entry) == QJS_INTL_INVALID_ARGUMENT);
        assert(!entry.code.data && !entry.alt.data && !entry.text.data && !entry.blocked);
        assert(!entry.code.length && !entry.alt.length && !entry.text.length && entry.plural == QJS_INTL_DATE_NO_PLURAL);
        assert(qjs_intl_date_available_entry_at(data, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    }
    assert(qjs_intl_date_available_node(data, 0, &node) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_available_group_at(data, data->group_count, &group) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_available_head(data, data->section.count, 0, &head) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_available_lookup(data, 1, bytes("y"), (QJSIntlDateAvailablePlural)-1,
                                         bytes(""), &entry) == QJS_INTL_INVALID_ARGUMENT);
    {
        QJSIntlBytes empty = {NULL, 0}, invalid = {NULL, 1};
        assert(qjs_intl_date_available_lookup(data, 1, empty, QJS_INTL_DATE_NO_PLURAL,
                                             empty, &entry) == QJS_INTL_UNSUPPORTED);
        assert(qjs_intl_date_available_lookup(data, 1, invalid, QJS_INTL_DATE_NO_PLURAL,
                                             empty, &entry) == QJS_INTL_INVALID_ARGUMENT);
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
    QJSIntlDateAvailableData result, zero = {0};
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_available_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

static void corrupt(const QJSIntlDateAvailableData *owner, const QJSIntlAllocator *allocator,
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

static void duplicate(const QJSIntlDateAvailableData *owner, const QJSIntlAllocator *allocator,
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

static void check_corruption(const QJSIntlDateAvailableData *data, const QJSIntlAllocator *allocator)
{
    size_t i;
    uint32_t child = 0, append_node = 0, pattern_map = UINT32_MAX;
    for (i = 0; i < 7; i++) {
        corrupt(data, allocator, data->section.bytes + i * 4, UINT32_MAX);
        corrupt(data, allocator, data->section.bytes + i * 4, 0);
    }
    corrupt(data, allocator, data->calendars, data->data.strings.count);
    corrupt(data, allocator, data->calendars, string_id(&data->data, "Generic"));
    corrupt(data, allocator, data->calendars + 4, qjs_intl_read_u32(data->calendars));
    corrupt(data, allocator, data->groups, data->calendar_count);
    corrupt(data, allocator, data->groups + 4, QJS_INTL_DATE_AVAILABLE_FAMILY_COUNT);
    corrupt(data, allocator, data->groups + 8, data->data.strings.count);
    corrupt(data, allocator, data->locale_rows, data->row_count);
    corrupt(data, allocator, data->rows, data->node_count + 1);
    corrupt(data, allocator, data->maps, 1);
    corrupt(data, allocator, data->maps + 4, 0);
    corrupt(data, allocator, data->nodes, data->data.strings.count);
    corrupt(data, allocator, data->nodes + 4, data->group_count);
    corrupt(data, allocator, data->nodes + 8, data->map_count);
    corrupt(data, allocator, data->nodes + 12, 1);
    for (i = 0; i < data->node_count; i++) {
        QJSIntlDateAvailableNode node;
        QJSIntlDateAvailableGroup group;
        assert(!qjs_intl_date_available_node(data, (uint32_t)i + 1, &node));
        assert(!qjs_intl_date_available_group_at(data, node.group, &group));
        if (node.parent && group.family == QJS_INTL_DATE_AVAILABLE_PATTERN && !child)
            child = (uint32_t)i;
        if (!node.parent && group.family == QJS_INTL_DATE_AVAILABLE_APPEND)
            append_node = (uint32_t)i + 1;
        if (!node.parent && group.family == QJS_INTL_DATE_AVAILABLE_PATTERN)
            pattern_map = node.map;
    }
    assert(child && append_node && append_node <= child && pattern_map != UINT32_MAX);
    corrupt(data, allocator, data->nodes + (size_t)child * 16 + 12, append_node);
    {
        QJSIntlDateAvailableNode node;
        uint32_t pattern_group, start;
        QJSIntlDateAvailableGroup group = {0};
        group.calendar = calendar_id(data, "generic");
        group.family = QJS_INTL_DATE_AVAILABLE_PATTERN;
        assert(!qjs_intl_date_available_find_group(data, &group, &pattern_group));
        corrupt(data, allocator, data->rows + (size_t)pattern_group * 4, append_node);
        assert(!qjs_intl_date_available_node(data, append_node, &node));
        corrupt(data, allocator, data->nodes + ((size_t)append_node - 1) * 16 + 8, pattern_map);
        start = qjs_intl_read_u32(data->maps + (size_t)node.map * 8);
        corrupt(data, allocator, data->entries + (size_t)start * 16 + 4, QJS_INTL_DATE_PLURAL_ONE);
        corrupt(data, allocator, data->entries + (size_t)start * 16, string_id(&data->data, "Unknown"));
    }
    for (i = 0; i < 5; i++) {
        static const char *const bad[] = {"", "En", "en-us", "bad_tag", "en-1901-1901"};
        corrupt(data, allocator, data->nodes, string_id(&data->data, bad[i]));
    }
    corrupt(data, allocator, data->entries, data->data.strings.count);
    corrupt(data, allocator, data->entries + 4, QJS_INTL_DATE_PLURAL_COUNT);
    corrupt(data, allocator, data->entries + 8, data->data.strings.count);
    corrupt(data, allocator, data->entries + 12, data->data.strings.count);
    corrupt(data, allocator, data->entries + 12, string_id(&data->data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91"));
    corrupt(data, allocator, data->entries + 12, string_id(&data->data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"));
    corrupt(data, allocator, data->entries, string_id(&data->data, "bad!"));
    duplicate(data, allocator, data->groups + 12, 12);
    duplicate(data, allocator, data->nodes + 16, 16);
    duplicate(data, allocator, data->rows + (size_t)data->group_count * 4, (size_t)data->group_count * 4);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlDateAvailableData owner, result, zero = {0};
    size_t i, calls, successful;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_date_available_data_open(&data, &allocator, &owner) && !state.live);
    successful = state.calls;
    assert(successful > 1); /* Multivariant source-locale validation allocates. */
    calls = state.calls;
    check_queries(&owner);
    assert(state.calls == calls && !state.live);
    for (i = 1; i <= successful; i++) {
        state.calls = 0;
        state.fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_available_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
        state.calls = state.fail_at = 0;
        assert(!qjs_intl_date_available_data_open(&data, &allocator, &result) && !state.live);
    }
    check_corruption(&owner, &allocator);
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_date_available_data_open(&data, &allocator, &result));
    check_queries(&result);
    free(unaligned);
    assert(!qjs_intl_data_open(date_available_empty_bytes, date_available_empty_length, &allocator, &data));
    assert(!qjs_intl_date_available_data_open(&data, &allocator, &result));
    assert(!result.map_count && !result.node_count && !result.entry_count && result.row_count == 1);
    for (i = 0; i < result.group_count; i++) {
        uint32_t head = 123;
        assert(!qjs_intl_date_available_head(&result, 0, (uint32_t)i, &head) && !head);
    }
    {
        QJSIntlDateAvailableGroup group;
        uint32_t index = 123;
        assert(result.group_count == 1);
        assert(!qjs_intl_date_available_group_at(&result, 0, &group));
        group.alt = bytes("missing");
        assert(qjs_intl_date_available_find_group(&result, &group, &index) == QJS_INTL_UNSUPPORTED && !index);
        group.family = (QJSIntlDateAvailableFamily)-1;
        assert(qjs_intl_date_available_find_group(&result, &group, &index) == QJS_INTL_INVALID_ARGUMENT);
    }
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_available_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
    return 0;
}
