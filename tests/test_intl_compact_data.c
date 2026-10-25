/* Compact metadata: real/source families, sparse selectors, corruption and OOM.
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
#include "../src/intl/number/compact-data.h"
#include "../src/intl/native/wire.h"
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

static int equal(QJSIntlBytes bytes, const char *text)
{
    return bytes.length == strlen(text) && !memcmp(bytes.data, text, bytes.length);
}

static uint32_t locale_id(const QJSIntlData *data, const char *text)
{
    uint32_t i, parent;
    QJSIntlBytes tag;
    for (i = 0; i < data->locales.count; i++) {
        assert(!qjs_intl_data_locale(data, i, &tag, &parent));
        if (equal(tag, text))
            return i;
    }
    abort();
}

static void put_u32(uint8_t *ptr, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        ptr[i] = value >> (8 * i);
}

static void invalid_blob(const QJSIntlAllocator *allocator, const uint8_t *bytes)
{
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlCompactData result;
    QJSIntlCompactData zero = {0};
    assert(!qjs_intl_data_open(bytes, qjs_intl_native_length, allocator, &data));
    assert(!qjs_intl_numbering_data_open(&data, allocator, &numbering));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_compact_data_open(&numbering, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

static void corrupt(const QJSIntlAllocator *allocator, size_t offset, uint32_t value)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    invalid_blob(allocator, copy);
    free(copy);
}

static QJSIntlCompactPattern lookup(const QJSIntlCompactData *data, const char *locale,
                                    uint32_t system, QJSIntlCompactKind kind, uint32_t magnitude,
                                    QJSIntlCompactCount count, QJSIntlCompactVariant variant)
{
    QJSIntlCompactMap map;
    QJSIntlCompactPattern result;
    assert(!qjs_intl_compact_map_at(data, locale_id(&data->numbering.data, locale), system, kind,
                                    &map));
    assert(!qjs_intl_compact_pattern_find(data, &map, magnitude, count, variant, &result));
    return result;
}

static void check_patterns(const QJSIntlCompactData *data, uint32_t latin, uint32_t mathbold)
{
    QJSIntlCompactPattern value;
    QJSIntlCompactMap map;
    QJSIntlNumberPatternToken token;
    value = lookup(data, "en", latin, QJS_INTL_COMPACT_DECIMAL_SHORT, 3, QJS_INTL_COMPACT_OTHER,
                   QJS_INTL_COMPACT_STANDARD);
    assert(value.state == QJS_INTL_NUMBER_PATTERN_VALUE && value.mode == QJS_INTL_COMPACT_SCALED);
    assert(value.exponent == 3 && value.zeros == 1);
    assert(!qjs_intl_compact_token_at(data, &value.forms[0], 0, &token));
    assert(token.kind == QJS_INTL_NUMBER_TOKEN_NUMBER && !token.text.data);
    if (TEST_INTL_AUTHORED) {
        assert(!qjs_intl_compact_map_at(data, locale_id(&data->numbering.data, "en"), mathbold,
                                        QJS_INTL_COMPACT_CURRENCY_SHORT, &map));
        assert(!map.count);
        assert(qjs_intl_compact_pattern_find(data, &map, 3, QJS_INTL_COMPACT_OTHER,
                                             QJS_INTL_COMPACT_STANDARD,
                                             &value) == QJS_INTL_UNSUPPORTED);
        value = lookup(data, "en", latin, QJS_INTL_COMPACT_DECIMAL_LONG, 3,
                       QJS_INTL_COMPACT_EXACT_ONE, QJS_INTL_COMPACT_STANDARD);
        assert(value.mode == QJS_INTL_COMPACT_LITERAL && value.exponent == 3 && !value.zeros);
        assert(value.forms[0].count == 1);
        assert(!qjs_intl_compact_token_at(data, &value.forms[0], 0, &token));
        assert(token.kind == QJS_INTL_NUMBER_TOKEN_LITERAL && equal(token.text, "mille"));
        value = lookup(data, "en", latin, QJS_INTL_COMPACT_CURRENCY_SHORT, 3,
                       QJS_INTL_COMPACT_OTHER, QJS_INTL_COMPACT_STANDARD);
        assert(value.mode == QJS_INTL_COMPACT_FALLBACK && !value.exponent && value.zeros == 1);
        value = lookup(data, "en", latin, QJS_INTL_COMPACT_CURRENCY_SHORT, 3,
                       QJS_INTL_COMPACT_OTHER, QJS_INTL_COMPACT_ALPHA);
        assert(value.mode == QJS_INTL_COMPACT_SCALED && value.exponent == 3);
        value = lookup(data, "en", latin, QJS_INTL_COMPACT_CURRENCY_SHORT, 3,
                       QJS_INTL_COMPACT_OTHER, QJS_INTL_COMPACT_NO_CURRENCY);
        assert(value.state == QJS_INTL_NUMBER_PATTERN_EMPTY && !value.forms[0].tokens);
        value = lookup(data, "root", latin, QJS_INTL_COMPACT_CURRENCY_SHORT, 3,
                       QJS_INTL_COMPACT_ONE, QJS_INTL_COMPACT_ALPHA);
        assert(value.state == QJS_INTL_NUMBER_PATTERN_BLOCKED && !value.forms[0].tokens);
    }
    if (TEST_INTL_PROFILE) {
        value = lookup(data, "it", latin, QJS_INTL_COMPACT_DECIMAL_LONG, 3, QJS_INTL_COMPACT_ONE,
                       QJS_INTL_COMPACT_STANDARD);
        assert(value.mode == QJS_INTL_COMPACT_LITERAL && value.exponent == 3);
        assert(!qjs_intl_compact_token_at(data, &value.forms[0], 0, &token));
        assert(equal(token.text, "mille"));
        value = lookup(data, "zh", latin, QJS_INTL_COMPACT_CURRENCY_SHORT, 3,
                       QJS_INTL_COMPACT_OTHER, QJS_INTL_COMPACT_STANDARD);
        assert(value.mode == QJS_INTL_COMPACT_FALLBACK && !value.exponent);
        value = lookup(data, "zh", latin, QJS_INTL_COMPACT_CURRENCY_SHORT, 3,
                       QJS_INTL_COMPACT_OTHER, QJS_INTL_COMPACT_ALPHA);
        assert(value.mode == QJS_INTL_COMPACT_SCALED && value.exponent == 3);
        value = lookup(data, "kok", mathbold, QJS_INTL_COMPACT_CURRENCY_SHORT, 14,
                       QJS_INTL_COMPACT_OTHER, QJS_INTL_COMPACT_STANDARD);
        assert(value.exponent == 14 && value.zeros == 1);
        value = lookup(data, "kok", mathbold, QJS_INTL_COMPACT_CURRENCY_SHORT, 14,
                       QJS_INTL_COMPACT_OTHER, QJS_INTL_COMPACT_ALPHA);
        assert(value.exponent == 12 && value.zeros == 3);
    }
    /* Indexed and exact queries agree, including sparse optional entries. */
    assert(!qjs_intl_compact_map_at(data, locale_id(&data->numbering.data, "en"), latin,
                                    QJS_INTL_COMPACT_DECIMAL_SHORT, &map));
    {
        uint32_t i;
        for (i = 0; i < map.count; i++) {
            QJSIntlCompactPattern exact;
            assert(!qjs_intl_compact_pattern_at(data, &map, i, &value));
            assert(!qjs_intl_compact_pattern_find(data, &map, value.magnitude, value.count,
                                                  value.variant, &exact));
            assert(!memcmp(&value, &exact, sizeof(value)));
        }
    }
    assert(qjs_intl_compact_pattern_find(data, &map, 2, QJS_INTL_COMPACT_OTHER,
                                         QJS_INTL_COMPACT_STANDARD,
                                         &value) == QJS_INTL_UNSUPPORTED);
    assert(!value.forms[0].tokens && !value.exponent);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlCompactData owner, retry;
    QJSIntlCompactMap map;
    QJSIntlCompactPattern value;
    QJSIntlNumberPatternToken token;
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    uint32_t latin, mathbold, roman, en, i;
    size_t calls, header, rows, maps, entries, patterns, affixes, tokens;
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_numbering_data_open(&data, &allocator, &numbering));
    state.calls = 0;
    assert(!qjs_intl_compact_data_open(&numbering, &allocator, &owner));
    assert(state.calls == 1 && !state.live);
    assert(!qjs_intl_numbering_system_find(&numbering, (QJSIntlBytes){"latn", 4}, &latin));
    assert(!qjs_intl_numbering_system_find(&numbering, (QJSIntlBytes){"mathbold", 8}, &mathbold));
    assert(!qjs_intl_numbering_system_find(&numbering, (QJSIntlBytes){"roman", 5}, &roman));
    en = locale_id(&data, "en");
    calls = state.calls;
    check_patterns(&owner, latin, mathbold);
    assert(qjs_intl_compact_map_at(&owner, en, roman, 0, &map) == QJS_INTL_UNSUPPORTED);
    assert(!map.entries && !map.count);
    assert(qjs_intl_compact_map_at(NULL, en, latin, 0, &map) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_compact_map_at(&owner, data.locales.count, latin, 0, &map) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_compact_map_at(&owner, en, numbering.systems.count, 0, &map) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_compact_map_at(&owner, en, latin, (QJSIntlCompactKind)-1, &map) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_compact_map_at(&owner, en, latin, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_compact_map_at(&owner, en, latin, 0, &map));
    assert(qjs_intl_compact_pattern_at(&owner, &map, map.count, &value) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!value.forms[0].tokens);
    assert(qjs_intl_compact_pattern_find(&owner, &map, 3, (QJSIntlCompactCount)-1, 0, &value) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_compact_pattern_find(&owner, &map, 3, 0, (QJSIntlCompactVariant)-1, &value) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_compact_pattern_at(NULL, &map, 0, &value) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_compact_pattern_at(&owner, NULL, 0, &value) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_compact_pattern_at(&owner, &map, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_compact_pattern_at(&owner, &map, 0, &value));
    assert(qjs_intl_compact_token_at(&owner, &value.forms[0], value.forms[0].count, &token) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!token.text.data && !token.text.length);
    assert(qjs_intl_compact_token_at(NULL, &value.forms[0], 0, &token) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_compact_token_at(&owner, NULL, 0, &token) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_compact_token_at(&owner, &value.forms[0], 0, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls);
    state.fail_at = state.calls + 1;
    assert(qjs_intl_compact_data_open(&numbering, &allocator, &retry) == QJS_INTL_NO_MEMORY);
    assert(!retry.section.bytes && !state.live);
    state.fail_at = 0;
    assert(!qjs_intl_compact_data_open(&numbering, &allocator, &retry));
    assert(qjs_intl_compact_data_open(NULL, &allocator, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(!retry.section.bytes);
    assert(qjs_intl_compact_data_open(&numbering, NULL, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_compact_data_open(&numbering, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    header = owner.section.bytes - data.pack.bytes;
    rows = owner.rows - data.pack.bytes;
    maps = owner.maps - data.pack.bytes;
    entries = owner.entries - data.pack.bytes;
    patterns = owner.patterns - data.pack.bytes;
    affixes = owner.affixes - data.pack.bytes;
    tokens = owner.tokens - data.pack.bytes;
    for (i = 0; i < 8; i++)
        corrupt(&allocator, header + i * 4, UINT32_MAX);
    corrupt(&allocator, header, 0);
    corrupt(&allocator, owner.systems - data.pack.bytes, numbering.systems.count);
    corrupt(&allocator, owner.locale_rows - data.pack.bytes, owner.row_count);
    corrupt(&allocator, rows, owner.map_count);
    corrupt(&allocator, maps, 1);
    corrupt(&allocator, maps + 4, owner.entry_count + 1);
    corrupt(&allocator, entries + 4, QJS_INTL_COMPACT_COUNT_COUNT);
    corrupt(&allocator, entries + 8, QJS_INTL_COMPACT_VARIANT_COUNT);
    corrupt(&allocator, entries + 12, UINT32_MAX);
    corrupt(&allocator, patterns, 3);
    corrupt(&allocator, patterns + 4, UINT32_MAX);
    corrupt(&allocator, patterns + 8, UINT32_MAX);
    corrupt(&allocator, patterns + 12, owner.affix_count);
    corrupt(&allocator, affixes, 1);
    corrupt(&allocator, affixes + 4, 0);
    corrupt(&allocator, tokens, QJS_INTL_NUMBER_TOKEN_COUNT);
    corrupt(&allocator, tokens + 4, data.strings.count);
    if (TEST_INTL_AUTHORED) {
        uint8_t *copy = malloc(qjs_intl_native_length);
        assert(copy && !qjs_intl_read_u32(owner.maps + 4));
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        put_u32(copy + rows, 0); /* Empty currency map in a decimal field. */
        invalid_blob(&allocator, copy);
        memcpy(copy + rows, owner.rows, 4);
        memcpy(copy + rows + (size_t)owner.system_count * 3 * 4, copy + rows,
               (size_t)owner.system_count * 3 * 4);
        invalid_blob(&allocator, copy); /* Duplicate canonical row. */
        free(copy);
    }
    assert(!state.live);
    free(unaligned);
    return 0;
}
