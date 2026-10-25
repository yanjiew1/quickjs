/* Numeric symbol owner: immutable views, optional fields, corruption and OOM.
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
#include "../src/intl/number/symbol-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

static void *allocate(void *opaque, size_t length)
{
    Allocation *state = opaque;
    void *ptr;
    assert(length);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(length);
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

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) && !memcmp(text.data, expected, text.length);
}

static void put_u32(uint8_t *ptr, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        ptr[i] = value >> (i * 8);
}

static uint32_t locale_id(const QJSIntlData *data, const char *expected)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes tag;
        assert(!qjs_intl_data_locale(data, i, &tag, &parent));
        if (equal(tag, expected))
            return i;
    }
    abort();
}

static uint32_t string_id(const QJSIntlData *data, const char *expected)
{
    uint32_t i;
    for (i = 0; i < data->strings.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_string(data, i, &text));
        if (equal(text, expected))
            return i;
    }
    abort();
}

static void invalid_blob(const QJSIntlAllocator *allocator, const uint8_t *copy,
                         size_t length)
{
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlNumberSymbolData symbols;
    assert(!qjs_intl_data_open(copy, length, allocator, &data));
    assert(!qjs_intl_numbering_data_open(&data, allocator, &numbering));
    memset(&symbols, 0xff, sizeof(symbols));
    assert(qjs_intl_number_symbol_data_open(&numbering, allocator, &symbols) ==
           QJS_INTL_DATA_ERROR);
    assert(!symbols.numbering.data.pack.bytes && !symbols.section.bytes &&
           !symbols.systems && !symbols.locale_rows && !symbols.rows && !symbols.vectors);
}

static void corrupt(const QJSIntlAllocator *allocator, size_t offset, uint32_t value)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    invalid_blob(allocator, copy, qjs_intl_native_length);
    free(copy);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlNumberSymbolData symbols, retry;
    QJSIntlNumberSymbols values;
    uint32_t latin, mathbold, roman, en, root, i;
    size_t calls, section_at, rows_at, vectors_at, locale_rows_at;
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_numbering_data_open(&data, &allocator, &numbering));
    state.calls = 0;
    assert(!qjs_intl_number_symbol_data_open(&numbering, &allocator, &symbols));
    assert(state.calls == 1 && !state.live);
    assert(symbols.system_count == (TEST_INTL_AUTHORED ? 2 : 78));
    assert(!qjs_intl_numbering_system_find(&numbering, bytes("latn"), &latin));
    assert(!qjs_intl_numbering_system_find(&numbering, bytes("mathbold"), &mathbold));
    assert(!qjs_intl_numbering_system_find(&numbering, bytes("roman"), &roman));
    en = locale_id(&data, "en");
    root = locale_id(&data, "root");
    calls = state.calls;
    assert(!qjs_intl_number_symbols_at(&symbols, en, latin, &values));
    assert(equal(values.fields[QJS_INTL_SYMBOL_DECIMAL].text, "."));
    if (TEST_INTL_AUTHORED) {
        assert(equal(values.fields[QJS_INTL_SYMBOL_GROUP].text, "\xc2\xa0"));
        assert(equal(values.fields[QJS_INTL_SYMBOL_MINUS].text, "\xd8\x9c-"));
        assert(values.fields[QJS_INTL_SYMBOL_PLUS].state == QJS_INTL_SYMBOL_VALUE &&
               !values.fields[QJS_INTL_SYMBOL_PLUS].text.length);
        assert(values.fields[QJS_INTL_SYMBOL_CURRENCY_DECIMAL].state == QJS_INTL_SYMBOL_VALUE &&
               !values.fields[QJS_INTL_SYMBOL_CURRENCY_DECIMAL].text.length);
        assert(values.fields[QJS_INTL_SYMBOL_CURRENCY_GROUP].state == QJS_INTL_SYMBOL_BLOCKED &&
               !values.fields[QJS_INTL_SYMBOL_CURRENCY_GROUP].text.data);
    } else {
        assert(equal(values.fields[QJS_INTL_SYMBOL_GROUP].text, ","));
    }
    assert(!qjs_intl_number_symbols_at(&symbols, en, mathbold, &values));
    assert(equal(values.fields[QJS_INTL_SYMBOL_DECIMAL].text, "."));
    assert(!qjs_intl_number_symbols_at(&symbols, root, latin, &values));
    assert(values.fields[QJS_INTL_SYMBOL_CURRENCY_GROUP].state == QJS_INTL_SYMBOL_ABSENT &&
           !values.fields[QJS_INTL_SYMBOL_CURRENCY_GROUP].text.data);
    if (TEST_INTL_PROFILE) {
        assert(!qjs_intl_number_symbols_at(&symbols, locale_id(&data, "fr"), latin, &values));
        assert(equal(values.fields[QJS_INTL_SYMBOL_GROUP].text, "\xe2\x80\xaf"));
    }
    for (i = 0; i < numbering.systems.count; i++) {
        QJSIntlNumberingSystem definition;
        QJSIntlStatus status;
        assert(!qjs_intl_numbering_system_at(&numbering, i, &definition));
        status = qjs_intl_number_symbols_at(&symbols, en, i, &values);
        assert(status == (definition.kind == QJS_INTL_NUMBERING_NUMERIC ? QJS_INTL_OK
                                                                       : QJS_INTL_UNSUPPORTED));
    }
    memset(&values, 0xff, sizeof(values));
    assert(qjs_intl_number_symbols_at(&symbols, en, roman, &values) == QJS_INTL_UNSUPPORTED);
    assert(!values.fields[0].text.data && !values.fields[0].state);
    assert(qjs_intl_number_symbols_at(&symbols, data.locales.count, latin, &values) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_symbols_at(&symbols, en, numbering.systems.count, &values) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_symbols_at(NULL, en, latin, &values) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_symbols_at(&symbols, en, latin, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls);
    state.fail_at = state.calls + 1;
    assert(qjs_intl_number_symbol_data_open(&numbering, &allocator, &retry) == QJS_INTL_NO_MEMORY);
    assert(!retry.section.bytes && !retry.numbering.data.pack.bytes && !state.live);
    state.fail_at = 0;
    assert(!qjs_intl_number_symbol_data_open(&numbering, &allocator, &retry));
    assert(!state.live);
    assert(qjs_intl_number_symbol_data_open(NULL, &allocator, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(!retry.section.bytes);
    assert(qjs_intl_number_symbol_data_open(&numbering, NULL, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_symbol_data_open(&numbering, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);

    section_at = symbols.section.bytes - data.pack.bytes;
    rows_at = symbols.rows - data.pack.bytes;
    vectors_at = symbols.vectors - data.pack.bytes;
    locale_rows_at = symbols.locale_rows - data.pack.bytes;
    corrupt(&allocator, section_at, 0);
    corrupt(&allocator, section_at, UINT32_MAX);
    corrupt(&allocator, section_at + 4, 0);
    corrupt(&allocator, section_at + 4, UINT32_MAX);
    corrupt(&allocator, section_at + 8, 0);
    corrupt(&allocator, section_at + 8, symbols.section.count + 1);
    corrupt(&allocator, section_at + 12, roman);
    corrupt(&allocator, locale_rows_at, symbols.row_count);
    corrupt(&allocator, rows_at, symbols.vector_count);
    corrupt(&allocator, vectors_at, data.strings.count);
    corrupt(&allocator, vectors_at, QJS_INTL_NUMBERING_ABSENT);
    corrupt(&allocator, vectors_at, QJS_INTL_NUMBERING_BLOCKED);
    if (TEST_INTL_AUTHORED) {
        uint8_t *copy = malloc(qjs_intl_native_length);
        size_t start, width;
        assert(copy);
        corrupt(&allocator, vectors_at, string_id(&data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91"));
        corrupt(&allocator, vectors_at, string_id(&data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"));
        for (i = 0; i < 2; i++) {
            start = i ? vectors_at : rows_at;
            width = (i ? QJS_INTL_SYMBOL_COUNT : symbols.system_count) * 4;
            memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
            memcpy(copy + start + width, copy + start, width);
            invalid_blob(&allocator, copy, qjs_intl_native_length);
        }
        /* Keep sorted rows valid, but make one row unreachable. */
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        for (i = 0; i < symbols.section.count; i++)
            put_u32(copy + locale_rows_at + (size_t)i * 4, 0);
        invalid_blob(&allocator, copy, qjs_intl_native_length);
        free(copy);
    }
    for (i = 0; i < data.pack.section_count; i++) {
        size_t directory = 64 + (size_t)i * 16;
        if (qjs_intl_read_u32(qjs_intl_native_bytes + directory) == QJS_INTL_SECTION_NUMBER_SYMBOLS) {
            uint8_t *copy = malloc(qjs_intl_native_length);
            assert(copy);
            /* The isolated authored fixture ends at symbols. Renaming that
               descriptor tests a missing section without colliding with the
               plural section present in the real prepared profiles. */
            if (TEST_INTL_AUTHORED) {
                assert(i + 1 == data.pack.section_count);
                corrupt(&allocator, directory, QJS_INTL_SECTION_NUMBER_SYMBOLS + 1);
            }
            corrupt(&allocator, directory + 12, 0);
            corrupt(&allocator, directory + 12, symbols.section.count + 1);
            /* Delete one symbol word while preserving later sections and
               the envelope, so the symbol owner diagnoses the truncation. */
            {
                size_t cut = section_at + symbols.section.length - 4;
                uint32_t j;
                memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
                memmove(copy + cut, copy + cut + 4, qjs_intl_native_length - cut - 4);
                put_u32(copy + 8, qjs_intl_native_length - 4);
                put_u32(copy + directory + 8, symbols.section.length - 4);
                for (j = i + 1; j < data.pack.section_count; j++) {
                    size_t next = 64 + (size_t)j * 16 + 4;
                    put_u32(copy + next, qjs_intl_read_u32(copy + next) - 4);
                }
                invalid_blob(&allocator, copy, qjs_intl_native_length - 4);
            }
            free(copy);
        }
    }
    free(unaligned);
    assert(!state.live);
    return 0;
}
