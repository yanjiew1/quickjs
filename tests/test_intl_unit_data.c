/* Native unit form views: sparse states, typed selectors, corruption and OOM.
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
#include "../src/intl/unit/data.h"
#include "../src/intl/unit/identifier.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdio.h>
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

/* Check the adapter's exact map against the complete prepared source inventory,
   including unknown/new matches. Suffix discovery is a test oracle, not the
   runtime mapping policy. Ambiguous legacy group prefixes must be reviewed. */
static void precomposed_inventory(const QJSIntlUnitData *data)
{
    uint32_t i, j, k, total = 0, count = qjs_intl_unit_definition_count();
    for (i = 0; i < count; i++) {
        for (j = 0; j < count; j++) {
            QJSIntlUnitIdentifier unit = {i, j};
            const char *mapped = qjs_intl_unit_identifier_cldr(&unit);
            char suffix[96];
            int length = snprintf(suffix, sizeof(suffix), "-%s-per-%s",
                                  qjs_intl_unit_definition_at(i)->identifier,
                                  qjs_intl_unit_definition_at(j)->identifier);
            unsigned int matches = 0;
            assert(length > 0 && (size_t)length < sizeof(suffix));
            for (k = 0; k < data->group_count; k++) {
                QJSIntlUnitGroup group;
                assert(!qjs_intl_unit_group_at(data, k, &group));
                if (group.kind == QJS_INTL_UNIT_GROUP_SINGLE && group.name.length > (size_t)length &&
                    !memcmp(group.name.data + group.name.length - length, suffix, length)) {
                    assert(mapped && equal(group.name, mapped));
                    matches++;
                }
            }
            assert(matches <= 1 && matches == !!mapped);
            total += matches;
        }
    }
    assert(total == 5);
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
    for (i = 0; i < 8; i++) {
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
    corrupt(allocator, owner->overlay_locales - data->pack.bytes, owner->overlay_row_count);
    corrupt(allocator, owner->overlay_rows - data->pack.bytes, owner->node_count + 1);
    corrupt(allocator, owner->nodes - data->pack.bytes, data->strings.count);
    corrupt(allocator, owner->nodes - data->pack.bytes + 4,
            owner->group_count * QJS_INTL_UNIT_WIDTH_COUNT);
    corrupt(allocator, owner->nodes - data->pack.bytes + 8, owner->map_count);
    corrupt(allocator, owner->nodes - data->pack.bytes + 12, 1);
    {
        uint32_t single = 0, compound = 0;
        for (i = 1; i <= owner->node_count; i++) {
            QJSIntlUnitOverlay overlay;
            QJSIntlUnitGroup group;
            assert(!qjs_intl_unit_overlay_at(owner, i, &overlay));
            assert(!qjs_intl_unit_group_at(owner, overlay.group, &group));
            if (group.kind == QJS_INTL_UNIT_GROUP_COMPOUND)
                compound = i;
            else
                single = i;
            if (single && compound)
                break;
        }
        assert(single && compound);
        corrupt(allocator, owner->overlay_rows - data->pack.bytes, single);
        corrupt(allocator, owner->nodes - data->pack.bytes +
                            (size_t)((single > compound ? single : compound) - 1) * 16 + 12,
                single < compound ? single : compound);
    }
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
    assert(owner->node_count > 1);
    copy = malloc(qjs_intl_native_length);
    assert(copy);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    memcpy(copy + (owner->nodes - data->pack.bytes) + 16, owner->nodes, 16);
    invalid_blob(allocator, copy, qjs_intl_native_length);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    memset(copy + (owner->overlay_rows - data->pack.bytes), 0,
           (size_t)owner->overlay_row_count * stride);
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
            if (equal(text, "en-1901-1901") || equal(text, "En") ||
                equal(text, "en-us") || equal(text, "bad_tag"))
                corrupt(allocator, owner->nodes - data->pack.bytes, i);
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

static void check_overlays(const QJSIntlUnitData *owner)
{
    QJSIntlUnitOverlay overlay;
    uint32_t id, head, previous;
    for (id = 1; id <= owner->node_count; id++) {
        assert(!qjs_intl_unit_overlay_at(owner, id, &overlay));
        assert(overlay.map.count && overlay.locale.length && overlay.next < id);
        assert((unsigned int)overlay.width < QJS_INTL_UNIT_WIDTH_COUNT);
        assert(overlay.group < owner->group_count);
    }
    for (id = 0; id < owner->data.locales.count; id++) {
        assert(!qjs_intl_unit_overlay_head(owner, id, QJS_INTL_UNIT_LONG, 0, &head));
        while (head) {
            previous = head;
            assert(!qjs_intl_unit_overlay_at(owner, head, &overlay));
            head = overlay.next;
            assert(head < previous);
        }
    }
    assert(qjs_intl_unit_overlay_at(owner, 0, &overlay) == QJS_INTL_INVALID_ARGUMENT);
    assert(!overlay.locale.data && !overlay.map.entries);
    assert(qjs_intl_unit_overlay_at(owner, owner->node_count + 1, &overlay) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_overlay_at(NULL, 1, &overlay) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_overlay_at(owner, 1, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_overlay_head(owner, owner->section.count, 0, 0, &head) ==
           QJS_INTL_INVALID_ARGUMENT && !head);
    assert(qjs_intl_unit_overlay_head(owner, 0, QJS_INTL_UNIT_WIDTH_COUNT, 0, &head) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_overlay_head(owner, 0, 0, owner->group_count, &head) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_overlay_head(NULL, 0, 0, 0, &head) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_overlay_head(owner, 0, 0, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
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
    if (!TEST_INTL_AUTHORED)
        precomposed_inventory(&owner);
    en = locale_id(&data, "en");
    meter = group_id(&owner, QJS_INTL_UNIT_GROUP_SINGLE, "length-meter");
    per = group_id(&owner, QJS_INTL_UNIT_GROUP_COMPOUND, "per");
    calls = state.calls;
    check_overlays(&owner);
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
        extern const uint8_t qjs_intl_unit_projected_bytes[];
        extern const size_t qjs_intl_unit_projected_length;
        extern const uint8_t qjs_intl_unit_empty_bytes[];
        extern const size_t qjs_intl_unit_empty_length;
        extern const uint8_t qjs_intl_unit_variant_bytes[];
        extern const size_t qjs_intl_unit_variant_length;
        QJSIntlUnitOverlay overlay;
        uint32_t head, depth = 0;
        const char *origins[] = {"en-GB", "en", "root"};
        assert(!qjs_intl_data_open(qjs_intl_unit_projected_bytes, qjs_intl_unit_projected_length,
                                   &allocator, &data));
        assert(!qjs_intl_unit_data_open(&data, &allocator, &owner));
        assert(data.locales.count == 2);
        meter = group_id(&owner, QJS_INTL_UNIT_GROUP_SINGLE, "length-meter");
        assert(!qjs_intl_unit_overlay_head(&owner, locale_id(&data, "de"), QJS_INTL_UNIT_NARROW,
                                           meter, &head));
        while (head) {
            assert(!qjs_intl_unit_overlay_at(&owner, head, &overlay));
            assert(depth < 3 && equal(overlay.locale, origins[depth]));
            assert(overlay.width == QJS_INTL_UNIT_LONG && overlay.group == meter);
            if (!depth) {
                assert(!qjs_intl_unit_form_find(&owner, &overlay.map, QJS_INTL_UNIT_PATTERN,
                                                QJS_INTL_UNIT_COUNT_OTHER, bytes(""), &form));
                assert(equal(form.text, "CHILD_OTHER {0}"));
            }
            depth++;
            head = overlay.next;
        }
        assert(depth == 3);
        assert(!qjs_intl_data_open(qjs_intl_unit_empty_bytes, qjs_intl_unit_empty_length,
                                   &allocator, &data));
        assert(!qjs_intl_unit_data_open(&data, &allocator, &owner));
        assert(!owner.entry_count && owner.case_count == 1 && owner.map_count == 1);
        assert(!owner.node_count);
        assert(!qjs_intl_unit_overlay_head(&owner, 0, 0, 0, &head) && !head);
        assert(!qjs_intl_unit_map_at(&owner, 0, 0, 0, &map) && !map.count);
        assert(qjs_intl_unit_form_find(&owner, &map, QJS_INTL_UNIT_COMPOUND_PATTERN,
                                       QJS_INTL_UNIT_COUNT_NONE, (QJSIntlBytes){0},
                                       &form) == QJS_INTL_UNSUPPORTED);
        assert(qjs_intl_unit_form_at(&owner, &map, 0, &form) == QJS_INTL_INVALID_ARGUMENT);
        assert(!qjs_intl_data_open(qjs_intl_unit_variant_bytes, qjs_intl_unit_variant_length,
                                   &allocator, &data));
        calls = state.calls;
        assert(!qjs_intl_unit_data_open(&data, &allocator, &owner));
        calls = state.calls - calls;
        assert(calls == 2 && !state.live); /* Owner scratch and nested variant scratch. */
        for (i = 1; i <= calls; i++) {
            state.fail_at = state.calls + i;
            memset(&retry, 0xff, sizeof(retry));
            assert(qjs_intl_unit_data_open(&data, &allocator, &retry) == QJS_INTL_NO_MEMORY);
            assert(!retry.section.bytes && !retry.nodes && !state.live);
        }
        state.fail_at = 0;
    }
#endif
    assert(!state.live);
    free(unaligned);
    return 0;
}
