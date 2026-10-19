/* Ordered number-bearing unit lookup over authored source records.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/unit/pattern.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t qjs_intl_unit_selector_bytes[];
extern const size_t qjs_intl_unit_selector_length;

typedef struct Rules {
    unsigned int calls;
    int different;
    QJSIntlStatus error;
    QJSIntlUnitCount category;
} Rules;

static unsigned int allocations;

static void *allocate(void *opaque, size_t size)
{
    (void)opaque;
    assert(size);
    allocations++;
    return malloc(size);
}

static void release(void *opaque, void *ptr)
{
    (void)opaque;
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int equal(QJSIntlBytes value, const char *text)
{
    return value.length == strlen(text) && (!value.length || !memcmp(value.data, text, value.length));
}

static QJSIntlStatus category(void *opaque, QJSIntlBytes locale, QJSIntlUnitCount *out)
{
    Rules *rules = opaque;
    rules->calls++;
    if (rules->error)
        return rules->error;
    *out = rules->different && equal(locale, "en") ? QJS_INTL_UNIT_COUNT_TWO : rules->category;
    return QJS_INTL_OK;
}

static uint32_t locale_id(const QJSIntlData *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes tag;
        assert(!qjs_intl_data_locale(data, i, &tag, &parent));
        if (equal(tag, name))
            return i;
    }
    abort();
}

static uint32_t head_for(const QJSIntlUnitData *data, const char *locale, const char *group)
{
    uint32_t index, head;
    assert(!qjs_intl_unit_group_find(data, QJS_INTL_UNIT_GROUP_SINGLE, bytes(group), &index));
    assert(!qjs_intl_unit_overlay_head(data, locale_id(&data->data, locale), QJS_INTL_UNIT_LONG,
                                       index, &head));
    return head;
}

static void expect(const QJSIntlUnitData *data, uint32_t head, const QJSIntlUnitPatternRequest *request,
                   const char *pattern, const char *locale)
{
    QJSIntlSelectedUnitPattern result;
    assert(!qjs_intl_unit_pattern_select(data, head, request, &result));
    assert(equal(result.form.text, pattern));
    assert(equal(result.locale, locale));
    assert(result.form.field == QJS_INTL_UNIT_PATTERN);
    assert(result.pattern.prefix.length + result.pattern.suffix.length + 3 == result.form.text.length);
}

static void failed(const QJSIntlUnitData *data, uint32_t head, const QJSIntlUnitPatternRequest *request,
                   QJSIntlStatus status)
{
    QJSIntlSelectedUnitPattern result, zero = {0};
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_unit_pattern_select(data, head, request, &result) == status);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

static void parser(void)
{
    QJSIntlUnitPattern value, zero = {0};
    const char *invalid[] = {"{", "}", "x {1}", "{00}", "{0} {0}", "{0}}", "{{0}"};
    size_t i;
    assert(!qjs_intl_unit_pattern_parse(bytes("before '{0}' after"), &value));
    assert(equal(value.prefix, "before '") && equal(value.suffix, "' after"));
    assert(!qjs_intl_unit_pattern_parse(bytes("{0}"), &value));
    assert(!value.prefix.length && !value.suffix.length);
    assert(qjs_intl_unit_pattern_parse(bytes("one meter"), &value) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&value, &zero, sizeof(value)));
    assert(qjs_intl_unit_pattern_parse(bytes(""), &value) == QJS_INTL_UNSUPPORTED);
    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        memset(&value, 0xff, sizeof(value));
        assert(qjs_intl_unit_pattern_parse(bytes(invalid[i]), &value) == QJS_INTL_DATA_ERROR);
        assert(!memcmp(&value, &zero, sizeof(value)));
    }
    assert(qjs_intl_unit_pattern_parse((QJSIntlBytes){NULL, 1}, &value) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_pattern_parse((QJSIntlBytes){"x", SIZE_MAX}, &value) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_pattern_parse(bytes("{0}"), NULL) == QJS_INTL_INVALID_ARGUMENT);
}

static void division_parser(void)
{
    QJSIntlUnitDivisionPattern result, zero = {0};
    static const char *invalid[] = {"{", "}", "{2}", "{00}", "{0}{0}", "{1}{1}",
                                     "{0}{1}{0}", "{0}{1}}", "{{0}}{1}"};
    static const char *missing[] = {"", "per", "{0} per", "every {1}"};
    size_t i;
    assert(!qjs_intl_unit_division_pattern_parse(bytes("prefix {0} per {1} suffix"), &result));
    assert(!result.first && equal(result.prefix, "prefix ") && equal(result.separator, " per ") &&
           equal(result.suffix, " suffix"));
    assert(!qjs_intl_unit_division_pattern_parse(bytes("每{1}{0}"), &result));
    assert(result.first == 1 && equal(result.prefix, "每") && !result.separator.length &&
           !result.suffix.length);
    assert(!qjs_intl_unit_division_pattern_parse(bytes("'{1}'당 {0}!"), &result));
    assert(result.first == 1 && equal(result.prefix, "'") && equal(result.separator, "'당 ") &&
           equal(result.suffix, "!"));
    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_unit_division_pattern_parse(bytes(invalid[i]), &result) == QJS_INTL_DATA_ERROR);
        assert(!memcmp(&result, &zero, sizeof(result)));
    }
    for (i = 0; i < sizeof(missing) / sizeof(*missing); i++) {
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_unit_division_pattern_parse(bytes(missing[i]), &result) == QJS_INTL_UNSUPPORTED);
        assert(!memcmp(&result, &zero, sizeof(result)));
    }
    assert(qjs_intl_unit_division_pattern_parse((QJSIntlBytes){NULL, 1}, &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_division_pattern_parse((QJSIntlBytes){"x", SIZE_MAX}, &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_division_pattern_parse(bytes("{0}/{1}"), NULL) == QJS_INTL_INVALID_ARGUMENT);
}

static void noun_parser(void)
{
    static const char *eligible[][2] = {
        {"{0} per second", "per second"},
        {" second {0}", "second"},
        {"ألف", "ألف"},
        {" \t{0}\xc2\xa0kilowatt hours\xe3\x80\x80", "kilowatt hours"},
        {"\xc2\x85{0} m/s\xc2\xa0", "m/s"},
        {"'seconds' {0}", "'seconds'"},
        {"\xef\xbb\xbf{0}", "\xef\xbb\xbf"}, /* BOM is not Unicode White_Space. */
    };
    static const char *unsupported[] = {"", " \t\xc2\xa0", "{0}", " {0} ",
                                         "摂氏 {0} 度", "ounce {0} na ruwa"};
    static const char *malformed[] = {"{1}", "a{0}{0}", "{0}}", "\xed\xa0\x80",
                                       "{0} \xf4\x90\x80\x80", "\xc0\xaf{0}", "{0}\xe3\x80"};
    QJSIntlBytes result, zero = {0};
    size_t i;
    for (i = 0; i < sizeof(eligible) / sizeof(*eligible); i++) {
        assert(!qjs_intl_unit_noun_parse(bytes(eligible[i][0]), &result));
        assert(equal(result, eligible[i][1]));
        assert(result.data >= eligible[i][0] &&
               result.data + result.length <= eligible[i][0] + strlen(eligible[i][0]));
    }
    for (i = 0; i < sizeof(unsupported) / sizeof(*unsupported); i++) {
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_unit_noun_parse(bytes(unsupported[i]), &result) == QJS_INTL_UNSUPPORTED);
        assert(!memcmp(&result, &zero, sizeof(result)));
    }
    for (i = 0; i < sizeof(malformed) / sizeof(*malformed); i++) {
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_unit_noun_parse(bytes(malformed[i]), &result) == QJS_INTL_DATA_ERROR);
        assert(!memcmp(&result, &zero, sizeof(result)));
    }
    assert(qjs_intl_unit_noun_parse((QJSIntlBytes){NULL, 1}, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_noun_parse((QJSIntlBytes){"x", SIZE_MAX}, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_noun_parse((QJSIntlBytes){NULL, 0}, &result) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_unit_noun_parse((QJSIntlBytes){"a\0b", 3}, &result) == QJS_INTL_DATA_ERROR);
    assert(qjs_intl_unit_noun_parse(bytes("noun"), NULL) == QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    QJSIntlAllocator allocator = {NULL, allocate, NULL, release};
    QJSIntlData shared;
    QJSIntlUnitData data;
    Rules rules = {0, 0, QJS_INTL_OK, QJS_INTL_UNIT_COUNT_ONE};
    QJSIntlUnitPatternRequest request = {{0}, category, &rules, QJS_INTL_UNIT_COUNT_NONE, -1};
    uint32_t head, compound;
    unsigned int calls;
    parser();
    division_parser();
    noun_parser();
    assert(!qjs_intl_data_open(qjs_intl_unit_selector_bytes, qjs_intl_unit_selector_length,
                               &allocator, &shared));
    assert(!qjs_intl_unit_data_open(&shared, &allocator, &data));
    calls = allocations;
    expect(&data, head_for(&data, "en-GB", "length-child"), &request, "CHILD_OTHER {0}", "en-GB");
    expect(&data, head_for(&data, "de", "length-child"), &request, "CHILD_OTHER {0}", "en-GB");
    request.case_name = bytes("genitive");
    expect(&data, head_for(&data, "en-GB", "length-case"), &request, "CHILD_DEFAULT {0}", "en-GB");
    expect(&data, head_for(&data, "en-GB", "length-order"), &request, "ONE_GENITIVE {0}", "en-GB");
    expect(&data, head_for(&data, "en-GB", "length-case-shadow"), &request,
           "PARENT_NOMINATIVE {0}", "en");
    request.case_name = bytes("accusative");
    expect(&data, head_for(&data, "en-GB", "length-order"), &request, "ONE_NOMINATIVE {0}", "en-GB");
    request.case_name = bytes("");
    expect(&data, head_for(&data, "en-GB", "length-order"), &request, "ONE_NOMINATIVE {0}", "en-GB");
    rules.calls = 0;
    rules.error = QJS_INTL_UNSUPPORTED;
    head = head_for(&data, "en-GB", "length-exact");
    request.exact = 1;
    expect(&data, head, &request, "EXACT_ONE {0}", "en-GB");
    request.exact = 0;
    expect(&data, head, &request, "EXACT_ZERO {0}", "en-GB");
    assert(!rules.calls); /* Eligible exact selectors do not need rule lookup. */
    request.exact = -1;
    failed(&data, head, &request, QJS_INTL_UNSUPPORTED);
    rules.error = QJS_INTL_OK;
    expect(&data, head, &request, "CATEGORY_ONE {0}", "en-GB");
    request.exact = 1;
    expect(&data, head_for(&data, "en-GB", "length-exact-shadow"), &request, "ROOT {0}", "root");
    request.exact = -1;
    failed(&data, head_for(&data, "en-GB", "length-block"), &request, QJS_INTL_UNSUPPORTED);
    failed(&data, head_for(&data, "en-GB", "length-literal"), &request, QJS_INTL_UNSUPPORTED);
    failed(&data, head_for(&data, "en-GB", "length-empty"), &request, QJS_INTL_UNSUPPORTED);
    expect(&data, head_for(&data, "en-GB", "length-alternative"), &request,
           "ALTERNATIVE {0}", "en-GB");
    rules.different = 1;
    expect(&data, head_for(&data, "en-GB", "length-source-category"), &request, "PARENT_TWO {0}", "en");
    rules.calls = 0;
    expect(&data, head_for(&data, "en-GB", "length-unselected"), &request, "ROOT {0}", "root");
    assert(rules.calls == 3); /* Different parent category cannot revive blocked TWO. */
    rules.different = 0;
    failed(&data, head_for(&data, "en-GB", "length-malformed"), &request, QJS_INTL_DATA_ERROR);
    rules.category = QJS_INTL_UNIT_COUNT_EXACT_ONE;
    failed(&data, head, &request, QJS_INTL_DATA_ERROR);
    rules.category = QJS_INTL_UNIT_COUNT_ONE;
    rules.error = QJS_INTL_NO_MEMORY;
    failed(&data, head, &request, QJS_INTL_NO_MEMORY);
    rules.error = QJS_INTL_OK;
    request.resolve = NULL;
    request.category = QJS_INTL_UNIT_COUNT_ONE;
    expect(&data, head_for(&data, "en-GB", "length-denominator"), &request,
           "DENOMINATOR_ONE {0}", "en-GB");
    request.category = QJS_INTL_UNIT_COUNT_OTHER;
    expect(&data, head_for(&data, "en-GB", "length-exact"), &request, "ROOT {0}", "root");
    request.exact = 0;
    failed(&data, head, &request, QJS_INTL_INVALID_ARGUMENT);
    request.exact = -1;
    request.category = QJS_INTL_UNIT_COUNT_NONE;
    failed(&data, head, &request, QJS_INTL_INVALID_ARGUMENT);
    request.category = QJS_INTL_UNIT_COUNT_ONE;
    request.case_name = (QJSIntlBytes){NULL, 1};
    failed(&data, head, &request, QJS_INTL_INVALID_ARGUMENT);
    request.case_name = bytes("bad-case");
    failed(&data, head, &request, QJS_INTL_INVALID_ARGUMENT);
    request.case_name = (QJSIntlBytes){0};
    failed(&data, 0, &request, QJS_INTL_UNSUPPORTED);
    failed(&data, data.node_count + 1, &request, QJS_INTL_INVALID_ARGUMENT);
    failed(NULL, 0, &request, QJS_INTL_INVALID_ARGUMENT);
    failed(&data, head, NULL, QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_pattern_select(&data, head, &request, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_unit_group_find(&data, QJS_INTL_UNIT_GROUP_COMPOUND, bytes("per"), &compound));
    assert(!qjs_intl_unit_overlay_head(&data, locale_id(&shared, "root"), QJS_INTL_UNIT_LONG,
                                       compound, &head));
    failed(&data, head, &request, QJS_INTL_INVALID_ARGUMENT);
    assert(allocations == calls);
    return 0;
}
