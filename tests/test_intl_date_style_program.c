/* Exact style selection, prepared hour variants and connector composition.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/style-program.h"
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

static uint32_t style_calendar(const QJSIntlDateStyleData *data, const char *name)
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

static uint32_t connector_calendar(const QJSIntlDateCombinationData *data, const char *name)
{
    uint32_t i;
    for (i = 0; i < data->calendar_count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_date_combination_calendar_at(data, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}

static void pattern_equal(const QJSIntlAllocator *allocator, const QJSIntlDatePattern *pattern, const char *text)
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

static QJSIntlDateSinglePrograms pair(const QJSIntlAllocator *allocator,
                                      const char *normal, const char *twelve)
{
    QJSIntlDateSinglePrograms result = {0};
    QJSIntlDatePatternInfo info;
    result.allocator = *allocator;
    assert(!qjs_intl_date_pattern_compile(allocator, bytes(normal), &result.normal.pattern));
    assert(!qjs_intl_date_pattern_compile(allocator, bytes(twelve), &result.twelve.pattern));
    assert(!qjs_intl_date_pattern_info(&result.normal.pattern, &info));
    result.components = info.components;
    return result;
}

static QJSIntlDateStyleSource source(const QJSIntlDateStyleData *data, const QJSIntlAllocator *allocator,
                                     uint32_t locale, uint32_t calendar, QJSIntlDateStyleFamily family,
                                     QJSIntlDateStyleWidth width)
{
    QJSIntlDateStyleSource result;
    assert(!qjs_intl_date_style_source_open(data, locale, calendar, family, width, 0, allocator, &result));
    return result;
}

static void check(const QJSIntlDateStyleData *styles, const QJSIntlDateCombinationData *connectors,
                   QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    uint32_t root = locale_id(&styles->data, "root"), en = locale_id(&styles->data, "en-GB");
    uint32_t greg = style_calendar(styles, "gregorian"), generic = style_calendar(styles, "generic");
    uint32_t jp = style_calendar(styles, "japanese"), connection = connector_calendar(connectors, "gregorian");
    QJSIntlDateStyleSource date, time, bad, source_zero = {0};
    QJSIntlDateStylePrograms result, zero = {0};
    QJSIntlDateSinglePrograms times;
    size_t i, baseline;
    assert(greg != connection);
    {
        uint32_t ascii;
        size_t base = state->live;
        for (ascii = 0; ascii < styles->alternate_count; ascii++) {
            QJSIntlBytes name;
            assert(!qjs_intl_date_style_alternate_at(styles, ascii, &name));
            if (equal(name, "ascii"))
                break;
        }
        assert(ascii < styles->alternate_count);
        assert(!qjs_intl_date_style_source_preferred(styles, en, greg, QJS_INTL_DATE_STYLE_TIME,
                   QJS_INTL_DATE_STYLE_MEDIUM, ascii, 0, allocator, &bad));
        assert(equal(bad.source.origin_alternate, "ascii"));
        assert(equal(bad.skeleton.text, "hm") && equal(bad.skeleton.origin_alternate, ""));
        assert(equal(bad.skeleton.numbers, "latn"));
        pattern_equal(allocator, &bad.pattern, "h:mm 'ASCII' a");
        qjs_intl_date_style_source_clear(&bad);
        state->calls = 0;
        state->fail_at = 1;
        assert(qjs_intl_date_style_source_preferred(styles, en, greg, QJS_INTL_DATE_STYLE_TIME,
                   QJS_INTL_DATE_STYLE_MEDIUM, ascii, 0, allocator, &bad) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&bad, &source_zero, sizeof(bad)) && state->live == base);
        state->calls = state->fail_at = 0;
        assert(!qjs_intl_date_style_source_preferred(styles, en, greg, QJS_INTL_DATE_STYLE_TIME,
                   QJS_INTL_DATE_STYLE_FULL, ascii, 0, allocator, &bad));
        assert(equal(bad.source.origin_alternate, ""));
        qjs_intl_date_style_source_clear(&bad);
        assert(!qjs_intl_date_style_source_preferred(styles, en, greg, QJS_INTL_DATE_STYLE_TIME,
                   QJS_INTL_DATE_STYLE_LONG, ascii, 0, allocator, &bad));
        assert(equal(bad.source.origin_alternate, ""));
        qjs_intl_date_style_source_clear(&bad);
        assert(!qjs_intl_date_style_source_preferred(styles, en, greg, QJS_INTL_DATE_STYLE_TIME,
                   QJS_INTL_DATE_STYLE_SHORT, ascii, 0, allocator, &bad));
        assert(equal(bad.source.origin_alternate, ""));
        assert(equal(bad.skeleton.origin_alternate, "ascii") && equal(bad.skeleton.text, "Bh"));
        qjs_intl_date_style_source_clear(&bad);
        assert(state->live == base);
    }
    date = source(styles, allocator, en, greg, QJS_INTL_DATE_STYLE_DATE, QJS_INTL_DATE_STYLE_FULL);
    time = source(styles, allocator, en, greg, QJS_INTL_DATE_STYLE_TIME, QJS_INTL_DATE_STYLE_MEDIUM);
    assert(equal(date.skeleton.text, "yMMMMEEEEd") && equal(time.skeleton.numbers, "latn"));
    times = pair(allocator, "H:mm", "h:mm a");
    assert(!qjs_intl_date_style_programs_open(&date, &time, &times, connectors, root, connection, allocator, &result));
    assert(result.connector.origin_width == QJS_INTL_DATE_STYLE_FULL);
    assert(result.connector.origin_form == QJS_INTL_DATE_COMBINATION_AT_TIME);
    pattern_equal(allocator, &result.normal, "EEEE, d MMMM y 'at' H:mm");
    pattern_equal(allocator, &result.twelve, "EEEE, d MMMM y 'at' h:mm a");
    qjs_intl_date_style_programs_clear(&result);
    assert(!qjs_intl_date_style_programs_open(&date, &time, &times, connectors, en, connection, allocator, &result));
    assert(result.connector.origin_form == QJS_INTL_DATE_COMBINATION_STANDARD);
    pattern_equal(allocator, &result.normal, "EEEE, d MMMM y, H:mm");
    qjs_intl_date_style_programs_clear(&result);
    /* Every partial allocation is released; inputs retain their ownership. */
    baseline = state->live;
    for (i = 1; i <= 3; i++) {
        state->calls = 0;
        state->fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_style_programs_open(&date, &time, &times, connectors, root, connection, allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(state->live == baseline && !memcmp(&result, &zero, sizeof(result)));
        state->fail_at = 0;
    }
    assert(!qjs_intl_date_style_programs_open(&date, NULL, NULL, NULL, UINT32_MAX, UINT32_MAX, allocator, &result));
    assert(!result.twelve.count && !result.connector.text.length && !result.time_source.text.length);
    qjs_intl_date_style_source_clear(&date);
    pattern_equal(allocator, &result.normal, "EEEE, d MMMM y");
    qjs_intl_date_style_programs_clear(&result);
    date = source(styles, allocator, root, greg, QJS_INTL_DATE_STYLE_DATE, QJS_INTL_DATE_STYLE_LONG);
    assert(equal(date.source.numbers, "M=romanlow"));
    assert(!qjs_intl_date_style_programs_open(&date, &time, &times, connectors, root, connection, allocator, &result));
    assert(equal(result.date_source.numbers, "M=romanlow"));
    assert(result.connector.origin_form == QJS_INTL_DATE_COMBINATION_STANDARD);
    pattern_equal(allocator, &result.normal, "H:mm / d MMMM y");
    qjs_intl_date_style_programs_clear(&result);
    qjs_intl_date_style_source_clear(&date);
    date = source(styles, allocator, root, greg, QJS_INTL_DATE_STYLE_DATE, QJS_INTL_DATE_STYLE_MEDIUM);
    assert(!qjs_intl_date_style_programs_open(&date, &time, &times, connectors, root, connection, allocator, &result));
    pattern_equal(allocator, &result.normal, "y-MM-dd ~ H:mm");
    qjs_intl_date_style_programs_clear(&result);
    qjs_intl_date_style_source_clear(&date);
    qjs_intl_date_single_programs_clear(&times);
    qjs_intl_date_style_source_clear(&time);
    date = source(styles, allocator, root, greg, QJS_INTL_DATE_STYLE_DATE, QJS_INTL_DATE_STYLE_SHORT);
    time = source(styles, allocator, root, greg, QJS_INTL_DATE_STYLE_TIME, QJS_INTL_DATE_STYLE_SHORT);
    times = pair(allocator, "BH:mm [z]", "Bh:mm [z]");
    assert(!qjs_intl_date_style_programs_open(&date, &time, &times, connectors, root, connection, allocator, &result));
    pattern_equal(allocator, &result.normal, "'{0}' y/M/d BH:mm [z]");
    pattern_equal(allocator, &result.twelve, "'{0}' y/M/d Bh:mm [z]");
    assert(result.components.values[QJS_INTL_DATETIME_DAY_PERIOD] == QJS_INTL_DATETIME_SHORT);
    qjs_intl_date_style_programs_clear(&result);
    qjs_intl_date_style_source_clear(&date);
    assert(!qjs_intl_date_style_programs_open(NULL, &time, &times, NULL, UINT32_MAX, UINT32_MAX, allocator, &result));
    qjs_intl_date_style_source_clear(&time);
    qjs_intl_date_single_programs_clear(&times);
    pattern_equal(allocator, &result.twelve, "Bh:mm [z]");
    qjs_intl_date_style_programs_clear(&result);
    time = source(styles, allocator, root, greg, QJS_INTL_DATE_STYLE_TIME, QJS_INTL_DATE_STYLE_FULL);
    times = pair(allocator, "H:mm:ss vvvv", "h:mm:ss a vvvv");
    assert(qjs_intl_date_style_programs_open(NULL, &time, &times, NULL, 0, 0, allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    qjs_intl_date_single_programs_clear(&times);
    times = pair(allocator, "H:mm:ss zzzz", "h:mm:ss a zzzz");
    assert(!qjs_intl_date_style_programs_open(NULL, &time, &times, NULL, 0, 0, allocator, &result));
    pattern_equal(allocator, &result.normal, "H:mm:ss zzzz");
    qjs_intl_date_style_programs_clear(&result);
    qjs_intl_date_single_programs_clear(&times);
    qjs_intl_date_style_source_clear(&time);
    /* The month/no-week CLDR workaround admits the date source; mixed family,
       no hour and missing/BLOCK/empty sources retain their existing behavior. */
    for (i = 0; i < 6; i++) {
        QJSIntlDateStyleFamily family = i == 1 || i == 2 ? QJS_INTL_DATE_STYLE_TIME : QJS_INTL_DATE_STYLE_DATE;
        QJSIntlDateStyleWidth width = i == 0 ? QJS_INTL_DATE_STYLE_SHORT :
                                      i == 1 || i == 3 ? QJS_INTL_DATE_STYLE_FULL :
                                      i == 2 || i == 5 ? QJS_INTL_DATE_STYLE_LONG : QJS_INTL_DATE_STYLE_MEDIUM;
        assert(qjs_intl_date_style_source_open(styles, en, i == 5 ? jp : generic, family, width, 0, allocator, &bad) ==
               (i == 0 || i == 5 ? QJS_INTL_OK : QJS_INTL_UNSUPPORTED));
        if (i == 0)
            pattern_equal(allocator, &bad.pattern, "dd/MM/y G");
        if (i == 0 || i == 5)
            qjs_intl_date_style_source_clear(&bad);
        assert(!memcmp(&bad, &source_zero, sizeof(bad)));
    }
    assert(qjs_intl_date_style_source_open(styles, root, jp, QJS_INTL_DATE_STYLE_DATE,
                                          QJS_INTL_DATE_STYLE_SHORT, 0, allocator, &bad) == QJS_INTL_UNSUPPORTED);
    date = source(styles, allocator, root, jp, QJS_INTL_DATE_STYLE_DATE, QJS_INTL_DATE_STYLE_LONG);
    assert(!qjs_intl_date_style_programs_open(&date, NULL, NULL, NULL, 0, 0, allocator, &result));
    pattern_equal(allocator, &result.normal, "r(U)");
    assert(result.components.values[QJS_INTL_DATETIME_YEAR] == QJS_INTL_DATETIME_NUMERIC);
    qjs_intl_date_style_programs_clear(&result);
    time = source(styles, allocator, root, jp, QJS_INTL_DATE_STYLE_TIME, QJS_INTL_DATE_STYLE_MEDIUM);
    times = pair(allocator, "H:mm", "h:mm a");
    assert(qjs_intl_date_style_programs_open(&date, &time, &times, connectors, root,
                                          connector_calendar(connectors, "japanese"), allocator, &result) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_date_style_programs_open(&date, &time, NULL, connectors, root, connection, allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_programs_open(NULL, NULL, NULL, NULL, 0, 0, allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_date_single_programs_clear(&times);
    qjs_intl_date_style_source_clear(&time);
    qjs_intl_date_style_source_clear(&date);
    state->calls = 0;
    state->fail_at = 1;
    assert(qjs_intl_date_style_source_open(styles, root, greg, QJS_INTL_DATE_STYLE_DATE,
                                          QJS_INTL_DATE_STYLE_FULL, 0, allocator, &bad) == QJS_INTL_NO_MEMORY);
    assert(!memcmp(&bad, &source_zero, sizeof(bad)));
    state->fail_at = 0;
    assert(qjs_intl_date_style_source_open(styles, root, greg, (QJSIntlDateStyleFamily)-1,
                                          QJS_INTL_DATE_STYLE_FULL, 0, allocator, &bad) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_source_open(styles, root, styles->calendar_count, QJS_INTL_DATE_STYLE_DATE,
                                          QJS_INTL_DATE_STYLE_FULL, 0, allocator, &bad) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_date_style_programs_clear(NULL);
    qjs_intl_date_style_programs_clear(&result);
    qjs_intl_date_style_programs_clear(&result);
    qjs_intl_date_style_source_clear(NULL);
    qjs_intl_date_style_source_clear(&date);
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlDateStyleData styles;
    QJSIntlDateCombinationData connectors;
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_date_style_data_open(&data, &allocator, &styles));
    assert(!qjs_intl_date_combination_data_open(&data, &allocator, &connectors));
    check(&styles, &connectors, &allocator);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_date_style_data_open(&data, &allocator, &styles));
    assert(!qjs_intl_date_combination_data_open(&data, &allocator, &connectors));
    check(&styles, &connectors, &allocator);
    free(unaligned);
    return 0;
}
