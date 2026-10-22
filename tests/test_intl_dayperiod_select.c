/* Whole-list inheritance and exact local-clock classification, without names.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/dayperiod.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t dayperiod_empty_bytes[], dayperiod_mixed_bytes[];
extern const size_t dayperiod_empty_length, dayperiod_mixed_length;

static void *allocate(void *opaque, size_t size)
{
    size_t *calls = opaque;
    assert(size);
    (*calls)++;
    return malloc(size);
}

static void release(void *opaque, void *ptr)
{
    (void)opaque;
    assert(ptr);
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static QJSIntlDayPeriodRuleList resolve(const QJSIntlDayPeriodData *data,
                                       QJSIntlDayPeriodRuleSetKind kind, const char *locale)
{
    QJSIntlDayPeriodRuleList result;
    assert(!qjs_intl_dayperiod_resolve(data, kind, bytes(locale), &result));
    return result;
}

static void same(QJSIntlDayPeriodRuleList a, QJSIntlDayPeriodRuleList b)
{
    assert(a.start == b.start && a.count == b.count);
}

static void clock_code(const QJSIntlDayPeriodData *data, const QJSIntlDayPeriodRuleList *rules,
                       unsigned int symbol, uint32_t second, unsigned int fraction,
                       QJSIntlDayPeriodCode expected)
{
    QJSIntlDayPeriodCode code = QJS_INTL_DAYPERIOD_CODE_COUNT;
    assert(!qjs_intl_dayperiod_select(data, rules, symbol, second, fraction, 1, &code));
    assert(code == expected);
}

static void check(const QJSIntlDayPeriodData *data)
{
    static const struct {
        uint32_t second;
        unsigned int fraction;
        QJSIntlDayPeriodCode code;
    } cases[] = {
        {0, 0, QJS_INTL_DAYPERIOD_MIDNIGHT}, {0, 1, QJS_INTL_DAYPERIOD_NIGHT1},
        {1, 0, QJS_INTL_DAYPERIOD_NIGHT1}, {21599, 1, QJS_INTL_DAYPERIOD_NIGHT1},
        {21600, 0, QJS_INTL_DAYPERIOD_MORNING1}, {43199, 1, QJS_INTL_DAYPERIOD_MORNING1},
        {43200, 0, QJS_INTL_DAYPERIOD_NOON}, {43200, 1, QJS_INTL_DAYPERIOD_MORNING1},
        {43201, 0, QJS_INTL_DAYPERIOD_MORNING1}, {44099, 1, QJS_INTL_DAYPERIOD_MORNING1},
        {44100, 0, QJS_INTL_DAYPERIOD_AFTERNOON1}, {64799, 1, QJS_INTL_DAYPERIOD_AFTERNOON1},
        {64800, 0, QJS_INTL_DAYPERIOD_NIGHT1}, {86399, 1, QJS_INTL_DAYPERIOD_NIGHT1}
    };
    QJSIntlDayPeriodRuleList en = resolve(data, QJS_INTL_DAYPERIOD_FORMAT, "en");
    QJSIntlDayPeriodRuleList root = resolve(data, QJS_INTL_DAYPERIOD_FORMAT, "root");
    QJSIntlDayPeriodRuleList selected = resolve(data, QJS_INTL_DAYPERIOD_SELECTION, "en");
    QJSIntlDayPeriodRuleList rules, invalid;
    QJSIntlDayPeriodCode code;
    size_t i;
    assert(en.count == 5 && root.count == 2 && selected.count == 3);
    same(en, resolve(data, QJS_INTL_DAYPERIOD_FORMAT, "en-GB"));
    same(en, resolve(data, QJS_INTL_DAYPERIOD_FORMAT, "sr-Latn"));
    same(en, resolve(data, QJS_INTL_DAYPERIOD_FORMAT, "hi-Latn-IN"));
    same(en, resolve(data, QJS_INTL_DAYPERIOD_FORMAT, "en-1901-1996-foobar"));
    same(root, resolve(data, QJS_INTL_DAYPERIOD_FORMAT, "und-ZZ"));
    for (i = 0; i < sizeof(cases) / sizeof(*cases); i++)
        clock_code(data, &en, 'B', cases[i].second, cases[i].fraction, cases[i].code);
    assert(!qjs_intl_dayperiod_select(data, &en, 'B', 0, 0, 0, &code));
    assert(code == QJS_INTL_DAYPERIOD_NIGHT1); /* Keep the variable range at 00:00. */
    assert(!qjs_intl_dayperiod_select(data, &en, 'b', 0, 0, 0, &code));
    assert(code == QJS_INTL_DAYPERIOD_AM);
    assert(!qjs_intl_dayperiod_select(data, &en, 'B', 43200, 0, 0, &code));
    assert(code == QJS_INTL_DAYPERIOD_NOON);
    assert(qjs_intl_dayperiod_select(data, &en, 'B', 0, 0, 2, &code) == QJS_INTL_INVALID_ARGUMENT);
    clock_code(data, &selected, 'B', 0, 0, QJS_INTL_DAYPERIOD_NIGHT1);
    clock_code(data, &selected, 'B', 43200, 0, QJS_INTL_DAYPERIOD_MORNING1);
    clock_code(data, &root, 'B', 0, 0, QJS_INTL_DAYPERIOD_AM);
    clock_code(data, &root, 'B', 43200, 0, QJS_INTL_DAYPERIOD_PM);
    /* Fixed b does not consult flexible rules or their point availability. */
    for (i = 0; i < 2; i++) {
        clock_code(NULL, NULL, 'a', 0, (unsigned int)i, QJS_INTL_DAYPERIOD_AM);
        clock_code(NULL, NULL, 'a', 43200, (unsigned int)i, QJS_INTL_DAYPERIOD_PM);
        clock_code(data, &en, 'b', 0, (unsigned int)i, i ? QJS_INTL_DAYPERIOD_AM : QJS_INTL_DAYPERIOD_MIDNIGHT);
        clock_code(data, &en, 'b', 43200, (unsigned int)i, i ? QJS_INTL_DAYPERIOD_PM : QJS_INTL_DAYPERIOD_NOON);
    }
    clock_code(data, &en, 'b', 43201, 0, QJS_INTL_DAYPERIOD_PM);
    clock_code(data, &en, 'b', 45000, 0, QJS_INTL_DAYPERIOD_PM);
    clock_code(NULL, NULL, 'b', 0, 0, QJS_INTL_DAYPERIOD_MIDNIGHT);
    clock_code(NULL, NULL, 'B', 43200, 0, QJS_INTL_DAYPERIOD_PM);
    rules = resolve(data, QJS_INTL_DAYPERIOD_FORMAT, "fr-CA");
    assert(!rules.count); /* Explicit empty child is not replaced with root. */
    clock_code(data, &rules, 'B', 0, 0, QJS_INTL_DAYPERIOD_AM);
    clock_code(data, &rules, 'B', 43200, 0, QJS_INTL_DAYPERIOD_PM);
    rules = resolve(data, QJS_INTL_DAYPERIOD_FORMAT, "gsw-CH");
    assert(rules.count == 1);
    clock_code(data, &rules, 'B', 0, 0, QJS_INTL_DAYPERIOD_MIDNIGHT);
    clock_code(data, &rules, 'B', 0, 1, QJS_INTL_DAYPERIOD_AM);
    clock_code(data, &rules, 'B', 43200, 0, QJS_INTL_DAYPERIOD_PM);
    clock_code(data, &rules, 'b', 43200, 0, QJS_INTL_DAYPERIOD_NOON);
    rules = resolve(data, QJS_INTL_DAYPERIOD_FORMAT, "xx");
    clock_code(data, &rules, 'B', 0, 0, QJS_INTL_DAYPERIOD_MORNING1);
    clock_code(data, &rules, 'B', 43200, 0, QJS_INTL_DAYPERIOD_NIGHT1);
    clock_code(data, &rules, 'B', 86399, 1, QJS_INTL_DAYPERIOD_NIGHT1);
    rules = resolve(data, QJS_INTL_DAYPERIOD_SELECTION, "zz");
    clock_code(data, &rules, 'B', 22499, 1, QJS_INTL_DAYPERIOD_NIGHT1);
    clock_code(data, &rules, 'B', 22500, 0, QJS_INTL_DAYPERIOD_MORNING1);
    clock_code(data, &rules, 'B', 65699, 1, QJS_INTL_DAYPERIOD_MORNING1);
    clock_code(data, &rules, 'B', 65700, 0, QJS_INTL_DAYPERIOD_NIGHT1);
    rules = (QJSIntlDayPeriodRuleList){UINT32_MAX, UINT32_MAX};
    assert(qjs_intl_dayperiod_resolve(data, QJS_INTL_DAYPERIOD_SELECTION, bytes("de"), &rules) == QJS_INTL_UNSUPPORTED);
    assert(!rules.start && !rules.count);
    assert(qjs_intl_dayperiod_resolve(data, (QJSIntlDayPeriodRuleSetKind)-1, bytes("en"), &rules) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_dayperiod_resolve(data, QJS_INTL_DAYPERIOD_FORMAT, (QJSIntlBytes){NULL, 1}, &rules) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_dayperiod_resolve(data, QJS_INTL_DAYPERIOD_FORMAT, bytes("en"), NULL) == QJS_INTL_INVALID_ARGUMENT);
    invalid = (QJSIntlDayPeriodRuleList){data->rule_count, 1};
    code = QJS_INTL_DAYPERIOD_NOON;
    assert(qjs_intl_dayperiod_select(data, &invalid, 'B', 0, 0, 1, &code) == QJS_INTL_INVALID_ARGUMENT && code == QJS_INTL_DAYPERIOD_AM);
    invalid = (QJSIntlDayPeriodRuleList){UINT32_MAX, UINT32_MAX};
    assert(qjs_intl_dayperiod_select(data, &invalid, 'B', 0, 0, 1, &code) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_dayperiod_select(NULL, &en, 'B', 0, 0, 1, &code) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_dayperiod_select(data, &en, 'X', 0, 0, 1, &code) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_dayperiod_select(data, &en, 'B', 86400, 0, 1, &code) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_dayperiod_select(data, &en, 'B', 0, 2, 1, &code) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_dayperiod_select(data, &en, 'B', 0, 0, 1, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    size_t calls = 0, before;
    QJSIntlAllocator allocator = {&calls, allocate, NULL, release};
    QJSIntlData shared;
    QJSIntlDayPeriodData data;
    QJSIntlDayPeriodRuleList rules;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &shared));
    assert(!qjs_intl_dayperiod_data_open(&shared, &allocator, &data));
    before = calls;
    check(&data);
    assert(calls == before);
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &shared));
    assert(!qjs_intl_dayperiod_data_open(&shared, &allocator, &data));
    before = calls;
    check(&data);
    assert(calls == before);
    free(unaligned);
    assert(!qjs_intl_data_open(dayperiod_mixed_bytes, dayperiod_mixed_length, &allocator, &shared));
    assert(!qjs_intl_dayperiod_data_open(&shared, &allocator, &data));
    rules = resolve(&data, QJS_INTL_DAYPERIOD_FORMAT, "yy");
    clock_code(&data, &rules, 'B', 21600, 0, QJS_INTL_DAYPERIOD_MORNING1);
    clock_code(&data, &rules, 'B', 45000, 0, QJS_INTL_DAYPERIOD_AFTERNOON1);
    clock_code(&data, &rules, 'B', 64800, 0, QJS_INTL_DAYPERIOD_NIGHT1);
    clock_code(&data, &rules, 'B', 43200, 0, QJS_INTL_DAYPERIOD_NOON);
    assert(!qjs_intl_data_open(dayperiod_empty_bytes, dayperiod_empty_length, &allocator, &shared));
    assert(!qjs_intl_dayperiod_data_open(&shared, &allocator, &data));
    assert(qjs_intl_dayperiod_resolve(&data, QJS_INTL_DAYPERIOD_FORMAT, bytes("en"), &rules) == QJS_INTL_UNSUPPORTED);
    assert(!rules.start && !rules.count);
    clock_code(&data, &rules, 'B', 0, 0, QJS_INTL_DAYPERIOD_AM);
    return 0;
}
