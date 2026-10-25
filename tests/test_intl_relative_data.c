/* Resolved relative facts: sparse selectors, provenance, corruption and OOM.
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
#include "../src/intl/relative/data.h"
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

static uint32_t string_id(const QJSIntlData *data, const char *text)
{
    uint32_t i;
    for (i = 0; i < data->strings.count; i++) {
        QJSIntlBytes candidate;
        assert(!qjs_intl_data_string(data, i, &candidate));
        if (equal(candidate, text))
            return i;
    }
    abort();
}

static uint32_t locale_id(const QJSIntlData *data, const char *text)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes candidate;
        assert(!qjs_intl_data_locale(data, i, &candidate, &parent));
        if (equal(candidate, text))
            return i;
    }
    abort();
}

static void put_u32(uint8_t *ptr, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        ptr[i] = value >> (8 * i);
}

static void corrupt(const QJSIntlRelativeData *owner, const QJSIntlAllocator *allocator,
                    const uint8_t *at, uint32_t value)
{
    QJSIntlData data;
    QJSIntlRelativeData result, zero = {0};
    uint8_t *copy = malloc(qjs_intl_native_length);
    size_t offset = at - owner->data.pack.bytes;
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_relative_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
    free(copy);
}

static void check_corruption(const QJSIntlRelativeData *owner, const QJSIntlAllocator *allocator)
{
    size_t i;
    const uint8_t *numeric = NULL;
    for (i = 0; i < 5; i++)
        corrupt(owner, allocator, owner->section.bytes + i * 4, UINT32_MAX);
    corrupt(owner, allocator, owner->section.bytes, 0);
    corrupt(owner, allocator, owner->section.bytes + 8, 0);
    corrupt(owner, allocator, owner->section.bytes + 12, 0);
    corrupt(owner, allocator, owner->locale_rows, owner->row_count);
    corrupt(owner, allocator, owner->rows, owner->map_count);
    corrupt(owner, allocator, owner->maps, 1);
    corrupt(owner, allocator, owner->maps + 4, owner->entry_count + 1);
    corrupt(owner, allocator, owner->entries, 12 + owner->key_count);
    corrupt(owner, allocator, owner->entries + 4, owner->data.strings.count);
    corrupt(owner, allocator, owner->entries + 8, owner->data.strings.count);
    corrupt(owner, allocator, owner->entries + 12, 24);
    for (i = 0; i < owner->entry_count; i++) {
        const uint8_t *entry = owner->entries + i * 16;
        if (qjs_intl_read_u32(entry) < 12) {
            numeric = entry;
            break;
        }
    }
    assert(numeric);
    if (TEST_INTL_AUTHORED) {
        const char *bad_keys[] = {"-0", "9007199254740993", "-9007199254740993"};
        const char *bad_origins[] = {"En", "en-us", "bad_tag", "en-1901-1901"};
        const char *bad_texts[] = {"\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91",
                                   "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85", "invalid {1}"};
        assert(owner->key_count);
        for (i = 0; i < sizeof(bad_keys) / sizeof(*bad_keys); i++)
            corrupt(owner, allocator, owner->keys, string_id(&owner->data, bad_keys[i]));
        for (i = 0; i < sizeof(bad_origins) / sizeof(*bad_origins); i++)
            corrupt(owner, allocator, numeric + 8, string_id(&owner->data, bad_origins[i]));
        for (i = 0; i < sizeof(bad_texts) / sizeof(*bad_texts); i++)
            corrupt(owner, allocator, numeric + 4, string_id(&owner->data, bad_texts[i]));
    }
    {
        QJSIntlRelativeMap map;
        assert(!qjs_intl_relative_map_at(owner, 0, QJS_INTL_RELATIVE_SECOND, QJS_INTL_RELATIVE_LONG, &map));
        for (i = 0; i < map.count; i++) {
            const uint8_t *entry = map.entries + i * 16;
            if (qjs_intl_read_u32(entry) == QJS_INTL_PLURAL_OTHER) {
                corrupt(owner, allocator, entry + 4, UINT32_C(0xfffffffe));
                break;
            }
        }
        assert(i < map.count);
    }
}

static void check_queries(const QJSIntlRelativeData *owner, Allocation *state)
{
    uint32_t locale;
    unsigned int unit, width, tense;
    size_t before = state->calls;
    QJSIntlRelativeMap map, map_zero = {0};
    QJSIntlRelativeForm form, form_zero = {0};
    for (locale = 0; locale < owner->section.count; locale++) {
        for (unit = 0; unit < QJS_INTL_RELATIVE_UNIT_COUNT; unit++) {
            for (width = 0; width < QJS_INTL_RELATIVE_WIDTH_COUNT; width++) {
                assert(!qjs_intl_relative_map_at(owner, locale, unit, width, &map));
                for (tense = 0; tense < 2; tense++) {
                    QJSIntlStatus status = qjs_intl_relative_pattern_find(owner, &map, tense,
                                                                          QJS_INTL_PLURAL_OTHER, &form);
                    if (!width)
                        assert(!status && !form.blocked);
                    else
                        assert(status == QJS_INTL_OK || status == QJS_INTL_UNSUPPORTED);
                }
            }
        }
    }
    assert(qjs_intl_relative_map_at(owner, locale, 0, 0, &map) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&map, &map_zero, sizeof(map)));
    locale = locale_id(&owner->data, TEST_INTL_AUTHORED ? "de" : "en");
    assert(!qjs_intl_relative_map_at(owner, locale, QJS_INTL_RELATIVE_DAY, QJS_INTL_RELATIVE_NARROW, &map));
    assert(qjs_intl_relative_pattern_find(owner, &map, 2, 0, &form) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&form, &form_zero, sizeof(form)));
    assert(qjs_intl_relative_literal_find(owner, &map, bytes("1.5"), &form) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&form, &form_zero, sizeof(form)));
    assert(qjs_intl_relative_literal_find(owner, &map, (QJSIntlBytes){NULL, 0}, &form) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_relative_literal_find(owner, &map, (QJSIntlBytes){NULL, 1}, &form) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&form, &form_zero, sizeof(form)));
    if (TEST_INTL_AUTHORED) {
        const char *keys[] = {"42", "9007199254740992", "-9007199254740992"};
        size_t i;
        for (i = 0; i < sizeof(keys) / sizeof(*keys); i++) {
            assert(!qjs_intl_relative_literal_find(owner, &map, bytes(keys[i]), &form));
            assert(!form.blocked && equal(form.origin_locale, "en-1901-1996"));
            assert(form.origin_unit == QJS_INTL_RELATIVE_DAY && form.origin_width == QJS_INTL_RELATIVE_LONG);
        }
        assert(!qjs_intl_relative_literal_find(owner, &map, bytes("0"), &form));
        assert(equal(form.text, "today {literal}") && equal(form.origin_locale, "en"));
        assert(!qjs_intl_relative_literal_find(owner, &map, bytes("1"), &form) && form.blocked);
        assert(!qjs_intl_relative_literal_find(owner, &map, bytes("2"), &form) && !form.blocked && !form.text.length);
        assert(!qjs_intl_relative_pattern_find(owner, &map, QJS_INTL_RELATIVE_PAST, QJS_INTL_PLURAL_ONE, &form));
        assert(equal(form.text, "yesterday") && equal(form.origin_locale, "en"));
    }
    assert(before == state->calls);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlRelativeData owner, result, zero = {0};
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    size_t calls, fail;
    assert(unaligned);
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_relative_data_open(&data, &allocator, &owner));
    calls = state.calls;
    assert(calls && !state.live);
    if (TEST_INTL_AUTHORED)
        assert(calls > 1);
    for (fail = 1; fail <= calls; fail++) {
        state.calls = 0;
        state.fail_at = fail;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_relative_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
    }
    state.fail_at = 0;
    assert(!qjs_intl_relative_data_open(&data, &allocator, &result));
    assert(!state.live);
    check_queries(&owner, &state);
    check_corruption(&owner, &allocator);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_relative_data_open(&data, &allocator, &result));
    assert(!state.live);
    data = owner.data;
    free(unaligned);
    assert(qjs_intl_relative_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_relative_data_open(&data, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    return 0;
}
