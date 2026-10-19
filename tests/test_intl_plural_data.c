/* Plural owner: real/authored records, borrowed views, corrupt data and OOM.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/plural/data.h"
#include "../src/intl/plural/native-engine.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

static void *allocate(void *opaque, size_t length)
{
    Allocation *state = opaque;
    void *ptr;
    assert(length);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(length);
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

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) && !memcmp(text.data, expected, text.length);
}

static void put_u32(uint8_t *ptr, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        ptr[i] = value >> (i * 8);
}

static uint32_t locale_id(const QJSIntlData *data, const char *expected)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes tag;
        assert(!qjs_intl_data_locale(data, i, &tag, &parent));
        if (equal(tag, expected))
            return i;
    }
    abort();
}

static uint32_t string_id(const QJSIntlData *data, const char *expected)
{
    uint32_t i;
    for (i = 0; i < data->strings.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_string(data, i, &text));
        if (equal(text, expected))
            return i;
    }
    abort();
}

static void invalid_blob(const QJSIntlAllocator *allocator, const uint8_t *copy, size_t length)
{
    QJSIntlData data;
    QJSIntlPluralData owner;
    assert(!qjs_intl_data_open(copy, length, allocator, &data));
    memset(&owner, 0xff, sizeof(owner));
    assert(qjs_intl_plural_data_open(&data, allocator, &owner) == QJS_INTL_DATA_ERROR);
    assert(!owner.data.pack.bytes && !owner.section.bytes && !owner.maps && !owner.rules &&
           !owner.ranges);
}

static void corrupt(const QJSIntlAllocator *allocator, size_t offset, uint32_t value)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    invalid_blob(allocator, copy, qjs_intl_native_length);
    free(copy);
}

static void select_category(const QJSIntlPluralRuleSet *rules, const char *text,
                            QJSIntlPluralCategory expected)
{
    QJSIntlPluralOperands number = {text, strlen(text), 0};
    QJSIntlPluralCategory category;
    assert(!qjs_intl_native_plural_select(rules, &number, &category));
    assert(category == expected);
}

static void native_selection(const QJSIntlPluralData *data, uint32_t en, uint32_t root)
{
    QJSIntlPluralRuleSet rules;
    QJSIntlPluralCategory category;
    QJSIntlPluralOperands invalid = {"-1", 2, 0};
    unsigned int start, end;
    assert(!qjs_intl_plural_rules_at(data, en, QJS_INTL_PLURAL_CARDINAL, &rules));
    select_category(&rules, "1", QJS_INTL_PLURAL_ONE);
    select_category(&rules, "0001", QJS_INTL_PLURAL_ONE);
    select_category(&rules, "1.0", QJS_INTL_PLURAL_OTHER);
    select_category(&rules, "1.5", QJS_INTL_PLURAL_OTHER);
    {
        QJSIntlPluralOperands number = {"0.00100", 7, 3};
        assert(!qjs_intl_native_plural_select_shifted(&rules, &number, 3, &category));
        assert(category == QJS_INTL_PLURAL_OTHER); /* Expanded 1.00 keeps v=2. */
        assert(!qjs_intl_native_plural_select_shifted(&rules, &number, 5, &category));
        assert(category == QJS_INTL_PLURAL_OTHER); /* Expanded 100. */
        number = (QJSIntlPluralOperands){"0.001", 5, 3};
        assert(!qjs_intl_native_plural_select_shifted(&rules, &number, 3, &category));
        assert(category == QJS_INTL_PLURAL_ONE);
    }
    select_category(&rules, "100000000000000000000000000000000000001", QJS_INTL_PLURAL_OTHER);
    assert(!qjs_intl_native_plural_range(&rules, QJS_INTL_PLURAL_ONE, QJS_INTL_PLURAL_OTHER,
                                         &category));
    assert(category == QJS_INTL_PLURAL_OTHER);
    assert(!qjs_intl_plural_rules_at(data, en, QJS_INTL_PLURAL_ORDINAL, &rules));
    select_category(&rules, "21", QJS_INTL_PLURAL_ONE);
    select_category(&rules, "22", QJS_INTL_PLURAL_TWO);
    select_category(&rules, "12", TEST_INTL_AUTHORED ? QJS_INTL_PLURAL_TWO : QJS_INTL_PLURAL_OTHER);
    select_category(&rules, "23", TEST_INTL_AUTHORED ? QJS_INTL_PLURAL_OTHER : QJS_INTL_PLURAL_FEW);
    if (TEST_INTL_PROFILE) {
        assert(!qjs_intl_plural_rules_at(data, locale_id(&data->data, "ar"),
                                         QJS_INTL_PLURAL_CARDINAL, &rules));
        select_category(&rules, "0", QJS_INTL_PLURAL_ZERO);
        select_category(&rules, "1.0", QJS_INTL_PLURAL_ONE);
        select_category(&rules, "2", QJS_INTL_PLURAL_TWO);
        select_category(&rules, "3", QJS_INTL_PLURAL_FEW);
        select_category(&rules, "11", QJS_INTL_PLURAL_MANY);
        select_category(&rules, "100", QJS_INTL_PLURAL_OTHER);
        select_category(&rules, "1.001", QJS_INTL_PLURAL_OTHER);
        assert(!qjs_intl_native_plural_range(&rules, QJS_INTL_PLURAL_ZERO,
                                            QJS_INTL_PLURAL_ONE, &category));
        assert(category == QJS_INTL_PLURAL_ZERO);
        assert(!qjs_intl_native_plural_range(&rules, QJS_INTL_PLURAL_ONE,
                                            QJS_INTL_PLURAL_TWO, &category));
        assert(category == QJS_INTL_PLURAL_OTHER);
    }
    assert(!qjs_intl_plural_rules_at(data, root, QJS_INTL_PLURAL_CARDINAL, &rules));
    select_category(&rules, "1.5000", QJS_INTL_PLURAL_OTHER);
    for (start = 0; start < QJS_INTL_PLURAL_CATEGORY_COUNT; start++) {
        for (end = 0; end < QJS_INTL_PLURAL_CATEGORY_COUNT; end++) {
            assert(!qjs_intl_native_plural_range(&rules, start, end, &category));
            assert((unsigned int)category == end);
        }
    }
    category = QJS_INTL_PLURAL_ZERO;
    assert(qjs_intl_native_plural_select(&rules, &invalid, &category) == QJS_INTL_INVALID_ARGUMENT);
    assert(category == QJS_INTL_PLURAL_OTHER);
    assert(qjs_intl_native_plural_select(&rules, NULL, &category) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_native_plural_select(NULL, &invalid, &category) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_native_plural_select(&rules, &invalid, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_native_plural_select_shifted(&rules, &invalid, 3, &category) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(category == QJS_INTL_PLURAL_OTHER);
    category = QJS_INTL_PLURAL_ZERO;
    assert(qjs_intl_native_plural_range(&rules, (QJSIntlPluralCategory)-1, QJS_INTL_PLURAL_ONE,
                                        &category) == QJS_INTL_INVALID_ARGUMENT);
    assert(category == QJS_INTL_PLURAL_OTHER);
    assert(qjs_intl_native_plural_range(NULL, QJS_INTL_PLURAL_ONE, QJS_INTL_PLURAL_TWO,
                                        &category) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_native_plural_range(&rules, QJS_INTL_PLURAL_ONE, QJS_INTL_PLURAL_TWO, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlPluralData owner, retry;
    QJSIntlPluralRuleSet rules;
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    size_t calls, section_at, maps_at, rules_at, ranges_at;
    uint32_t en, root, i;
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_plural_data_open(&data, &allocator, &owner));
    assert(state.calls == 1 && !state.live);
    en = locale_id(&data, "en");
    root = locale_id(&data, "root");
    calls = state.calls;
    native_selection(&owner, en, root);
    assert(state.calls == calls);
    assert(!qjs_intl_plural_rules_at(&owner, en, QJS_INTL_PLURAL_CARDINAL, &rules));
    assert(rules.present == (1u << QJS_INTL_PLURAL_ONE | 1u << QJS_INTL_PLURAL_OTHER));
    assert(equal(rules.conditions[QJS_INTL_PLURAL_ONE], "i = 1 and v = 0"));
    assert(equal(rules.conditions[QJS_INTL_PLURAL_OTHER], ""));
    assert(!rules.conditions[QJS_INTL_PLURAL_ZERO].data);
    assert(rules.ranges[QJS_INTL_PLURAL_ONE * 6 + QJS_INTL_PLURAL_OTHER] == QJS_INTL_PLURAL_OTHER);
    assert(!qjs_intl_plural_rules_at(&owner, en, QJS_INTL_PLURAL_ORDINAL, &rules));
    assert(rules.present == (TEST_INTL_AUTHORED ? 38u : 46u));
    assert(!qjs_intl_plural_rules_at(&owner, root, QJS_INTL_PLURAL_CARDINAL, &rules));
    assert(rules.present == 1u << QJS_INTL_PLURAL_OTHER);
    for (i = 0; i < 36; i++)
        assert(rules.ranges[i] == QJS_INTL_PLURAL_RANGE_ABSENT);
    if (TEST_INTL_PROFILE) {
        assert(!qjs_intl_plural_rules_at(&owner, locale_id(&data, "ar"), QJS_INTL_PLURAL_CARDINAL,
                                         &rules));
        assert(rules.present == 63);
    }
    memset(&rules, 0xff, sizeof(rules));
    assert(qjs_intl_plural_rules_at(&owner, data.locales.count, QJS_INTL_PLURAL_CARDINAL, &rules) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!rules.present && !rules.ranges && !rules.conditions[0].data);
    assert(qjs_intl_plural_rules_at(&owner, en, (QJSIntlPluralType)-1, &rules) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_plural_rules_at(NULL, en, QJS_INTL_PLURAL_CARDINAL, &rules) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_plural_rules_at(&owner, en, QJS_INTL_PLURAL_CARDINAL, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls);
    state.fail_at = state.calls + 1;
    assert(qjs_intl_plural_data_open(&data, &allocator, &retry) == QJS_INTL_NO_MEMORY);
    assert(!retry.section.bytes && !retry.data.pack.bytes && !state.live);
    state.fail_at = 0;
    assert(!qjs_intl_plural_data_open(&data, &allocator, &retry));
    assert(!state.live);
    assert(qjs_intl_plural_data_open(NULL, &allocator, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(!retry.section.bytes);
    assert(qjs_intl_plural_data_open(&data, NULL, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_plural_data_open(&data, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);

    section_at = owner.section.bytes - data.pack.bytes;
    maps_at = owner.maps - data.pack.bytes;
    rules_at = owner.rules - data.pack.bytes;
    ranges_at = owner.ranges - data.pack.bytes;
    corrupt(&allocator, section_at, 0);
    corrupt(&allocator, section_at, UINT32_MAX);
    corrupt(&allocator, section_at + 4, 0);
    corrupt(&allocator, section_at + 4, UINT32_MAX);
    corrupt(&allocator, maps_at, owner.rule_count);
    corrupt(&allocator, maps_at + 4, owner.range_count);
    corrupt(&allocator, rules_at, data.strings.count);
    corrupt(&allocator, rules_at + QJS_INTL_PLURAL_OTHER * 4, UINT32_MAX);
    corrupt(&allocator, rules_at + QJS_INTL_PLURAL_OTHER * 4, string_id(&data, "i = 1 and v = 0"));
    corrupt(&allocator, ranges_at, 6);
    if (TEST_INTL_AUTHORED) {
        static const char *const malformed[] = {
            "x = 0", " n = 1", "n  = 1", "n = 1 @integer 1", "n\xe2\x80\x8e=1", ""};
        uint8_t *copy = malloc(qjs_intl_native_length);
        assert(copy);
        for (i = 0; i < sizeof(malformed) / sizeof(malformed[0]); i++)
            corrupt(&allocator, rules_at, string_id(&data, malformed[i]));
        for (i = 0; i < 2; i++) {
            size_t at = i ? ranges_at : rules_at;
            size_t width = i ? 36 : 24;
            memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
            memcpy(copy + at + width, copy + at, width);
            invalid_blob(&allocator, copy, qjs_intl_native_length);
        }
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        for (i = 0; i < owner.section.count * 4; i++)
            put_u32(copy + maps_at + (size_t)i * 4, 0);
        invalid_blob(&allocator, copy, qjs_intl_native_length);
        free(copy);
    }
    for (i = 0; i < data.pack.section_count; i++) {
        size_t directory = 64 + (size_t)i * 16;
        if (qjs_intl_read_u32(qjs_intl_native_bytes + directory) == QJS_INTL_SECTION_PLURALS) {
            uint8_t *copy = malloc(qjs_intl_native_length);
            size_t cut = section_at + owner.section.length - 4;
            uint32_t j;
            assert(copy);
            if (TEST_INTL_AUTHORED) {
                assert(i + 1 == data.pack.section_count);
                corrupt(&allocator, directory, QJS_INTL_SECTION_PLURALS + 1);
            }
            corrupt(&allocator, directory + 12, 0);
            corrupt(&allocator, directory + 12, owner.section.count + 1);
            memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
            memmove(copy + cut, copy + cut + 4, qjs_intl_native_length - cut - 4);
            put_u32(copy + 8, qjs_intl_native_length - 4);
            put_u32(copy + directory + 8, owner.section.length - 4);
            for (j = i + 1; j < data.pack.section_count; j++) {
                size_t next = 64 + (size_t)j * 16 + 4;
                put_u32(copy + next, qjs_intl_read_u32(copy + next) - 4);
            }
            invalid_blob(&allocator, copy, qjs_intl_native_length - 4);
            free(copy);
        }
    }
    free(unaligned);
    assert(!state.live);
    return 0;
}
