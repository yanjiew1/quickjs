/* Prepared global metazone facts and allocation-free half-open lookups.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/timezone-presentation-data.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    state->calls++;
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

static int same_period(const QJSIntlTimeZonePeriod *a, const QJSIntlTimeZonePeriod *b)
{
    return a->start == b->start && a->end == b->end && a->metazone == b->metazone &&
           a->flags == b->flags && a->standard_offset == b->standard_offset && a->daylight_offset == b->daylight_offset;
}

static uint32_t zone_id(const QJSIntlTimeZonePresentationData *data, const char *text)
{
    uint32_t result;
    assert(!qjs_intl_timezone_presentation_find_zone(data, bytes(text), &result));
    return result;
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlTimeZonePresentationData owner;
    QJSIntlTimeZonePeriod period, found;
    QJSIntlBytes text, short_id;
    uint32_t i, j, count, start, zone;
    size_t calls;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_timezone_presentation_data_open(&data, &allocator, &owner) && !state.live);
    assert(owner.zone_count == 419 && owner.period_count == 636 && owner.identity_count == 155);
    assert(owner.preferred_count == 283 && owner.primary_count == 11);
    calls = state.calls;
    for (i = 0; i < owner.zone_count; i++) {
        assert(!qjs_intl_timezone_presentation_zone(&owner, i, &text, &start, &count));
        assert(!qjs_intl_timezone_presentation_find_zone(&owner, text, &zone) && zone == i);
        for (j = 0; j < count; j++) {
            int64_t instant;
            assert(!qjs_intl_timezone_presentation_period(&owner, start + j, &period));
            instant = period.flags & QJS_INTL_TIMEZONE_PERIOD_START ? period.start : INT64_MIN;
            assert(!qjs_intl_timezone_presentation_at(&owner, i, instant, &found));
            assert(same_period(&period, &found));
            if (period.flags & QJS_INTL_TIMEZONE_PERIOD_END) {
                QJSIntlStatus status = qjs_intl_timezone_presentation_at(&owner, i, period.end, &found);
                if (j + 1 < count) {
                    QJSIntlTimeZonePeriod next;
                    assert(!qjs_intl_timezone_presentation_period(&owner, start + j + 1, &next));
                    if (next.start == period.end) {
                        assert(!status && same_period(&next, &found));
                        continue;
                    }
                }
                assert(status == QJS_INTL_UNSUPPORTED);
            }
        }
    }
    zone = zone_id(&owner, "Africa/Monrovia");
    assert(qjs_intl_timezone_presentation_at(&owner, zone, 63593069, &period) == QJS_INTL_UNSUPPORTED);
    assert(!qjs_intl_timezone_presentation_at(&owner, zone, 63593070, &period));
    assert(!qjs_intl_timezone_presentation_identity(&owner, period.metazone, &text, &short_id) && equal(text, "GMT"));
    zone = zone_id(&owner, "Africa/Casablanca");
    assert(qjs_intl_timezone_presentation_at(&owner, zone, 1560000000, &period) == QJS_INTL_UNSUPPORTED);
    zone = zone_id(&owner, "Europe/Dublin");
    assert(qjs_intl_timezone_presentation_at(&owner, zone, 0, &period) == QJS_INTL_UNSUPPORTED);
    assert(!qjs_intl_timezone_presentation_at(&owner, zone, 57722400, &period));
    assert(period.flags & QJS_INTL_TIMEZONE_PERIOD_STANDARD);
    assert(period.flags & QJS_INTL_TIMEZONE_PERIOD_DAYLIGHT);
    assert(!period.standard_offset && period.daylight_offset == 3600);
    zone = zone_id(&owner, "America/Los_Angeles");
    assert(!qjs_intl_timezone_presentation_at(&owner, zone, 0, &period));
    assert(!qjs_intl_timezone_presentation_preferred(&owner, period.metazone, bytes("001"), &text));
    assert(equal(text, "America/Los_Angeles"));
    assert(!qjs_intl_timezone_presentation_preferred(&owner, period.metazone, bytes("CA"), &text));
    assert(equal(text, "America/Vancouver"));
    assert(qjs_intl_timezone_presentation_preferred(&owner, period.metazone, bytes("GB"), &text) == QJS_INTL_UNSUPPORTED);
    assert(!qjs_intl_timezone_presentation_primary(&owner, bytes("CN"), &text) && equal(text, "Asia/Shanghai"));
    assert(state.calls == calls && !state.live);
    return 0;
}
