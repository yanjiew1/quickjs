/* Native currency views: sparse fields, fraction facts, corruption and OOM.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/currency/data.h"
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

static QJSIntlBytes text(const char *value)
{
    return (QJSIntlBytes){value, strlen(value)};
}

static int equal(QJSIntlBytes value, const char *expected)
{
    return value.length == strlen(expected) && !memcmp(value.data, expected, value.length);
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

static void check_field(const QJSIntlCurrencyData *data, const char *locale, const char *code,
                        QJSIntlCurrencyField field, unsigned int state, const char *expected)
{
    QJSIntlCurrencyText result;
    uint32_t index;
    assert(!qjs_intl_currency_code_find(data, text(code), &index));
    assert(
        !qjs_intl_currency_field_at(data, locale_id(&data->data, locale), index, field, &result));
    assert(result.state == state);
    if (state == QJS_INTL_CURRENCY_VALUE)
        assert(equal(result.text, expected));
    else
        assert(!result.text.data && !result.text.length);
}

static void check_fractions(const QJSIntlCurrencyData *data, const char *code, uint32_t digits,
                            uint32_t cash_digits, uint32_t cash_rounding)
{
    QJSIntlCurrencyFractions result;
    assert(!qjs_intl_currency_fractions(data, text(code), &result));
    assert(result.digits == digits && result.cash_digits == cash_digits);
    assert(!result.rounding && result.cash_rounding == cash_rounding);
}

static void put_u32(uint8_t *bytes, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        bytes[i] = value >> (8 * i);
}

static void invalid_blob(const QJSIntlAllocator *allocator, const uint8_t *bytes)
{
    QJSIntlData data;
    QJSIntlCurrencyData result, zero = {0};
    assert(!qjs_intl_data_open(bytes, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_currency_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
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

static void check_corruption(const QJSIntlCurrencyData *data, const QJSIntlAllocator *allocator)
{
    size_t header = data->section.bytes - data->data.pack.bytes;
    size_t i, last = 0;
    uint8_t *copy;
    for (i = 0; i < 5; i++)
        corrupt(allocator, header + i * 4, i == 0 ? 0 : UINT32_MAX);
    corrupt(allocator, header + 5 * 4, 101);
    corrupt(allocator, header + 7 * 4, 101);
    corrupt(allocator, data->codes - data->data.pack.bytes, data->data.strings.count);
    corrupt(allocator, data->codes - data->data.pack.bytes + 4, qjs_intl_read_u32(data->codes));
    corrupt(allocator, data->fractions - data->data.pack.bytes, data->code_count);
    corrupt(allocator, data->fractions - data->data.pack.bytes + 4, 101);
    corrupt(allocator, data->fractions - data->data.pack.bytes + 12, 101);
    corrupt(allocator, data->locale_maps - data->data.pack.bytes, data->map_count);
    corrupt(allocator, data->maps - data->data.pack.bytes, data->vector_count);
    /* Canonical order and complete reference coverage are required. */
    copy = malloc(qjs_intl_native_length);
    assert(copy && data->map_count > 1 && data->vector_count > 1);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    memcpy(copy + (data->maps - data->data.pack.bytes) + (size_t)data->code_count * 4, data->maps,
           (size_t)data->code_count * 4);
    invalid_blob(allocator, copy);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    memcpy(copy + (data->vectors - data->data.pack.bytes) + QJS_INTL_CURRENCY_FIELD_COUNT * 4,
           data->vectors, QJS_INTL_CURRENCY_FIELD_COUNT * 4);
    invalid_blob(allocator, copy);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    memset(copy + (data->vectors - data->data.pack.bytes) +
               (size_t)(data->vector_count - 1) * QJS_INTL_CURRENCY_FIELD_COUNT * 4,
           0xff, QJS_INTL_CURRENCY_FIELD_COUNT * 4);
    invalid_blob(allocator, copy);
    free(copy);
    for (i = 0; i < (size_t)data->vector_count * QJS_INTL_CURRENCY_FIELD_COUNT; i++) {
        uint32_t ref = qjs_intl_read_u32(data->vectors + i * 4);
        if (ref < data->data.strings.count && ref)
            last = i;
    }
    assert(last);
    last = data->vectors - data->data.pack.bytes + last * 4;
    corrupt(allocator, last, 0);
    corrupt(allocator, last, data->data.strings.count);
    if (TEST_INTL_AUTHORED) {
        for (i = 0; i < data->data.strings.count; i++) {
            QJSIntlBytes value;
            assert(!qjs_intl_data_string(&data->data, i, &value));
            if (equal(value, "↑↑↑") || equal(value, "∅∅∅"))
                corrupt(allocator, last, i);
        }
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlCurrencyData owner, retry;
    QJSIntlCurrencyText field;
    QJSIntlCurrencyFractions fractions;
    QJSIntlBytes code;
    uint32_t i, index, en;
    size_t calls;
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_currency_data_open(&data, &allocator, &owner));
    assert(!state.live);
    calls = state.calls;
    for (i = 0; i < owner.code_count; i++) {
        assert(!qjs_intl_currency_code_at(&owner, i, &code));
        assert(!qjs_intl_currency_code_find(&owner, code, &index) && index == i);
    }
    check_fractions(&owner, "USD", 2, 2, 0);
    check_fractions(&owner, "JPY", 0, 0, 0);
    check_fractions(&owner, "CHF", 2, 2, 5);
    check_fractions(&owner, "ZZZ", 2, 2, 0);
    en = locale_id(&data, "en");
    if (TEST_INTL_AUTHORED) {
        check_field(&owner, "en", "USD", QJS_INTL_CURRENCY_NAME_ONE, QJS_INTL_CURRENCY_VALUE,
                    "dollar");
        check_field(&owner, "en", "USD", QJS_INTL_CURRENCY_NAME_OTHER, QJS_INTL_CURRENCY_BLOCKED,
                    NULL);
        check_field(&owner, "en", "USD", QJS_INTL_CURRENCY_SYMBOL, QJS_INTL_CURRENCY_EMPTY, NULL);
        check_field(&owner, "en", "USD", QJS_INTL_CURRENCY_NAME_EXACT_ONE, QJS_INTL_CURRENCY_ABSENT,
                    NULL);
        check_field(&owner, "en", "CHF", QJS_INTL_CURRENCY_NAME, QJS_INTL_CURRENCY_ABSENT, NULL);
        check_field(&owner, "root", "USD", QJS_INTL_CURRENCY_SYMBOL, QJS_INTL_CURRENCY_VALUE, "$");
        check_field(&owner, "en", "USD", QJS_INTL_CURRENCY_DECIMAL, QJS_INTL_CURRENCY_VALUE, ";");
        check_field(&owner, "en", "USD", QJS_INTL_CURRENCY_GROUP, QJS_INTL_CURRENCY_VALUE, " ");
        check_field(&owner, "en", "USD", QJS_INTL_CURRENCY_PATTERN, QJS_INTL_CURRENCY_VALUE,
                    "¤0.00");
    } else {
        assert(owner.code_count == 307);
        check_fractions(&owner, "BHD", 3, 3, 0);
        check_field(&owner, "en", "USD", QJS_INTL_CURRENCY_NAME, QJS_INTL_CURRENCY_VALUE,
                    "US Dollar");
        check_field(&owner, "en", "USD", QJS_INTL_CURRENCY_NAME_OTHER, QJS_INTL_CURRENCY_VALUE,
                    "US dollars");
        check_field(&owner, "en", "USD", QJS_INTL_CURRENCY_SYMBOL, QJS_INTL_CURRENCY_VALUE, "$");
        if (TEST_INTL_PROFILE)
            check_field(&owner, "fr", "USD", QJS_INTL_CURRENCY_SYMBOL, QJS_INTL_CURRENCY_VALUE,
                        "$US");
    }
    assert(!qjs_intl_currency_code_find(&owner, text("USD"), &index));
    assert(qjs_intl_currency_code_at(&owner, owner.code_count, &code) == QJS_INTL_INVALID_ARGUMENT);
    assert(!code.data && !code.length);
    assert(qjs_intl_currency_code_find(&owner, text("ZZZ"), &i) == QJS_INTL_UNSUPPORTED && !i);
    assert(qjs_intl_currency_code_find(&owner, text("usd"), &i) == QJS_INTL_INVALID_ARGUMENT && !i);
    assert(qjs_intl_currency_code_find(&owner, (QJSIntlBytes){NULL, 3}, &i) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_code_find(NULL, text("USD"), &i) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_code_find(&owner, text("USD"), NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_code_at(NULL, 0, &code) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_code_at(&owner, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_field_at(&owner, en, index, QJS_INTL_CURRENCY_FIELD_COUNT, &field) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!field.text.data && !field.state);
    assert(qjs_intl_currency_field_at(&owner, en, index, (QJSIntlCurrencyField)-1, &field) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_field_at(&owner, data.locales.count, index, 0, &field) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_field_at(&owner, en, owner.code_count, 0, &field) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_field_at(NULL, en, 0, 0, &field) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_field_at(&owner, en, 0, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_fractions(&owner, text("US"), &fractions) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!fractions.digits && !fractions.cash_digits);
    assert(qjs_intl_currency_fractions(NULL, text("USD"), &fractions) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_fractions(&owner, text("USD"), NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls);
    state.fail_at = state.calls + 1;
    assert(qjs_intl_currency_data_open(&data, &allocator, &retry) == QJS_INTL_NO_MEMORY);
    assert(!retry.section.bytes && !state.live);
    state.fail_at = 0;
    assert(!qjs_intl_currency_data_open(&data, &allocator, &retry));
    assert(qjs_intl_currency_data_open(NULL, &allocator, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(!retry.section.bytes);
    assert(qjs_intl_currency_data_open(&data, NULL, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_data_open(&data, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    /* Corruption offsets use the original immutable pack. */
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_currency_data_open(&data, &allocator, &owner));
    check_corruption(&owner, &allocator);
    if (TEST_INTL_AUTHORED) {
#if TEST_INTL_AUTHORED
        extern const uint8_t qjs_intl_currency_empty_bytes[];
        extern const size_t qjs_intl_currency_empty_length;
        assert(!qjs_intl_data_open(qjs_intl_currency_empty_bytes, qjs_intl_currency_empty_length,
                                   &allocator, &data));
        assert(!qjs_intl_currency_data_open(&data, &allocator, &owner) && !owner.vector_count);
        check_field(&owner, "en", "USD", QJS_INTL_CURRENCY_SYMBOL, QJS_INTL_CURRENCY_ABSENT, NULL);
        check_fractions(&owner, "JPY", 0, 0, 0);
#endif
    }
    assert(!state.live);
    free(unaligned);
    return 0;
}
