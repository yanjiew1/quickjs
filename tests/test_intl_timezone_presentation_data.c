/* Immutable presentation facts, half-open membership and hostile records.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/timezone-presentation-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t timezone_presentation_empty_bytes[];
extern const size_t timezone_presentation_empty_length;

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

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static uint32_t string_id(const QJSIntlData *data, const char *value)
{
    uint32_t i;
    QJSIntlBytes text;
    for (i = 0; i < data->strings.count; i++) {
        assert(!qjs_intl_data_string(data, i, &text));
        if (equal(text, value))
            return i;
    }
    abort();
}

static uint32_t zone_id(const QJSIntlTimeZonePresentationData *data, const char *value)
{
    uint32_t result;
    assert(!qjs_intl_timezone_presentation_find_zone(data, bytes(value), &result));
    return result;
}

static void check_queries(const QJSIntlTimeZonePresentationData *data)
{
    QJSIntlTimeZonePeriod period, zero = {0};
    QJSIntlBytes text, short_id;
    uint32_t i, index, start, count, alpha = zone_id(data, "Area/Alpha");
    assert(data->zone_count == 4 && data->period_count == 6 && data->identity_count == 3);
    for (i = 0; i < data->zone_count; i++) {
        assert(!qjs_intl_timezone_presentation_zone(data, i, &text, &start, &count));
        assert(!qjs_intl_timezone_presentation_find_zone(data, text, &index) && index == i);
        assert(count <= data->period_count - start);
    }
    for (i = 0; i < data->period_count; i++) {
        assert(!qjs_intl_timezone_presentation_period(data, i, &period));
        assert(!qjs_intl_timezone_presentation_identity(data, period.metazone, &text, &short_id));
        assert(equal(text, "West") || equal(text, "East"));
    }
    assert(!qjs_intl_timezone_presentation_identity(data, 1, &text, &short_id));
    assert(equal(text, "Unused") && equal(short_id, "unuu"));
    assert(!qjs_intl_timezone_presentation_at(data, alpha, INT64_MIN, &period));
    assert(period.flags == (QJS_INTL_TIMEZONE_PERIOD_END | QJS_INTL_TIMEZONE_PERIOD_STANDARD));
    assert(!period.end && !period.standard_offset);
    assert(!qjs_intl_timezone_presentation_at(data, alpha, -1, &period) && period.metazone == 2);
    assert(!qjs_intl_timezone_presentation_at(data, alpha, 0, &period));
    assert(period.flags == (QJS_INTL_TIMEZONE_PERIOD_START | QJS_INTL_TIMEZONE_PERIOD_END |
                            QJS_INTL_TIMEZONE_PERIOD_DAYLIGHT));
    assert(!period.start && period.end == 10 && !period.daylight_offset);
    assert(qjs_intl_timezone_presentation_at(data, alpha, 10, &period) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&period, &zero, sizeof(period)));
    assert(qjs_intl_timezone_presentation_at(data, alpha, 19, &period) == QJS_INTL_UNSUPPORTED);
    assert(!qjs_intl_timezone_presentation_at(data, alpha, 20, &period));
    assert(period.metazone == 0 && period.standard_offset == -86340 && period.daylight_offset == 86340);
    assert(!qjs_intl_timezone_presentation_at(data, alpha, INT64_MAX, &period));
    index = zone_id(data, "Area/Extremes");
    assert(!qjs_intl_timezone_presentation_at(data, index, INT64_MIN, &period) && period.start == INT64_MIN);
    assert(qjs_intl_timezone_presentation_at(data, index, -1, &period) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_timezone_presentation_at(data, index, 0, &period) == QJS_INTL_UNSUPPORTED);
    assert(!qjs_intl_timezone_presentation_at(data, index, 1, &period) && period.end == INT64_MAX);
    assert(qjs_intl_timezone_presentation_at(data, index, INT64_MAX, &period) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_timezone_presentation_at(data, zone_id(data, "Area/Empty"), 0, &period) == QJS_INTL_UNSUPPORTED);
    assert(!qjs_intl_timezone_presentation_preferred(data, 2, bytes("001"), &text) && equal(text, "Area/Beta"));
    assert(!qjs_intl_timezone_presentation_preferred(data, 2, bytes("GB"), &text) && equal(text, "Area/Alpha"));
    assert(qjs_intl_timezone_presentation_preferred(data, 2, bytes("US"), &text) == QJS_INTL_UNSUPPORTED);
    assert(!text.data && !text.length); /* No implicit 001 fallback. */
    assert(!qjs_intl_timezone_presentation_primary(data, bytes("GB"), &text) && equal(text, "Area/NoPeriods"));
    assert(!qjs_intl_timezone_presentation_primary(data, bytes("US"), &text) && equal(text, "Area/NoPeriods"));
    assert(qjs_intl_timezone_presentation_primary(data, bytes("CA"), &text) == QJS_INTL_UNSUPPORTED && !text.data);
    assert(qjs_intl_timezone_presentation_find_zone(data, bytes("Area/NoPeriods"), &index) == QJS_INTL_UNSUPPORTED && !index);
    assert(qjs_intl_timezone_presentation_find_zone(data, (QJSIntlBytes){NULL, 1}, &index) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_timezone_presentation_identity(data, data->identity_count, &text, &short_id) == QJS_INTL_INVALID_ARGUMENT);
    assert(!text.data && !short_id.data);
    assert(qjs_intl_timezone_presentation_zone(data, data->zone_count, &text, &start, &count) == QJS_INTL_INVALID_ARGUMENT);
    assert(!text.data && !start && !count);
    assert(qjs_intl_timezone_presentation_period(data, data->period_count, &period) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&period, &zero, sizeof(period)));
    assert(qjs_intl_timezone_presentation_at(data, data->zone_count, 0, &period) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_timezone_presentation_preferred(data, data->identity_count, bytes("GB"), &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_timezone_presentation_primary(data, (QJSIntlBytes){NULL, 1}, &text) == QJS_INTL_INVALID_ARGUMENT);
}

