/* Exact-key shadowing precedes interpretation; retain borrowed provenance.
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
#include "../src/intl/datetime/available-source.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t available_source_empty_bytes[];
extern const size_t available_source_empty_length;

typedef struct Allocation {
    size_t calls, fail_at, live, last_size;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    state->last_size = size;
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

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static int compare(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t count = a.length < b.length ? a.length : b.length;
    int order = count ? memcmp(a.data, b.data, count) : 0;
    return order ? order : (a.length > b.length) - (a.length < b.length);
}

static uint32_t source_head(const QJSIntlDateAvailableData *data, const char *locale, const char *calendar,
                            QJSIntlDateAvailableFamily family)
{
    uint32_t i, parent, group, head;
    QJSIntlBytes text;
    QJSIntlDateAvailableGroup key = {0};
    for (i = 0; i < data->calendar_count; i++) {
        assert(!qjs_intl_date_available_calendar_at(data, i, &text));
        if (equal(text, calendar))
            break;
    }
    assert(i < data->calendar_count);
    key.calendar = i;
    key.family = family;
    assert(!qjs_intl_date_available_find_group(data, &key, &group));
    for (i = 0; i < data->section.count; i++) {
        assert(!qjs_intl_data_locale(&data->data, i, &text, &parent));
        if (equal(text, locale))
            break;
    }
    assert(i < data->section.count);
    assert(!qjs_intl_date_available_head(data, i, group, &head) && head);
    return head;
}

static void check(const QJSIntlDateAvailableSources *sources, const char *code,
                  QJSIntlDateAvailablePlural plural, const char *alt, const char *text,
                  unsigned int blocked, const char *locale)
{
    size_t i, matches = 0;
    QJSIntlDateAvailableEntry entry, previous = {0};
    QJSIntlDateAvailableNode origin;
    for (i = 0; i < sources->count; i++) {
        assert(!qjs_intl_date_available_source_at(sources, i, &entry, &origin));
        if (i) {
            int order = compare(previous.code, entry.code);
            if (!order)
                order = (previous.plural > entry.plural) - (previous.plural < entry.plural);
            if (!order)
                order = compare(previous.alt, entry.alt);
            assert(order < 0);
        }
        previous = entry;
        if (equal(entry.code, code) && entry.plural == plural && equal(entry.alt, alt)) {
            matches++;
            assert(equal(entry.text, text) && entry.blocked == blocked);
            assert(equal(origin.origin_locale, locale));
        }
    }
    assert(matches == 1);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData shared;
    QJSIntlDateAvailableData owner, temporary;
    QJSIntlDateAvailableSources sources;
    QJSIntlDateAvailableEntry entry;
    QJSIntlDateAvailableNode origin;
    uint32_t en, gb, us;
    size_t calls, en_capacity, i;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &shared));
    assert(!qjs_intl_date_available_data_open(&shared, &allocator, &owner) && !state.live);
    en = source_head(&owner, "en", "gregorian", QJS_INTL_DATE_AVAILABLE_PATTERN);
    gb = source_head(&owner, "en-GB", "gregorian", QJS_INTL_DATE_AVAILABLE_PATTERN);
    us = source_head(&owner, "en-US", "gregorian", QJS_INTL_DATE_AVAILABLE_PATTERN);
    state.calls = 0;
    temporary = owner;
    assert(!qjs_intl_date_available_sources_open(&temporary, gb, &allocator, &sources));
    memset(&temporary, 0xff, sizeof(temporary)); /* The view was copied. */
    assert(sources.count == 7 && state.calls == 2 && state.live == 1);
    calls = state.calls;
    check(&sources, "y", QJS_INTL_DATE_NO_PLURAL, "", "Q", 0, "en");
    check(&sources, "d", QJS_INTL_DATE_NO_PLURAL, "", "", 1, "en");
    check(&sources, "M", QJS_INTL_DATE_NO_PLURAL, "", "", 0, "en");
    check(&sources, "j", QJS_INTL_DATE_NO_PLURAL, "", "'literal'", 0, "en");
    check(&sources, "y", QJS_INTL_DATE_NO_PLURAL, "variant", "'GB'", 0, "en-GB");
    check(&sources, "y", QJS_INTL_DATE_PLURAL_OTHER, "", "yy", 0, "root");
    check(&sources, "Hm", QJS_INTL_DATE_NO_PLURAL, "", "HH:mm", 0, "root");
    assert(calls == state.calls && state.live == 1);
    memset(&entry, 0xff, sizeof(entry));
    memset(&origin, 0xff, sizeof(origin));
    assert(qjs_intl_date_available_source_at(&sources, sources.count, &entry, &origin) == QJS_INTL_INVALID_ARGUMENT);
    assert(!entry.code.data && !entry.text.data && !entry.blocked && !origin.origin_locale.data && !origin.parent);
    assert(qjs_intl_date_available_source_at(&sources, 0, NULL, &origin) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_date_available_sources_clear(&sources);
    qjs_intl_date_available_sources_clear(&sources);
    assert(!state.live && !sources.records && !sources.data.section.bytes);
    assert(!qjs_intl_date_available_sources_open(&owner, en, &allocator, &sources));
    en_capacity = state.last_size;
    qjs_intl_date_available_sources_clear(&sources);
    assert(!qjs_intl_date_available_sources_open(&owner, us, &allocator, &sources));
    /* en-US repeats the root's exact physical map across an intervening en
       layer. Collect it once, while preserving the nearest source origin. */
    assert(state.last_size == en_capacity);
    check(&sources, "y", QJS_INTL_DATE_NO_PLURAL, "", "y", 0, "en-US");
    check(&sources, "d", QJS_INTL_DATE_NO_PLURAL, "", "dd", 0, "en-US");
    qjs_intl_date_available_sources_clear(&sources);
    assert(!qjs_intl_date_available_sources_open(&owner, source_head(&owner, "de", "generic", QJS_INTL_DATE_AVAILABLE_PATTERN), &allocator, &sources));
    check(&sources, "y", QJS_INTL_DATE_NO_PLURAL, "", "'external'", 0, "en-1901-1996");
    qjs_intl_date_available_sources_clear(&sources);
    assert(!qjs_intl_date_available_sources_open(&owner,
                source_head(&owner, "en-GB", "gregorian", QJS_INTL_DATE_AVAILABLE_APPEND), &allocator, &sources));
    assert(sources.count == 1);
    check(&sources, "Year", QJS_INTL_DATE_NO_PLURAL, "", "", 1, "en");
    qjs_intl_date_available_sources_clear(&sources);
    for (i = 1; i <= 2; i++) {
        state.calls = 0;
        state.fail_at = i;
        memset(&sources, 0xff, sizeof(sources));
        assert(qjs_intl_date_available_sources_open(&owner, en, &allocator, &sources) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !sources.records && !sources.count && !sources.data.section.bytes);
        state.fail_at = 0;
        assert(!qjs_intl_date_available_sources_open(&owner, en, &allocator, &sources));
        qjs_intl_date_available_sources_clear(&sources);
    }
    assert(qjs_intl_date_available_sources_open(&owner, owner.node_count + 1, &allocator, &sources) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_available_sources_open(&owner, en, NULL, &sources) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_available_sources_open(&owner, en, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &shared));
    assert(!qjs_intl_date_available_data_open(&shared, &allocator, &owner));
    assert(!qjs_intl_date_available_sources_open(&owner, source_head(&owner, "en", "gregorian", QJS_INTL_DATE_AVAILABLE_PATTERN), &allocator, &sources));
    check(&sources, "y", QJS_INTL_DATE_NO_PLURAL, "", "Q", 0, "en");
    qjs_intl_date_available_sources_clear(&sources);
    free(unaligned);
    assert(!qjs_intl_data_open(available_source_empty_bytes, available_source_empty_length, &allocator, &shared));
    assert(!qjs_intl_date_available_data_open(&shared, &allocator, &owner));
    state.calls = 0;
    state.fail_at = 1;
    assert(!qjs_intl_date_available_sources_open(&owner, 0, &allocator, &sources));
    assert(!sources.count && !sources.records && !state.calls && !state.live);
    qjs_intl_date_available_sources_clear(&sources);
    qjs_intl_date_available_sources_clear(NULL);
    return 0;
}
