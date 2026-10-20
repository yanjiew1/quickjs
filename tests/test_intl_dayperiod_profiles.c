/* Global clock rules remain available independently of prepared locale rows.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/dayperiod-data.h"
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

static uint32_t find(const QJSIntlDayPeriodData *data, QJSIntlDayPeriodRuleSetKind kind,
                     const char *locale)
{
    uint32_t map;
    assert(!qjs_intl_dayperiod_find(data, kind, bytes(locale), &map));
    return map;
}

static int has(const QJSIntlDayPeriodData *data, uint32_t map, QJSIntlDayPeriodCode code)
{
    uint32_t i, start, count;
    QJSIntlDayPeriodRule rule;
    assert(!qjs_intl_dayperiod_map(data, map, &start, &count));
    for (i = 0; i < count; i++) {
        assert(!qjs_intl_dayperiod_rule(data, start + i, &rule));
        if (rule.code == code)
            return 1;
    }
    return 0;
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlDayPeriodData owner;
    QJSIntlDayPeriodRuleSetKind kind;
    QJSIntlDayPeriodRule rule;
    QJSIntlBytes origin;
    uint32_t i, j, map, found, start, count, kinds[QJS_INTL_DAYPERIOD_KIND_COUNT] = {0};
    size_t calls;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_dayperiod_data_open(&data, &allocator, &owner) && !state.live);
    assert(owner.row_count == 178 && owner.map_count == 117 && owner.rule_count == 651);
    calls = state.calls;
    for (i = 0; i < owner.row_count; i++) {
        assert(!qjs_intl_dayperiod_row(&owner, i, &kind, &origin, &map));
        assert(!qjs_intl_dayperiod_find(&owner, kind, origin, &found) && found == map);
        kinds[kind]++;
    }
    assert(kinds[QJS_INTL_DAYPERIOD_FORMAT] == 89 && kinds[QJS_INTL_DAYPERIOD_SELECTION] == 89);
    for (i = 0; i < owner.map_count; i++) {
        assert(!qjs_intl_dayperiod_map(&owner, i, &start, &count));
        for (j = 0; j < count; j++) {
            assert(!qjs_intl_dayperiod_rule(&owner, start + j, &rule));
            assert(qjs_intl_dayperiod_code_find(qjs_intl_dayperiod_code_name(rule.code)) == (int)rule.code);
        }
    }
    map = find(&owner, QJS_INTL_DAYPERIOD_FORMAT, "en");
    assert(has(&owner, map, QJS_INTL_DAYPERIOD_MIDNIGHT));
    assert(has(&owner, map, QJS_INTL_DAYPERIOD_NOON));
    map = find(&owner, QJS_INTL_DAYPERIOD_SELECTION, "en");
    assert(!has(&owner, map, QJS_INTL_DAYPERIOD_MIDNIGHT));
    assert(!has(&owner, map, QJS_INTL_DAYPERIOD_NOON));
    map = find(&owner, QJS_INTL_DAYPERIOD_SELECTION, "gsw");
    assert(has(&owner, map, QJS_INTL_DAYPERIOD_MIDNIGHT));
    map = find(&owner, QJS_INTL_DAYPERIOD_FORMAT, "hi-Latn");
    assert(has(&owner, map, QJS_INTL_DAYPERIOD_NIGHT1));
    map = find(&owner, QJS_INTL_DAYPERIOD_FORMAT, "sr");
    assert(has(&owner, map, QJS_INTL_DAYPERIOD_MORNING1));
    assert(qjs_intl_dayperiod_find(&owner, QJS_INTL_DAYPERIOD_FORMAT, bytes("sr-Latn"), &map) == QJS_INTL_UNSUPPORTED);
    map = find(&owner, QJS_INTL_DAYPERIOD_FORMAT, "root");
    assert(!qjs_intl_dayperiod_map(&owner, map, &start, &count) && count == 2);
    assert(!qjs_intl_dayperiod_rule(&owner, start, &rule));
    assert(rule.code == QJS_INTL_DAYPERIOD_AM && rule.form == QJS_INTL_DAYPERIOD_RANGE && !rule.start && rule.end == 43200);
    assert(!qjs_intl_dayperiod_rule(&owner, start + 1, &rule));
    assert(rule.code == QJS_INTL_DAYPERIOD_PM && rule.start == 43200 && rule.end == 86400);
    assert(state.calls == calls && !state.live);
    return 0;
}
