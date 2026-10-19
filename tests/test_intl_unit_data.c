/* Native unit form views: sparse states, typed selectors, corruption and OOM.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/unit/data.h"
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
    return (QJSIntlBytes){text, strlen(text)};
}

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) &&
           (!text.length || !memcmp(text.data, expected, text.length));
}

static uint32_t locale_id(const QJSIntlData *data, const char *expected)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes tag;
        assert(!qjs_intl_data_locale(data, i, &tag, &parent));
        if (equal(tag, expected))
            return i;
    }
    abort();
}

static uint32_t group_id(const QJSIntlUnitData *data, QJSIntlUnitGroupKind kind, const char *name)
{
    uint32_t index;
    assert(!qjs_intl_unit_group_find(data, kind, bytes(name), &index));
    return index;
}

static QJSIntlUnitForm lookup(const QJSIntlUnitData *data, uint32_t locale, QJSIntlUnitWidth width,
                              uint32_t group, QJSIntlUnitField field, QJSIntlUnitCount count,
                              const char *case_name)
{
    QJSIntlUnitMap map;
    QJSIntlUnitForm result;
    assert(!qjs_intl_unit_map_at(data, locale, width, group, &map));
    assert(!qjs_intl_unit_form_find(data, &map, field, count, bytes(case_name), &result));
    return result;
}

static void put_u32(uint8_t *ptr, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        ptr[i] = value >> (8 * i);
}

static void invalid_blob(const QJSIntlAllocator *allocator, const uint8_t *copy, size_t length)
{
    QJSIntlData data;
    QJSIntlUnitData result, zero = {0};
    assert(!qjs_intl_data_open(copy, length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_unit_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

static void corrupt(const QJSIntlAllocator *allocator, size_t offset, uint32_t value)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    invalid_blob(allocator, copy, qjs_intl_native_length);
    free(copy);
}

static void check_corruption(const QJSIntlUnitData *owner, const QJSIntlAllocator *allocator)
{
    const QJSIntlData *data = &owner->data;
    size_t header = owner->section.bytes - data->pack.bytes;
    size_t entries = owner->entries - data->pack.bytes;
    size_t rows = owner->rows - data->pack.bytes;
    size_t i, last = 0, stride = (size_t)owner->group_count * QJS_INTL_UNIT_WIDTH_COUNT * 4;
    uint8_t *copy;
    for (i = 0; i < 6; i++) {
        corrupt(allocator, header + i * 4, 0);
        corrupt(allocator, header + i * 4, UINT32_MAX);
    }
    corrupt(allocator, owner->groups - data->pack.bytes, QJS_INTL_UNIT_GROUP_SINGLE);
    corrupt(allocator, owner->groups - data->pack.bytes + 4, 0);
    corrupt(allocator, owner->groups - data->pack.bytes + 4, data->strings.count);
    corrupt(allocator, owner->cases - data->pack.bytes, data->strings.count);
    corrupt(allocator, owner->locale_rows - data->pack.bytes, owner->row_count);
    corrupt(allocator, rows, owner->map_count);
    corrupt(allocator, owner->maps - data->pack.bytes, 1);
    corrupt(allocator, owner->maps - data->pack.bytes + 4, owner->entry_count + 1);
    corrupt(allocator, entries, QJS_INTL_UNIT_FIELD_COUNT << 24);
    corrupt(allocator, entries, QJS_INTL_UNIT_COUNT_COUNT << 16);
    corrupt(allocator, entries, owner->case_count);
    corrupt(allocator, entries + 4, data->strings.count);
    corrupt(allocator, entries + 4, UINT32_MAX);
    copy = malloc(qjs_intl_native_length);
    assert(copy && owner->row_count > 1);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    memcpy(copy + rows + stride, copy + rows, stride);
    invalid_blob(allocator, copy, qjs_intl_native_length);
    free(copy);
    for (i = 0; i < owner->entry_count; i++) {
        if (qjs_intl_read_u32(owner->entries + i * 8 + 4) < data->strings.count)
            last = i;
    }
    if (TEST_INTL_AUTHORED) {
        for (i = 0; i < data->strings.count; i++) {
            QJSIntlBytes text;
            assert(!qjs_intl_data_string(data, i, &text));
            if (equal(text, "↑↑↑") || equal(text, "∅∅∅"))
                corrupt(allocator, entries + last * 8 + 4, i);
        }
    }
    for (i = 0; i < data->pack.section_count; i++) {
        size_t directory = 64 + i * 16;
        if (qjs_intl_read_u32(data->pack.bytes + directory) == QJS_INTL_SECTION_UNIT_PATTERNS) {
            size_t cut = header + owner->section.length - 4, j;
            copy = malloc(qjs_intl_native_length);
            assert(copy);
            corrupt(allocator, directory + 12, owner->section.count + 1);
            if (TEST_INTL_AUTHORED) {
                assert(i + 1 == data->pack.section_count);
                corrupt(allocator, directory, QJS_INTL_SECTION_UNIT_PATTERNS + 1);
            }
            memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
            memmove(copy + cut, copy + cut + 4, qjs_intl_native_length - cut - 4);
            put_u32(copy + 8, qjs_intl_native_length - 4);
            put_u32(copy + directory + 8, owner->section.length - 4);
            for (j = i + 1; j < data->pack.section_count; j++) {
                size_t next = 64 + j * 16 + 4;
                put_u32(copy + next, qjs_intl_read_u32(copy + next) - 4);
            }
            invalid_blob(allocator, copy, qjs_intl_native_length - 4);
            free(copy);
        }
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlUnitData owner, retry;
    QJSIntlUnitMap map;
    QJSIntlUnitForm form;
    QJSIntlUnitGroup group;
    uint32_t en, meter, per, i, index;
    size_t calls;
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_unit_data_open(&data, &allocator, &owner));
    assert(!state.live && owner.group_count == (TEST_INTL_AUTHORED ? 3 : 239));
    en = locale_id(&data, "en");
    meter = group_id(&owner, QJS_INTL_UNIT_GROUP_SINGLE, "length-meter");
    per = group_id(&owner, QJS_INTL_UNIT_GROUP_COMPOUND, "per");
    calls = state.calls;
    for (i = 0; i < owner.group_count; i++) {
        assert(!qjs_intl_unit_group_at(&owner, i, &group));
        assert(!qjs_intl_unit_group_find(&owner, group.kind, group.name, &index) && index == i);
    }
    form = lookup(&owner, en, QJS_INTL_UNIT_LONG, meter, QJS_INTL_UNIT_PATTERN,
                  QJS_INTL_UNIT_COUNT_ONE, "");
    assert(form.state == QJS_INTL_UNIT_FORM_VALUE && equal(form.text, "{0} meter"));
    form = lookup(&owner, en, QJS_INTL_UNIT_LONG, per, QJS_INTL_UNIT_COMPOUND_PATTERN,
                  QJS_INTL_UNIT_COUNT_NONE, "");
    assert(equal(form.text, TEST_INTL_AUTHORED ? "{0}/{1}" : "{0} per {1}"));
    if (TEST_INTL_AUTHORED) {
        form = lookup(&owner, en, QJS_INTL_UNIT_LONG, meter, QJS_INTL_UNIT_PATTERN,
                      QJS_INTL_UNIT_COUNT_ONE, "genitive");
        assert(form.state == QJS_INTL_UNIT_FORM_BLOCKED && !form.text.data);
        form = lookup(&owner, en, QJS_INTL_UNIT_LONG, meter, QJS_INTL_UNIT_PATTERN,
                      QJS_INTL_UNIT_COUNT_TWO, "");
        assert(form.state == QJS_INTL_UNIT_FORM_VALUE && !form.text.length);
        form = lookup(&owner, en, QJS_INTL_UNIT_LONG, meter, QJS_INTL_UNIT_GENDER,
                      QJS_INTL_UNIT_COUNT_NONE, "");
        assert(equal(form.text, "common"));
        form = lookup(&owner, en, QJS_INTL_UNIT_NARROW, meter, QJS_INTL_UNIT_PATTERN,
                      QJS_INTL_UNIT_COUNT_OTHER, "");
        assert(equal(form.text, "{0} mt"));
        form = lookup(&owner, en, QJS_INTL_UNIT_LONG, per, QJS_INTL_UNIT_COMPOUND_PATTERN,
                      QJS_INTL_UNIT_COUNT_NONE, "genitive");
        assert(equal(form.text, "from {0}/{1}"));
    } else if (TEST_INTL_PROFILE) {
        assert(owner.case_count == 17);
        form = lookup(&owner, locale_id(&data, "fr"), QJS_INTL_UNIT_LONG, meter,
                      QJS_INTL_UNIT_GENDER, QJS_INTL_UNIT_COUNT_NONE, "");
        assert(equal(form.text, "masculine"));
    }
    assert(!qjs_intl_unit_map_at(&owner, en, QJS_INTL_UNIT_LONG, meter, &map));
    for (i = 0; i < map.count; i++) {
        QJSIntlUnitForm found;
        assert(!qjs_intl_unit_form_at(&owner, &map, i, &form));
        assert(
            !qjs_intl_unit_form_find(&owner, &map, form.field, form.count, form.case_name, &found));
        assert(!memcmp(&form, &found, sizeof(form)));
    }
    assert(!qjs_intl_unit_form_find(&owner, &map, QJS_INTL_UNIT_PATTERN, QJS_INTL_UNIT_COUNT_ONE,
                                    (QJSIntlBytes){0}, &form));
    assert(qjs_intl_unit_form_find(&owner, &map, QJS_INTL_UNIT_PATTERN, QJS_INTL_UNIT_COUNT_ONE,
                                   bytes("unknowncase"), &form) == QJS_INTL_UNSUPPORTED);
    assert(!form.text.data && !form.case_name.data);
    assert(qjs_intl_unit_group_find(&owner, QJS_INTL_UNIT_GROUP_SINGLE, bytes("missing-unit"),
                                    &index) == QJS_INTL_UNSUPPORTED &&
           !index);
    assert(qjs_intl_unit_group_at(&owner, owner.group_count, &group) == QJS_INTL_INVALID_ARGUMENT);
    assert(!group.name.data);
    assert(qjs_intl_unit_group_find(&owner, (QJSIntlUnitGroupKind)-1, bytes("length-meter"),
                                    &index) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_group_find(&owner, QJS_INTL_UNIT_GROUP_SINGLE, bytes("Bad"), &index) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_group_find(NULL, QJS_INTL_UNIT_GROUP_SINGLE, bytes("length-meter"),
                                    &index) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_group_find(&owner, QJS_INTL_UNIT_GROUP_SINGLE, bytes("length-meter"),
                                    NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_group_at(NULL, 0, &group) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_group_at(&owner, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_map_at(&owner, data.locales.count, 0, meter, &map) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!map.entries && !map.count);
    assert(qjs_intl_unit_map_at(&owner, en, (QJSIntlUnitWidth)-1, meter, &map) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_map_at(&owner, en, 0, owner.group_count, &map) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_map_at(NULL, en, 0, meter, &map) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_map_at(&owner, en, 0, meter, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_unit_map_at(&owner, en, 0, meter, &map));
    assert(qjs_intl_unit_form_at(&owner, &map, map.count, &form) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_form_find(&owner, &map, QJS_INTL_UNIT_PATTERN, QJS_INTL_UNIT_COUNT_NONE,
                                   (QJSIntlBytes){0}, &form) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_form_find(&owner, &map, QJS_INTL_UNIT_GENDER, QJS_INTL_UNIT_COUNT_NONE,
                                   bytes("genitive"), &form) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_form_find(&owner, &map, QJS_INTL_UNIT_PATTERN, QJS_INTL_UNIT_COUNT_ONE,
                                   (QJSIntlBytes){NULL, 2}, &form) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_form_find(NULL, &map, 0, 0, (QJSIntlBytes){0}, &form) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_form_find(&owner, NULL, 0, 0, (QJSIntlBytes){0}, &form) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_form_find(&owner, &map, 0, 0, (QJSIntlBytes){0}, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_form_at(NULL, &map, 0, &form) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_form_at(&owner, NULL, 0, &form) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_form_at(&owner, &map, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls);
    state.fail_at = state.calls + 1;
    assert(qjs_intl_unit_data_open(&data, &allocator, &retry) == QJS_INTL_NO_MEMORY);
    assert(!retry.section.bytes && !state.live);
    state.fail_at = 0;
    assert(!qjs_intl_unit_data_open(&data, &allocator, &retry));
    assert(qjs_intl_unit_data_open(NULL, &allocator, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(!retry.section.bytes);
    assert(qjs_intl_unit_data_open(&data, NULL, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_data_open(&data, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_unit_data_open(&data, &allocator, &owner));
    check_corruption(&owner, &allocator);
#if TEST_INTL_AUTHORED
    {
        extern const uint8_t qjs_intl_unit_empty_bytes[];
        extern const size_t qjs_intl_unit_empty_length;
        assert(!qjs_intl_data_open(qjs_intl_unit_empty_bytes, qjs_intl_unit_empty_length,
                                   &allocator, &data));
        assert(!qjs_intl_unit_data_open(&data, &allocator, &owner));
        assert(!owner.entry_count && owner.case_count == 1 && owner.map_count == 1);
        assert(!qjs_intl_unit_map_at(&owner, 0, 0, 0, &map) && !map.count);
        assert(qjs_intl_unit_form_find(&owner, &map, QJS_INTL_UNIT_COMPOUND_PATTERN,
                                       QJS_INTL_UNIT_COUNT_NONE, (QJSIntlBytes){0},
                                       &form) == QJS_INTL_UNSUPPORTED);
        assert(qjs_intl_unit_form_at(&owner, &map, 0, &form) == QJS_INTL_INVALID_ARGUMENT);
    }
#endif
    assert(!state.live);
    free(unaligned);
    return 0;
}
