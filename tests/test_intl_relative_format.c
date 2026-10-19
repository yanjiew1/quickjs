/* MakePartsList number provenance, literal roles, output bounds and OOM.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/relative/format.h"
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
static void check_text(const QJSIntlRelativeFormatted *result, const uint16_t *text, size_t count)
{
    assert(result->length == count);
    assert(!count || !memcmp(result->text, text, count * sizeof(*text)));
}
static void check_zero(const QJSIntlRelativeFormatted *result)
{
    QJSIntlRelativeFormatted zero = {0};
    assert(!memcmp(result, &zero, sizeof(zero)));
}
int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    uint16_t text[] = {'1', ',', '2'};
    QJSIntlPart parts[] = {{0, 1, QJS_INTL_PART_INTEGER}, {1, 2, QJS_INTL_PART_LITERAL},
                            {2, 3, QJS_INTL_PART_INTEGER}};
    QJSIntlFormatted body = {text, 3, parts, 3};
    QJSIntlRelativeFormatted result;
    char pattern[] = "\xf0\x9f\x98\x80 {0}!";
    uint16_t expected[] = {0xd83d, 0xde00, ' ', '1', ',', '2', '!'};
    const char *bad_patterns[] = {"{", "}", "{1}", "{0}{0}", "{00}", "{0}x}", "\xc0\x80"};
    size_t i;
    assert(!qjs_intl_relative_pattern_format(&allocator, bytes(pattern), &body,
                                              QJS_INTL_RELATIVE_DAY, 7, &result));
    check_text(&result, expected, 7);
    assert(result.part_count == 5);
    for (i = 0; i < result.part_count; i++) {
        if (i == 0 || i == 4) {
            assert(result.parts[i].type == QJS_INTL_PART_LITERAL);
            assert(result.parts[i].unit == QJS_INTL_RELATIVE_UNIT_COUNT);
        } else {
            assert(result.parts[i].type == parts[i - 1].type);
            assert(result.parts[i].unit == QJS_INTL_RELATIVE_DAY);
            assert(result.parts[i].start == parts[i - 1].start + 3);
            assert(result.parts[i].end == parts[i - 1].end + 3);
        }
    }
    pattern[4] = 'X';
    text[0] = '9';
    parts[0].type = QJS_INTL_PART_FRACTION;
    check_text(&result, expected, 7);
    assert(result.parts[1].type == QJS_INTL_PART_INTEGER);
    pattern[4] = ' ';
    text[0] = '1';
    parts[0].type = QJS_INTL_PART_INTEGER;
    qjs_intl_relative_result_clear(&allocator, &result);
    qjs_intl_relative_result_clear(&allocator, &result);
    assert(!state.live);
    /* Auto literals retain braces and exactly one part, even when empty. */
    assert(!qjs_intl_relative_literal_format(&allocator, bytes("{0}"), 3, &result));
    assert(result.length == 3 && result.part_count == 1 && result.parts[0].end == 3 &&
           result.parts[0].unit == QJS_INTL_RELATIVE_UNIT_COUNT);
    qjs_intl_relative_result_clear(&allocator, &result);
    assert(!qjs_intl_relative_literal_format(&allocator, (QJSIntlBytes){0}, 1, &result));
    assert(!result.length && result.part_count == 1 && !result.parts[0].start && !result.parts[0].end);
    qjs_intl_relative_result_clear(&allocator, &result);
    /* Literal-only counted patterns do not insert a formatted number. */
    assert(!qjs_intl_relative_pattern_format(&allocator, bytes("yesterday"), NULL,
                                              QJS_INTL_RELATIVE_DAY, 9, &result));
    assert(result.length == 9 && result.part_count == 1 && result.parts[0].unit == QJS_INTL_RELATIVE_UNIT_COUNT);
    qjs_intl_relative_result_clear(&allocator, &result);
    assert(!qjs_intl_relative_pattern_format(&allocator, (QJSIntlBytes){0}, NULL,
                                              QJS_INTL_RELATIVE_DAY, 1, &result));
    assert(!result.length && !result.part_count);
    qjs_intl_relative_result_clear(&allocator, &result);
    assert(!qjs_intl_relative_pattern_format(&allocator, bytes("{0}"), &body,
                                              QJS_INTL_RELATIVE_DAY, 3, &result));
    assert(result.part_count == 3 && result.parts[1].unit == QJS_INTL_RELATIVE_DAY);
    qjs_intl_relative_result_clear(&allocator, &result);
    for (i = 0; i < sizeof(bad_patterns) / sizeof(*bad_patterns); i++) {
        assert(qjs_intl_relative_pattern_format(&allocator, bytes(bad_patterns[i]), &body,
                                                 QJS_INTL_RELATIVE_DAY, 40, &result) == QJS_INTL_DATA_ERROR);
        check_zero(&result);
    }
    assert(qjs_intl_relative_pattern_format(&allocator, bytes("{0}"), NULL,
                                             QJS_INTL_RELATIVE_DAY, 20, &result) == QJS_INTL_INVALID_ARGUMENT);
    check_zero(&result);
    parts[1].start = 0;
    assert(qjs_intl_relative_pattern_format(&allocator, bytes("{0}"), &body,
                                             QJS_INTL_RELATIVE_DAY, 20, &result) == QJS_INTL_INVALID_ARGUMENT);
    check_zero(&result);
    parts[1].start = 1;
    assert(qjs_intl_relative_pattern_format(&allocator, bytes("\xf0\x9f\x98\x80 {0}!"), &body,
                                             QJS_INTL_RELATIVE_DAY, 6, &result) == QJS_INTL_OVERFLOW);
    check_zero(&result);
    assert(qjs_intl_relative_literal_format(&allocator, bytes("abc"), 2, &result) == QJS_INTL_OVERFLOW);
    check_zero(&result);
    assert(qjs_intl_relative_literal_format(&allocator, (QJSIntlBytes){"a\0b", 3}, 10, &result) == QJS_INTL_DATA_ERROR);
    check_zero(&result);
    for (i = 1; i <= 2; i++) {
        state.calls = 0;
        state.fail_at = i;
        assert(qjs_intl_relative_pattern_format(&allocator, bytes("in {0} days"), &body,
                                                 QJS_INTL_RELATIVE_DAY, 30, &result) == QJS_INTL_NO_MEMORY);
        check_zero(&result);
        assert(!state.live && state.calls == i);
    }
    state.fail_at = 0;
    assert(!qjs_intl_relative_pattern_format(&allocator, bytes("in {0} days"), &body,
                                              QJS_INTL_RELATIVE_DAY, 30, &result));
    qjs_intl_relative_result_clear(&allocator, &result);
    assert(!state.live);
    qjs_intl_relative_result_clear(&allocator, NULL);
    return 0;
}
