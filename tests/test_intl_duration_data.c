/* Immutable digital-duration facts, exact queries, corruption and allocation failure.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/duration/data.h"
#include "../src/intl/duration/native-digital.h"
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

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) &&
           (!text.length || !memcmp(text.data, expected, text.length));
}

static uint32_t locale_id(const QJSIntlData *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_locale(data, i, &text, &parent));
        if (equal(text, name))
            return i;
    }
    abort();
}

#if TEST_INTL_AUTHORED
static uint32_t string_id(const QJSIntlData *data, const char *name)
{
    uint32_t i;
    for (i = 0; i < data->strings.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_string(data, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}

static void put_u32(uint8_t *at, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        at[i] = value >> (i * 8);
}

static void corrupt(const QJSIntlDurationData *owner, const QJSIntlAllocator *allocator,
                    const uint8_t *at, uint32_t value)
{
    QJSIntlData data;
    QJSIntlDurationData result, zero = {0};
    uint8_t *copy = malloc(qjs_intl_native_length);
    size_t offset = at - owner->data.pack.bytes;
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_duration_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
    free(copy);
}

static void check_corruption(const QJSIntlDurationData *owner, const QJSIntlAllocator *allocator)
{
    const uint8_t *value = NULL, *missing = NULL, *blocked = NULL;
    size_t i;
    for (i = 0; i < 3; i++)
        corrupt(owner, allocator, owner->section.bytes + i * 4, UINT32_MAX);
    corrupt(owner, allocator, owner->section.bytes, 0);
    corrupt(owner, allocator, owner->section.bytes + 4, 0);
    corrupt(owner, allocator, owner->section.bytes + 8, 0);
    corrupt(owner, allocator, owner->locale_rows, owner->row_count);
    corrupt(owner, allocator, owner->rows, owner->record_count);
    corrupt(owner, allocator, owner->rows + 12, qjs_intl_read_u32(owner->rows));
    for (i = 0; i < owner->record_count; i++) {
        const uint8_t *record = owner->records + i * 16;
        uint32_t state = qjs_intl_read_u32(record);
        if (state == QJS_INTL_DURATION_PATTERN_VALUE)
            value = record;
        else if (state == QJS_INTL_DURATION_PATTERN_MISSING)
            missing = record;
        else
            blocked = record;
    }
    assert(value && missing && blocked);
    corrupt(owner, allocator, value, 3);
    corrupt(owner, allocator, value + 4, owner->data.strings.count);
    corrupt(owner, allocator, value + 8, owner->data.strings.count);
    corrupt(owner, allocator, value + 12, 3);
    corrupt(owner, allocator, value + 4, string_id(&owner->data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91"));
    corrupt(owner, allocator, value + 4, string_id(&owner->data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"));
    corrupt(owner, allocator, blocked + 4, string_id(&owner->data, "h:mm"));
    corrupt(owner, allocator, missing + 12, 1);
    for (i = 0; i < 5; i++) {
        static const char *const bad[] = {"", "En", "en-us", "bad_tag", "en-1901-1901"};
        corrupt(owner, allocator, value + 8, string_id(&owner->data, bad[i]));
    }
}
#endif

static void check_queries(const QJSIntlDurationData *owner)
{
    QJSIntlDurationPattern pattern, zero = {0};
    uint32_t locale, key;
    for (locale = 0; locale < owner->section.count; locale++) {
        for (key = 0; key < QJS_INTL_DURATION_SKELETON_COUNT; key++) {
            assert(!qjs_intl_duration_pattern_at(owner, locale, key, &pattern));
            assert(pattern.state <= QJS_INTL_DURATION_PATTERN_BLOCKED);
            if (pattern.state == QJS_INTL_DURATION_PATTERN_MISSING)
                assert(!memcmp(&pattern, &zero, sizeof(pattern)));
            else {
                assert(pattern.origin_locale.length);
                assert(pattern.origin_skeleton < QJS_INTL_DURATION_SKELETON_COUNT);
                if (pattern.state == QJS_INTL_DURATION_PATTERN_BLOCKED)
                    assert(!pattern.text.length);
            }
        }
    }
    assert(!qjs_intl_duration_pattern_at(owner, locale_id(&owner->data, "root"),
                                        QJS_INTL_DURATION_HM, &pattern));
    assert(pattern.state == QJS_INTL_DURATION_PATTERN_VALUE && equal(pattern.text, "h:mm"));
#if TEST_INTL_AUTHORED
    locale = locale_id(&owner->data, "de");
    assert(!qjs_intl_duration_pattern_at(owner, locale, QJS_INTL_DURATION_HM, &pattern));
    assert(pattern.state == QJS_INTL_DURATION_PATTERN_BLOCKED && equal(pattern.origin_locale, "en"));
    assert(!qjs_intl_duration_pattern_at(owner, locale, QJS_INTL_DURATION_HMS, &pattern));
    assert(pattern.state == QJS_INTL_DURATION_PATTERN_VALUE && equal(pattern.text, "'ga' hh:mm:ss"));
    assert(equal(pattern.origin_locale, "en-1901-1996") && pattern.origin_skeleton == QJS_INTL_DURATION_HMS);
    assert(!qjs_intl_duration_pattern_at(owner, locale, QJS_INTL_DURATION_MS, &pattern));
    assert(pattern.state == QJS_INTL_DURATION_PATTERN_VALUE && !pattern.text.length);
    assert(!qjs_intl_duration_pattern_at(owner, locale_id(&owner->data, "root"),
                                        QJS_INTL_DURATION_MS, &pattern));
    assert(pattern.state == QJS_INTL_DURATION_PATTERN_MISSING);
#endif
    memset(&pattern, 0xff, sizeof(pattern));
    assert(qjs_intl_duration_pattern_at(owner, UINT32_MAX, QJS_INTL_DURATION_HM, &pattern) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&pattern, &zero, sizeof(pattern)));
    assert(qjs_intl_duration_pattern_at(owner, 0, -1, &pattern) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&pattern, &zero, sizeof(pattern)));
    assert(qjs_intl_duration_pattern_at(NULL, 0, 0, &pattern) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_duration_pattern_at(owner, 0, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

static void check_projection(const QJSIntlDurationData *owner, const QJSIntlAllocator *allocator)
{
    uint32_t locale;
    for (locale = 0; locale < owner->section.count; locale++) {
        QJSIntlDurationPattern pattern;
        QJSIntlNativeDurationDigital digital;
        assert(!qjs_intl_duration_pattern_at(owner, locale, QJS_INTL_DURATION_HMS, &pattern));
        assert(pattern.state == QJS_INTL_DURATION_PATTERN_VALUE);
        assert(!qjs_intl_duration_native_digital(allocator, pattern.text, &digital));
        assert(digital.hour_width == 1 || digital.hour_width == 2);
        qjs_intl_duration_native_digital_clear(allocator, &digital);
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data, unaligned_data;
    QJSIntlDurationData owner, result, zero = {0};
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    size_t calls, step;
    assert(unaligned && !qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                                           &allocator, &data) && !state.live);
    state.calls = 0;
    assert(!qjs_intl_duration_data_open(&data, &allocator, &owner) && !state.live);
    calls = state.calls;
    assert(calls);
    check_queries(&owner);
    assert(state.calls == calls); /* Every query is allocation-free. */
    check_projection(&owner, &allocator);
#if TEST_INTL_AUTHORED
    assert(calls >= 2); /* Validation scratch and multi-variant origin scratch. */
    check_corruption(&owner, &allocator);
#endif
    for (step = 1; step <= calls; step++) {
        state.calls = 0;
        state.fail_at = step;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_duration_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
        state.fail_at = 0;
        assert(!qjs_intl_duration_data_open(&data, &allocator, &result) && !state.live);
    }
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &unaligned_data));
    assert(!qjs_intl_duration_data_open(&unaligned_data, &allocator, &result));
    check_queries(&result);
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_duration_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_duration_data_open(&data, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_duration_data_open(&data, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    free(unaligned);
    assert(!state.live);
    return 0;
}
