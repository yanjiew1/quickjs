/* Layered name ownership, exact queries, corruptions and allocation failures.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/display/data.h"
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

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) &&
           (!text.length || !memcmp(text.data, expected, text.length));
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

#if !TEST_INTL_PREPARED
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

static void put_u32(uint8_t *at, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        at[i] = value >> (i * 8);
}

static void corrupt(const QJSIntlNameData *owner, const QJSIntlAllocator *allocator,
                    const uint8_t *at, uint32_t value)
{
    QJSIntlData data;
    QJSIntlNameData result, zero = {0};
    uint8_t *copy = malloc(qjs_intl_native_length);
    size_t offset = at - owner->data.pack.bytes;
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_name_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
    free(copy);
}

static void check_corruption(const QJSIntlNameData *owner, const QJSIntlAllocator *allocator)
{
    size_t i;
    for (i = 0; i < 6; i++)
        corrupt(owner, allocator, owner->section.bytes + i * 4, UINT32_MAX);
    corrupt(owner, allocator, owner->section.bytes, 0);
    corrupt(owner, allocator, owner->section.bytes + 4, 0);
    corrupt(owner, allocator, owner->locale_rows, owner->row_count);
    corrupt(owner, allocator, owner->rows, owner->node_count + 1);
#if !TEST_INTL_EMPTY
    corrupt(owner, allocator, owner->maps, 1);
    corrupt(owner, allocator, owner->maps + 4, 0);
    corrupt(owner, allocator, owner->maps + 4, owner->entry_count + 1);
    corrupt(owner, allocator, owner->nodes, owner->data.strings.count);
    corrupt(owner, allocator, owner->nodes + 4, QJS_INTL_NAME_GROUP_COUNT);
    corrupt(owner, allocator, owner->nodes + 8, owner->map_count);
    corrupt(owner, allocator, owner->nodes + 12, 1);
    for (i = 0; i < 5; i++) {
        static const char *const invalid[] = {"", "En", "en-us", "bad_tag", "en-1901-1901"};
        corrupt(owner, allocator, owner->nodes, string_id(&owner->data, invalid[i]));
    }
    for (i = 0; i < 6; i++)
        corrupt(owner, allocator, owner->entries + i * 4, owner->data.strings.count);
    corrupt(owner, allocator, owner->entries, string_id(&owner->data, "WRONG"));
    corrupt(owner, allocator, owner->entries + 20,
            string_id(&owner->data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91"));
    corrupt(owner, allocator, owner->entries + 20,
            string_id(&owner->data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"));
    corrupt(owner, allocator, owner->keys, QJS_INTL_NAME_GROUP_COUNT);
    corrupt(owner, allocator, owner->keys + 4, owner->data.strings.count);
    /* The final bracket group has one key. Another valid pooled string keeps
       inventory ordering valid, but no longer covers the actual bracket. */
    corrupt(owner, allocator, owner->keys + ((size_t)owner->key_count - 1) * 8 + 4,
            string_id(&owner->data, "WRONG"));
    /* The root siblings have no parents. A valid but earlier source locale
       makes the second sibling noncanonical without breaking graph bounds. */
    assert(owner->node_count > 1 && !qjs_intl_read_u32(owner->nodes + 12) &&
           !qjs_intl_read_u32(owner->nodes + 28));
    corrupt(owner, allocator, owner->nodes + 16, string_id(&owner->data, "en-GB"));
    /* A well-bounded head must still belong to the row's category. */
    for (i = 0; i < (size_t)owner->row_count * QJS_INTL_NAME_GROUP_COUNT; i++) {
        uint32_t head = qjs_intl_read_u32(owner->rows + i * 4);
        if (head) {
            size_t other = i / QJS_INTL_NAME_GROUP_COUNT * QJS_INTL_NAME_GROUP_COUNT +
                           (i + 1) % QJS_INTL_NAME_GROUP_COUNT;
            corrupt(owner, allocator, owner->rows + other * 4, head);
            break;
        }
    }
#else
    (void)string_id; /* The empty profile still carries corruption strings. */
#endif
}
#endif