static void put_u32(uint8_t *at, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        at[i] = value >> (i * 8);
}

static void corrupt(const QJSIntlTimeZonePresentationData *owner, const QJSIntlAllocator *allocator,
                    const uint8_t *at, uint32_t value)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    QJSIntlData data;
    QJSIntlTimeZonePresentationData result, zero = {0};
    assert(copy);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + (at - owner->data.pack.bytes), value);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_timezone_presentation_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
    free(copy);
}

static void check_corruption(const QJSIntlTimeZonePresentationData *owner, const QJSIntlAllocator *allocator)
{
    size_t i;
    uint32_t bad = string_id(&owner->data, "Bad ID");
    const uint8_t *p = owner->periods;
    for (i = 0; i < 5; i++)
        corrupt(owner, allocator, owner->section.bytes + i * 4, i ? UINT32_MAX : 0);
    corrupt(owner, allocator, owner->identities, owner->data.strings.count);
    corrupt(owner, allocator, owner->identities, bad);
    corrupt(owner, allocator, owner->identities + 4, string_id(&owner->data, "BAD!"));
    corrupt(owner, allocator, owner->identities + 8, qjs_intl_read_u32(owner->identities));
    corrupt(owner, allocator, owner->identities + 12, qjs_intl_read_u32(owner->identities + 4));
    corrupt(owner, allocator, owner->zones, bad);
    corrupt(owner, allocator, owner->zones + 4, 1);
    corrupt(owner, allocator, owner->zones + 8, UINT32_MAX);
    corrupt(owner, allocator, owner->zones + 12, qjs_intl_read_u32(owner->zones));
    corrupt(owner, allocator, owner->zones + 20, owner->period_count);
    corrupt(owner, allocator, p + 16, owner->identity_count);
    corrupt(owner, allocator, p + 20, 16);
    corrupt(owner, allocator, p, 1); /* Absent start carries payload. */
    corrupt(owner, allocator, p + 28, 60); /* Absent daylight offset. */
    corrupt(owner, allocator, p + 24, 1); /* Offset is not whole minutes. */
    corrupt(owner, allocator, p + 24, 86400);
    corrupt(owner, allocator, p + 24, (uint32_t)-86400);
    corrupt(owner, allocator, p + 32 + 8, 0); /* Empty [0,0). */
    corrupt(owner, allocator, p + 32 + 20, QJS_INTL_TIMEZONE_PERIOD_END); /* Second unbounded start. */
    corrupt(owner, allocator, p + 20, QJS_INTL_TIMEZONE_PERIOD_STANDARD); /* Unbounded previous end. */
    corrupt(owner, allocator, p + 64, 9); /* Overlap with [0,10). */
    corrupt(owner, allocator, p + 64 + 8, 1); /* Absent end carries payload. */
    corrupt(owner, allocator, owner->preferred, owner->identity_count);
    corrupt(owner, allocator, owner->preferred + 4, bad);
    corrupt(owner, allocator, owner->preferred + 8, zone_id(owner, "Area/Empty"));
    corrupt(owner, allocator, owner->preferred + 12, 0); /* Duplicate metazone/territory. */
    corrupt(owner, allocator, owner->preferred + 12 + 8, zone_id(owner, "Area/Empty"));
    corrupt(owner, allocator, owner->primary, bad);
    corrupt(owner, allocator, owner->primary + 4, bad);
    corrupt(owner, allocator, owner->primary + 8, qjs_intl_read_u32(owner->primary));
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlTimeZonePresentationData owner, result, zero = {0};
    size_t i, successful, calls;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_timezone_presentation_data_open(&data, &allocator, &owner) && !state.live);
    successful = state.calls;
    assert(successful == 2);
    calls = state.calls;
    check_queries(&owner);
    assert(state.calls == calls && !state.live);
    for (i = 1; i <= successful; i++) {
        state.calls = 0;
        state.fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_timezone_presentation_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
        state.calls = state.fail_at = 0;
        assert(!qjs_intl_timezone_presentation_data_open(&data, &allocator, &result) && !state.live);
    }
    check_corruption(&owner, &allocator);
    assert(!state.live);
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_timezone_presentation_data_open(&data, &allocator, &result));
    check_queries(&result);
    free(unaligned);
    assert(!qjs_intl_data_open(timezone_presentation_empty_bytes, timezone_presentation_empty_length, &allocator, &data));
    assert(!qjs_intl_timezone_presentation_data_open(&data, &allocator, &result));
    assert(!result.zone_count && !result.period_count && !result.identity_count && !result.preferred_count && !result.primary_count);
    {
        uint32_t index = 123;
        assert(qjs_intl_timezone_presentation_find_zone(&result, bytes("Area/Alpha"), &index) == QJS_INTL_UNSUPPORTED && !index);
    }
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_timezone_presentation_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
    assert(qjs_intl_timezone_presentation_data_open(&data, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    return 0;
}
