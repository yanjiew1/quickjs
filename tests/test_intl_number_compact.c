/* Authored compact affixes: Unicode boundaries, symbols, parts and ownership.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "../src/intl/number/compact-format.h"
#include "../src/intl/number/compact-family.h"
#include "../src/intl/number/body.h"
#include "../src/cutils/cutils.h"
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

static void expected(const QJSIntlFormatted *result, const char *text)
{
    const uint8_t *at = (const uint8_t *)text, *end = at + strlen(text), *next;
    size_t pos = 0, i, part_end = 0;
    while (at < end) {
        int c = unicode_from_utf8(at, (int)(end - at), &next);
        assert(c > 0);
        if (c > 0xffff) {
            c -= 0x10000;
            assert(pos + 1 < result->length);
            assert(result->text[pos++] == 0xd800 + (c >> 10));
            assert(result->text[pos++] == 0xdc00 + (c & 1023));
        } else {
            assert(pos < result->length);
            assert(result->text[pos++] == c);
        }
        at = next;
    }
    assert(pos == result->length);
    for (i = 0; i < result->part_count; i++) {
        assert(result->parts[i].start == part_end);
        part_end = result->parts[i].end;
        assert(part_end >= result->parts[i].start && part_end <= pos);
        assert(part_end > result->parts[i].start || result->parts[i].type == QJS_INTL_PART_CURRENCY);
    }
    assert(part_end == pos);
}

static QJSIntlCompactPattern pattern(const QJSIntlCompactData *data, QJSIntlCompactKind kind,
                                     uint32_t magnitude, QJSIntlCompactCount count)
{
    QJSIntlCompactMap map;
    QJSIntlCompactPattern out;
    uint32_t latin;
    assert(!qjs_intl_numbering_system_find(&data->numbering, bytes("latn"), &latin));
    assert(!qjs_intl_compact_map_at(data, 0, latin, kind, &map));
    assert(!qjs_intl_compact_pattern_find(data, &map, magnitude, count, QJS_INTL_COMPACT_STANDARD,
                                          &out));
    return out;
}

static QJSIntlStatus format(const QJSIntlAllocator *allocator, const QJSIntlCompactData *data,
                            const QJSIntlCompactPattern *pattern, const QJSIntlFormatted *body,
                            size_t limit, QJSIntlFormatted *out)
{
    /* A multi-scalar minus symbol also exercises UTF-16 length accounting. */
    QJSIntlNumberAffixSymbols symbols = {.minus = bytes("؜-"), .plus = bytes("+"),
                                          .currency = bytes("USD")};
    return qjs_intl_number_compact_format(allocator, data, pattern, &symbols, body, limit, out);
}

