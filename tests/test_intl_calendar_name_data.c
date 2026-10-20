/* Immutable calendar-name queries, malformed input and validation allocation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/name-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t calendar_name_empty_bytes[];
extern const size_t calendar_name_empty_length;

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

static uint32_t calendar_id(const QJSIntlCalendarNameData *data, const char *name)
{
    uint32_t i;
    for (i = 0; i < data->calendar_count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_calendar_name_calendar_at(data, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}

static uint32_t month_head(const QJSIntlCalendarNameData *data, const char *locale, const char *calendar)
{
    QJSIntlCalendarNameGroup group = {0};
    uint32_t index, head;
    group.calendar = calendar_id(data, calendar);
    group.family = QJS_INTL_CAL_NAME_MONTH;
    group.context = QJS_INTL_CAL_NAME_FORMAT;
    group.width = QJS_INTL_CAL_NAME_WIDE;
    assert(!qjs_intl_calendar_name_find_group(data, &group, &index));
    assert(!qjs_intl_calendar_name_head(data, locale_id(&data->data, locale), index, &head));
    return head;
}

static void check_resolved_queries(const QJSIntlCalendarNameData *data)
{
    static const struct {
        const char *code;
        QJSIntlCalendarNameYear year;
        const char *alt, *text;
        unsigned int blocked;
        const char *locale, *calendar;
    } cases[] = {
        {"1", QJS_INTL_CAL_NAME_NO_YEAR, "", "root", 0, "root", "gregorian"},
        {"1", QJS_INTL_CAL_NAME_NO_YEAR, "variant", "child", 0, "en", "gregorian"},
        {"2", QJS_INTL_CAL_NAME_NO_YEAR, "", "", 1, "root", "gregorian"},
        {"3", QJS_INTL_CAL_NAME_NO_YEAR, "", "", 0, "root", "gregorian"},
        {"4", QJS_INTL_CAL_NAME_NO_YEAR, "", "own", 0, "en-GB", "generic"},
        {"6", QJS_INTL_CAL_NAME_NO_YEAR, "", "", 1, "en", "gregorian"},
        {"8", QJS_INTL_CAL_NAME_NO_YEAR, "", "", 0, "en", "gregorian"},
        {"7", QJS_INTL_CAL_NAME_STANDARD_YEAR, "", "ordinary", 0, "root", "gregorian"},
        {"7", QJS_INTL_CAL_NAME_LEAP_YEAR, "variant", "leap\xf0\x9f\x98\x80", 0, "root", "gregorian"}
    };
    QJSIntlCalendarNameEntry entry;
    QJSIntlCalendarNameNode origin;
    QJSIntlCalendarNameGroup group;
    uint32_t head = month_head(data, "en-GB", "generic");
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        assert(!qjs_intl_calendar_name_resolve(data, head, bytes(cases[i].code), cases[i].year,
                                              bytes(cases[i].alt), &entry, &origin));
        assert(equal(entry.text, cases[i].text) && entry.blocked == cases[i].blocked);
        assert(equal(origin.origin_locale, cases[i].locale));
        assert(!qjs_intl_calendar_name_group_at(data, origin.group, &group));
        assert(group.calendar == calendar_id(data, cases[i].calendar));
    }
    for (i = 0; i < 3; i++) {
        QJSIntlCalendarNameYear year = i == 1 ? QJS_INTL_CAL_NAME_LEAP_YEAR : QJS_INTL_CAL_NAME_NO_YEAR;
        memset(&entry, 0xff, sizeof(entry));
        memset(&origin, 0xff, sizeof(origin));
        assert(qjs_intl_calendar_name_resolve(data, head, bytes(i == 2 ? "5" : "7"), year,
                                             bytes(""), &entry, &origin) == QJS_INTL_UNSUPPORTED);
        assert(!entry.code.data && !entry.text.length && !entry.alt.data && !entry.blocked && !entry.year);
        assert(!origin.origin_locale.data && !origin.parent && !origin.group && !origin.map);
    }
    head = month_head(data, "de", "generic");
    assert(!qjs_intl_calendar_name_resolve(data, head, bytes("5"), QJS_INTL_CAL_NAME_NO_YEAR,
                                          bytes(""), &entry, &origin));
    assert(equal(entry.text, "external") && equal(origin.origin_locale, "en-1901-1996"));
    assert(qjs_intl_calendar_name_resolve(data, 0, bytes("1"), QJS_INTL_CAL_NAME_NO_YEAR,
                                         bytes(""), &entry, &origin) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_calendar_name_resolve(data, data->node_count + 1, bytes("1"), QJS_INTL_CAL_NAME_NO_YEAR,
                                         bytes(""), &entry, &origin) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_name_resolve(data, 0, (QJSIntlBytes){NULL, 1}, QJS_INTL_CAL_NAME_NO_YEAR,
                                         bytes(""), &entry, &origin) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_name_resolve(data, head, bytes("1"), (QJSIntlCalendarNameYear)-1,
                                         bytes(""), &entry, &origin) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_name_resolve(data, head, bytes("1"), QJS_INTL_CAL_NAME_NO_YEAR,
                                         (QJSIntlBytes){NULL, 1}, &entry, &origin) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_name_resolve(data, head, bytes("1"), QJS_INTL_CAL_NAME_NO_YEAR,
                                         bytes(""), NULL, &origin) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_name_resolve(data, head, bytes("1"), QJS_INTL_CAL_NAME_NO_YEAR,
                                         bytes(""), &entry, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

static void check_queries(const QJSIntlCalendarNameData *data)
{
    QJSIntlCalendarNameEntry entry;
    QJSIntlCalendarNameNode node;
    QJSIntlCalendarNameGroup group;
    uint32_t i, j, found, head = month_head(data, "en-GB", "generic");
    check_resolved_queries(data);
    assert(head);
    assert(qjs_intl_calendar_name_lookup(data, head, bytes("1"), QJS_INTL_CAL_NAME_NO_YEAR,
                                         bytes(""), &entry) == QJS_INTL_UNSUPPORTED);
    {
        QJSIntlCalendarNameEntry zero = {0};
        assert(!memcmp(&entry, &zero, sizeof(entry)));
    }
    assert(!qjs_intl_calendar_name_lookup(data, head, bytes("4"), QJS_INTL_CAL_NAME_NO_YEAR, bytes(""), &entry));
    assert(equal(entry.text, "own") && !entry.blocked);
    assert(!qjs_intl_calendar_name_node(data, head, &node) && equal(node.origin_locale, "en-GB"));
    head = node.parent;
    assert(!qjs_intl_calendar_name_lookup(data, head, bytes("1"), QJS_INTL_CAL_NAME_NO_YEAR,
                                         bytes("variant"), &entry));
    assert(equal(entry.text, "child"));
    assert(!qjs_intl_calendar_name_node(data, head, &node) && equal(node.origin_locale, "en"));
    head = node.parent;
    assert(!qjs_intl_calendar_name_node(data, head, &node) && equal(node.origin_locale, "root"));
    assert(!qjs_intl_calendar_name_group_at(data, node.group, &group));
    assert(group.calendar == calendar_id(data, "gregorian"));
    assert(!qjs_intl_calendar_name_lookup(data, head, bytes("1"), QJS_INTL_CAL_NAME_NO_YEAR, bytes(""), &entry));
    assert(equal(entry.text, "root"));
    assert(!qjs_intl_calendar_name_lookup(data, head, bytes("2"), QJS_INTL_CAL_NAME_NO_YEAR, bytes(""), &entry));
    assert(entry.blocked && !entry.text.length);
    assert(!qjs_intl_calendar_name_lookup(data, head, bytes("3"), QJS_INTL_CAL_NAME_NO_YEAR, bytes(""), &entry));
    assert(!entry.blocked && !entry.text.length);
    assert(!qjs_intl_calendar_name_lookup(data, head, bytes("7"), QJS_INTL_CAL_NAME_STANDARD_YEAR, bytes(""), &entry));
    assert(equal(entry.text, "ordinary"));
    assert(!qjs_intl_calendar_name_lookup(data, head, bytes("7"), QJS_INTL_CAL_NAME_LEAP_YEAR,
                                         bytes("variant"), &entry));
    assert(equal(entry.text, "leap\xf0\x9f\x98\x80"));
    head = month_head(data, "de", "generic");
    assert(!qjs_intl_calendar_name_node(data, head, &node) && equal(node.origin_locale, "en-1901-1996"));
    assert(!qjs_intl_calendar_name_lookup(data, head, bytes("5"), QJS_INTL_CAL_NAME_NO_YEAR, bytes(""), &entry));
    assert(equal(entry.text, "external"));
    for (i = 0; i < data->group_count; i++) {
        assert(!qjs_intl_calendar_name_group_at(data, i, &group));
        assert(!qjs_intl_calendar_name_find_group(data, &group, &found) && found == i);
        for (j = 0; j < data->section.count; j++) {
            assert(!qjs_intl_calendar_name_head(data, j, i, &head));
            if (head) {
                QJSIntlCalendarNameGroup origin;
                assert(!qjs_intl_calendar_name_node(data, head, &node));
                assert(!qjs_intl_calendar_name_group_at(data, node.group, &origin));
                assert(origin.family == group.family);
            }
        }
    }
    /* Every stored entry, including long raw era IDs, must be found exactly. */
    for (i = 1; i <= data->node_count; i++) {
        uint32_t start, count;
        assert(!qjs_intl_calendar_name_node(data, i, &node));
        start = qjs_intl_read_u32(data->maps + (size_t)node.map * 8);
        count = qjs_intl_read_u32(data->maps + (size_t)node.map * 8 + 4);
        for (j = 0; j < count; j++) {
            const uint8_t *field = data->entries + ((size_t)start + j) * 16;
            QJSIntlBytes code, alt, text;
            uint32_t value = qjs_intl_read_u32(field + 12);
            QJSIntlCalendarNameYear year = qjs_intl_read_u32(field + 4);
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field), &code));
            assert(!qjs_intl_data_string(&data->data, qjs_intl_read_u32(field + 8), &alt));
            assert(!qjs_intl_calendar_name_lookup(data, i, code, year, alt, &entry));
            assert(entry.year == year && entry.code.length == code.length);
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
    assert(qjs_intl_calendar_name_node(data, 0, &node) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_name_group_at(data, data->group_count, &group) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_name_head(data, data->section.count, 0, &head) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_name_lookup(data, 1, bytes("1"), (QJSIntlCalendarNameYear)-1,
                                         bytes(""), &entry) == QJS_INTL_INVALID_ARGUMENT);
    {
        QJSIntlBytes empty = {NULL, 0}, invalid = {NULL, 1};
        assert(qjs_intl_calendar_name_lookup(data, 1, empty, QJS_INTL_CAL_NAME_NO_YEAR,
                                             empty, &entry) == QJS_INTL_UNSUPPORTED);
        assert(qjs_intl_calendar_name_lookup(data, 1, invalid, QJS_INTL_CAL_NAME_NO_YEAR,
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
    QJSIntlCalendarNameData result, zero = {0};
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_calendar_name_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

static void corrupt(const QJSIntlCalendarNameData *owner, const QJSIntlAllocator *allocator,
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

static void duplicate(const QJSIntlCalendarNameData *owner, const QJSIntlAllocator *allocator,
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

static void check_corruption(const QJSIntlCalendarNameData *data, const QJSIntlAllocator *allocator)
{
    size_t i;
    uint32_t child = 0, day_node = 0;
    for (i = 0; i < 7; i++) {
        corrupt(data, allocator, data->section.bytes + i * 4, UINT32_MAX);
        corrupt(data, allocator, data->section.bytes + i * 4, 0);
    }
    corrupt(data, allocator, data->calendars, data->data.strings.count);
    corrupt(data, allocator, data->calendars, string_id(&data->data, "Generic"));
    corrupt(data, allocator, data->calendars + 4, qjs_intl_read_u32(data->calendars));
    corrupt(data, allocator, data->groups, data->calendar_count);
    corrupt(data, allocator, data->groups + 4, QJS_INTL_CAL_NAME_FAMILY_COUNT);
    corrupt(data, allocator, data->groups + 8, QJS_INTL_CAL_NAME_CYCLE_COUNT);
    corrupt(data, allocator, data->groups + 12, QJS_INTL_CAL_NAME_CONTEXT_COUNT);
    corrupt(data, allocator, data->groups + 16, QJS_INTL_CAL_NAME_WIDTH_COUNT);
    corrupt(data, allocator, data->groups + 8, QJS_INTL_CAL_NAME_NO_CYCLE);
    corrupt(data, allocator, data->locale_rows, data->row_count);
    corrupt(data, allocator, data->rows, data->node_count + 1);
    corrupt(data, allocator, data->maps, 1);
    corrupt(data, allocator, data->maps + 4, 0);
    corrupt(data, allocator, data->nodes, data->data.strings.count);
    corrupt(data, allocator, data->nodes + 4, data->group_count);
    corrupt(data, allocator, data->nodes + 8, data->map_count);
    corrupt(data, allocator, data->nodes + 12, 1);
    for (i = 0; i < data->node_count; i++) {
        QJSIntlCalendarNameNode node;
        QJSIntlCalendarNameGroup group;
        assert(!qjs_intl_calendar_name_node(data, (uint32_t)i + 1, &node));
        assert(!qjs_intl_calendar_name_group_at(data, node.group, &group));
        if (node.parent && !child)
            child = (uint32_t)i;
        if (!node.parent && group.family == QJS_INTL_CAL_NAME_DAY)
            day_node = (uint32_t)i + 1;
    }
    assert(child && day_node && day_node <= child);
    corrupt(data, allocator, data->nodes + (size_t)child * 16 + 12, day_node);
    {
        QJSIntlCalendarNameNode node;
        uint32_t month_group;
        QJSIntlCalendarNameGroup group = {0};
        group.calendar = calendar_id(data, "generic");
        group.family = QJS_INTL_CAL_NAME_MONTH;
        group.context = QJS_INTL_CAL_NAME_FORMAT;
        group.width = QJS_INTL_CAL_NAME_WIDE;
        assert(!qjs_intl_calendar_name_find_group(data, &group, &month_group));
        corrupt(data, allocator, data->rows + (size_t)month_group * 4, day_node);
        assert(!qjs_intl_calendar_name_node(data, day_node, &node));
        corrupt(data, allocator, data->nodes + (size_t)child * 16 + 8, node.map);
        corrupt(data, allocator, data->entries +
                (size_t)qjs_intl_read_u32(data->maps + (size_t)node.map * 8) * 16 + 4,
                QJS_INTL_CAL_NAME_STANDARD_YEAR);
    }
    for (i = 0; i < 5; i++) {
        static const char *const bad[] = {"", "En", "en-us", "bad_tag", "en-1901-1901"};
        corrupt(data, allocator, data->nodes, string_id(&data->data, bad[i]));
    }
    corrupt(data, allocator, data->entries, data->data.strings.count);
    corrupt(data, allocator, data->entries + 4, QJS_INTL_CAL_NAME_YEAR_COUNT);
    corrupt(data, allocator, data->entries + 8, data->data.strings.count);
    corrupt(data, allocator, data->entries + 12, data->data.strings.count);
    corrupt(data, allocator, data->entries + 12, string_id(&data->data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91"));
    corrupt(data, allocator, data->entries + 12, string_id(&data->data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"));
    corrupt(data, allocator, data->entries, string_id(&data->data, "bad!"));
    duplicate(data, allocator, data->groups + 20, 20);
    duplicate(data, allocator, data->nodes + 16, 16);
    duplicate(data, allocator, data->rows + (size_t)data->group_count * 4, (size_t)data->group_count * 4);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlCalendarNameData owner, result, zero = {0};
    size_t i, calls, successful;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_calendar_name_data_open(&data, &allocator, &owner) && !state.live);
    successful = state.calls;
    assert(successful > 1); /* Multivariant source-locale validation allocates. */
    calls = state.calls;
    check_queries(&owner);
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
    check_corruption(&owner, &allocator);
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_calendar_name_data_open(&data, &allocator, &result));
    check_queries(&result);
    free(unaligned);
    assert(!qjs_intl_data_open(calendar_name_empty_bytes, calendar_name_empty_length, &allocator, &data));
    assert(!qjs_intl_calendar_name_data_open(&data, &allocator, &result));
    assert(!result.map_count && !result.node_count && !result.entry_count && result.row_count == 1);
    for (i = 0; i < result.group_count; i++) {
        uint32_t head = 123;
        assert(!qjs_intl_calendar_name_head(&result, 0, (uint32_t)i, &head) && !head);
    }
    {
        QJSIntlCalendarNameGroup group;
        uint32_t index = 123;
        assert(result.group_count == 1);
        assert(!qjs_intl_calendar_name_group_at(&result, 0, &group));
        group.width = QJS_INTL_CAL_NAME_NARROW;
        assert(qjs_intl_calendar_name_find_group(&result, &group, &index) == QJS_INTL_UNSUPPORTED && !index);
        group.family = (QJSIntlCalendarNameFamily)-1;
        assert(qjs_intl_calendar_name_find_group(&result, &group, &index) == QJS_INTL_INVALID_ARGUMENT);
    }
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_calendar_name_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
    return 0;
}
