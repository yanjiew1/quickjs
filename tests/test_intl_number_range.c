/* Range composition: displayed-text identity, source parts and bounded ownership.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/number/range.h"
#include "../src/intl/number/number.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, live, fail_at;
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

static void parser(void)
{
    static const char *patterns[] = {"{0}–{1}", "{0}～{1}", "{0}..{1}", "{0} – {1}"};
    static const char *invalid[] = {"{1}-{0}", "{0}x{1}tail", "{0}{0}{1}", "{0}{2}{1}",
                                     "{0}}{1}", "{0}\xed\xa0\x80{1}"};
    QJSIntlBytes result, zero = {0};
    size_t i;
    for (i = 0; i < sizeof(patterns) / sizeof(*patterns); i++) {
        assert(!qjs_intl_number_range_separator(bytes(patterns[i]), &result));
        assert(result.data == patterns[i] + 3 && result.length == strlen(patterns[i]) - 6);
    }
    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_number_range_separator(bytes(invalid[i]), &result) == QJS_INTL_DATA_ERROR);
        assert(!memcmp(&result, &zero, sizeof(result)));
    }
    assert(qjs_intl_number_range_separator(bytes("{0}{1}"), &result) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_number_range_separator((QJSIntlBytes){0}, &result) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_number_range_separator((QJSIntlBytes){NULL, 1}, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_range_separator((QJSIntlBytes){"x", SIZE_MAX}, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_range_separator(bytes("{0}-{1}"), NULL) == QJS_INTL_INVALID_ARGUMENT);
}

static void failed(const QJSIntlAllocator *allocator, QJSIntlBytes separator, QJSIntlBytes approximately,
                   const QJSIntlFormatted *start, const QJSIntlFormatted *end, size_t limit,
                   QJSIntlStatus status)
{
    QJSIntlFormattedRange result, zero = {0};
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_number_range_compose(allocator, separator, approximately, start, end, limit, &result) == status);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    uint16_t a[] = {'1', 'K', ' ', 'm'}, b[] = {'2', 'K', ' ', 'm'};
    QJSIntlPart parts[] = {{0, 0, QJS_INTL_PART_CURRENCY}, {0, 1, QJS_INTL_PART_INTEGER},
                           {1, 2, QJS_INTL_PART_COMPACT}, {2, 3, QJS_INTL_PART_LITERAL},
                           {3, 4, QJS_INTL_PART_UNIT}};
    QJSIntlPart different_parts[] = {{0, 4, QJS_INTL_PART_INTEGER}};
    QJSIntlFormatted start = {a, 4, parts, 5}, end = {b, 4, parts, 5}, invalid;
    QJSIntlFormattedRange result;
    size_t i, calls;
    parser();
    assert(!qjs_intl_number_range_compose(&allocator, bytes("–"), bytes("~"), &start, &end, 9, &result));
    assert(state.calls == 2 && result.length == 9 && result.part_count == 11);
    assert(!memcmp(result.text, a, sizeof(a)) && result.text[4] == 0x2013 &&
           !memcmp(result.text + 5, b, sizeof(b)));
    for (i = 0; i < 5; i++) {
        assert(result.parts[i].source == QJS_INTL_RANGE_START && result.parts[i].type == parts[i].type);
        assert(result.parts[i].start == parts[i].start && result.parts[i].end == parts[i].end);
        assert(result.parts[i + 6].source == QJS_INTL_RANGE_END && result.parts[i + 6].type == parts[i].type);
        assert(result.parts[i + 6].start == parts[i].start + 5 && result.parts[i + 6].end == parts[i].end + 5);
    }
    assert(result.parts[5].source == QJS_INTL_RANGE_SHARED && result.parts[5].type == QJS_INTL_PART_LITERAL &&
           result.parts[5].start == 4 && result.parts[5].end == 5);
    a[0] = '9';
    assert(result.text[0] == '1');
    a[0] = '1';
    qjs_intl_number_range_result_clear(&allocator, &result);
    qjs_intl_number_range_result_clear(&allocator, &result);
    failed(&allocator, bytes("–"), bytes("~"), &start, &end, 8, QJS_INTL_OVERFLOW);
    for (i = 1; i <= 2; i++) {
        state.fail_at = state.calls + i;
        failed(&allocator, bytes("–"), bytes("~"), &start, &end, 9, QJS_INTL_NO_MEMORY);
        assert(!state.live);
    }
    state.fail_at = 0;
    b[0] = '1';
    end.parts = different_parts;
    end.part_count = 1;
    assert(!qjs_intl_number_range_compose(&allocator, bytes("–"), bytes("ca. "), &start, &end, 8, &result));
    assert(result.length == 8 && result.part_count == 6);
    assert(result.text[0] == 'c' && result.text[1] == 'a' && result.text[2] == '.' && result.text[3] == ' ');
    assert(result.parts[0].type == QJS_INTL_PART_APPROXIMATELY_SIGN && result.parts[0].end == 4);
    for (i = 0; i < result.part_count; i++)
        assert(result.parts[i].source == QJS_INTL_RANGE_SHARED);
    for (i = 0; i < start.part_count; i++) {
        assert(result.parts[i + 1].type == parts[i].type && result.parts[i + 1].start == parts[i].start + 4 &&
               result.parts[i + 1].end == parts[i].end + 4);
    }
    qjs_intl_number_range_result_clear(&allocator, &result);
    failed(&allocator, bytes("–"), bytes("ca. "), &start, &end, 7, QJS_INTL_OVERFLOW);
    assert(!qjs_intl_number_range_compose(&allocator, bytes("–"), (QJSIntlBytes){0}, &start, &end, 4, &result));
    assert(result.part_count == 5 && result.length == 4);
    for (i = 0; i < result.part_count; i++)
        assert(result.parts[i].source == QJS_INTL_RANGE_SHARED && result.parts[i].type == parts[i].type);
    qjs_intl_number_range_result_clear(&allocator, &result);
    assert(!qjs_intl_number_range_compose(&allocator, bytes("–"), bytes("\xf0\x9f\x93\x88"), &start, &end, 6, &result));
    assert(result.length == 6 && result.text[0] == 0xd83d && result.text[1] == 0xdcc8 &&
           result.parts[0].end == 2 && result.parts[1].start == 2);
    qjs_intl_number_range_result_clear(&allocator, &result);
    a[0] = b[0] = 0xd800; /* Existing UTF-16 body code units remain unchanged. */
    assert(!qjs_intl_number_range_compose(&allocator, bytes("～"), bytes("約"), &start, &end, 5, &result));
    assert(result.text[0] == 0x7d04 && result.text[1] == 0xd800 &&
           result.parts[0].type == QJS_INTL_PART_APPROXIMATELY_SIGN);
    qjs_intl_number_range_result_clear(&allocator, &result);
    a[0] = b[0] = '1';
    for (i = 1; i <= 2; i++) {
        state.fail_at = state.calls + i;
        failed(&allocator, bytes("–"), bytes("~"), &start, &end, 5, QJS_INTL_NO_MEMORY);
        assert(!state.live);
    }
    state.fail_at = 0;
    {
        uint16_t x[] = {0, 0xd83d, 0xde80, '1'}, y[] = {0, 0xd83d, 0xde80, '2'};
        QJSIntlPart span = {0, 4, QJS_INTL_PART_LITERAL};
        QJSIntlFormatted left = {x, 4, &span, 1}, right = {y, 4, &span, 1};
        assert(!qjs_intl_number_range_compose(&allocator, bytes("～"), bytes("約"),
                                               &left, &right, 9, &result));
        assert(result.length == 9 && result.part_count == 3 && result.text[4] == 0xff5e &&
               !memcmp(result.text, x, sizeof(x)) && !memcmp(result.text + 5, y, sizeof(y)));
        assert(result.parts[0].source == QJS_INTL_RANGE_START && result.parts[2].start == 5 &&
               result.parts[2].source == QJS_INTL_RANGE_END);
        qjs_intl_number_range_result_clear(&allocator, &result);
        y[3] = '1';
        assert(!qjs_intl_number_range_compose(&allocator, bytes("～"), bytes("約"),
                                               &left, &right, 5, &result));
        assert(result.length == 5 && result.part_count == 2 && result.text[0] == 0x7d04 &&
               !memcmp(result.text + 1, x, sizeof(x)) && result.parts[1].start == 1 &&
               result.parts[1].source == QJS_INTL_RANGE_SHARED);
        qjs_intl_number_range_result_clear(&allocator, &result);
    }
    calls = state.calls;
    failed(&allocator, bytes("–"), bytes("\xed\xa0\x80"), &start, &end, 20, QJS_INTL_DATA_ERROR);
    b[0] = '2';
    failed(&allocator, bytes("\xc0\xaf"), bytes("~"), &start, &end, 20, QJS_INTL_DATA_ERROR);
    failed(&allocator, (QJSIntlBytes){0}, bytes("~"), &start, &end, 20, QJS_INTL_INVALID_ARGUMENT);
    failed(&allocator, bytes("–"), (QJSIntlBytes){NULL, 1}, &start, &end, 20, QJS_INTL_INVALID_ARGUMENT);
    failed(&allocator, bytes("–"), bytes("~"), &start, &end, 0, QJS_INTL_INVALID_ARGUMENT);
    failed(NULL, bytes("–"), bytes("~"), &start, &end, 20, QJS_INTL_INVALID_ARGUMENT);
    failed(&allocator, bytes("–"), bytes("~"), NULL, &end, 20, QJS_INTL_INVALID_ARGUMENT);
    invalid = start;
    invalid.length = SIZE_MAX;
    failed(&allocator, bytes("–"), bytes("~"), &invalid, &end, 20, QJS_INTL_INVALID_ARGUMENT);
    invalid = start;
    invalid.part_count = SIZE_MAX;
    failed(&allocator, bytes("–"), bytes("~"), &invalid, &end, 20, QJS_INTL_INVALID_ARGUMENT);
    parts[1].start = 1;
    failed(&allocator, bytes("–"), bytes("~"), &start, &end, 20, QJS_INTL_INVALID_ARGUMENT);
    parts[1].start = 0;
    parts[1].type = QJS_INTL_PART_ELEMENT;
    failed(&allocator, bytes("–"), bytes("~"), &start, &end, 20, QJS_INTL_INVALID_ARGUMENT);
    parts[1].type = QJS_INTL_PART_INTEGER;
    assert(state.calls == calls && !state.live);
    qjs_intl_number_range_result_clear(&allocator, NULL);
    return 0;
}
