/* Whole time-style sources, explicit numeric widths and proven zone appends.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/style-hour.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;
static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        ++state->live;
    return ptr;
}
static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    --state->live;
    free(ptr);
}
static QJSIntlBytes bytes(const char *text) { return (QJSIntlBytes){text, strlen(text)}; }
static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
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
static uint32_t calendar_id(const QJSIntlDateStyleData *data, const char *name)
{
    uint32_t i;
    for (i = 0; i < data->calendar_count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_date_style_calendar_at(data, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}
static void pool_open(const QJSIntlAllocator *allocator, const char *const *patterns, size_t count,
                      QJSIntlDateSourceCandidates *pool)
{
    size_t i;
    memset(pool, 0, sizeof(*pool));
    pool->allocator = *allocator;
    pool->records = allocator->malloc(allocator->opaque, count * sizeof(*pool->records));
    assert(pool->records);
    pool->count = count;
    memset(pool->records, 0, count * sizeof(*pool->records));
    for (i = 0; i < count; i++) {
        assert(!qjs_intl_date_pattern_compile(allocator, bytes(patterns[i]), &pool->records[i].pattern));
        assert(!qjs_intl_date_pattern_info(&pool->records[i].pattern, &pool->records[i].info));
    }
}
static void pattern_equal(const QJSIntlAllocator *allocator, const QJSIntlDatePattern *pattern, const char *text)
{
    QJSIntlDatePattern expected;
    size_t i;
    assert(!qjs_intl_date_pattern_compile(allocator, bytes(text), &expected));
    assert(pattern->count == expected.count);
    for (i = 0; i < pattern->count; i++) {
        const QJSIntlDatePatternToken *a = &pattern->tokens[i], *b = &expected.tokens[i];
        assert(a->symbol == b->symbol && a->width == b->width && a->literal.length == b->literal.length);
        assert(!a->literal.length || !memcmp(a->literal.data, b->literal.data, a->literal.length));
    }
    qjs_intl_date_pattern_clear(allocator, &expected);
}
static QJSIntlDateStyleSource style(const QJSIntlDateStyleData *data, uint32_t locale,
                                    uint32_t calendar, QJSIntlDateStyleWidth width,
                                    const QJSIntlAllocator *allocator)
{
    QJSIntlDateStyleSource source;
    assert(!qjs_intl_date_style_source_open(data, locale, calendar, QJS_INTL_DATE_STYLE_TIME,
                                           width, 0, allocator, &source));
    return source;
}
static void append_open(const QJSIntlDateAvailableData *data, uint32_t locale, const char *calendar,
                        const QJSIntlAllocator *allocator, QJSIntlDateAvailableSources *out)
{
    uint32_t i, group, head;
    QJSIntlDateAvailableGroup key = {0};
    for (i = 0; i < data->calendar_count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_date_available_calendar_at(data, i, &text));
        if (equal(text, calendar))
            break;
    }
    assert(i < data->calendar_count);
    key.calendar = i;
    key.family = QJS_INTL_DATE_AVAILABLE_APPEND;
    assert(!qjs_intl_date_available_find_group(data, &key, &group));
    assert(!qjs_intl_date_available_head(data, locale, group, &head));
    assert(!qjs_intl_date_available_sources_open(data, head, allocator, out));
}
static void verify(const QJSIntlDateSinglePrograms *programs)
{
    QJSIntlDatePatternInfo info;
    size_t i, j;
    for (j = 0; j < 2; j++) {
        const QJSIntlDateSingleProgram *program = j ? &programs->twelve : &programs->normal;
        assert(!qjs_intl_date_pattern_info(&program->pattern, &info));
        for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
            assert(info.components.values[i] == programs->components.values[i]);
        assert(j ? (info.hour_symbol == 'h' || info.hour_symbol == 'K') :
                   (info.hour_symbol == 'H' || info.hour_symbol == 'k'));
        assert(j ? info.period_symbol != 0 : info.period_symbol != 'a');
    }
}
static void check(const QJSIntlDateStyleData *styles, const QJSIntlDateAvailableData *available,
                   QJSIntlAllocator *allocator)
{
    static const char *const bare[] = {"HH:mm:ss"};
    static const struct { const char *calendar, *expected; } layouts[] = {
        {"plain", "H:mm:ss zzzz"}, {"paren", "H:mm:ss (zzzz)"},
        {"reverse", "zzzz H:mm:ss"}, {"comma", "H:mm:ss, zzzz"},
        {"cyrillic", "H:mm:ss ч. zzzz"}, {"letters", "'''at''' H:mm:ss + zzzz"}
    };
    static const char *const whole[] = {"HH:mm:ss vvvv", "HH:mm:ss zzzz", "HH:mm:ss"};
    static const char *const twelve[] = {"h:mm:ss a"};
    static const char *const narrow_numeric[] = {"h:m:s a"};
    static const char *const no_period[] = {"HH:m:s", "h:m:s"};
    Allocation *state = allocator->opaque;
    QJSIntlDateSourceCandidates pool;
    QJSIntlDateAvailableSources append;
    QJSIntlDateStyleSource source;
    QJSIntlDateSinglePrograms result, zero = {0};
    uint32_t en = locale_id(&styles->data, "en-GB"), plain = calendar_id(styles, "plain");
    size_t i, j, baseline;
    pool_open(allocator, bare, 1, &pool);
    for (i = 0; i < sizeof(layouts) / sizeof(*layouts); i++) {
        source = style(styles, en, calendar_id(styles, layouts[i].calendar), QJS_INTL_DATE_STYLE_FULL, allocator);
        append_open(available, en, layouts[i].calendar, allocator, &append);
        assert(!qjs_intl_date_style_time_programs_open(&source, &pool, &append, allocator, &result));
        verify(&result);
        assert(result.normal.source_index == 0 && result.twelve.source_index == SIZE_MAX);
        assert(result.normal.adaptations == (QJS_INTL_DATE_ADAPT_HOUR_WIDTH | QJS_INTL_DATE_ADAPT_ZONE_APPEND));
        assert(!result.twelve.adaptations);
        assert(result.normal.source_info.components.values[QJS_INTL_DATETIME_ZONE] == 0);
        pattern_equal(allocator, &result.normal.pattern, layouts[i].expected);
        pattern_equal(allocator, &source.pattern, "h:mm:ss a zzzz");
        baseline = state->live;
        for (j = 1; j <= 3; j++) {
            QJSIntlDateSinglePrograms failed;
            state->calls = 0;
            state->fail_at = j;
            assert(qjs_intl_date_style_time_programs_open(&source, &pool, &append, allocator, &failed) == QJS_INTL_NO_MEMORY);
            assert(!memcmp(&failed, &zero, sizeof(failed)) && state->live == baseline);
            state->fail_at = 0;
        }
        qjs_intl_date_style_source_clear(&source);
        qjs_intl_date_available_sources_clear(&append);
        pattern_equal(allocator, &result.normal.pattern, layouts[i].expected);
        qjs_intl_date_single_programs_clear(&result);
    }
    for (i = 0; i < 4; i++) {
        const char *calendar = i == 0 ? "blocked" : i == 1 ? "empty" : i == 2 ? "missing" : "malformed";
        source = style(styles, en, calendar_id(styles, calendar), QJS_INTL_DATE_STYLE_FULL, allocator);
        append_open(available, en, calendar, allocator, &append);
        assert(qjs_intl_date_style_time_programs_open(&source, &pool, &append, allocator, &result) ==
               (i == 3 ? QJS_INTL_DATA_ERROR : QJS_INTL_UNSUPPORTED));
        assert(!memcmp(&result, &zero, sizeof(result)));
        qjs_intl_date_style_source_clear(&source);
        qjs_intl_date_available_sources_clear(&append);
    }
    qjs_intl_date_source_candidates_clear(&pool);
    pool_open(allocator, whole, 3, &pool);
    source = style(styles, en, plain, QJS_INTL_DATE_STYLE_FULL, allocator);
    assert(!qjs_intl_date_style_time_programs_open(&source, &pool, NULL, allocator, &result));
    assert(result.normal.source_index == 1 && !(result.normal.adaptations & QJS_INTL_DATE_ADAPT_ZONE_APPEND));
    pattern_equal(allocator, &result.normal.pattern, "H:mm:ss zzzz");
    qjs_intl_date_single_programs_clear(&result);
    qjs_intl_date_style_source_clear(&source);
    qjs_intl_date_source_candidates_clear(&pool);
    pool_open(allocator, twelve, 1, &pool);
    source = style(styles, en, plain, QJS_INTL_DATE_STYLE_MEDIUM, allocator);
    assert(!qjs_intl_date_style_time_programs_open(&source, &pool, NULL, allocator, &result));
    assert(result.twelve.adaptations == (QJS_INTL_DATE_ADAPT_MINUTE_WIDTH | QJS_INTL_DATE_ADAPT_SECOND_WIDTH));
    pattern_equal(allocator, &result.twelve.pattern, "h:m:s a");
    assert(result.twelve.source_info.components.values[QJS_INTL_DATETIME_MINUTE] == QJS_INTL_DATETIME_TWO_DIGIT);
    qjs_intl_date_single_programs_clear(&result);
    qjs_intl_date_style_source_clear(&source);
    source = style(styles, en, plain, QJS_INTL_DATE_STYLE_LONG, allocator);
    append_open(available, en, "plain", allocator, &append);
    assert(!qjs_intl_date_style_time_programs_open(&source, &pool, &append, allocator, &result));
    pattern_equal(allocator, &result.twelve.pattern, "hh:mm:ss a z");
    qjs_intl_date_source_candidates_clear(&pool);
    qjs_intl_date_style_source_clear(&source);
    qjs_intl_date_available_sources_clear(&append);
    pattern_equal(allocator, &result.twelve.pattern, "hh:mm:ss a z");
    qjs_intl_date_single_programs_clear(&result);
    pool_open(allocator, narrow_numeric, 1, &pool);
    source = style(styles, en, plain, QJS_INTL_DATE_STYLE_LONG, allocator);
    append_open(available, en, "plain", allocator, &append);
    assert(!qjs_intl_date_style_time_programs_open(&source, &pool, &append, allocator, &result));
    assert(result.twelve.adaptations == (QJS_INTL_DATE_ADAPT_HOUR_WIDTH | QJS_INTL_DATE_ADAPT_MINUTE_WIDTH |
                                           QJS_INTL_DATE_ADAPT_SECOND_WIDTH | QJS_INTL_DATE_ADAPT_ZONE_APPEND));
    pattern_equal(allocator, &result.twelve.pattern, "hh:mm:ss a z");
    qjs_intl_date_single_programs_clear(&result);
    qjs_intl_date_style_source_clear(&source);
    qjs_intl_date_available_sources_clear(&append);
    qjs_intl_date_source_candidates_clear(&pool);
    source = style(styles, en, plain, QJS_INTL_DATE_STYLE_SHORT, allocator);
    assert(!qjs_intl_date_style_time_programs_open(&source, NULL, NULL, allocator, &result));
    assert(result.normal.source_index == SIZE_MAX && result.twelve.source_index == SIZE_MAX);
    assert(result.normal.adaptations == QJS_INTL_DATE_ADAPT_HOUR_FAMILY);
    pattern_equal(allocator, &result.normal.pattern, "BH:mm [z]");
    pattern_equal(allocator, &result.twelve.pattern, "Bh:mm [z]");
    qjs_intl_date_single_programs_clear(&result);
    qjs_intl_date_style_source_clear(&source);
    pool_open(allocator, no_period, 2, &pool);
    source = style(styles, en, plain, QJS_INTL_DATE_STYLE_MEDIUM, allocator);
    assert(qjs_intl_date_style_time_programs_open(&source, &pool, NULL, allocator, &result) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_date_style_time_programs_open(NULL, &pool, NULL, allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_time_programs_open(&source, &pool, NULL, allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_date_style_source_clear(&source);
    qjs_intl_date_source_candidates_clear(&pool);
    assert(!state->live);
}
int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData shared;
    QJSIntlDateStyleData styles;
    QJSIntlDateAvailableData available;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &shared));
    assert(!qjs_intl_date_style_data_open(&shared, &allocator, &styles));
    assert(!qjs_intl_date_available_data_open(&shared, &allocator, &available));
    check(&styles, &available, &allocator);
    return 0;
}