static void test_families(const QJSIntlCompactData *data)
{
    QJSIntlCompactFamily family, zero = {0};
    QJSIntlCompactPattern selected, empty = {0};
    QJSIntlCompactMap map;
    QJSIntlCurrencyBoundary boundary = {1, 1, 1, 1, 1};
    uint32_t latin;
    assert(!qjs_intl_numbering_system_find(&data->numbering, bytes("latn"), &latin));
    assert(!qjs_intl_compact_map_at(data, 0, latin, QJS_INTL_COMPACT_CURRENCY_SHORT, &map));
    assert(!qjs_intl_compact_family_at(data, &map, 3, &boundary, &family));
    assert(family.baseline.mode == QJS_INTL_COMPACT_FALLBACK && !family.baseline.exponent);
    assert(family.requested == QJS_INTL_COMPACT_STANDARD && !family.alpha_available);
    assert(qjs_intl_compact_family_pattern(data, &map, &family, QJS_INTL_COMPACT_ONE,
                                            &selected) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&selected, &empty, sizeof(empty)));
    assert(!qjs_intl_compact_map_at(data, 1, latin, QJS_INTL_COMPACT_CURRENCY_SHORT, &map));
    assert(!qjs_intl_compact_family_at(data, &map, 15, &boundary, &family));
    assert(family.requested == QJS_INTL_COMPACT_ALPHA && family.alpha_available);
    assert(family.baseline.variant == QJS_INTL_COMPACT_ALPHA && family.baseline.exponent == 13);
    assert(qjs_intl_compact_family_pattern(data, &map, &family, QJS_INTL_COMPACT_ONE,
                                            &selected) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&selected, &empty, sizeof(empty)));
    assert(!qjs_intl_compact_family_at(data, &map, 18, &boundary, &family));
    assert(family.requested == QJS_INTL_COMPACT_ALPHA && family.alpha_available);
    assert(family.baseline.variant == QJS_INTL_COMPACT_STANDARD && family.baseline.exponent == 18);
    assert(!qjs_intl_compact_family_pattern(data, &map, &family, QJS_INTL_COMPACT_ONE, &selected));
    assert(selected.variant == QJS_INTL_COMPACT_ALPHA && selected.exponent == 18);
    /* A punctuation-ended symbol such as Fr. selects the ordinary pattern,
       but its sparse alpha family still suppresses legacy currency spacing. */
    boundary.compact_before_number = 0;
    assert(!qjs_intl_compact_family_at(data, &map, 18, &boundary, &family));
    assert(family.requested == QJS_INTL_COMPACT_STANDARD && family.alpha_available);
    assert(family.baseline.exponent == 18);
    assert(!qjs_intl_compact_family_pattern(data, &map, &family, QJS_INTL_COMPACT_OTHER, &selected));
    assert(selected.variant == QJS_INTL_COMPACT_STANDARD);
    boundary.compact_before_number = 1;
    assert(!qjs_intl_compact_family_at(data, &map, 21, &boundary, &family));
    assert(!family.alpha_available); /* An explicitly empty alpha is unusable. */
    assert(!qjs_intl_compact_family_pattern(data, &map, &family, QJS_INTL_COMPACT_ONE, &selected));
    assert(selected.variant == QJS_INTL_COMPACT_STANDARD && selected.exponent == 21);
    assert(!qjs_intl_compact_family_at(data, &map, 24, &boundary, &family));
    assert(family.requested == QJS_INTL_COMPACT_STANDARD && family.alpha_available);
    assert(family.baseline.exponent == 24); /* Literal blocks compact adjacency. */
    assert(!qjs_intl_compact_family_at(data, &map, 15, NULL, &family));
    assert(family.requested == QJS_INTL_COMPACT_STANDARD && family.baseline.exponent == 15);
    memset(&family, 0xff, sizeof(family));
    assert(qjs_intl_compact_family_at(data, &map, 99, &boundary, &family) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&family, &zero, sizeof(zero)));
    assert(qjs_intl_compact_family_at(data, &map, 3, &boundary, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData pack;
    QJSIntlNumberingData numbering;
    QJSIntlCompactData data;
    QJSIntlCompactPattern selected;
    uint16_t digits[] = {'1', '.', '2'};
    QJSIntlPart parts[] = {{0, 1, QJS_INTL_PART_INTEGER},
                           {1, 2, QJS_INTL_PART_DECIMAL},
                           {2, 3, QJS_INTL_PART_FRACTION}};
    QJSIntlFormatted body = {digits, 3, parts, 3}, result, zero = {0};
    size_t i;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &pack));
    assert(!qjs_intl_numbering_data_open(&pack, &allocator, &numbering));
    assert(!qjs_intl_compact_data_open(&numbering, &allocator, &data));
    assert(!state.live);
    test_families(&data);
    selected = pattern(&data, QJS_INTL_COMPACT_DECIMAL_LONG, 3, QJS_INTL_COMPACT_EXACT_ONE);
    assert(!format(&allocator, &data, &selected, NULL, 5, &result));
    expected(&result, "mille");
    assert(result.part_count == 1 && result.parts[0].type == QJS_INTL_PART_COMPACT);
    qjs_intl_number_body_clear(&allocator, &result);
    assert(format(&allocator, &data, &selected, NULL, 4, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)));
    selected = pattern(&data, QJS_INTL_COMPACT_DECIMAL_SHORT, 9, QJS_INTL_COMPACT_OTHER);
    assert(!format(&allocator, &data, &selected, &body, 100, &result));
    expected(&result, " 1.2 giga name ");
    assert(result.part_count == 7);
    assert(result.parts[0].type == QJS_INTL_PART_LITERAL);
    assert(result.parts[4].type == QJS_INTL_PART_LITERAL);
    assert(result.parts[5].type == QJS_INTL_PART_COMPACT);
    assert(result.parts[5].end - result.parts[5].start == 9);
    assert(result.parts[6].type == QJS_INTL_PART_LITERAL);
    qjs_intl_number_body_clear(&allocator, &result);
    selected = pattern(&data, QJS_INTL_COMPACT_DECIMAL_SHORT, 12, QJS_INTL_COMPACT_OTHER);
    assert(!format(&allocator, &data, &selected, &body, 5, &result));
    expected(&result, "1.2💯");
    assert(result.parts[3].start == 3 && result.parts[3].end == 5);
    qjs_intl_number_body_clear(&allocator, &result);
    assert(format(&allocator, &data, &selected, &body, 4, &result) == QJS_INTL_OVERFLOW);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(format(&allocator, &data, &selected, NULL, 100, &result) == QJS_INTL_INVALID_ARGUMENT);
    parts[1].start = 0;
    assert(format(&allocator, &data, &selected, &body, 100, &result) == QJS_INTL_INVALID_ARGUMENT);
    parts[1].start = 1;
    selected = pattern(&data, QJS_INTL_COMPACT_DECIMAL_SHORT, 15, QJS_INTL_COMPACT_OTHER);
    assert(!format(&allocator, &data, &selected, &body, 100, &result));
    expected(&result, "1.2 name؜-part end");
    assert(result.part_count == 5 && result.parts[4].type == QJS_INTL_PART_COMPACT);
    qjs_intl_number_body_clear(&allocator, &result);
    selected = pattern(&data, QJS_INTL_COMPACT_DECIMAL_SHORT, 18, QJS_INTL_COMPACT_OTHER);
    assert(!format(&allocator, &data, &selected, &body, 100, &result));
    expected(&result, "1.2\t\n\r\xc2\x85\u1680\u2000\u2028\u2029\u202f\u205f\u3000");
    assert(result.part_count == 4 && result.parts[3].type == QJS_INTL_PART_LITERAL);
    qjs_intl_number_body_clear(&allocator, &result);
    for (i = 1; i <= 2; i++) {
        state.fail_at = state.calls + i;
        assert(format(&allocator, &data, &selected, &body, 100, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
    }
    state.fail_at = 0;
    {
        QJSIntlCompactMap map;
        QJSIntlNumberAffixSymbols symbols = {0};
        uint32_t latin;
        assert(!qjs_intl_numbering_system_find(&data.numbering, bytes("latn"), &latin));
        assert(!qjs_intl_compact_map_at(&data, 1, latin, QJS_INTL_COMPACT_CURRENCY_SHORT, &map));
        assert(!qjs_intl_compact_pattern_find(&data, &map, 3, QJS_INTL_COMPACT_OTHER,
                                              QJS_INTL_COMPACT_STANDARD, &selected));
        assert(!format(&allocator, &data, &selected, &body, 100, &result));
        expected(&result, "USD1.2K");
        assert(result.parts[0].type == QJS_INTL_PART_CURRENCY && result.parts[0].end == 3);
        assert(result.parts[4].type == QJS_INTL_PART_COMPACT);
        qjs_intl_number_body_clear(&allocator, &result);
        assert(format(&allocator, &data, &selected, &body, 6, &result) == QJS_INTL_OVERFLOW);
        assert(!memcmp(&result, &zero, sizeof(result)));
        assert(!qjs_intl_number_compact_format(&allocator, &data, &selected, &symbols, &body, 100, &result));
        expected(&result, "1.2K");
        assert(result.parts[0].type == QJS_INTL_PART_CURRENCY && !result.parts[0].end);
        qjs_intl_number_body_clear(&allocator, &result);
        symbols.currency = bytes("\xed\xa0\x80");
        assert(qjs_intl_number_compact_format(&allocator, &data, &selected, &symbols, &body, 100, &result) == QJS_INTL_DATA_ERROR);
        assert(!memcmp(&result, &zero, sizeof(result)));
        symbols.currency = bytes("USD");
        symbols.after_currency = bytes("~");
        assert(!qjs_intl_number_compact_format(&allocator, &data, &selected, &symbols, &body, 100, &result));
        expected(&result, "USD~1.2K");
        assert(result.parts[1].type == QJS_INTL_PART_LITERAL);
        qjs_intl_number_body_clear(&allocator, &result);
        for (i = 1; i <= 2; i++) {
            state.fail_at = state.calls + i;
            assert(format(&allocator, &data, &selected, &body, 100, &result) == QJS_INTL_NO_MEMORY);
            assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
        }
        state.fail_at = 0;
        symbols.before_currency = bytes("_");
        assert(!qjs_intl_compact_pattern_find(&data, &map, 6, QJS_INTL_COMPACT_OTHER,
                                              QJS_INTL_COMPACT_STANDARD, &selected));
        assert(!qjs_intl_number_compact_format(&allocator, &data, &selected, &symbols,
                                                &body, 100, &result));
        expected(&result, "1.2_USDK");
        assert(result.parts[3].type == QJS_INTL_PART_LITERAL);
        qjs_intl_number_body_clear(&allocator, &result);
        symbols.currency = bytes("");
        assert(!qjs_intl_number_compact_format(&allocator, &data, &selected, &symbols,
                                                &body, 100, &result));
        expected(&result, "1.2K");
        assert(result.parts[3].type == QJS_INTL_PART_CURRENCY && result.parts[3].start == 3 &&
               result.parts[3].end == 3);
        qjs_intl_number_body_clear(&allocator, &result);
        symbols.currency = bytes("USD");
        assert(!qjs_intl_compact_pattern_find(&data, &map, 9, QJS_INTL_COMPACT_OTHER,
                                              QJS_INTL_COMPACT_STANDARD, &selected));
        assert(!qjs_intl_number_compact_format(&allocator, &data, &selected, &symbols,
                                                &body, 100, &result));
        expected(&result, "1.2K USD");
        qjs_intl_number_body_clear(&allocator, &result);
        assert(!qjs_intl_compact_pattern_find(&data, &map, 12, QJS_INTL_COMPACT_OTHER,
                                              QJS_INTL_COMPACT_STANDARD, &selected));
        assert(!qjs_intl_number_compact_format(&allocator, &data, &selected, &symbols,
                                                &body, 100, &result));
        expected(&result, "USD 1.2K");
        qjs_intl_number_body_clear(&allocator, &result);
    }
    assert(!state.live);
    return 0;
}
