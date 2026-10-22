/* Owned source operands, quoted connectors, range provenance and failures. */
#include "../src/intl/datetime/range-layout.h"
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

static QJSIntlBytes bytes(const char *value)
{
    return (QJSIntlBytes){value, strlen(value)};
}

static void expected(const QJSIntlFormattedRange *value, const char *text)
{
    size_t i;
    assert(value->length == strlen(text));
    for (i = 0; i < value->length; i++)
        assert(value->text[i] == (unsigned char)text[i]);
    for (i = 0; i < value->part_count; i++) {
        const QJSIntlRangePart *part = &value->parts[i];
        assert(part->start == (i ? value->parts[i - 1].end : 0));
        assert(part->end > part->start && part->end <= value->length);
        if (part->type == QJS_INTL_PART_HOUR)
            assert(part->source == (value->text[part->start] == 'A' ? QJS_INTL_RANGE_START : QJS_INTL_RANGE_END));
        else
            assert(part->source == QJS_INTL_RANGE_SHARED);
    }
    assert(value->parts[value->part_count - 1].end == value->length);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlDatePattern date, clock, connector, ordinary, different;
    QJSIntlDateRangeLayout layout, zero = {0};
    QJSIntlFormattedRange result, empty = {0};
    QJSIntlPairPattern fallback = {0};
    uint16_t d[] = {'D'}, a[] = {'A'}, b[] = {'B'};
    QJSIntlPart day = {0, 1, QJS_INTL_PART_YEAR}, hour = {0, 1, QJS_INTL_PART_HOUR};
    QJSIntlFormatted date_text = {d, 1, &day, 1};
    QJSIntlFormatted clocks[] = {{a, 1, &hour, 1}, {b, 1, &hour, 1}};
    QJSIntlBytes source = bytes("'<'{0}' on '{1}'>'");
    size_t calls, baseline, i;
    assert(!qjs_intl_date_pattern_compile(&allocator, bytes("y"), &date));
    assert(!qjs_intl_date_pattern_compile(&allocator, bytes("'('HH:mm')'"), &clock));
    assert(!qjs_intl_date_combination_compile(&allocator, source, &connector));
    assert(!qjs_intl_date_pattern_combine(&allocator, &connector, &clock, &date, &ordinary));
    assert(!qjs_intl_date_pattern_compile(&allocator, bytes("y HH:mm"), &different));
    baseline = state.live;
    assert(qjs_intl_date_range_layout_open(&allocator, &date, &clock, source, &different, &layout) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&layout, &zero, sizeof(layout)) && state.live == baseline);
    assert(qjs_intl_date_range_layout_open(&allocator, &clock, &date, source, &ordinary, &layout) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&layout, &zero, sizeof(layout)) && state.live == baseline);
    state.calls = 0;
    assert(!qjs_intl_date_range_layout_open(&allocator, &date, &clock, source, &ordinary, &layout));
    calls = state.calls;
    qjs_intl_date_range_layout_clear(&layout);
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        memset(&layout, 0xff, sizeof(layout));
        assert(qjs_intl_date_range_layout_open(&allocator, &date, &clock, source, &ordinary, &layout) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&layout, &zero, sizeof(layout)) && state.live == baseline);
    }
    state.calls = state.fail_at = 0;
    assert(!qjs_intl_date_range_layout_open(&allocator, &date, &clock, source, &ordinary, &layout));
    qjs_intl_date_pattern_clear(&allocator, &date);
    qjs_intl_date_pattern_clear(&allocator, &clock);
    qjs_intl_date_pattern_clear(&allocator, &connector);
    qjs_intl_date_pattern_clear(&allocator, &ordinary);
    qjs_intl_date_pattern_clear(&allocator, &different);
    assert(state.live == 3); /* No returned pattern borrows the input owner. */
    assert(layout.clock.tokens[0].literal.data[0] == '(');
    assert(!qjs_intl_pair_pattern_parse(bytes("[{1}~{0}]"), &fallback));
    state.calls = 0;
    assert(!qjs_intl_date_range_layout_compose(&layout, &fallback, &date_text, clocks, 100, &result));
    expected(&result, "<[B~A] on D>");
    calls = state.calls;
    qjs_intl_formatted_range_clear(&allocator, &result);
    baseline = state.live;
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_range_layout_compose(&layout, &fallback, &date_text, clocks, 100, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &empty, sizeof(result)) && state.live == baseline);
    }
    state.calls = state.fail_at = 0;
    assert(qjs_intl_date_range_layout_compose(&layout, &fallback, &date_text, clocks, 11, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &empty, sizeof(result)) && state.live == baseline);
    assert(!qjs_intl_date_range_layout_compose(&layout, &fallback, &date_text, clocks, 12, &result));
    expected(&result, "<[B~A] on D>");
    qjs_intl_formatted_range_clear(&allocator, &result);
    qjs_intl_date_range_layout_clear(&layout);
    qjs_intl_date_range_layout_clear(&layout);
    assert(!state.live);
    return 0;
}
