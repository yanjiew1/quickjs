/* Unit parts preserve numeric parts, exact limits and independent lifetimes.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/unit/format.h"
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

static void failed(const QJSIntlAllocator *allocator, const QJSIntlUnitPattern *pattern,
                   const QJSIntlFormatted *body, size_t limit, QJSIntlStatus status)
{
    QJSIntlFormatted result, zero = {0};
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_unit_pattern_format(allocator, pattern, body, limit, &result) == status);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    uint16_t text[] = {'-', '1', '.', '2', 'E', '+', '3'};
    QJSIntlPart parts[] = {{0, 1, QJS_INTL_PART_MINUS_SIGN}, {1, 2, QJS_INTL_PART_INTEGER},
                          {2, 3, QJS_INTL_PART_DECIMAL}, {3, 4, QJS_INTL_PART_FRACTION},
                          {4, 5, QJS_INTL_PART_EXPONENT_SEPARATOR}, {5, 6, QJS_INTL_PART_PLUS_SIGN},
                          {6, 7, QJS_INTL_PART_EXPONENT_INTEGER}};
    QJSIntlFormatted body = {text, 7, parts, 7}, result;
    QJSIntlUnitPattern pattern = {bytes(" about "), bytes("\xc2\xa0kilowatt hours ")};
    size_t i, calls;
    assert(!qjs_intl_unit_pattern_format(&allocator, &pattern, &body, 30, &result));
    assert(result.text != body.text && result.parts != body.parts);
    assert(result.length == 30 && result.part_count == 13);
    assert(result.parts[0].type == QJS_INTL_PART_LITERAL && result.parts[0].end == 1);
    assert(result.parts[1].type == QJS_INTL_PART_UNIT && result.parts[1].start == 1 && result.parts[1].end == 6);
    assert(result.parts[2].type == QJS_INTL_PART_LITERAL && result.parts[2].end == 7);
    for (i = 0; i < 7; i++) {
        assert(result.parts[i + 3].type == parts[i].type);
        assert(result.parts[i + 3].start == parts[i].start + 7);
        assert(result.parts[i + 3].end == parts[i].end + 7);
        assert(result.text[i + 7] == text[i]);
    }
    assert(result.parts[10].type == QJS_INTL_PART_LITERAL && result.text[14] == 0xa0);
    assert(result.parts[11].type == QJS_INTL_PART_UNIT && result.parts[11].end - result.parts[11].start == 14);
    assert(result.parts[12].type == QJS_INTL_PART_LITERAL);
    text[0] = '?';
    assert(result.text[7] == '-');
    text[0] = '-';
    qjs_intl_unit_format_clear(&allocator, &result);
    assert(!state.live && !result.text && !result.parts);
    failed(&allocator, &pattern, &body, 29, QJS_INTL_OVERFLOW);
    for (i = 1; i <= 2; i++) {
        state.fail_at = state.calls + i;
        failed(&allocator, &pattern, &body, 30, QJS_INTL_NO_MEMORY);
        assert(!state.live);
    }
    state.fail_at = 0;
    pattern = (QJSIntlUnitPattern){{0}, bytes("\xc2\xa0\xf0\x9f\x8c\xa1")};
    assert(!qjs_intl_unit_pattern_format(&allocator, &pattern, &body, 10, &result));
    assert(result.length == 10 && result.part_count == 9);
    assert(result.text[8] == 0xd83c && result.text[9] == 0xdf21);
    assert(result.parts[8].type == QJS_INTL_PART_UNIT && result.parts[8].start == 8);
    qjs_intl_unit_format_clear(&allocator, &result);
    failed(&allocator, &pattern, &body, 9, QJS_INTL_OVERFLOW);
    pattern = (QJSIntlUnitPattern){{0}, {0}};
    assert(!qjs_intl_unit_pattern_format(&allocator, &pattern, &body, 7, &result));
    assert(result.length == 7 && result.part_count == 8);
    assert(result.parts[7].type == QJS_INTL_PART_UNIT && result.parts[7].start == 7 && result.parts[7].end == 7);
    qjs_intl_unit_format_clear(&allocator, &result);
    pattern.suffix = bytes(" \t");
    assert(!qjs_intl_unit_pattern_format(&allocator, &pattern, &body, 9, &result));
    assert(result.parts[7].type == QJS_INTL_PART_LITERAL && result.parts[7].end == 9);
    assert(result.parts[8].type == QJS_INTL_PART_UNIT && result.parts[8].start == 9 && result.parts[8].end == 9);
    qjs_intl_unit_format_clear(&allocator, &result);
    calls = state.calls;
    pattern.suffix = bytes("\xc0\xaf");
    failed(&allocator, &pattern, &body, 50, QJS_INTL_DATA_ERROR);
    pattern.suffix = bytes("\xed\xa0\x80");
    failed(&allocator, &pattern, &body, 50, QJS_INTL_DATA_ERROR);
    pattern.suffix = bytes("\xf4\x90\x80\x80");
    failed(&allocator, &pattern, &body, 50, QJS_INTL_DATA_ERROR);
    pattern.suffix = (QJSIntlBytes){"\0", 1};
    failed(&allocator, &pattern, &body, 50, QJS_INTL_DATA_ERROR);
    pattern.suffix = (QJSIntlBytes){NULL, 1};
    failed(&allocator, &pattern, &body, 50, QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls && !state.live);
    pattern.suffix = bytes(" m");
    {
        uint16_t unusual[] = {0, 0xd800};
        QJSIntlPart span = {0, 2, QJS_INTL_PART_INTEGER};
        QJSIntlFormatted borrowed = {unusual, 2, &span, 1};
        assert(!qjs_intl_unit_pattern_format(&allocator, &pattern, &borrowed, 4, &result));
        assert(result.text[0] == 0 && result.text[1] == 0xd800);
        assert(result.parts[0].type == QJS_INTL_PART_INTEGER && result.parts[0].end == 2);
        qjs_intl_unit_format_clear(&allocator, &result);
    }
    calls = state.calls;
    parts[0].start = 1;
    failed(&allocator, &pattern, &body, 50, QJS_INTL_INVALID_ARGUMENT);
    parts[0].start = 0;
    parts[0].type = QJS_INTL_PART_ELEMENT;
    failed(&allocator, &pattern, &body, 50, QJS_INTL_INVALID_ARGUMENT);
    parts[0].type = QJS_INTL_PART_MINUS_SIGN;
    failed(&allocator, &pattern, &body, 0, QJS_INTL_INVALID_ARGUMENT);
    failed(NULL, &pattern, &body, 50, QJS_INTL_INVALID_ARGUMENT);
    failed(&allocator, NULL, &body, 50, QJS_INTL_INVALID_ARGUMENT);
    failed(&allocator, &pattern, NULL, 50, QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_pattern_format(&allocator, &pattern, &body, 50, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls && !state.live);
    qjs_intl_unit_format_clear(&allocator, NULL);
    return 0;
}
