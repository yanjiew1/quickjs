/* ECMA component metadata preserves distinct native source semantics.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/pattern-info.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    state->calls++;
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

static int empty(const QJSIntlDatePatternInfo *info)
{
    size_t i;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        if (info->components.values[i])
            return 0;
    }
    return !info->hour_symbol && !info->period_symbol && !info->period_width && !info->year_kinds;
}

static QJSIntlDatePatternInfo inspect(const QJSIntlAllocator *allocator, const char *source,
                                      QJSIntlStatus expected)
{
    Allocation *state = allocator->opaque;
    QJSIntlDatePattern pattern;
    QJSIntlDatePatternInfo info;
    size_t calls;
    assert(!qjs_intl_date_pattern_compile(allocator, bytes(source), &pattern));
    calls = state->calls;
    memset(&info, 0xff, sizeof(info));
    assert(qjs_intl_date_pattern_info(&pattern, &info) == expected);
    assert(calls == state->calls);
    if (expected)
        assert(empty(&info));
    qjs_intl_date_pattern_clear(allocator, &pattern);
    assert(!state->live);
    return info;
}

int main(void)
{
    static const struct {
        const char *source;
        QJSIntlDateTimeComponent field;
        int value;
    } cases[] = {
        {"G", QJS_INTL_DATETIME_ERA, QJS_INTL_DATETIME_SHORT},
        {"GGGG", QJS_INTL_DATETIME_ERA, QJS_INTL_DATETIME_LONG},
        {"GGGGG", QJS_INTL_DATETIME_ERA, QJS_INTL_DATETIME_NARROW},
        {"y", QJS_INTL_DATETIME_YEAR, QJS_INTL_DATETIME_NUMERIC},
        {"yy", QJS_INTL_DATETIME_YEAR, QJS_INTL_DATETIME_TWO_DIGIT},
        {"yyyy", QJS_INTL_DATETIME_YEAR, QJS_INTL_DATETIME_NUMERIC},
        {"rr", QJS_INTL_DATETIME_YEAR, QJS_INTL_DATETIME_NUMERIC},
        {"UU", QJS_INTL_DATETIME_YEAR, QJS_INTL_DATETIME_NUMERIC},
        {"M", QJS_INTL_DATETIME_MONTH, QJS_INTL_DATETIME_NUMERIC},
        {"MM", QJS_INTL_DATETIME_MONTH, QJS_INTL_DATETIME_TWO_DIGIT},
        {"MMM", QJS_INTL_DATETIME_MONTH, QJS_INTL_DATETIME_SHORT},
        {"MMMM", QJS_INTL_DATETIME_MONTH, QJS_INTL_DATETIME_LONG},
        {"MMMMM", QJS_INTL_DATETIME_MONTH, QJS_INTL_DATETIME_NARROW},
        {"LLLL", QJS_INTL_DATETIME_MONTH, QJS_INTL_DATETIME_LONG},
        {"E", QJS_INTL_DATETIME_WEEKDAY, QJS_INTL_DATETIME_SHORT},
        {"EEEE", QJS_INTL_DATETIME_WEEKDAY, QJS_INTL_DATETIME_LONG},
        {"EEEEE", QJS_INTL_DATETIME_WEEKDAY, QJS_INTL_DATETIME_NARROW},
        {"EEEEEE", QJS_INTL_DATETIME_WEEKDAY, QJS_INTL_DATETIME_SHORT},
        {"cccccc", QJS_INTL_DATETIME_WEEKDAY, QJS_INTL_DATETIME_SHORT},
        {"eee", QJS_INTL_DATETIME_WEEKDAY, QJS_INTL_DATETIME_SHORT},
        {"dd", QJS_INTL_DATETIME_DAY, QJS_INTL_DATETIME_TWO_DIGIT},
        {"HH", QJS_INTL_DATETIME_HOUR, QJS_INTL_DATETIME_TWO_DIGIT},
        {"mm", QJS_INTL_DATETIME_MINUTE, QJS_INTL_DATETIME_TWO_DIGIT},
        {"s", QJS_INTL_DATETIME_SECOND, QJS_INTL_DATETIME_NUMERIC},
        {"S", QJS_INTL_DATETIME_FRACTION, 1},
        {"SSS", QJS_INTL_DATETIME_FRACTION, 3},
        {"b", QJS_INTL_DATETIME_DAY_PERIOD, QJS_INTL_DATETIME_SHORT},
        {"BBBB", QJS_INTL_DATETIME_DAY_PERIOD, QJS_INTL_DATETIME_LONG},
        {"BBBBB", QJS_INTL_DATETIME_DAY_PERIOD, QJS_INTL_DATETIME_NARROW},
        {"z", QJS_INTL_DATETIME_ZONE, QJS_INTL_DATETIME_ZONE_SHORT},
        {"zzzz", QJS_INTL_DATETIME_ZONE, QJS_INTL_DATETIME_ZONE_LONG},
        {"v", QJS_INTL_DATETIME_ZONE, QJS_INTL_DATETIME_ZONE_SHORT_GENERIC},
        {"vvvv", QJS_INTL_DATETIME_ZONE, QJS_INTL_DATETIME_ZONE_LONG_GENERIC},
        {"O", QJS_INTL_DATETIME_ZONE, QJS_INTL_DATETIME_ZONE_SHORT_OFFSET},
        {"OOOO", QJS_INTL_DATETIME_ZONE, QJS_INTL_DATETIME_ZONE_LONG_OFFSET}
    };
    static const char *const unsupported[] = {
        "Q", "w", "SSSS", "GGGGGG", "MMMMMM", "EEEEEEE", "e", "cc", "bbbbbb",
        "hhh", "ddd", "mmm", "sss", "zzzzz", "vv", "OO", "UUUUUU", "Z", "X", "VVVV",
        "y Q", "y y", "r r", "U U", "y r", "r y", "y U", "M L", "E ccc",
        "B b", "a B", "h H", "HH HH", "zz v", "S SS", "d dd"
    };
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlDatePatternInfo info;
    QJSIntlDatePattern pattern;
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        info = inspect(&allocator, cases[i].source, QJS_INTL_OK);
        assert(info.components.values[cases[i].field] == cases[i].value);
    }
    info = inspect(&allocator, "r(U)", QJS_INTL_OK);
    assert(info.year_kinds == (QJS_INTL_DATE_YEAR_RELATED | QJS_INTL_DATE_YEAR_CYCLIC));
    assert(info.components.values[QJS_INTL_DATETIME_YEAR] == QJS_INTL_DATETIME_NUMERIC);
    info = inspect(&allocator, "U(r)", QJS_INTL_OK);
    assert(info.year_kinds == (QJS_INTL_DATE_YEAR_RELATED | QJS_INTL_DATE_YEAR_CYCLIC));
    info = inspect(&allocator, "UU", QJS_INTL_OK);
    assert(info.year_kinds == QJS_INTL_DATE_YEAR_CYCLIC);
    info = inspect(&allocator, "a", QJS_INTL_OK);
    assert(info.period_symbol == 'a' && info.period_width == 1 && !info.hour_symbol);
    assert(!info.components.values[QJS_INTL_DATETIME_DAY_PERIOD] && !info.components.values[QJS_INTL_DATETIME_HOUR]);
    info = inspect(&allocator, "B", QJS_INTL_OK);
    assert(info.period_symbol == 'B' && !info.hour_symbol);
    for (i = 0; i < 4; i++) {
        char source[2] = {"hHKk"[i], 0};
        info = inspect(&allocator, source, QJS_INTL_OK);
        assert(info.hour_symbol == (unsigned char)source[0]);
    }
    info = inspect(&allocator, "'hB' h", QJS_INTL_OK);
    assert(info.hour_symbol == 'h' && !info.period_symbol);
    info = inspect(&allocator, "'literal'", QJS_INTL_OK);
    assert(empty(&info));
    for (i = 0; i < sizeof(unsupported) / sizeof(*unsupported); i++)
        inspect(&allocator, unsupported[i], QJS_INTL_UNSUPPORTED);
    {
        char source[258];
        memset(source, 'y', sizeof(source) - 1);
        source[sizeof(source) - 1] = 0;
        info = inspect(&allocator, source, QJS_INTL_OK);
        assert(info.components.values[QJS_INTL_DATETIME_YEAR] == QJS_INTL_DATETIME_NUMERIC);
        memset(source, 'r', sizeof(source) - 1);
        info = inspect(&allocator, source, QJS_INTL_OK);
        assert(info.components.values[QJS_INTL_DATETIME_YEAR] == QJS_INTL_DATETIME_NUMERIC);
    }
    assert(!qjs_intl_date_combination_compile(&allocator, bytes("{1}{0}"), &pattern));
    assert(qjs_intl_date_pattern_info(&pattern, &info) == QJS_INTL_UNSUPPORTED && empty(&info));
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    assert(!qjs_intl_date_interval_compile(&allocator, bytes("latestFirst:y"), &pattern));
    assert(qjs_intl_date_pattern_info(&pattern, &info) == QJS_INTL_UNSUPPORTED && empty(&info));
    qjs_intl_date_pattern_clear(&allocator, &pattern);
    assert(qjs_intl_date_pattern_info(NULL, &info) == QJS_INTL_INVALID_ARGUMENT && empty(&info));
    assert(qjs_intl_date_pattern_info(&pattern, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_pattern_info(&pattern, &info) == QJS_INTL_INVALID_ARGUMENT && empty(&info));
    assert(!state.live);
    return 0;
}
