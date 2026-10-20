/* Required formats preserve honest fields, source masks and owned programs.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/ordinary-programs.h"
#include <assert.h>
#include <stdio.h>
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

static QJSIntlBytes bytes(const char *text) { return (QJSIntlBytes){text, strlen(text)}; }

static void inventory(const QJSIntlDateAvailableData *data, uint32_t locale, uint32_t calendar,
                      QJSIntlDateAvailableFamily family, const QJSIntlAllocator *allocator,
                      QJSIntlDateAvailableSources *out)
{
    QJSIntlDateAvailableGroup key = {0};
    uint32_t index, head;
    key.calendar = calendar;
    key.family = family;
    assert(!qjs_intl_date_available_find_group(data, &key, &index));
    assert(!qjs_intl_date_available_head(data, locale, index, &head));
    assert(!qjs_intl_date_available_sources_open(data, head, allocator, out));
}

static unsigned int mask(const QJSIntlDateTimeComponents *components)
{
    size_t i;
    unsigned int result = 0;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        if (components->values[i])
            result |= 1U << i;
    return result;
}

static void verify(const QJSIntlDateOrdinaryPrograms *programs)
{
    /* The normative fourteen combinations, spelled independently of the
       constructor's mask table: w=weekday, B=dayPeriod, S=fractional digits. */
    static const char fields[] = "wGyMdBHmsSz";
    static const char *const required[] = {"wyMdHmsS", "wyMdHms", "wyMd", "yMd", "yM", "Md", "M",
                                          "HmsS", "Hms", "Hm", "BHmsS", "BHms", "BHm", "BH"};
    unsigned int seen[sizeof(required) / sizeof(*required)] = {0};
    size_t i, j, field;
    assert(programs->count);
    for (i = 0; i < programs->count; i++) {
        const QJSIntlDateOrdinaryProgram *record = &programs->records[i];
        QJSIntlDatePatternInfo normal, twelve;
        assert(!qjs_intl_date_pattern_info(&record->single.normal.pattern, &normal));
        for (field = 0; field < QJS_INTL_DATETIME_COMPONENT_COUNT; field++)
            assert(normal.components.values[field] == record->single.components.values[field]);
        for (j = 0; j < sizeof(required) / sizeof(*required); j++) {
            const char *at;
            unsigned int wanted = 0;
            for (at = required[j]; *at; at++) {
                const char *field = strchr(fields, *at);
                assert(field);
                wanted |= 1U << (field - fields);
            }
            if (mask(&normal.components) == wanted)
                seen[j]++;
        }
        for (j = 0; j < i; j++)
            assert(memcmp(&record->single.components, &programs->records[j].single.components,
                          sizeof(record->single.components)));
        if (normal.hour_symbol) {
            assert(!qjs_intl_date_pattern_info(&record->single.twelve.pattern, &twelve));
            assert(normal.hour_symbol == 'H' || normal.hour_symbol == 'k');
            assert(twelve.hour_symbol == 'h' || twelve.hour_symbol == 'K');
            assert(normal.year_kinds == twelve.year_kinds);
            assert(!memcmp(&normal.components, &twelve.components, sizeof(normal.components)));
            assert(twelve.period_symbol == (normal.period_symbol ? normal.period_symbol : 'a'));
        }
        if (record->first != SIZE_MAX)
            assert(record->first < i);
        if (record->second != SIZE_MAX)
            assert(record->second < i);
    }
    for (j = 0; j < sizeof(required) / sizeof(*required); j++)
        assert(seen[j]);
}

static const QJSIntlDateOrdinaryProgram *find(const QJSIntlDateOrdinaryPrograms *programs,
                                              unsigned int fields, int fraction)
{
    size_t i;
    for (i = 0; i < programs->count; i++)
        if (mask(&programs->records[i].single.components) == fields &&
            (!fraction || programs->records[i].single.components.values[QJS_INTL_DATETIME_FRACTION] == fraction))
            return &programs->records[i];
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
        assert(a->symbol == b->symbol && a->width == b->width && a->literal.length == b->literal.length);
        assert(!a->literal.length || !memcmp(a->literal.data, b->literal.data, a->literal.length));
    }
    qjs_intl_date_pattern_clear(allocator, &expected);
}

