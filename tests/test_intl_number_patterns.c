/* Compiled numeric patterns: exact typed affixes, metadata, corruption and OOM.
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
#include "../src/intl/number/pattern-data.h"
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

static int equal(QJSIntlBytes bytes, const char *expected)
{
    return bytes.length == strlen(expected) && !memcmp(bytes.data, expected, bytes.length);
}

static uint32_t locale_id(const QJSIntlData *data, const char *expected)
{
    uint32_t i, parent;
    QJSIntlBytes tag;
    for (i = 0; i < data->locales.count; i++) {
        assert(!qjs_intl_data_locale(data, i, &tag, &parent));
        if (equal(tag, expected))
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

static void invalid_blob(const QJSIntlAllocator *allocator, const uint8_t *bytes, size_t length)
{
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlNumberPatternData patterns;
    assert(!qjs_intl_data_open(bytes, length, allocator, &data));
    assert(!qjs_intl_numbering_data_open(&data, allocator, &numbering));
    memset(&patterns, 0xff, sizeof(patterns));
    assert(qjs_intl_number_pattern_data_open(&numbering, allocator, &patterns) ==
           QJS_INTL_DATA_ERROR);
    assert(!patterns.section.bytes && !patterns.numbering.data.pack.bytes && !patterns.systems &&
           !patterns.rows && !patterns.vectors && !patterns.patterns && !patterns.affixes &&
           !patterns.tokens);
}

static void corrupt(const QJSIntlAllocator *allocator, size_t at, uint32_t value)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    assert(copy && at <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + at, value);
    invalid_blob(allocator, copy, qjs_intl_native_length);
    free(copy);
}

static void check_token(const QJSIntlNumberPatternData *data, const QJSIntlNumberAffix *affix,
                        uint32_t at, QJSIntlNumberTokenKind kind, const char *literal)
{
    QJSIntlNumberPatternToken token;
    assert(!qjs_intl_number_pattern_token_at(data, affix, at, &token));
    assert(token.kind == kind);
    if (literal)
        assert(equal(token.text, literal));
    else
        assert(!token.text.data && !token.text.length);
}

static void check_patterns(const QJSIntlNumberPatternData *data, uint32_t en, uint32_t root,
                           uint32_t latin, uint32_t mathbold)
{
    QJSIntlNumberPattern pattern, other;
    unsigned int i;
    uint32_t at;
    assert(!qjs_intl_number_pattern_at(data, en, latin, QJS_INTL_PATTERN_DECIMAL, &pattern));
    assert(pattern.state == QJS_INTL_NUMBER_PATTERN_VALUE);
    assert(pattern.minimum_grouping_digits == (TEST_INTL_AUTHORED ? 2u : 1u));
    assert(pattern.primary_grouping == 3 &&
           pattern.secondary_grouping == (TEST_INTL_AUTHORED ? 2u : 3u));
    assert(pattern.minimum_integer_digits == 1);
    assert(pattern.minimum_fraction_digits == (TEST_INTL_AUTHORED ? 2u : 0u));
    assert(pattern.maximum_fraction_digits == (TEST_INTL_AUTHORED ? 4u : 3u));
    assert(!qjs_intl_number_pattern_at(data, en, mathbold, QJS_INTL_PATTERN_DECIMAL, &other));
    assert(!memcmp(&pattern, &other, sizeof(pattern)));
    if (TEST_INTL_AUTHORED) {
        assert(pattern.forms[QJS_INTL_NUMBER_PATTERN_POSITIVE].count == 2);
        check_token(data, &pattern.forms[QJS_INTL_NUMBER_PATTERN_POSITIVE], 0,
                    QJS_INTL_NUMBER_TOKEN_LITERAL, "#{}");
        check_token(data, &pattern.forms[QJS_INTL_NUMBER_PATTERN_POSITIVE], 1,
                    QJS_INTL_NUMBER_TOKEN_NUMBER, NULL);
        assert(
            !qjs_intl_number_pattern_at(data, root, latin, QJS_INTL_PATTERN_SCIENTIFIC, &pattern));
        assert(pattern.exponent_digits == 1 && !pattern.exponent_plus);
        check_token(data, &pattern.forms[QJS_INTL_NUMBER_PATTERN_POSITIVE], 0,
                    QJS_INTL_NUMBER_TOKEN_LITERAL, "[");
        check_token(data, &pattern.forms[QJS_INTL_NUMBER_PATTERN_POSITIVE], 2,
                    QJS_INTL_NUMBER_TOKEN_LITERAL, "]");
        assert(!qjs_intl_number_pattern_at(data, en, latin, QJS_INTL_PATTERN_SCIENTIFIC, &pattern));
        assert(pattern.exponent_digits == 3 && pattern.exponent_plus);
        assert(!qjs_intl_number_pattern_at(data, en, latin, QJS_INTL_PATTERN_CURRENCY_STANDARD,
                                           &pattern));
        assert(pattern.forms[QJS_INTL_NUMBER_PATTERN_NEGATIVE].count == 4);
        check_token(data, &pattern.forms[QJS_INTL_NUMBER_PATTERN_NEGATIVE], 0,
                    QJS_INTL_NUMBER_TOKEN_CURRENCY, NULL);
        check_token(data, &pattern.forms[QJS_INTL_NUMBER_PATTERN_NEGATIVE], 1,
                    QJS_INTL_NUMBER_TOKEN_LITERAL, "#\xc2\xa0");
        check_token(data, &pattern.forms[QJS_INTL_NUMBER_PATTERN_NEGATIVE], 3,
                    QJS_INTL_NUMBER_TOKEN_MINUS, NULL);
        check_token(data, &pattern.forms[QJS_INTL_NUMBER_PATTERN_PLUS], 3,
                    QJS_INTL_NUMBER_TOKEN_PLUS, NULL);
        assert(!qjs_intl_number_pattern_at(data, en, latin, QJS_INTL_PATTERN_CURRENCY_ACCOUNTING,
                                           &pattern));
        assert(pattern.forms[QJS_INTL_NUMBER_PATTERN_PLUS].tokens ==
               pattern.forms[QJS_INTL_NUMBER_PATTERN_POSITIVE].tokens);
        for (i = QJS_INTL_PATTERN_CURRENCY_STANDARD_ALPHA; i < QJS_INTL_NUMBER_PATTERN_FIELD_COUNT;
             i++) {
            assert(!qjs_intl_number_pattern_at(data, en, latin, i, &pattern));
            assert(pattern.state == (i < QJS_INTL_PATTERN_CURRENCY_STANDARD_NONE
                                         ? QJS_INTL_NUMBER_PATTERN_BLOCKED
                                         : QJS_INTL_NUMBER_PATTERN_EMPTY));
            assert(pattern.minimum_grouping_digits == 2 && !pattern.forms[0].tokens);
            assert(!qjs_intl_number_pattern_at(data, root, latin, i, &pattern));
            assert(pattern.state == QJS_INTL_NUMBER_PATTERN_ABSENT && !pattern.forms[0].tokens);
        }
    }
    if (TEST_INTL_PROFILE) {
        uint32_t hi = locale_id(&data->numbering.data, "hi");
        assert(!qjs_intl_number_pattern_at(data, hi, latin, QJS_INTL_PATTERN_DECIMAL, &pattern));
        assert(pattern.primary_grouping == 3 && pattern.secondary_grouping == 2);
        assert(!qjs_intl_number_pattern_at(data, hi, latin, QJS_INTL_PATTERN_SCIENTIFIC, &pattern));
        check_token(data, &pattern.forms[0], 0, QJS_INTL_NUMBER_TOKEN_LITERAL, "[");
        check_token(data, &pattern.forms[0], 2, QJS_INTL_NUMBER_TOKEN_LITERAL, "]");
    }
    if (TEST_INTL_AUTHORED) {
        assert(data->override_count == 3);
        assert(!qjs_intl_number_pattern_at(data, root, latin, QJS_INTL_PATTERN_DECIMAL, &other));
        assert(!qjs_intl_number_pattern_override(data, (QJSIntlBytes){"#,##0.###", 9}, &pattern));
        assert(pattern.forms[0].tokens == other.forms[0].tokens);
        assert(!qjs_intl_number_pattern_override(
            data, (QJSIntlBytes){"0.00 ¤¤", sizeof("0.00 ¤¤") - 1}, &pattern));
        assert(pattern.minimum_fraction_digits == 2 && pattern.maximum_fraction_digits == 2);
        assert(!pattern.minimum_grouping_digits && !pattern.primary_grouping);
        check_token(data, &pattern.forms[0], 2, QJS_INTL_NUMBER_TOKEN_CURRENCY_ISO, NULL);
        assert(!qjs_intl_number_pattern_override(
            data,
            (QJSIntlBytes){"¤'legacy'0.0;¤'legacy'0.0-", sizeof("¤'legacy'0.0;¤'legacy'0.0-") - 1},
            &pattern));
        check_token(data, &pattern.forms[0], 1, QJS_INTL_NUMBER_TOKEN_LITERAL, "legacy");
        check_token(data, &pattern.forms[1], 3, QJS_INTL_NUMBER_TOKEN_MINUS, NULL);
    }
    if (TEST_INTL_PROFILE) {
        assert(data->override_count);
        assert(!qjs_intl_number_pattern_override(
            data, (QJSIntlBytes){"#,##0.00 ¤", sizeof("#,##0.00 ¤") - 1}, &pattern));
        assert(pattern.primary_grouping == 3);
    }
    assert(qjs_intl_number_pattern_override(data, (QJSIntlBytes){"not a source pattern", 20},
                                            &pattern) == QJS_INTL_UNSUPPORTED);
    assert(!pattern.forms[0].tokens && !pattern.minimum_grouping_digits);
    assert(qjs_intl_number_pattern_override(data, (QJSIntlBytes){0}, &pattern) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_pattern_override(NULL, (QJSIntlBytes){"0", 1}, &pattern) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_pattern_override(data, (QJSIntlBytes){"0", 1}, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    /* Every field is available as either a typed pattern or a distinct marker,
       and every number pattern has one numeric placeholder per sign form. */
    for (i = 0; i < QJS_INTL_NUMBER_PATTERN_FIELD_COUNT; i++) {
        assert(!qjs_intl_number_pattern_at(data, en, latin, i, &pattern));
        if (pattern.state == QJS_INTL_NUMBER_PATTERN_VALUE) {
            unsigned int form;
            for (form = 0; form < QJS_INTL_NUMBER_PATTERN_FORM_COUNT; form++) {
                unsigned int numbers = 0;
                for (at = 0; at < pattern.forms[form].count; at++) {
                    QJSIntlNumberPatternToken token;
                    assert(
                        !qjs_intl_number_pattern_token_at(data, &pattern.forms[form], at, &token));
                    numbers += token.kind == QJS_INTL_NUMBER_TOKEN_NUMBER;
                }
                assert(numbers == 1);
            }
        }
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlNumberPatternData owner, retry;
    QJSIntlNumberPattern pattern;
    QJSIntlNumberPatternToken token;
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    uint32_t en, root, latin, mathbold, roman, i;
    size_t calls, header, maps, rows, vectors, patterns, affixes, tokens;
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_numbering_data_open(&data, &allocator, &numbering));
    state.calls = 0;
    assert(!qjs_intl_number_pattern_data_open(&numbering, &allocator, &owner));
    assert(state.calls == 1 && !state.live);
    assert(!qjs_intl_numbering_system_find(&numbering, (QJSIntlBytes){"latn", 4}, &latin));
    assert(!qjs_intl_numbering_system_find(&numbering, (QJSIntlBytes){"mathbold", 8}, &mathbold));
    assert(!qjs_intl_numbering_system_find(&numbering, (QJSIntlBytes){"roman", 5}, &roman));
    en = locale_id(&data, "en");
    root = locale_id(&data, "root");
    calls = state.calls;
    check_patterns(&owner, en, root, latin, mathbold);
    assert(qjs_intl_number_pattern_at(&owner, en, roman, QJS_INTL_PATTERN_DECIMAL, &pattern) ==
           QJS_INTL_UNSUPPORTED);
    assert(!pattern.minimum_grouping_digits && !pattern.forms[0].tokens);
    assert(qjs_intl_number_pattern_at(&owner, data.locales.count, latin, QJS_INTL_PATTERN_DECIMAL,
                                      &pattern) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_pattern_at(&owner, en, numbering.systems.count, QJS_INTL_PATTERN_DECIMAL,
                                      &pattern) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_pattern_at(&owner, en, latin, (QJSIntlNumberPatternField)-1, &pattern) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_pattern_at(NULL, en, latin, QJS_INTL_PATTERN_DECIMAL, &pattern) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_pattern_at(&owner, en, latin, QJS_INTL_PATTERN_DECIMAL, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_number_pattern_at(&owner, en, latin, QJS_INTL_PATTERN_DECIMAL, &pattern));
    memset(&token, 0xff, sizeof(token));
    assert(qjs_intl_number_pattern_token_at(&owner, &pattern.forms[0], pattern.forms[0].count,
                                            &token) == QJS_INTL_INVALID_ARGUMENT);
    assert(!token.text.data && !token.text.length);
    assert(qjs_intl_number_pattern_token_at(NULL, &pattern.forms[0], 0, &token) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_pattern_token_at(&owner, NULL, 0, &token) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_pattern_token_at(&owner, &pattern.forms[0], 0, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls);
    state.fail_at = state.calls + 1;
    assert(qjs_intl_number_pattern_data_open(&numbering, &allocator, &retry) == QJS_INTL_NO_MEMORY);
    assert(!retry.section.bytes && !retry.numbering.data.pack.bytes && !state.live);
    state.fail_at = 0;
    assert(!qjs_intl_number_pattern_data_open(&numbering, &allocator, &retry));
    assert(!state.live);
    assert(qjs_intl_number_pattern_data_open(NULL, &allocator, &retry) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!retry.section.bytes);
    assert(qjs_intl_number_pattern_data_open(&numbering, NULL, &retry) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_pattern_data_open(&numbering, &allocator, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    header = owner.section.bytes - data.pack.bytes;
    maps = owner.locale_rows - data.pack.bytes;
    rows = owner.rows - data.pack.bytes;
    vectors = owner.vectors - data.pack.bytes;
    patterns = owner.patterns - data.pack.bytes;
    affixes = owner.affixes - data.pack.bytes;
    tokens = owner.tokens - data.pack.bytes;
    for (i = 0; i < 8; i++)
        corrupt(&allocator, header + i * 4, UINT32_MAX);
    if (owner.override_count) {
        size_t overrides = owner.overrides - data.pack.bytes;
        corrupt(&allocator, overrides, 0);
        corrupt(&allocator, overrides, data.strings.count);
        corrupt(&allocator, overrides + 4, owner.pattern_count);
        if (owner.override_count > 1)
            corrupt(&allocator, overrides + 8, qjs_intl_read_u32(owner.overrides));
    }
    corrupt(&allocator, header, 0);
    corrupt(&allocator, maps, 0);
    corrupt(&allocator, maps + 4, owner.row_count);
    corrupt(&allocator, rows, owner.vector_count);
    corrupt(&allocator, vectors, UINT32_MAX);
    corrupt(&allocator, vectors + 5 * 4, owner.pattern_count);
    corrupt(&allocator, patterns + 3 * 4, 0);
    corrupt(&allocator, patterns + 4 * 4, UINT32_MAX);
    corrupt(&allocator, patterns + 6 * 4, 2);
    corrupt(&allocator, patterns + 8 * 4, 2);
    corrupt(&allocator, patterns + 9 * 4, owner.affix_count);
    corrupt(&allocator, affixes, 1);
    corrupt(&allocator, affixes + 4, 0);
    corrupt(&allocator, tokens, QJS_INTL_NUMBER_TOKEN_COUNT);
    corrupt(&allocator, tokens + 4, data.strings.count);
    for (i = 0; i < owner.token_count; i++) {
        if (qjs_intl_read_u32(owner.tokens + (size_t)i * 8) == QJS_INTL_NUMBER_TOKEN_NUMBER) {
            corrupt(&allocator, tokens + (size_t)i * 8, QJS_INTL_NUMBER_TOKEN_MINUS);
            corrupt(&allocator, tokens + (size_t)i * 8 + 4, 1);
            break;
        }
    }
    if (TEST_INTL_AUTHORED) {
        uint8_t *copy = malloc(qjs_intl_native_length);
        assert(copy && owner.row_count > 1);
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        for (i = 0; i < owner.section.count; i++)
            put_u32(copy + maps + (size_t)i * 8 + 4, 0);
        invalid_blob(&allocator, copy, qjs_intl_native_length);
        for (i = 0; i < data.pack.section_count; i++) {
            size_t directory = 64 + (size_t)i * 16;
            if (qjs_intl_read_u32(qjs_intl_native_bytes + directory) ==
                QJS_INTL_SECTION_NUMBER_PATTERNS) {
                assert(i + 1 == data.pack.section_count);
                corrupt(&allocator, directory, QJS_INTL_SECTION_NUMBER_PATTERNS + 1);
                corrupt(&allocator, directory + 12, 0);
                memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
                put_u32(copy + directory + 8, owner.section.length - 4);
                put_u32(copy + 8, qjs_intl_native_length - 4);
                invalid_blob(&allocator, copy, qjs_intl_native_length - 4);
            }
        }
        free(copy);
    }
    assert(!state.live);
    free(unaligned);
    return 0;
}
