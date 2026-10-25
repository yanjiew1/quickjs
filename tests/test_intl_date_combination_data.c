/* Shared immutable date/time connectors: exact queries, corruption and allocation failure.
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
#include "../src/intl/datetime/combination-data.h"
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

#if TEST_INTL_AUTHORED
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

static void corrupt(const QJSIntlDateCombinationData *owner, const QJSIntlAllocator *allocator,
                    const uint8_t *at, uint32_t value)
{
    QJSIntlData data;
    QJSIntlDateCombinationData result, zero = {0};
    uint8_t *copy = malloc(qjs_intl_native_length);
    size_t offset = at - owner->data.pack.bytes;
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_combination_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
    free(copy);
}

static void duplicate(const QJSIntlDateCombinationData *owner, const QJSIntlAllocator *allocator,
                      const uint8_t *at, size_t length)
{
    QJSIntlData data;
    QJSIntlDateCombinationData result, zero = {0};
    uint8_t *copy = malloc(qjs_intl_native_length);
    size_t offset = at - owner->data.pack.bytes;
    assert(copy && offset >= length && offset <= qjs_intl_native_length - length);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    memcpy(copy + offset, copy + offset - length, length);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_combination_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
    free(copy);
}

#endif

static uint32_t calendar_id(const QJSIntlDateCombinationData *owner, const char *name)
{
    uint32_t i;
    for (i = 0; i < owner->calendar_count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_date_combination_calendar_at(owner, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}

#if TEST_INTL_AUTHORED
static void check_corruption(const QJSIntlDateCombinationData *owner, const QJSIntlAllocator *allocator)
{
    const uint8_t *value = NULL, *missing = NULL, *blocked = NULL;
    size_t i;
    for (i = 0; i < 5; i++) {
        corrupt(owner, allocator, owner->section.bytes + i * 4, UINT32_MAX);
        corrupt(owner, allocator, owner->section.bytes + i * 4, 0);
    }
    corrupt(owner, allocator, owner->calendars, owner->data.strings.count);
    corrupt(owner, allocator, owner->calendars, string_id(&owner->data, "Generic"));
    corrupt(owner, allocator, owner->calendars + 4, qjs_intl_read_u32(owner->calendars));
    corrupt(owner, allocator, owner->calendars + 8, string_id(&owner->data, "japanese-"));
    corrupt(owner, allocator, owner->alternates, 1);
    corrupt(owner, allocator, owner->alternates + 4, owner->data.strings.count);
    corrupt(owner, allocator, owner->alternates + 4, string_id(&owner->data, "bad!"));
    corrupt(owner, allocator, owner->locale_rows, owner->row_count);
    corrupt(owner, allocator, owner->rows, owner->record_count);
    for (i = 0; i < owner->record_count; i++) {
        const uint8_t *record = owner->records + i * 32;
        uint32_t state = qjs_intl_read_u32(record);
        if (state == QJS_INTL_DATE_COMBINATION_VALUE)
            value = record;
        else if (state == QJS_INTL_DATE_COMBINATION_MISSING)
            missing = record;
        else
            blocked = record;
    }
    assert(value && missing && blocked);
    corrupt(owner, allocator, value, 3);
    for (i = 1; i <= 3; i++)
        corrupt(owner, allocator, value + i * 4, owner->data.strings.count);
    corrupt(owner, allocator, value + 16, owner->calendar_count);
    corrupt(owner, allocator, value + 20, QJS_INTL_DATE_STYLE_WIDTH_COUNT);
    corrupt(owner, allocator, value + 24, QJS_INTL_DATE_COMBINATION_FORM_COUNT);
    corrupt(owner, allocator, value + 28, owner->alternate_count);
    corrupt(owner, allocator, value + 4, string_id(&owner->data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91"));
    corrupt(owner, allocator, value + 4, string_id(&owner->data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"));
    corrupt(owner, allocator, blocked + 4, string_id(&owner->data, "{1}, {0}"));
    corrupt(owner, allocator, blocked + 8, string_id(&owner->data, "latn"));
    corrupt(owner, allocator, missing + 16, 1);
    for (i = 0; i < 5; i++) {
        static const char *const bad[] = {"", "En", "en-us", "bad_tag", "en-1901-1901"};
        corrupt(owner, allocator, value + 12, string_id(&owner->data, bad[i]));
    }
    /* Strict ordering also excludes duplicate shared records and rows. */
    duplicate(owner, allocator, owner->records + 32, 32);
    duplicate(owner, allocator, owner->rows + (size_t)owner->row_width * 4,
              (size_t)owner->row_width * 4);
    assert(owner->row_count == owner->section.count);
    corrupt(owner, allocator, owner->locale_rows, qjs_intl_read_u32(owner->locale_rows + 4));
}

