/* Range assembly preserves caller-selected ordering, sources and ownership. */
#include "../src/intl/common/range-compose.h"
#include <assert.h>
#include <stdlib.h>

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *value;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    value = malloc(size);
    if (value)
        state->live++;
    return value;
}

static void release(void *opaque, void *value)
{
    Allocation *state = opaque;
    assert(value && state->live);
    state->live--;
    free(value);
}

static void fails(const QJSIntlAllocator *allocator, const QJSIntlRangePiece *pieces,
                   size_t count, size_t limit, QJSIntlStatus expected)
{
    QJSIntlFormattedRange result, zero = {0};
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_range_compose(allocator, pieces, count, limit, &result) == expected);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    uint16_t start[] = {'2', '0', '2', '4'}, end[] = {'2', '0', '2', '5'};
    QJSIntlPart part = {0, 4, QJS_INTL_PART_YEAR};
    QJSIntlFormatted a = {start, 4, &part, 1}, b = {end, 4, &part, 1};
    QJSIntlRangePiece pieces[] = {
        {NULL, {"[", 1}, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED},
        {&b, {0}, 0, QJS_INTL_RANGE_END},
        {NULL, {" \xe2\x86\x92 ", 5}, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED},
        {&a, {0}, 0, QJS_INTL_RANGE_START},
        {NULL, {"]", 1}, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED},
    };
    QJSIntlFormattedRange result;
    size_t i;
    assert(!qjs_intl_range_compose(&allocator, pieces, 5, 13, &result));
    assert(state.calls == 2 && state.live == 2 && result.length == 13 && result.part_count == 5);
    assert(result.text[0] == '[' && result.text[4] == '5' && result.text[6] == 0x2192 &&
           result.text[11] == '4' && result.text[12] == ']');
    assert(result.parts[1].source == QJS_INTL_RANGE_END && result.parts[1].start == 1 &&
           result.parts[1].end == 5 && result.parts[1].type == QJS_INTL_PART_YEAR);
    assert(result.parts[3].source == QJS_INTL_RANGE_START && result.parts[3].start == 8 &&
           result.parts[3].end == 12);
    start[0] = '9';
    assert(result.text[8] == '2');
    start[0] = '2';
    qjs_intl_formatted_range_clear(&allocator, &result);
    qjs_intl_formatted_range_clear(&allocator, &result);
    assert(!state.live);
    fails(&allocator, pieces, 5, 12, QJS_INTL_OVERFLOW);
    for (i = 1; i <= 2; i++) {
        state.fail_at = state.calls + i;
        fails(&allocator, pieces, 5, 13, QJS_INTL_NO_MEMORY);
        assert(!state.live);
    }
    state.fail_at = 0;
    pieces[0].literal = (QJSIntlBytes){0};
    pieces[4].literal = (QJSIntlBytes){0};
    assert(!qjs_intl_range_compose(&allocator, pieces, 5, 11, &result));
    assert(result.part_count == 3 && result.length == 11);
    qjs_intl_formatted_range_clear(&allocator, &result);
    pieces[2].literal = (QJSIntlBytes){"\xf0\x9f\x93\x85", 4};
    assert(!qjs_intl_range_compose(&allocator, pieces, 5, 10, &result));
    assert(result.length == 10 && result.text[4] == 0xd83d && result.text[5] == 0xdcc5);
    qjs_intl_formatted_range_clear(&allocator, &result);
    pieces[2].literal = (QJSIntlBytes){"\xed\xa0\x80", 3};
    fails(&allocator, pieces, 5, 20, QJS_INTL_DATA_ERROR);
    pieces[2].literal = (QJSIntlBytes){"-", 1};
    pieces[2].source = (QJSIntlRangeSource)-1;
    fails(&allocator, pieces, 5, 20, QJS_INTL_INVALID_ARGUMENT);
    pieces[2].source = QJS_INTL_RANGE_SHARED;
    part.start = 1;
    fails(&allocator, pieces, 5, 20, QJS_INTL_INVALID_ARGUMENT);
    part.start = 0;
    part.type = (QJSIntlPartType)-1;
    fails(&allocator, pieces, 5, 20, QJS_INTL_INVALID_ARGUMENT);
    part.type = QJS_INTL_PART_YEAR;
    {
        /* Native bodies are length-delimited UTF-16, not scalar UTF-8. */
        uint16_t text[] = {0, 0xd800};
        QJSIntlPart parts[] = {{0, 0, QJS_INTL_PART_ERA}, {0, 2, QJS_INTL_PART_YEAR}};
        QJSIntlFormatted body = {text, 2, parts, 2};
        QJSIntlRangePiece piece = {&body, {0}, 0, QJS_INTL_RANGE_END};
        assert(!qjs_intl_range_compose(&allocator, &piece, 1, 2, &result));
        assert(result.length == 2 && result.text[0] == 0 && result.text[1] == 0xd800);
        assert(result.part_count == 2 && result.parts[0].start == 0 && result.parts[0].end == 0 &&
               result.parts[0].type == QJS_INTL_PART_ERA && result.parts[0].source == QJS_INTL_RANGE_END);
        qjs_intl_formatted_range_clear(&allocator, &result);
    }
    fails(&allocator, pieces, 5, 0, QJS_INTL_INVALID_ARGUMENT);
    fails(&allocator, pieces, 0, 20, QJS_INTL_INVALID_ARGUMENT);
    pieces[0].literal = (QJSIntlBytes){0};
    fails(&allocator, pieces, 1, 20, QJS_INTL_INVALID_ARGUMENT);
    assert(!state.live);
    return 0;
}