static void check_queries(const QJSIntlNameData *owner)
{
    QJSIntlNameEntry entry, zero_entry = {0};
    QJSIntlNameNode node, zero_node = {0};
    QJSIntlBytes text, empty = {0};
    QJSIntlNameGroup group;
    uint32_t locale, head, index;
    for (locale = 0; locale < owner->section.count; locale++) {
        for (group = 0; group < QJS_INTL_NAME_GROUP_COUNT; group++) {
            uint32_t depth = 0;
            assert(!qjs_intl_name_head(owner, locale, group, &head));
            while (head) {
                assert(++depth <= owner->node_count);
                assert(!qjs_intl_name_node(owner, head, &node));
                assert(node.group == group && node.parent < head && node.origin_locale.length);
                head = node.parent;
            }
        }
    }
    for (index = 0; index < owner->key_count; index++) {
        assert(!qjs_intl_name_key_at(owner, index, &group, &text));
        assert(group < QJS_INTL_NAME_GROUP_COUNT && text.length);
    }
#if TEST_INTL_PREPARED
    assert(owner->node_count && owner->map_count && owner->entry_count && owner->key_count);
    assert(!qjs_intl_name_head(owner, locale_id(&owner->data, "en"), QJS_INTL_NAME_LANGUAGE, &head));
    assert(!qjs_intl_name_lookup(owner, head, bytes("en"), empty, empty, empty, &entry));
    assert(!entry.blocked && equal(entry.text, "English"));
#elif TEST_INTL_EMPTY
    assert(!owner->node_count && !owner->map_count && !owner->entry_count && !owner->key_count);
    assert(!qjs_intl_name_head(owner, locale_id(&owner->data, "en"), QJS_INTL_NAME_LANGUAGE, &head));
    assert(!head);
#else
    locale = locale_id(&owner->data, "en");
    assert(!qjs_intl_name_head(owner, locale, QJS_INTL_NAME_LANGUAGE, &head));
    assert(!qjs_intl_name_lookup(owner, head, bytes("zh-hant"), bytes("short"), empty, empty, &entry));
    assert(equal(entry.normalized, "zh-hant") && equal(entry.raw_code, "ZH_hant"));
    assert(equal(entry.alt, "short") && equal(entry.text, "short") && !entry.blocked);
    assert(!qjs_intl_name_lookup(owner, head, bytes("zh-hant"), empty, empty, empty, &entry));
    assert(equal(entry.raw_code, "zh_Hant") && equal(entry.text, "default"));
    assert(!qjs_intl_name_lookup(owner, head, bytes("en"), empty, bytes("core"), empty, &entry));
    assert(equal(entry.menu, "core") && equal(entry.text, "English core"));
    assert(!qjs_intl_name_lookup(owner, head, bytes("fr"), empty, empty, empty, &entry));
    assert(!entry.blocked && !entry.text.length && equal(entry.raw_code, "fr"));
    assert(!qjs_intl_name_lookup(owner, head, bytes("de"), empty, empty, empty, &entry));
    assert(entry.blocked && !entry.text.length);
    assert(qjs_intl_name_lookup(owner, head, bytes("absent"), empty, empty, empty, &entry) ==
           QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&entry, &zero_entry, sizeof(entry)));
    /* Lookup neither normalizes callers nor silently walks parent layers. */
    assert(qjs_intl_name_lookup(owner, head, bytes("ZH_hant"), empty, empty, empty, &entry) ==
           QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_name_lookup(owner, head, bytes("ca-valencia"), empty, empty, empty, &entry) ==
           QJS_INTL_UNSUPPORTED);
    assert(!qjs_intl_name_node(owner, head, &node) && equal(node.origin_locale, "en"));
    assert(!qjs_intl_name_lookup(owner, node.parent, bytes("ca-valencia"), empty, empty, empty, &entry));
    assert(equal(entry.raw_code, "ca_VALENCIA") && equal(entry.text, "Valencian"));
    assert(!qjs_intl_name_head(owner, locale, QJS_INTL_NAME_CALENDAR, &head));
    assert(!qjs_intl_name_lookup(owner, head, bytes("gregorian"), empty, empty, bytes("core"), &entry));
    assert(equal(entry.scope, "core") && equal(entry.text, "Gregorian"));
    assert(!qjs_intl_name_head(owner, locale_id(&owner->data, "de"), QJS_INTL_NAME_REGION, &head));
    assert(!qjs_intl_name_node(owner, head, &node) && equal(node.origin_locale, "en-1901-1996"));
    assert(!qjs_intl_name_lookup(owner, head, bytes("us"), empty, empty, empty, &entry));
    assert(equal(entry.raw_code, "US") && equal(entry.text, "external region"));
    assert(!qjs_intl_name_head(owner, locale_id(&owner->data, "en-GB"), QJS_INTL_NAME_LANGUAGE, &head));
    assert(!qjs_intl_name_node(owner, head, &node) && equal(node.origin_locale, "en-GB"));
    assert(!qjs_intl_name_lookup(owner, head, bytes("en"), empty, empty, empty, &entry));
    assert(equal(entry.text, "English")); /* Equal child text retains its origin. */
    assert(!qjs_intl_name_head(owner, locale, QJS_INTL_NAME_PATTERN, &head));
    assert(!qjs_intl_name_lookup(owner, head, bytes("localePattern"), empty, empty, empty, &entry));
    assert(equal(entry.text, "{0} ({1})"));
    assert(!qjs_intl_name_head(owner, locale, QJS_INTL_NAME_BRACKET, &head));
    assert(!qjs_intl_name_lookup(owner, head, bytes("("), empty, empty, empty, &entry));
    assert(equal(entry.text, "["));
    {
        QJSIntlBytes invalid = {NULL, 1};
        assert(qjs_intl_name_lookup(owner, head, invalid, empty, empty, empty, &entry) ==
               QJS_INTL_INVALID_ARGUMENT);
        assert(qjs_intl_name_lookup(owner, head, bytes("("), invalid, empty, empty, &entry) ==
               QJS_INTL_INVALID_ARGUMENT);
        assert(qjs_intl_name_lookup(owner, head, bytes("("), empty, invalid, empty, &entry) ==
               QJS_INTL_INVALID_ARGUMENT);
        assert(qjs_intl_name_lookup(owner, head, bytes("("), empty, empty, invalid, &entry) ==
               QJS_INTL_INVALID_ARGUMENT);
    }
