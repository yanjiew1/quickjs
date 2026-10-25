/* Source admission preserves masking, ownership and raw pattern distinctions.
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
#include "../src/intl/datetime/source-candidate.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t candidate_bad_quote_bytes[], candidate_bad_field_bytes[];
extern const size_t candidate_bad_quote_length, candidate_bad_field_length;
extern const uint8_t candidate_calendar_year_bytes[], candidate_week_year_bytes[];
extern const size_t candidate_calendar_year_length, candidate_week_year_length;

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

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static void sources_open(const void *blob, size_t length, const QJSIntlAllocator *allocator,
                         const char *locale, const char *calendar, QJSIntlDateAvailableFamily family,
                         QJSIntlDateAvailableSources *out)
{
    QJSIntlData shared;
    QJSIntlDateAvailableData data;
    QJSIntlDateAvailableGroup group = {0};
    QJSIntlBytes text;
    uint32_t i, parent, group_index, head;
    assert(!qjs_intl_data_open(blob, length, allocator, &shared));
    assert(!qjs_intl_date_available_data_open(&shared, allocator, &data));
    for (i = 0; i < data.calendar_count; i++) {
        assert(!qjs_intl_date_available_calendar_at(&data, i, &text));
        if (equal(text, calendar))
            break;
    }
    assert(i < data.calendar_count);
    group.calendar = i;
    group.family = family;
    assert(!qjs_intl_date_available_find_group(&data, &group, &group_index));
    for (i = 0; i < data.section.count; i++) {
        assert(!qjs_intl_data_locale(&data.data, i, &text, &parent));
        if (equal(text, locale))
            break;
    }
    assert(i < data.section.count);
    assert(!qjs_intl_date_available_head(&data, i, group_index, &head));
    assert(!qjs_intl_date_available_sources_open(&data, head, allocator, out));
}

static const QJSIntlDateSourceCandidate *find(const QJSIntlDateSourceCandidates *list, const char *code)
{
    const QJSIntlDateSourceCandidate *record;
    size_t i;
    for (i = 0; i < list->count; i++) {
        assert(!qjs_intl_date_source_candidate_at(list, i, &record));
        if (equal(record->source.code, code))
            return record;
    }
    return NULL;
}

static void check_empty(const QJSIntlDateSourceCandidates *list)
{
    assert(!list->records && !list->count && !list->data.section.bytes && !list->allocator.malloc);
}

int main(void)
{
    Allocation source_state = {0}, state = {0};
    QJSIntlAllocator source_allocator = {&source_state, allocate, NULL, release};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlDateAvailableSources sources;
    QJSIntlDateSourceCandidates list;
    const QJSIntlDateSourceCandidate *record, *other;
    QJSIntlDateAvailableGroup group;
    QJSIntlBytes calendar;
    size_t calls, total, i;
    uint8_t *unaligned;
    sources_open(qjs_intl_native_bytes, qjs_intl_native_length, &source_allocator,
                 "root", "gregorian", QJS_INTL_DATE_AVAILABLE_PATTERN, &sources);
    assert(source_state.live == 1);
    assert(!qjs_intl_date_source_candidates_open(&sources, bytes(""), &allocator, &list));
    total = state.calls;
    assert(list.count == 12 && state.live == list.count + 1);
    calls = state.calls;
    record = find(&list, "y");
    other = find(&list, "yM");
    assert(record && other && record != other);
    assert(!memcmp(&record->info.components, &other->info.components, sizeof(record->info.components)));
    assert(record->pattern.tokens[0].symbol == 'y' && record->pattern.tokens[0].width == 4);
    assert(record->info.components.values[QJS_INTL_DATETIME_YEAR] == QJS_INTL_DATETIME_NUMERIC);
    assert(find(&list, "yy")->info.components.values[QJS_INTL_DATETIME_YEAR] == QJS_INTL_DATETIME_TWO_DIGIT);
    assert(find(&list, "M")->pattern.tokens[0].symbol == 'L');
    record = find(&list, "h");
    other = find(&list, "K");
    assert(record->info.hour_symbol == 'h' && other->info.hour_symbol == 'K');
    assert(!memcmp(&record->info.components, &other->info.components, sizeof(record->info.components)));
    assert(record->info.period_symbol == 'a' && !record->info.components.values[QJS_INTL_DATETIME_DAY_PERIOD]);
    assert(find(&list, "B")->info.components.values[QJS_INTL_DATETIME_DAY_PERIOD] == QJS_INTL_DATETIME_LONG);
    assert(find(&list, "Bhm")->info.period_symbol == 'B');
    assert(find(&list, "rU")->info.year_kinds == (QJS_INTL_DATE_YEAR_RELATED | QJS_INTL_DATE_YEAR_CYCLIC));
    assert(find(&list, "SSS")->info.components.values[QJS_INTL_DATETIME_FRACTION] == 3);
    assert(find(&list, "E")->pattern.tokens[0].width == 6);
    assert(!find(&list, "Q") && !find(&list, "j") && !find(&list, "a"));
    for (i = 0; i < list.count; i++) {
        assert(!qjs_intl_date_source_candidate_at(&list, i, &record));
        assert(record->source.plural == QJS_INTL_DATE_NO_PLURAL && !record->source.alt.length);
        assert(equal(record->origin.origin_locale, "root"));
        if (i) {
            size_t common;
            assert(!qjs_intl_date_source_candidate_at(&list, i - 1, &other));
            common = record->source.code.length < other->source.code.length ? record->source.code.length : other->source.code.length;
            assert(memcmp(other->source.code.data, record->source.code.data, common) < 0 ||
                   (other->source.code.length < record->source.code.length &&
                    !memcmp(other->source.code.data, record->source.code.data, common)));
        }
    }
    assert(state.calls == calls);
    record = (const QJSIntlDateSourceCandidate *)1;
    assert(qjs_intl_date_source_candidate_at(&list, list.count, &record) == QJS_INTL_INVALID_ARGUMENT && !record);
    assert(qjs_intl_date_source_candidate_at(&list, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_date_source_candidates_clear(&list);
    assert(!state.live);
    for (i = 1; i <= total; i++) {
        state.calls = 0;
        state.fail_at = i;
        memset(&list, 0xff, sizeof(list));
        assert(qjs_intl_date_source_candidates_open(&sources, bytes(""), &allocator, &list) == QJS_INTL_NO_MEMORY);
        check_empty(&list);
        assert(!state.live && source_state.live == 1);
        state.fail_at = 0;
        assert(!qjs_intl_date_source_candidates_open(&sources, bytes(""), &allocator, &list));
        qjs_intl_date_source_candidates_clear(&list);
    }
    assert(!qjs_intl_date_source_candidates_open(&sources, bytes("variant"), &allocator, &list));
    assert(list.count == 1 && equal(list.records[0].source.alt, "variant"));
    qjs_intl_date_source_candidates_clear(&list);
    /* Unusable preferred alternatives may fall back to their separately
       resolved ordinary peers, without reviving masked same-key ancestors. */
    assert(!qjs_intl_date_source_candidates_preferred(&sources, bytes("variant"), bytes(""), &allocator, &list));
    assert(list.count == 12 && find(&list, "y") && find(&list, "yM"));
    assert(equal(find(&list, "y")->source.alt, "variant"));
    assert(find(&list, "M") && find(&list, "d"));
    assert(equal(find(&list, "M")->source.alt, "") && equal(find(&list, "d")->source.alt, ""));
    qjs_intl_date_source_candidates_clear(&list);
    assert(!qjs_intl_date_source_candidates_preferred(&sources, bytes("unsupported"), bytes(""), &allocator, &list));
    assert(find(&list, "y") && equal(find(&list, "y")->source.alt, "") && find(&list, "yM"));
    qjs_intl_date_source_candidates_clear(&list);
    assert(!qjs_intl_date_source_candidates_preferred(&sources, bytes("ascii"), bytes(""), &allocator, &list));
    assert(find(&list, "y") && equal(find(&list, "y")->source.alt, ""));
    qjs_intl_date_source_candidates_clear(&list);
    {
        size_t allocations;
        state.calls = 0;
        assert(!qjs_intl_date_source_candidates_preferred(&sources, bytes("variant"), bytes(""), &allocator, &list));
        allocations = state.calls;
        qjs_intl_date_source_candidates_clear(&list);
        for (size_t step = 1; step <= allocations; step++) {
            state.calls = 0;
            state.fail_at = step;
            assert(qjs_intl_date_source_candidates_preferred(&sources, bytes("variant"), bytes(""),
                       &allocator, &list) == QJS_INTL_NO_MEMORY);
            check_empty(&list);
            assert(!state.live);
        }
        state.calls = state.fail_at = 0;
    }
    assert(qjs_intl_date_source_candidates_open(&sources, bytes("ascii"), &allocator, &list) == QJS_INTL_DATA_ERROR);
    check_empty(&list);
    assert(!state.live);
    state.calls = 0;
    assert(!qjs_intl_date_source_candidates_open(&sources, bytes("missing"), &allocator, &list));
    assert(!list.count && !list.records && !state.calls);
    qjs_intl_date_source_candidates_clear(&list);
    assert(!qjs_intl_date_source_candidates_open(&sources, bytes("unsupported"), &allocator, &list));
    assert(!list.count && !list.records && !state.live && state.calls == 4);
    qjs_intl_date_source_candidates_clear(&list);
    assert(qjs_intl_date_source_candidates_open(&sources, (QJSIntlBytes){NULL, 1}, &allocator, &list) == QJS_INTL_INVALID_ARGUMENT);
    check_empty(&list);
    assert(qjs_intl_date_source_candidates_open(&sources, bytes(""), NULL, &list) == QJS_INTL_INVALID_ARGUMENT);
    check_empty(&list);
    assert(qjs_intl_date_source_candidates_open(&sources, bytes(""), &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_date_available_sources_clear(&sources);
    sources_open(qjs_intl_native_bytes, qjs_intl_native_length, &source_allocator,
                 "en", "gregorian", QJS_INTL_DATE_AVAILABLE_PATTERN, &sources);
    assert(!qjs_intl_date_source_candidates_open(&sources, bytes(""), &allocator, &list));
    assert(list.count == 7);
    assert(!find(&list, "y") && !find(&list, "d") && !find(&list, "M") && !find(&list, "K") && !find(&list, "h"));
    assert(find(&list, "yM") && find(&list, "B"));
    qjs_intl_date_source_candidates_clear(&list);
    qjs_intl_date_available_sources_clear(&sources);
    sources_open(qjs_intl_native_bytes, qjs_intl_native_length, &source_allocator,
                 "en-GB", "generic", QJS_INTL_DATE_AVAILABLE_PATTERN, &sources);
    assert(!qjs_intl_date_source_candidates_open(&sources, bytes(""), &allocator, &list));
    qjs_intl_date_available_sources_clear(&sources);
    memset(&sources, 0xff, sizeof(sources));
    record = find(&list, "y");
    assert(record && equal(record->source.text, "yyyy") && equal(record->origin.origin_locale, "root"));
    assert(!qjs_intl_date_available_group_at(&list.data, record->origin.group, &group));
    assert(!qjs_intl_date_available_calendar_at(&list.data, group.calendar, &calendar));
    assert(equal(calendar, "gregorian"));
    qjs_intl_date_source_candidates_clear(&list);
    qjs_intl_date_source_candidates_clear(&list);
    check_empty(&list);
    sources_open(qjs_intl_native_bytes, qjs_intl_native_length, &source_allocator,
                 "en", "gregorian", QJS_INTL_DATE_AVAILABLE_APPEND, &sources);
    state.calls = 0;
    assert(qjs_intl_date_source_candidates_open(&sources, bytes("missing"), &allocator, &list) == QJS_INTL_INVALID_ARGUMENT);
    assert(!state.calls);
    check_empty(&list);
    qjs_intl_date_available_sources_clear(&sources);
    for (i = 0; i < 2; i++) {
        sources_open(i ? candidate_bad_field_bytes : candidate_bad_quote_bytes,
                     i ? candidate_bad_field_length : candidate_bad_quote_length, &source_allocator,
                     "root", "gregorian", QJS_INTL_DATE_AVAILABLE_PATTERN, &sources);
        assert(qjs_intl_date_source_candidates_open(&sources, bytes(""), &allocator, &list) == QJS_INTL_DATA_ERROR);
        check_empty(&list);
        assert(!state.live);
        qjs_intl_date_available_sources_clear(&sources);
    }
    for (i = 0; i < 2; i++) {
        sources_open(i ? candidate_week_year_bytes : candidate_calendar_year_bytes,
                     i ? candidate_week_year_length : candidate_calendar_year_length, &source_allocator,
                     "root", "gregorian", QJS_INTL_DATE_AVAILABLE_PATTERN, &sources);
        assert(!qjs_intl_date_source_candidates_open(&sources, bytes(""), &allocator, &list));
        record = find(&list, "zzzz");
        if (i) {
            assert(!record);
        } else {
            assert(record && equal(record->source.text, "Y/M/d"));
            assert(record->info.year_kinds == QJS_INTL_DATE_YEAR_CALENDAR);
            assert(record->pattern.tokens[0].symbol == 'y');
        }
        qjs_intl_date_source_candidates_clear(&list);
        qjs_intl_date_available_sources_clear(&sources);
        assert(!state.live && !source_state.live);
    }
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    sources_open(unaligned + 1, qjs_intl_native_length, &source_allocator,
                 "root", "gregorian", QJS_INTL_DATE_AVAILABLE_PATTERN, &sources);
    assert(!qjs_intl_date_source_candidates_open(&sources, bytes(""), &allocator, &list));
    qjs_intl_date_available_sources_clear(&sources);
    qjs_intl_date_source_candidates_clear(&list);
    free(unaligned);
    sources_open(qjs_intl_native_bytes, qjs_intl_native_length, &source_allocator,
                 "root", "generic", QJS_INTL_DATE_AVAILABLE_PATTERN, &sources);
    state.calls = 0;
    state.fail_at = 1;
    assert(!sources.count);
    assert(!qjs_intl_date_source_candidates_open(&sources, bytes(""), &allocator, &list));
    assert(!state.calls && !list.records && !list.count);
    qjs_intl_date_source_candidates_clear(&list);
    qjs_intl_date_available_sources_clear(&sources);
    assert(qjs_intl_date_source_candidates_open(NULL, bytes(""), &allocator, &list) == QJS_INTL_INVALID_ARGUMENT);
    check_empty(&list);
    qjs_intl_date_source_candidates_clear(NULL);
    assert(!state.live && !source_state.live);
    return 0;
}