static void authored(const QJSIntlAllocator *allocator, const QJSIntlDateSourceCandidates *pool,
                     const QJSIntlDateOrdinaryPrograms *programs)
{
    const QJSIntlDateOrdinaryProgram *record;
    size_t i;
    record = find(programs, 12, 0); /* year + month */
    assert(record->construction == QJS_INTL_DATE_ORDINARY_APPEND);
    assert(record->append.code.length == 4 && !memcmp(record->append.code.data, "Year", 4));
    assert(record->origin.origin_locale.length == 4 && !memcmp(record->origin.origin_locale.data, "root", 4));
    pattern_equal(allocator, &record->single.normal.pattern, "y' [year] 'L");
    record = find(programs, 29, 0); /* weekday + year + month + day */
    pattern_equal(allocator, &record->single.normal.pattern, "EEE', 'y' [year] 'M/d");
    record = find(programs, 192, 0); /* plain hour + minute */
    assert(record->construction & QJS_INTL_DATE_ORDINARY_PERIOD);
    assert(record->single.twelve.source_info.period_symbol == 'B');
    pattern_equal(allocator, &record->single.normal.pattern, "H:mm");
    pattern_equal(allocator, &record->single.twelve.pattern, "a h:mm");
    record = find(programs, 224, 0); /* explicit dayPeriod + hour + minute */
    pattern_equal(allocator, &record->single.normal.pattern, "B H:mm");
    pattern_equal(allocator, &record->single.twelve.pattern, "B h:mm");
    record = find(programs, 960, 3); /* clock with localized fractional separator */
    pattern_equal(allocator, &record->single.normal.pattern, "H:mm:ss\xd9\xabSSS");
    pattern_equal(allocator, &record->single.twelve.pattern, "a h:mm:ss\xd9\xabSSS");
    record = find(programs, 477, 0); /* complete date + clock, exact connector */
    assert(record->construction == QJS_INTL_DATE_ORDINARY_COMBINE);
    assert(record->connector.origin_width == QJS_INTL_DATE_STYLE_SHORT &&
           record->connector.origin_form == QJS_INTL_DATE_COMBINATION_STANDARD);
    pattern_equal(allocator, &record->single.twelve.pattern, "EEE', 'y' [year] 'M/d' at 'a h:mm:ss");
    for (i = 0; i < pool->count; i++)
        if (pool->records[i].source.code.length == 3 && !memcmp(pool->records[i].source.code.data, "Bhm", 3)) {
            assert(pool->records[i].info.period_symbol == 'B');
            pattern_equal(allocator, &pool->records[i].pattern, "B h:mm");
        }
}

static void run(const QJSIntlAllocator *allocator, const uint8_t *raw, size_t length,
                int blocked, int failures)
{
    QJSIntlData pack;
    QJSIntlDateAvailableData available;
    QJSIntlDateCombinationData connectors;
    QJSIntlDateAvailableSources source, append;
    QJSIntlDateSourceCandidates pool;
    QJSIntlDateFormatSources groups;
    QJSIntlDateOrdinaryPrograms programs, zero = {0};
    Allocation *state = allocator->opaque;
    size_t baseline, calls, i;
    assert(!qjs_intl_data_open(raw, length, allocator, &pack));
    assert(!qjs_intl_date_available_data_open(&pack, allocator, &available));
    assert(!qjs_intl_date_combination_data_open(&pack, allocator, &connectors));
    inventory(&available, 0, 0, QJS_INTL_DATE_AVAILABLE_PATTERN, allocator, &source);
    inventory(&available, 0, 0, QJS_INTL_DATE_AVAILABLE_APPEND, allocator, &append);
    assert(!qjs_intl_date_source_candidates_open(&source, (QJSIntlBytes){0}, allocator, &pool));
    assert(!qjs_intl_date_format_sources_open(&pool, allocator, &groups));
    baseline = state->live;
    assert(qjs_intl_date_ordinary_programs_open(&pool, &groups, &append, &connectors, 0, 0,
               bytes(""), allocator, &programs) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&programs, &zero, sizeof(programs)) && state->live == baseline);
    assert(qjs_intl_date_ordinary_programs_open(&pool, &groups, &append, &connectors, 0, 0,
               bytes("\xc0\x80"), allocator, &programs) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&programs, &zero, sizeof(programs)) && state->live == baseline);
    if (blocked) {
        assert(qjs_intl_date_ordinary_programs_open(&pool, &groups, &append, &connectors, 0, 0,
                   bytes("."), allocator, &programs) == QJS_INTL_UNSUPPORTED);
        assert(!memcmp(&programs, &zero, sizeof(programs)) && state->live == baseline);
    } else {
        state->calls = 0;
        assert(!qjs_intl_date_ordinary_programs_open(&pool, &groups, &append, &connectors, 0, 0,
                    bytes("\xd9\xab"), allocator, &programs));
        calls = state->calls;
        verify(&programs);
        authored(allocator, &pool, &programs);
        qjs_intl_date_ordinary_programs_clear(&programs);
        if (failures) {
            for (i = 1; i <= calls; i++) {
                state->calls = 0;
                state->fail_at = i;
                assert(qjs_intl_date_ordinary_programs_open(&pool, &groups, &append, &connectors, 0, 0,
                           bytes("."), allocator, &programs) == QJS_INTL_NO_MEMORY);
                assert(!memcmp(&programs, &zero, sizeof(programs)) && state->live == baseline);
            }
            state->calls = state->fail_at = 0;
        }
        assert(!qjs_intl_date_ordinary_programs_open(&pool, &groups, &append, &connectors, 0, 0,
                    bytes("."), allocator, &programs));
    }
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_source_candidates_clear(&pool);
    qjs_intl_date_available_sources_clear(&append);
    qjs_intl_date_available_sources_clear(&source);
    if (!blocked) {
        verify(&programs); /* Pattern storage is independent of all input owners. */
        qjs_intl_date_ordinary_programs_clear(&programs);
    }
}