#endif
    head = UINT32_MAX;
    assert(qjs_intl_name_head(owner, UINT32_MAX, 0, &head) == QJS_INTL_INVALID_ARGUMENT && !head);
    assert(qjs_intl_name_head(owner, 0, -1, &head) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_head(NULL, 0, 0, &head) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_head(owner, 0, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    memset(&node, 0xff, sizeof(node));
    assert(qjs_intl_name_node(owner, 0, &node) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&node, &zero_node, sizeof(node)));
    assert(qjs_intl_name_node(owner, owner->node_count + 1, &node) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_node(NULL, 1, &node) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_node(owner, 1, NULL) == QJS_INTL_INVALID_ARGUMENT);
    memset(&entry, 0xff, sizeof(entry));
    assert(qjs_intl_name_lookup(owner, 0, empty, empty, empty, empty, &entry) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&entry, &zero_entry, sizeof(entry)));
    assert(qjs_intl_name_lookup(NULL, 1, empty, empty, empty, empty, &entry) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_lookup(owner, 1, empty, empty, empty, empty, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_key_at(owner, owner->key_count, &group, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&text, &empty, sizeof(text)));
    assert(qjs_intl_name_key_at(owner, 0, NULL, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_key_at(owner, 0, &group, NULL) == QJS_INTL_INVALID_ARGUMENT);
    (void)bytes;
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data, unaligned_data;
    QJSIntlNameData owner, result, zero = {0};
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    size_t calls, step;
    assert(unaligned && !qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                                           &allocator, &data) && !state.live);
    state.calls = 0;
    assert(!qjs_intl_name_data_open(&data, &allocator, &owner) && !state.live);
    calls = state.calls;
    assert(calls);
    check_queries(&owner);
    assert(state.calls == calls);
#if !TEST_INTL_PREPARED
    check_corruption(&owner, &allocator);
#endif
    for (step = 1; step <= calls; step++) {
        state.calls = 0;
        state.fail_at = step;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_name_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
        state.fail_at = 0;
        assert(!qjs_intl_name_data_open(&data, &allocator, &result) && !state.live);
    }
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &unaligned_data));
    assert(!qjs_intl_name_data_open(&unaligned_data, &allocator, &result));
    check_queries(&result);
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_name_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_name_data_open(&data, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_name_data_open(&data, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    free(unaligned);
    assert(!state.live);
    return 0;
}
