/* Native preference owner validation and allocation-failure regressions.
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
#include "../src/intl/locale/preference-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    unsigned int calls, fail_at, live;
    size_t limit, largest;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (size > state->largest)
        state->largest = size;
    if (++state->calls == state->fail_at || (state->limit && size > state->limit))
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

static QJSIntlBytes text(const char *value)
{
    return (QJSIntlBytes){value, strlen(value)};
}

static void equal(QJSIntlBytes value, const char *expected)
{
    assert(value.length == strlen(expected));
    assert(!memcmp(value.data, expected, value.length));
}

static void put_u32(uint8_t *bytes, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        bytes[i] = value >> (8 * i);
}

static uint32_t string_id(const QJSIntlData *data, const char *value)
{
    uint32_t i;
    for (i = 0; i < data->strings.count; i++) {
        QJSIntlBytes item;
        assert(!qjs_intl_data_string(data, i, &item));
        if (item.length == strlen(value) && !memcmp(item.data, value, item.length))
            return i;
    }
    abort();
}

static void corrupt(const QJSIntlAllocator *allocator, size_t offset, uint32_t value)
{
    QJSIntlData data;
    QJSIntlLocalePreferences preferences;
    uint8_t *copy = malloc(qjs_intl_native_length);
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    assert(qjs_intl_locale_preferences_open(&data, allocator, &preferences) ==
           QJS_INTL_DATA_ERROR);
    assert(!preferences.data.pack.bytes && !preferences.weeks.bytes);
    free(copy);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlLocalePreferences preferences;
    QJSIntlWeekInfo week;
    QJSIntlBytes value;
    unsigned int calls, i, j, direction;
    int shared = 0;
    uint32_t count, gregorian;
    size_t weeks_at, calendars_at, hours_at, scripts_at;
    uint8_t *copy;

    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                               &allocator, &data));
    /* Preference scratch must follow the few hundred preference rows, not
       the much larger shared string pool. Keep the raw-data gate separate. */
    state.calls = 0;
    state.largest = 0;
    state.limit = 8192;
    assert(!qjs_intl_locale_preferences_open(&data, &allocator, &preferences));
    assert(state.largest <= state.limit);
    state.limit = 0;
    calls = state.calls;
    assert(calls > 1 && !state.live);
    assert(preferences.weeks.count == 156 && preferences.calendars.count == 52);
    assert(preferences.hours.count == 277 && preferences.scripts.count == 184);
    assert(!qjs_intl_preference_week(&preferences, text("US"), &week));
    assert(week.first_day == 7 && week.minimum_days == 1 && week.weekend == 96);
    assert(!qjs_intl_preference_week(&preferences, text("AF"), &week));
    assert(week.first_day == 6 && week.weekend == 24);
    assert(!qjs_intl_preference_week(&preferences, text("001"), &week));
    assert(week.first_day == 1 && week.weekend == 96);
    assert(qjs_intl_preference_week(&preferences, text("ZZ"), &week) ==
           QJS_INTL_UNSUPPORTED);
    assert(!week.first_day && !week.weekend);
    assert(!qjs_intl_preference_list(&preferences, 0, text("TH"), &value, &count));
    equal(value, "buddhist gregorian");
    assert(count == 2);
    assert(!qjs_intl_preference_list(&preferences, 1, text("en-IL"), &value, &count));
    equal(value, "h23 h12");
    assert(count == 2);
    assert(!qjs_intl_preference_list(&preferences, 1, text("JP"), &value, &count));
    equal(value, "h23 h11 h12");
    assert(count == 3);
    assert(qjs_intl_preference_list(&preferences, 1, text("no-record"), &value,
                                    &count) == QJS_INTL_UNSUPPORTED);
    assert(!value.data && !count);
    assert(!qjs_intl_preference_direction(&preferences, text("Arab"), &direction));
    assert(direction == QJS_INTL_DIRECTION_RTL);
    assert(!qjs_intl_preference_direction(&preferences, text("Latn"), &direction));
    assert(direction == QJS_INTL_DIRECTION_LTR);
    assert(!qjs_intl_preference_direction(&preferences, text("Zzzz"), &direction));
    assert(direction == QJS_INTL_DIRECTION_UNKNOWN);
    assert(qjs_intl_preference_direction(&preferences, text("Qaaa"), &direction) ==
           QJS_INTL_UNSUPPORTED);
    assert(direction == QJS_INTL_DIRECTION_UNKNOWN);
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        assert(qjs_intl_locale_preferences_open(&data, &allocator, &preferences) ==
               QJS_INTL_NO_MEMORY);
        assert(!preferences.data.pack.bytes && !state.live);
    }
    state.fail_at = 0;
    assert(!qjs_intl_locale_preferences_open(&data, &allocator, &preferences));
    weeks_at = preferences.weeks.bytes - data.pack.bytes;
    calendars_at = preferences.calendars.bytes - data.pack.bytes;
    hours_at = preferences.hours.bytes - data.pack.bytes;
    scripts_at = preferences.scripts.bytes - data.pack.bytes;
    corrupt(&allocator, weeks_at, string_id(&data, "en"));
    corrupt(&allocator, weeks_at + 4, 0);
    corrupt(&allocator, weeks_at + 8, 8);
    corrupt(&allocator, weeks_at + 12, 0);
    corrupt(&allocator, weeks_at + 12, 128);
    corrupt(&allocator, weeks_at + 16, qjs_intl_read_u32(preferences.weeks.bytes));
    corrupt(&allocator, calendars_at + 4, data.strings.count);
    corrupt(&allocator, calendars_at + 8, 2);
    /* Nonadjacent rows sharing a list must still agree on its count. */
    for (i = 2; i < preferences.calendars.count && !shared; i++) {
        const uint8_t *row = preferences.calendars.bytes + (size_t)i * 12;
        for (j = 0; j + 1 < i; j++) {
            const uint8_t *earlier = preferences.calendars.bytes + (size_t)j * 12;
            if (qjs_intl_read_u32(row + 4) == qjs_intl_read_u32(earlier + 4)) {
                corrupt(&allocator, calendars_at + (size_t)i * 12 + 8,
                        qjs_intl_read_u32(row + 8) + 1);
                shared = 1;
                break;
            }
        }
    }
    assert(shared);
    /* A pooled token accepted as a calendar must be checked again against
       hour-cycle grammar when referenced by the other table. */
    gregorian = string_id(&data, "gregorian");
    for (i = 0; i < preferences.calendars.count; i++) {
        const uint8_t *row = preferences.calendars.bytes + (size_t)i * 12;
        if (qjs_intl_read_u32(row + 4) == gregorian &&
            qjs_intl_read_u32(row + 8) == 1)
            break;
    }
    assert(i < preferences.calendars.count);
    for (i = 0; i < preferences.hours.count; i++) {
        const uint8_t *row = preferences.hours.bytes + (size_t)i * 12;
        if (qjs_intl_read_u32(row + 8) == 1)
            break;
    }
    assert(i < preferences.hours.count);
    corrupt(&allocator, hours_at + (size_t)i * 12 + 4, gregorian);
    corrupt(&allocator, hours_at + 4, string_id(&data, "en"));
    corrupt(&allocator, hours_at + 8, 5);
    corrupt(&allocator, scripts_at, string_id(&data, "en"));
    corrupt(&allocator, scripts_at + 4, 3);
    copy = malloc(qjs_intl_native_length + 1);
    assert(copy);
    memcpy(copy + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(copy + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_locale_preferences_open(&data, &allocator, &preferences));
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    for (i = 0; i < qjs_intl_read_u32(copy + 12); i++) {
        uint8_t *entry = copy + 64 + (size_t)i * 16;
        uint32_t identity = qjs_intl_read_u32(entry);
        if (identity == QJS_INTL_SECTION_WEEKS)
            corrupt(&allocator, entry - copy + 12, 0);
        if (identity >= QJS_INTL_SECTION_WEEKS)
            put_u32(entry, identity + 20);
    }
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, &allocator, &data));
    assert(qjs_intl_locale_preferences_open(&data, &allocator, &preferences) ==
           QJS_INTL_DATA_ERROR);
    assert(qjs_intl_locale_preferences_open(NULL, &allocator, &preferences) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_locale_preferences_open(&data, NULL, &preferences) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_locale_preferences_open(&data, &allocator, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_preference_week(NULL, text("US"), &week) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_preference_direction(NULL, text("Latn"), &direction) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                               &allocator, &data));
    assert(!qjs_intl_locale_preferences_open(&data, &allocator, &preferences));
    assert(qjs_intl_preference_list(NULL, 0, text("001"), &value, &count) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!value.data && !count);
    assert(qjs_intl_preference_list(&preferences, 2, text("001"), &value, &count) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!value.data && !count);
    assert(qjs_intl_preference_week(&preferences, (QJSIntlBytes){NULL, 2}, &week) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!week.first_day && !week.weekend);
    free(copy);
    assert(!state.live);
    puts("Intl native preference reader tests passed");
    return 0;
}