#ifdef TEST_INTL_ORDINARY_PROFILE
static void prepared(const QJSIntlAllocator *allocator)
{
    QJSIntlData pack;
    QJSIntlDateAvailableData available;
    QJSIntlDateCombinationData connectors;
    uint32_t locale, calendar;
    size_t checked = 0;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, allocator, &pack));
    assert(!qjs_intl_date_available_data_open(&pack, allocator, &available));
    assert(!qjs_intl_date_combination_data_open(&pack, allocator, &connectors));
    for (locale = 0; locale < available.section.count; locale++) {
        QJSIntlBytes tag;
        uint32_t parent;
        assert(!qjs_intl_data_locale(&pack, locale, &tag, &parent));
        if (TEST_INTL_ORDINARY_PROFILE == 1 &&
            !((tag.length == 2 && (!memcmp(tag.data, "en", 2) || !memcmp(tag.data, "gl", 2))) ||
              (tag.length == 3 && !memcmp(tag.data, "kek", 3)) ||
              (tag.length == 7 && !memcmp(tag.data, "zh-Hant", 7))))
            continue;
        for (calendar = 0; calendar < available.calendar_count; calendar++) {
            QJSIntlDateAvailableSources source, append;
            QJSIntlDateSourceCandidates pool;
            QJSIntlDateFormatSources groups;
            QJSIntlDateOrdinaryPrograms programs;
            QJSIntlBytes raw;
            QJSIntlStatus status;
            uint32_t cc;
            assert(!qjs_intl_date_available_calendar_at(&available, calendar, &raw));
            for (cc = 0; cc < connectors.calendar_count; cc++) {
                QJSIntlBytes name;
                assert(!qjs_intl_date_combination_calendar_at(&connectors, cc, &name));
                if (name.length == raw.length && !memcmp(name.data, raw.data, raw.length))
                    break;
            }
            assert(cc < connectors.calendar_count);
            inventory(&available, locale, calendar, QJS_INTL_DATE_AVAILABLE_PATTERN, allocator, &source);
            inventory(&available, locale, calendar, QJS_INTL_DATE_AVAILABLE_APPEND, allocator, &append);
            assert(!qjs_intl_date_source_candidates_open(&source, (QJSIntlBytes){0}, allocator, &pool));
            assert(!qjs_intl_date_format_sources_open(&pool, allocator, &groups));
            status = qjs_intl_date_ordinary_programs_open(&pool, &groups, &append, &connectors,
                         locale, cc, bytes("."), allocator, &programs);
            if (status)
                fprintf(stderr, "ordinary status %d at %.*s/%.*s\n", status,
                        (int)tag.length, tag.data, (int)raw.length, raw.data);
            assert(!status);
            verify(&programs);
            qjs_intl_date_ordinary_programs_clear(&programs);
            qjs_intl_date_format_sources_clear(&groups);
            qjs_intl_date_source_candidates_clear(&pool);
            qjs_intl_date_available_sources_clear(&append);
            qjs_intl_date_available_sources_clear(&source);
            checked++;
        }
    }
    assert(checked);
    printf("checked %zu prepared locale/calendar heads\n", checked);
}
#endif

extern const uint8_t qjs_intl_date_ordinary_fixture_bytes[], qjs_intl_date_ordinary_blocked_bytes[];
extern const size_t qjs_intl_date_ordinary_fixture_length, qjs_intl_date_ordinary_blocked_length;

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    run(&allocator, qjs_intl_date_ordinary_fixture_bytes, qjs_intl_date_ordinary_fixture_length, 0, 1);
    run(&allocator, qjs_intl_date_ordinary_blocked_bytes, qjs_intl_date_ordinary_blocked_length, 1, 0);
#ifdef TEST_INTL_ORDINARY_PROFILE
    prepared(&allocator);
#endif
    assert(!state.live);
    return 0;
}
