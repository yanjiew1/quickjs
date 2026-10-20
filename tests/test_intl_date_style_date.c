/* Native calendar-year reconstruction preserves unsupported style provenance.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/style-date.h"
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

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int equal(QJSIntlBytes a, const char *b)
{
    return a.length == strlen(b) && (!a.length || !memcmp(a.data, b, a.length));
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

static uint32_t calendar_id(const QJSIntlDateStyleData *styles, const char *name)
{
    uint32_t i;
    for (i = 0; i < styles->calendar_count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_date_style_calendar_at(styles, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}

static void pattern_equal(const QJSIntlAllocator *allocator, const QJSIntlDatePattern *pattern,
                           const char *text)
{
    QJSIntlDatePattern expected;
    size_t i;
    assert(!qjs_intl_date_pattern_compile(allocator, bytes(text), &expected));
    assert(pattern->count == expected.count);
    for (i = 0; i < pattern->count; i++) {
        const QJSIntlDatePatternToken *a = &pattern->tokens[i], *b = &expected.tokens[i];
        assert(a->symbol == b->symbol && a->width == b->width);
        assert(a->literal.length == b->literal.length);
        assert(!a->literal.length || !memcmp(a->literal.data, b->literal.data, a->literal.length));
    }
    qjs_intl_date_pattern_clear(allocator, &expected);
}

static QJSIntlDateSourceCandidates candidates(const QJSIntlDateAvailableData *data,
                                               uint32_t locale, const char *name,
                                               const QJSIntlAllocator *allocator)
{
    uint32_t i, group, head;
    QJSIntlDateAvailableGroup key = {0};
    QJSIntlDateAvailableSources sources;
    QJSIntlDateSourceCandidates result;
    for (i = 0; i < data->calendar_count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_date_available_calendar_at(data, i, &text));
        if (equal(text, name))
            break;
    }
    assert(i < data->calendar_count);
    key.calendar = i;
    key.family = QJS_INTL_DATE_AVAILABLE_PATTERN;
    assert(!qjs_intl_date_available_find_group(data, &key, &group));
    assert(!qjs_intl_date_available_head(data, locale, group, &head));
    assert(!qjs_intl_date_available_sources_open(data, head, allocator, &sources));
    assert(!qjs_intl_date_source_candidates_open(&sources, bytes(""), allocator, &result));
    qjs_intl_date_available_sources_clear(&sources);
    return result;
}

static void check(const QJSIntlDateStyleData *styles, const QJSIntlDateAvailableData *available,
                   const QJSIntlAllocator *allocator)
{
    const char *names[] = {"exact", "galician", "kekchi", "priority"};
    const char *patterns[] = {"y-MM-dd", "EEEE, d 'de' MMMM 'de' y G", "dd-MM-y G", "EEEE [d] MMMM y G"};
    Allocation *state = allocator->opaque;
    uint32_t root = locale_id(&styles->data, "root"), en = locale_id(&styles->data, "en");
    size_t j, i;
    for (j = 0; j < sizeof(names) / sizeof(*names); j++) {
        QJSIntlDateSourceCandidates pool = candidates(available, root, names[j], allocator);
        uint32_t calendar = calendar_id(styles, names[j]), selected = j == 2 ? en : root;
        QJSIntlDatePreparedStyle result, zero = {0};
        QJSIntlDateStyleSource exact, exact_zero = {0};
        QJSIntlDateStylePrograms composed;
        size_t calls, baseline = state->live;
        state->calls = 0;
        assert(!qjs_intl_date_style_date_open(styles, selected, calendar, QJS_INTL_DATE_STYLE_FULL, 0,
                                             &pool, allocator, &result));
        calls = state->calls;
        assert(result.style.info.year_kinds == QJS_INTL_DATE_YEAR_CALENDAR);
        assert(result.adaptations == (j ? QJS_INTL_DATE_STYLE_REBUILT : 0));
        assert((result.source_index == SIZE_MAX) == !j);
        if (j) {
            assert(result.source_index < pool.count);
            assert(equal(result.replacement.code, j == 1 ? "GyMMMEd" : j == 2 ? "GyMd" : "GyMMMMEEEEd"));
            assert(equal(result.origin.origin_locale, "root"));
            assert(qjs_intl_date_style_source_open(styles, selected, calendar, QJS_INTL_DATE_STYLE_DATE,
                         QJS_INTL_DATE_STYLE_FULL, 0, allocator, &exact) == QJS_INTL_UNSUPPORTED);
            assert(!memcmp(&exact, &exact_zero, sizeof(exact)));
            assert(equal(result.style.source.text, j == 2 ? "dd/MM/Y G" : "EEEE, d 'de' MMMM 'de' Y G"));
            if (j == 1 || j == 2) {
                assert(equal(result.style.skeleton.text, j == 1 ? "GYMMMMEEEEd" : "GGGGGyMMdd"));
                assert(equal(result.style.skeleton.origin_locale, j == 1 ? "root" : "en"));
                assert(equal(result.style.source.origin_locale, "root"));
            }
        }
        qjs_intl_date_prepared_style_clear(&result);
        for (i = 1; i <= calls; i++) {
            state->calls = 0;
            state->fail_at = i;
            memset(&result, 0xff, sizeof(result));
            assert(qjs_intl_date_style_date_open(styles, selected, calendar, QJS_INTL_DATE_STYLE_FULL,
                       0, &pool, allocator, &result) == QJS_INTL_NO_MEMORY);
            assert(state->live == baseline && !memcmp(&result, &zero, sizeof(result)));
            state->fail_at = 0;
        }
        assert(!qjs_intl_date_style_date_open(styles, selected, calendar, QJS_INTL_DATE_STYLE_FULL,
                                             0, &pool, allocator, &result));
        qjs_intl_date_source_candidates_clear(&pool);
        pattern_equal(allocator, &result.style.pattern, patterns[j]);
        assert(!qjs_intl_date_style_programs_open(&result.style, NULL, NULL, NULL, 0, 0, allocator, &composed));
        qjs_intl_date_prepared_style_clear(&result);
        pattern_equal(allocator, &composed.normal, patterns[j]);
        qjs_intl_date_style_programs_clear(&composed);
        assert(!state->live);
    }
    {
        const char *bad[] = {"override", "context", "numeric", "quarter", "twodigit",
                             "duplicate", "mixed", "related", "time", "blocked", "empty", "missing"};
        for (j = 0; j < sizeof(bad) / sizeof(*bad); j++) {
            QJSIntlDateSourceCandidates pool = candidates(available, en, bad[j], allocator);
            QJSIntlDatePreparedStyle result, zero = {0};
            size_t baseline = state->live;
            memset(&result, 0xff, sizeof(result));
            assert(qjs_intl_date_style_date_open(styles, en, calendar_id(styles, bad[j]),
                       QJS_INTL_DATE_STYLE_FULL, 0, &pool, allocator, &result) == QJS_INTL_UNSUPPORTED);
            assert(!memcmp(&result, &zero, sizeof(result)) && state->live == baseline);
            qjs_intl_date_source_candidates_clear(&pool);
        }
    }
    {
        QJSIntlDatePreparedStyle result, zero = {0};
        uint32_t calendar = calendar_id(styles, "exact");
        assert(!qjs_intl_date_style_date_open(styles, root, calendar, QJS_INTL_DATE_STYLE_SHORT,
                                             0, NULL, allocator, &result));
        assert(result.source_index == SIZE_MAX);
        qjs_intl_date_prepared_style_clear(&result);
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_style_date_open(styles, root, calendar_id(styles, "galician"),
                   QJS_INTL_DATE_STYLE_FULL, 0, NULL, allocator, &result) == QJS_INTL_UNSUPPORTED);
        assert(!memcmp(&result, &zero, sizeof(result)));
        assert(qjs_intl_date_style_date_open(styles, root, calendar, QJS_INTL_DATE_STYLE_FULL,
                   0, NULL, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
        assert(!memcmp(&result, &zero, sizeof(result)));
    }
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlDateStyleData styles;
    QJSIntlDateAvailableData available;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_date_style_data_open(&data, &allocator, &styles));
    assert(!qjs_intl_date_available_data_open(&data, &allocator, &available));
    assert(!state.live);
    check(&styles, &available, &allocator);
    qjs_intl_date_prepared_style_clear(NULL);
    return 0;
}