#endif

static void get(const QJSIntlDateCombinationData *owner, const char *locale, const char *calendar,
                QJSIntlDateCombinationForm form, uint32_t alternate, QJSIntlDateCombinationRecord *out)
{
    assert(!qjs_intl_date_combination_record_at(owner, locale_id(&owner->data, locale),
                                               calendar_id(owner, calendar), QJS_INTL_DATE_STYLE_LONG,
                                               form, alternate, out));
}

static void check_queries(const QJSIntlDateCombinationData *owner)
{
    QJSIntlDateCombinationRecord record, zero = {0};
    QJSIntlBytes text, empty = {0};
    uint32_t locale, index;
#if TEST_INTL_AUTHORED
    assert(owner->calendar_count == 3 && owner->alternate_count == 2);
    assert(!qjs_intl_date_combination_calendar_at(owner, 0, &text) && equal(text, "generic"));
    assert(!qjs_intl_date_combination_alternate_at(owner, 0, &text) && equal(text, ""));
    assert(!qjs_intl_date_combination_alternate_at(owner, 1, &text) && equal(text, "variant"));
#endif
    for (locale = 0; locale < owner->section.count; locale++) {
        for (index = 0; index < owner->row_width; index++) {
            uint32_t selector = index, alternate = selector % owner->alternate_count;
            uint32_t form, width, calendar, row, id;
            const uint8_t *expected;
            selector /= owner->alternate_count;
            form = selector % QJS_INTL_DATE_COMBINATION_FORM_COUNT;
            selector /= QJS_INTL_DATE_COMBINATION_FORM_COUNT;
            width = selector % QJS_INTL_DATE_STYLE_WIDTH_COUNT;
            calendar = selector / QJS_INTL_DATE_STYLE_WIDTH_COUNT;
            assert(!qjs_intl_date_combination_record_at(owner, locale, calendar, width, form, alternate, &record));
            row = qjs_intl_read_u32(owner->locale_rows + locale * 4);
            id = qjs_intl_read_u32(owner->rows + ((size_t)row * owner->row_width + index) * 4);
            expected = owner->records + (size_t)id * 32;
            assert((unsigned int)record.state == qjs_intl_read_u32(expected));
            if (!record.state) {
                assert(!memcmp(&record, &zero, sizeof(record)));
            } else {
                QJSIntlBytes value;
                assert(!qjs_intl_data_string(&owner->data, qjs_intl_read_u32(expected + 4), &value));
                assert(value.data == record.text.data && value.length == record.text.length);
                assert(!qjs_intl_data_string(&owner->data, qjs_intl_read_u32(expected + 8), &value));
                assert(value.data == record.numbers.data && value.length == record.numbers.length);
                assert(!qjs_intl_data_string(&owner->data, qjs_intl_read_u32(expected + 12), &value));
                assert(value.data == record.origin_locale.data && value.length == record.origin_locale.length);
                assert(!qjs_intl_date_combination_calendar_at(owner, qjs_intl_read_u32(expected + 16), &value));
                assert(value.data == record.origin_calendar.data && value.length == record.origin_calendar.length);
                assert((unsigned int)record.origin_width == qjs_intl_read_u32(expected + 20));
                assert((unsigned int)record.origin_form == qjs_intl_read_u32(expected + 24));
                assert(!qjs_intl_date_combination_alternate_at(owner, qjs_intl_read_u32(expected + 28), &value));
                assert(value.data == record.origin_alternate.data && value.length == record.origin_alternate.length);
            }
        }
    }
#if TEST_INTL_AUTHORED
    get(owner, "en-GB", "japanese", QJS_INTL_DATE_COMBINATION_STANDARD, 0, &record);
    assert(equal(record.text, "{0} – {1}") && equal(record.origin_locale, "en-GB"));
    assert(equal(record.origin_calendar, "gregorian") && !record.numbers.length);
    get(owner, "en", "gregorian", QJS_INTL_DATE_COMBINATION_STANDARD, 0, &record);
    assert(record.state == QJS_INTL_DATE_COMBINATION_VALUE && !record.text.length);
    get(owner, "en", "generic", QJS_INTL_DATE_COMBINATION_STANDARD, 1, &record);
    assert(equal(record.text, "{1} 'at' {0}") && equal(record.numbers, "latn"));
    get(owner, "root", "gregorian", QJS_INTL_DATE_COMBINATION_STANDARD, 0, &record);
    assert(equal(record.text, "{1}, {0}") && equal(record.numbers, "y=hebr"));
    get(owner, "root", "japanese", QJS_INTL_DATE_COMBINATION_AT_TIME, 0, &record);
    assert(record.state == QJS_INTL_DATE_COMBINATION_BLOCKED && equal(record.origin_locale, "root"));
    get(owner, "root", "gregorian", QJS_INTL_DATE_COMBINATION_RELATIVE, 0, &record);
    assert(!memcmp(&record, &zero, sizeof(record)));
    get(owner, "de", "gregorian", QJS_INTL_DATE_COMBINATION_STANDARD, 0, &record);
    assert(equal(record.text, "{1} 'external' 😀 {0}") && equal(record.origin_locale, "en-1901-1996"));
#else
    get(owner, "en", "gregorian", QJS_INTL_DATE_COMBINATION_STANDARD, 0, &record);
    assert(record.state == QJS_INTL_DATE_COMBINATION_VALUE && equal(record.text, "{1}, {0}"));
    assert(!record.numbers.length && equal(record.origin_locale, "en"));
    get(owner, "en", "gregorian", QJS_INTL_DATE_COMBINATION_AT_TIME, 0, &record);
    assert(equal(record.text, "{1} 'at' {0}"));
    get(owner, "en", "gregorian", QJS_INTL_DATE_COMBINATION_RELATIVE, 0, &record);
    assert(equal(record.text, "{1} 'at' {0}"));
#endif
    memset(&record, 0xff, sizeof(record));
    assert(qjs_intl_date_combination_record_at(owner, UINT32_MAX, 0, 0, 0, 0, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&record, &zero, sizeof(record)));
    assert(qjs_intl_date_combination_record_at(owner, 0, owner->calendar_count, 0, 0, 0, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_combination_record_at(owner, 0, 0, -1, 0, 0, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_combination_record_at(owner, 0, 0, 0, -1, 0, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_combination_record_at(owner, 0, 0, 0, 0, owner->alternate_count, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_combination_record_at(NULL, 0, 0, 0, 0, 0, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_combination_record_at(owner, 0, 0, 0, 0, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_combination_calendar_at(owner, owner->calendar_count, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&text, &empty, sizeof(text)));
    assert(qjs_intl_date_combination_calendar_at(NULL, 0, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_combination_calendar_at(owner, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_combination_alternate_at(owner, owner->alternate_count, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&text, &empty, sizeof(text)));
    assert(qjs_intl_date_combination_alternate_at(NULL, 0, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_combination_alternate_at(owner, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data, unaligned_data;
    QJSIntlDateCombinationData owner, result, zero = {0};
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    size_t calls, step;
    assert(unaligned && !qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                                         &allocator, &data) && !state.live);
    state.calls = 0;
    assert(!qjs_intl_date_combination_data_open(&data, &allocator, &owner) && !state.live);
    calls = state.calls;
    assert(calls);
#if TEST_INTL_AUTHORED
    assert(calls >= 2);
#endif
    check_queries(&owner);
    assert(state.calls == calls);
#if TEST_INTL_AUTHORED
    check_corruption(&owner, &allocator);
#endif
    for (step = 1; step <= calls; step++) {
        state.calls = 0;
        state.fail_at = step;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_combination_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
        state.fail_at = 0;
        assert(!qjs_intl_date_combination_data_open(&data, &allocator, &result) && !state.live);
    }
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &unaligned_data));
    assert(!qjs_intl_date_combination_data_open(&unaligned_data, &allocator, &result));
    check_queries(&result);
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_combination_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_date_combination_data_open(&data, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_combination_data_open(&data, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    free(unaligned);
    assert(!state.live);
    return 0;
}
