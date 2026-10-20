/* Whole-pattern pairing preserves year semantics, periods and literals.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/single-program.h"
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

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static void pool_open(const QJSIntlAllocator *allocator, const char *const *patterns, size_t count,
                      QJSIntlDateSourceCandidates *pool, QJSIntlDateFormatSources *groups)
{
    size_t i;
    memset(pool, 0, sizeof(*pool));
    pool->allocator = *allocator;
    pool->records = allocator->malloc(allocator->opaque, count * sizeof(*pool->records));
    assert(pool->records);
    memset(pool->records, 0, count * sizeof(*pool->records));
    pool->count = count;
    for (i = 0; i < count; i++) {
        QJSIntlDateSourceCandidate *source = &pool->records[i];
        source->source.text = bytes(patterns[i]);
        assert(!qjs_intl_date_pattern_compile(allocator, source->source.text, &source->pattern));
        assert(!qjs_intl_date_pattern_info(&source->pattern, &source->info));
    }
    assert(!qjs_intl_date_format_sources_open(pool, allocator, groups));
}

static size_t hour_group(const QJSIntlDateFormatSources *groups, int hour)
{
    QJSIntlDateTimeComponents components;
    size_t i, count;
    for (i = 0; i < groups->count; i++) {
        assert(!qjs_intl_date_format_source_group(groups, i, &components, &count));
        if (components.values[QJS_INTL_DATETIME_HOUR] == hour)
            return i;
    }
    abort();
}

static void verify(const QJSIntlDateSinglePrograms *programs)
{
    QJSIntlDatePatternInfo normal, twelve;
    size_t i;
    assert(!qjs_intl_date_pattern_info(&programs->normal.pattern, &normal));
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        assert(normal.components.values[i] == programs->components.values[i]);
    if (programs->components.values[QJS_INTL_DATETIME_HOUR]) {
        assert(!qjs_intl_date_pattern_info(&programs->twelve.pattern, &twelve));
        for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
            assert(twelve.components.values[i] == programs->components.values[i]);
        assert(normal.hour_symbol == 'H' || normal.hour_symbol == 'k');
        assert(twelve.hour_symbol == 'h' || twelve.hour_symbol == 'K');
        assert(normal.period_symbol != 'a' && twelve.period_symbol);
        assert(normal.year_kinds == twelve.year_kinds);
        if (programs->components.values[QJS_INTL_DATETIME_DAY_PERIOD])
            assert(normal.period_symbol == twelve.period_symbol);
    } else {
        assert(!programs->twelve.pattern.storage);
    }
}

static void empty(const QJSIntlDateSinglePrograms *programs)
{
    size_t i;
    assert(!programs->normal.pattern.storage && !programs->twelve.pattern.storage && !programs->allocator.malloc);
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        assert(!programs->components.values[i]);
}

int main(void)
{
    static const char *const english[] = {("h:mm\xe2\x80\xaf" "a"), "HH:mm", "h:mm a"};
    static const char *const flexible[] = {"h B", "HH B"};
    static const char *const inverse[] = {"H B", "hh B"};
    static const char *const periods[] = {"h b", "HH B"};
    static const char *const period_tie[] = {"H B", "h b"};
    static const char *const contextual[] = {"aK:mm'\xe6\x99\x82'", "HH:mm 'Uhr'"};
    static const char *const years[] = {"y h a", "r HH", "r h a"};
    static const char *const mismatched[] = {"y h a", "r HH"};
    static const char *const incompatible[] = {"h a", "HH:mm"};
    static const char *const cyclic[] = {"r(U) h a", "r(U) HH"};
    static const char *const no_hour[] = {"LLLL d"};
    static const char *const year_choice[] = {"M/d/U", "M/d/r"};
    static const char *const year_pairs[] = {"U H", "U h a", "r H", "r h a"};
    static const char *const cyclic_fallback[] = {"U H", "U h a", "r H"};
    static const char *const missing[] = {"h a", "H"};
    Allocation input = {0}, output = {0};
    QJSIntlAllocator a = {&input, allocate, NULL, release};
    QJSIntlAllocator b = {&output, allocate, NULL, release};
    QJSIntlDateSourceCandidates pool;
    QJSIntlDateFormatSources groups;
    QJSIntlDateSinglePrograms programs;
    size_t group, i, calls, input_calls;
    pool_open(&a, english, 3, &pool, &groups);
    group = hour_group(&groups, QJS_INTL_DATETIME_NUMERIC);
    input_calls = input.calls;
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs));
    verify(&programs);
    assert(output.live == 2 && output.calls == 2 && input.calls == input_calls);
    assert(programs.normal.source_index == 1 && programs.normal.adaptations == QJS_INTL_DATE_ADAPT_HOUR_WIDTH);
    assert(programs.twelve.source_index == 0 && !programs.twelve.adaptations);
    assert(equal(programs.twelve.pattern.tokens[3].literal, "\xe2\x80\xaf"));
    qjs_intl_date_single_programs_clear(&programs);
    group = hour_group(&groups, QJS_INTL_DATETIME_TWO_DIGIT);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs));
    verify(&programs);
    assert(!programs.normal.adaptations && programs.twelve.adaptations == QJS_INTL_DATE_ADAPT_HOUR_WIDTH);
    qjs_intl_date_single_programs_clear(&programs);
    for (i = 1; i <= 2; i++) {
        output.calls = 0;
        output.fail_at = i;
        memset(&programs, 0xff, sizeof(programs));
        assert(qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs) == QJS_INTL_NO_MEMORY);
        empty(&programs);
        assert(!output.live && input.calls == input_calls);
        output.fail_at = 0;
        assert(!qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs));
        verify(&programs);
        qjs_intl_date_single_programs_clear(&programs);
    }
    assert(qjs_intl_date_single_programs_open(&pool, &groups, groups.count, &b, &programs) == QJS_INTL_INVALID_ARGUMENT);
    empty(&programs);
    assert(qjs_intl_date_single_programs_open(NULL, &groups, group, &b, &programs) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_single_programs_open(&pool, &groups, group, NULL, &programs) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_single_programs_open(&pool, &groups, group, &b, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs));
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);
    assert(!input.live);
    verify(&programs); /* Both programs remain owned after releasing inputs. */
    qjs_intl_date_single_programs_clear(&programs);
    qjs_intl_date_single_programs_clear(&programs);

    pool_open(&a, flexible, 2, &pool, &groups);
    group = hour_group(&groups, QJS_INTL_DATETIME_NUMERIC);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs));
    verify(&programs);
    /* Direct family beats a lower-index, exact-width cross-family source. */
    assert(programs.normal.source_index == 1 && programs.normal.adaptations == QJS_INTL_DATE_ADAPT_HOUR_WIDTH);
    assert(programs.twelve.source_index == 0 && !programs.twelve.adaptations);
    qjs_intl_date_single_programs_clear(&programs);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);
    pool_open(&a, inverse, 2, &pool, &groups);
    group = hour_group(&groups, QJS_INTL_DATETIME_NUMERIC);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs));
    verify(&programs);
    assert(programs.normal.source_index == 0 && programs.twelve.source_index == 1);
    assert(programs.twelve.adaptations == QJS_INTL_DATE_ADAPT_HOUR_WIDTH);
    qjs_intl_date_single_programs_clear(&programs);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);
    pool_open(&a, periods, 2, &pool, &groups);
    group = hour_group(&groups, QJS_INTL_DATETIME_NUMERIC);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs));
    verify(&programs);
    assert(programs.normal.source_info.period_symbol == 'b' && programs.twelve.source_info.period_symbol == 'b');
    assert(programs.normal.adaptations == QJS_INTL_DATE_ADAPT_HOUR_FAMILY);
    group = hour_group(&groups, QJS_INTL_DATETIME_TWO_DIGIT);
    qjs_intl_date_single_programs_clear(&programs);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs));
    verify(&programs);
    assert(programs.normal.source_info.period_symbol == 'B' && programs.twelve.source_info.period_symbol == 'B');
    assert(programs.twelve.adaptations == QJS_INTL_DATE_ADAPT_HOUR_FAMILY);
    qjs_intl_date_single_programs_clear(&programs);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);

    pool_open(&a, period_tie, 2, &pool, &groups);
    assert(groups.count == 1);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, 0, &b, &programs));
    verify(&programs);
    /* Equal total costs choose original index zero, not period-class order. */
    assert(!programs.normal.source_index && !programs.twelve.source_index);
    assert(programs.normal.source_info.period_symbol == 'B' && programs.twelve.source_info.period_symbol == 'B');
    qjs_intl_date_single_programs_clear(&programs);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);

    pool_open(&a, contextual, 2, &pool, &groups);
    group = hour_group(&groups, QJS_INTL_DATETIME_NUMERIC);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs));
    verify(&programs);
    assert(programs.twelve.pattern.tokens[0].symbol == 'a' && programs.twelve.pattern.tokens[1].symbol == 'K');
    assert(equal(programs.twelve.pattern.tokens[4].literal, "\xe6\x99\x82"));
    assert(equal(programs.normal.pattern.tokens[3].literal, " Uhr"));
    qjs_intl_date_single_programs_clear(&programs);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);
    pool_open(&a, years, 3, &pool, &groups);
    group = hour_group(&groups, QJS_INTL_DATETIME_NUMERIC);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs));
    verify(&programs);
    assert(programs.normal.source_index == 1 && programs.twelve.source_index == 2);
    assert(programs.normal.source_info.year_kinds == QJS_INTL_DATE_YEAR_RELATED);
    qjs_intl_date_single_programs_clear(&programs);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);
    pool_open(&a, cyclic, 2, &pool, &groups);
    group = hour_group(&groups, QJS_INTL_DATETIME_NUMERIC);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, group, &b, &programs));
    verify(&programs);
    assert(programs.normal.source_info.year_kinds == (QJS_INTL_DATE_YEAR_RELATED | QJS_INTL_DATE_YEAR_CYCLIC));
    qjs_intl_date_single_programs_clear(&programs);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);

    pool_open(&a, mismatched, 2, &pool, &groups);
    calls = output.calls;
    for (i = 0; i < groups.count; i++) {
        assert(qjs_intl_date_single_programs_open(&pool, &groups, i, &b, &programs) == QJS_INTL_UNSUPPORTED);
        empty(&programs);
    }
    assert(calls == output.calls);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);
    pool_open(&a, incompatible, 2, &pool, &groups);
    calls = output.calls;
    for (i = 0; i < groups.count; i++) {
        assert(qjs_intl_date_single_programs_open(&pool, &groups, i, &b, &programs) == QJS_INTL_UNSUPPORTED);
        empty(&programs);
    }
    assert(calls == output.calls);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);
    for (i = 0; i < 2; i++) {
        pool_open(&a, missing + i, 1, &pool, &groups);
        calls = output.calls;
        assert(qjs_intl_date_single_programs_open(&pool, &groups, 0, &b, &programs) == QJS_INTL_UNSUPPORTED);
        empty(&programs);
        assert(calls == output.calls);
        qjs_intl_date_format_sources_clear(&groups);
        qjs_intl_date_source_candidates_clear(&pool);
    }
    /* Equal public components must not let source ordering hide a genuine
       numeric year. Preserve cyclic-only sources when there is no alternative. */
    for (i = 1; i <= 2; i++) {
        pool_open(&a, year_choice, i, &pool, &groups);
        assert(groups.count == 1);
        assert(!qjs_intl_date_single_programs_open(&pool, &groups, 0, &b, &programs));
        verify(&programs);
        assert(programs.normal.source_index == i - 1);
        assert(programs.normal.source_info.year_kinds ==
               (i == 1 ? QJS_INTL_DATE_YEAR_CYCLIC : QJS_INTL_DATE_YEAR_RELATED));
        assert(programs.normal.pattern.tokens[4].symbol == (i == 1 ? 'U' : 'r'));
        qjs_intl_date_single_programs_clear(&programs);
        qjs_intl_date_format_sources_clear(&groups);
        qjs_intl_date_source_candidates_clear(&pool);
    }
    pool_open(&a, year_pairs, 4, &pool, &groups);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, 0, &b, &programs));
    verify(&programs);
    assert(programs.normal.source_index == 2 && programs.twelve.source_index == 3);
    qjs_intl_date_single_programs_clear(&programs);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);
    pool_open(&a, cyclic_fallback, 3, &pool, &groups);
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, 0, &b, &programs));
    verify(&programs);
    assert(programs.normal.source_index == 0 && programs.twelve.source_index == 1);
    qjs_intl_date_single_programs_clear(&programs);
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);

    pool_open(&a, no_hour, 1, &pool, &groups);
    calls = output.calls;
    assert(!qjs_intl_date_single_programs_open(&pool, &groups, 0, &b, &programs));
    verify(&programs);
    assert(output.calls == calls + 1 && !programs.twelve.pattern.storage && programs.normal.pattern.tokens[0].symbol == 'L');
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);
    verify(&programs);
    qjs_intl_date_single_programs_clear(&programs);
    qjs_intl_date_single_programs_clear(NULL);
    assert(!input.live && !output.live);
    return 0;
}
