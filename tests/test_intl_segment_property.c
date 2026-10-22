/* Native Unicode range ownership, domains and failure propagation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../tools/intl/segment-property.h"
#include "../src/cutils/cutils.h"
#include "../src/unicode/libunicode.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, live, fail_at;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *pointer;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    pointer = malloc(size);
    assert(pointer);
    state->live++;
    return pointer;
}

static void *resize(void *opaque, void *pointer, size_t size)
{
    Allocation *state = opaque;
    assert(pointer && size);
    if (++state->calls == state->fail_at)
        return NULL;
    pointer = realloc(pointer, size);
    assert(pointer);
    return pointer;
}

static void release(void *opaque, void *pointer)
{
    Allocation *state = opaque;
    assert(pointer && state->live);
    state->live--;
    free(pointer);
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int contains(const QJSIntlSegmentRanges *ranges, uint32_t code)
{
    uint32_t i;
    for (i = 0; i < ranges->count; i += 2) {
        if (ranges->points[i] <= code && code < ranges->points[i + 1])
            return 1;
    }
    return 0;
}

static void query(const QJSIntlAllocator *allocator, const char *property, const char *value,
                   QJSIntlSegmentRanges *ranges)
{
    uint32_t i;
    assert(qjs_intl_segment_property(allocator, bytes(property), bytes(value), ranges) == QJS_INTL_OK);
    assert(!(ranges->count & 1));
    for (i = 0; i < ranges->count; i += 2) {
        assert(ranges->points[i] < ranges->points[i + 1]);
        assert(ranges->points[i + 1] <= 0x110000);
        if (i)
            assert(ranges->points[i - 1] < ranges->points[i]);
    }
}

static void allocation_failures(const QJSIntlAllocator *allocator, const char *property,
                                 const char *value)
{
    Allocation *state = allocator->opaque;
    QJSIntlSegmentRanges ranges = {0};
    size_t calls, i;
    assert(!state->live);
    state->calls = state->fail_at = 0;
    query(allocator, property, value, &ranges);
    calls = state->calls;
    qjs_intl_segment_ranges_clear(allocator, &ranges);
    assert(calls && !state->live);
    for (i = 1; i <= calls; i++) {
        state->calls = 0;
        state->fail_at = i;
        assert(qjs_intl_segment_property(allocator, bytes(property), bytes(value), &ranges) == QJS_INTL_NO_MEMORY);
        assert(!ranges.points && !ranges.count && !state->live);
    }
    state->fail_at = 0;
    query(allocator, property, value, &ranges);
    qjs_intl_segment_ranges_clear(allocator, &ranges);
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlSegmentRanges ranges = {0};
    size_t i;
    static const struct {
        const char *property, *value;
        uint32_t yes, no;
    } cases[] = {
        {"Grapheme_Cluster_Break", "Extend", 0x0301, 'A'},
        {"Word_Break", "ALetter", 'A', '.'},
        {"Sentence_Break", "ATerm", '.', 'A'},
        {"Indic_Conjunct_Break", "Linker", 0x094d, 0x0915},
        {"Extended_Pictographic", "True", 0x1f600, 'A'},
        {"Extended_Pictographic", "False", 'A', 0x1f600},
        {"", "Hiragana", 0x3042, 0x30a2},
        {"gc", "Cs", 0xd800, 'A'},
        {"scx", "Hira", 0x30fc, 'A'},
        {"Script", "Unknown", 0x0378, 'A'},
        {"General_Category", "L", 'A', '0'},
        {"Assigned", "True", 'A', 0x0378},
        {"Assigned", "False", 0x0378, 'A'},
        {"Grapheme_Base", "True", 'A', 0x0301},
        {"ASCII", "No", 0x10ffff, 0x7f},
    };
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        query(&allocator, cases[i].property, cases[i].value, &ranges);
        assert(contains(&ranges, cases[i].yes));
        assert(!contains(&ranges, cases[i].no));
        assert(!contains(&ranges, 0x110000));
        qjs_intl_segment_ranges_clear(&allocator, &ranges);
        assert(!state.live && !ranges.points && !ranges.count);
    }
    query(&allocator, "", "Any", &ranges);
    assert(ranges.count == 2 && !ranges.points[0] && ranges.points[1] == 0x110000);
    qjs_intl_segment_ranges_clear(&allocator, &ranges);
    query(&allocator, "Any", "False", &ranges);
    assert(!ranges.count);
    qjs_intl_segment_ranges_clear(&allocator, &ranges);

    /* These are private compiler properties, not new JavaScript RegExp names. */
    assert(!lre_unicode_property_allowed("Word_Break"));
    assert(!lre_unicode_property_allowed("Grapheme_Cluster_Break"));
    assert(qjs_intl_segment_property(&allocator, bytes("WB"), bytes("missing"), &ranges) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_segment_property(&allocator, bytes("missing"), bytes("True"), &ranges) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_segment_property(&allocator, (QJSIntlBytes){"WB\0x", 4}, bytes("Other"), &ranges) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_segment_property(&allocator, bytes("WB"), (QJSIntlBytes){NULL, 1}, &ranges) == QJS_INTL_INVALID_ARGUMENT);
    assert(!ranges.points && !ranges.count && !state.live);

    /* Exercise every physical range-growth allocation, including failed
       realloc which leaves the previous owned block requiring cleanup. */
    allocation_failures(&allocator, "GCB", "Extend");
    allocation_failures(&allocator, "scx", "Hira");
    allocation_failures(&allocator, "Assigned", "True");
    allocation_failures(&allocator, "Grapheme_Base", "True");
    allocation_failures(&allocator, "Assigned", "False");
    return 0;
}
