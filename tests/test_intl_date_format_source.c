/* Group full component vectors while preserving source identities and order.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/format-source.h"
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

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static void pool_open(const QJSIntlAllocator *allocator, QJSIntlDateSourceCandidates *out)
{
    QJSIntlData shared;
    QJSIntlDateAvailableData data;
    QJSIntlDateAvailableSources sources;
    QJSIntlDateAvailableGroup key = {0};
    QJSIntlBytes text;
    uint32_t i, group, parent, head;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, allocator, &shared));
    assert(!qjs_intl_date_available_data_open(&shared, allocator, &data));
    for (i = 0; i < data.calendar_count; i++) {
        assert(!qjs_intl_date_available_calendar_at(&data, i, &text));
        if (equal(text, "gregorian"))
            break;
    }
    key.calendar = i;
    assert(!qjs_intl_date_available_find_group(&data, &key, &group));
    for (i = 0; i < data.section.count; i++) {
        assert(!qjs_intl_data_locale(&data.data, i, &text, &parent));
        if (equal(text, "root"))
            break;
    }
    assert(!qjs_intl_date_available_head(&data, i, group, &head));
    assert(!qjs_intl_date_available_sources_open(&data, head, allocator, &sources));
    assert(!qjs_intl_date_source_candidates_open(&sources, (QJSIntlBytes){0}, allocator, out));
    qjs_intl_date_available_sources_clear(&sources);
}

static void verify(const QJSIntlDateSourceCandidates *pool, const QJSIntlDateFormatSources *groups)
{
    size_t i, j, index, first = 0, year_members = 0, hour_members = 0;
    unsigned char seen[12] = {0};
    assert(pool->count == 12 && groups->source_count == 12 && groups->count == 9);
    for (i = 0; i < groups->count; i++) {
        QJSIntlDateTimeComponents components;
        size_t count, previous = 0;
        assert(!qjs_intl_date_format_source_group(groups, i, &components, &count));
        assert(count);
        for (j = 0; j < count; j++) {
            const QJSIntlDateSourceCandidate *candidate;
            size_t field;
            assert(!qjs_intl_date_format_source_member(groups, i, j, &index));
            assert(index < pool->count && !seen[index]++);
            if (!j) {
                assert(!i || index > first);
                first = index;
            } else {
                assert(index > previous);
            }
            previous = index;
            candidate = &pool->records[index];
            for (field = 0; field < QJS_INTL_DATETIME_COMPONENT_COUNT; field++)
                assert(candidate->info.components.values[field] == components.values[field]);
            if (equal(candidate->source.code, "y") || equal(candidate->source.code, "yM") || equal(candidate->source.code, "rU")) {
                assert(count == 3);
                year_members++;
                if (equal(candidate->source.code, "rU"))
                    assert(candidate->info.year_kinds == (QJS_INTL_DATE_YEAR_RELATED | QJS_INTL_DATE_YEAR_CYCLIC));
            }
            if (equal(candidate->source.code, "h") || equal(candidate->source.code, "K")) {
                assert(count == 2 && candidate->pattern.tokens[0].symbol == candidate->info.hour_symbol);
                hour_members++;
            }
        }
    }
    for (i = 0; i < pool->count; i++)
        assert(seen[i] == 1 && pool->records[i].pattern.storage);
    assert(year_members == 3 && hour_members == 2);
}

int main(void)
{
    Allocation source_state = {0}, state = {0};
    QJSIntlAllocator a = {&source_state, allocate, NULL, release};
    QJSIntlAllocator b = {&state, allocate, NULL, release};
    QJSIntlDateSourceCandidates pool, view, no_sources = {0};
    QJSIntlDateFormatSources groups, again;
    QJSIntlDateSourceCandidate distinctions[QJS_INTL_DATETIME_COMPONENT_COUNT + 1] = {0};
    QJSIntlDateTimeComponents components;
    size_t i, j, count, index, calls, source_calls;
    pool_open(&a, &pool);
    source_calls = source_state.calls;
    assert(!qjs_intl_date_format_sources_open(&pool, &b, &groups));
    assert(state.calls == 2 && state.live == 2);
    calls = state.calls;
    verify(&pool, &groups);
    assert(calls == state.calls && source_calls == source_state.calls);
    assert(!qjs_intl_date_format_sources_open(&pool, &b, &again));
    assert(groups.groups != again.groups && groups.members != again.members);
    verify(&pool, &again);
    for (i = 0; i < groups.count; i++) {
        QJSIntlDateTimeComponents other;
        size_t other_count;
        assert(!qjs_intl_date_format_source_group(&groups, i, &components, &count));
        assert(!qjs_intl_date_format_source_group(&again, i, &other, &other_count));
        assert(count == other_count && !memcmp(&components, &other, sizeof(components)));
        for (j = 0; j < count; j++) {
            size_t other_index;
            assert(!qjs_intl_date_format_source_member(&groups, i, j, &index));
            assert(!qjs_intl_date_format_source_member(&again, i, j, &other_index));
            assert(index == other_index);
        }
    }
    qjs_intl_date_format_sources_clear(&again);
    index = count = SIZE_MAX;
    memset(&components, 0xff, sizeof(components));
    assert(qjs_intl_date_format_source_group(&groups, groups.count, &components, &count) == QJS_INTL_INVALID_ARGUMENT && !count);
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        assert(!components.values[i]);
    assert(qjs_intl_date_format_source_member(&groups, 0, SIZE_MAX, &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_date_format_source_member(&groups, 0, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_format_source_group(&groups, 0, NULL, &count) == QJS_INTL_INVALID_ARGUMENT && !count);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_format_sources_clear(&groups);
    assert(!state.live && source_state.calls == source_calls);
    for (i = 1; i <= 2; i++) {
        state.calls = 0;
        state.fail_at = i;
        memset(&groups, 0xff, sizeof(groups));
        assert(qjs_intl_date_format_sources_open(&pool, &b, &groups) == QJS_INTL_NO_MEMORY);
        assert(!groups.groups && !groups.members && !groups.count && !groups.source_count && !state.live);
        state.fail_at = 0;
        assert(!qjs_intl_date_format_sources_open(&pool, &b, &groups));
        verify(&pool, &groups);
        qjs_intl_date_format_sources_clear(&groups);
    }
    view = pool;
    view.count = 1; /* A read-only prefix, still owned entirely by pool. */
    assert(!qjs_intl_date_format_sources_open(&view, &b, &groups));
    assert(groups.count == 1 && groups.source_count == 1);
    assert(!qjs_intl_date_format_source_member(&groups, 0, 0, &index) && !index);
    qjs_intl_date_format_sources_clear(&groups);
    state.calls = 0;
    state.fail_at = 1;
    assert(!qjs_intl_date_format_sources_open(&no_sources, &b, &groups));
    assert(!state.calls && !groups.count && !groups.groups && !groups.members);
    qjs_intl_date_format_sources_clear(&groups);
    state.fail_at = 0;
    assert(qjs_intl_date_format_sources_open(NULL, &b, &groups) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_format_sources_open(&pool, NULL, &groups) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_format_sources_open(&pool, &b, NULL) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_date_source_candidates_clear(&pool);
    assert(!source_state.live && !state.live);

    /* Metadata-only stress input: grouping never reads or owns patterns.
       These 36 full vectors repeat in original order across a large pool. */
    memset(&view, 0, sizeof(view));
    view.count = 6000;
    view.records = calloc(view.count, sizeof(*view.records));
    assert(view.records);
    for (i = 0; i < view.count; i++) {
        QJSIntlDateTimeComponents *c = &view.records[i].info.components;
        c->values[QJS_INTL_DATETIME_YEAR] = i % 2 ? QJS_INTL_DATETIME_TWO_DIGIT : QJS_INTL_DATETIME_NUMERIC;
        c->values[QJS_INTL_DATETIME_FRACTION] = 1 + (i / 2) % 3;
        c->values[QJS_INTL_DATETIME_ZONE] = 1 + (i / 6) % 6;
    }
    assert(!qjs_intl_date_format_sources_open(&view, &b, &groups));
    assert(groups.count == 36 && groups.source_count == view.count);
    for (i = 0; i < groups.count; i++) {
        assert(!qjs_intl_date_format_source_group(&groups, i, &components, &count));
        for (j = 0; j < count; j++) {
            assert(!qjs_intl_date_format_source_member(&groups, i, j, &index));
            assert(index == i + j * 36);
        }
    }
    qjs_intl_date_format_sources_clear(&groups);
    free(view.records);
    /* Every field independently changes group identity, not just presence
       masks or the year/fraction/zone fields used in the stress input. */
    distinctions[0].info.components.values[QJS_INTL_DATETIME_YEAR] = QJS_INTL_DATETIME_NUMERIC;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        distinctions[i + 1].info.components = distinctions[0].info.components;
        distinctions[i + 1].info.components.values[i] =
            i == QJS_INTL_DATETIME_YEAR ? QJS_INTL_DATETIME_TWO_DIGIT :
            i == QJS_INTL_DATETIME_FRACTION ? 1 :
            i == QJS_INTL_DATETIME_ZONE ? QJS_INTL_DATETIME_ZONE_SHORT :
            i == QJS_INTL_DATETIME_WEEKDAY || i == QJS_INTL_DATETIME_ERA ||
            i == QJS_INTL_DATETIME_DAY_PERIOD ? QJS_INTL_DATETIME_SHORT : QJS_INTL_DATETIME_NUMERIC;
    }
    view.records = distinctions;
    view.count = QJS_INTL_DATETIME_COMPONENT_COUNT + 1;
    assert(!qjs_intl_date_format_sources_open(&view, &b, &groups));
    assert(groups.count == view.count);
    for (i = 0; i < view.count; i++) {
        assert(!qjs_intl_date_format_source_group(&groups, i, &components, &count) && count == 1);
        assert(!qjs_intl_date_format_source_member(&groups, i, 0, &index) && index == i);
    }
    qjs_intl_date_format_sources_clear(&groups);
    calls = state.calls;
    view.count = SIZE_MAX;
    memset(&groups, 0xff, sizeof(groups));
    assert(qjs_intl_date_format_sources_open(&view, &b, &groups) == QJS_INTL_OVERFLOW);
    assert(!groups.count && !groups.groups && !groups.members && state.calls == calls);
    qjs_intl_date_format_sources_clear(NULL);
    assert(!state.live);
    return 0;
}
