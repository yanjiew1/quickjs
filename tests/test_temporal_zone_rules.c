/* Common Temporal policy against deliberately close synthetic transitions.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include <assert.h>
#include <string.h>
#include "../src/temporal/time-zone.h"

struct QJSTimeZone { int triple; };
static const QJSTimeZone gap_zone = { 0 }, triple_zone = { 1 };
static int transition_mode;

int qjs_timezone_lookup(const QJSTimeZone **result,
                        const char *identifier, size_t length)
{
    if (length == 8 && !memcmp(identifier, "Test/Gap", 8))
        *result = &gap_zone;
    else if (length == 11 && !memcmp(identifier, "Test/Triple", 11))
        *result = &triple_zone;
    else
        return QJS_TZ_RANGE;
    return 0;
}

const char *qjs_timezone_identifier(const QJSTimeZone *zone)
{
    return zone->triple ? "Test/Triple" : "Test/Gap";
}

const char *qjs_timezone_primary_identifier(const QJSTimeZone *zone)
{
    return qjs_timezone_identifier(zone);
}

int qjs_timezone_offset(const QJSTimeZone *zone, int64_t seconds, int32_t *result)
{
    if (zone->triple)
        *result = seconds < 0 ? 7200 : seconds < 1800 ? 3600 : 0;
    else
        *result = seconds < -3600 ? 1800 : seconds < 0 ? 0 : 7200;
    return 0;
}

int qjs_timezone_candidates(const QJSTimeZone *zone, int64_t local,
                            QJSTimeZoneVisit *visit, void *opaque,
                            size_t *count, int32_t *before,
                            int32_t *after, int64_t *gap_end)
{
    const int offsets[] = { 7200, 3600, 1800, 0 };
    size_t n = 0;
    int i;

    /* Descending offsets produce ascending instants. Evaluate each against
       the rule active at that instant, independently of common policy. */
    for (i = 0; i < 4; i++) {
        int32_t actual;
        int64_t epoch = local - offsets[i];
        qjs_timezone_offset(zone, epoch, &actual);
        if (actual == offsets[i]) {
            visit(opaque, epoch);
            n++;
        }
    }
    if (!n) {
        assert(!zone->triple && local >= 0 && local < 7200);
        *before = 0;
        *after = 7200;
        *gap_end = 0;
    }
    *count = n;
    return 0;
}

int qjs_timezone_transition(const QJSTimeZone *zone, int64_t epoch,
                            int next, int inclusive, int64_t *when, int *found)
{
    if (transition_mode) {
        *when = next ? INT64_MAX : INT64_MIN;
        *found = 1;
    } else {
        assert(epoch == -1);
        assert(inclusive == !next);
        *found = 0;
    }
    return 0;
}

typedef struct Candidates {
    int64_t seconds[3];
    size_t count;
} Candidates;

static void collect(void *opaque, QJSTemporalEpochNs epoch)
{
    Candidates *values = opaque;
    QJSTemporalEpochNs quotient;
    uint64_t fraction;

    assert(values->count < 3);
    assert(!qjs_temporal_epoch_ns_divide(&quotient, &fraction, epoch, 1000000000));
    assert(!qjs_temporal_epoch_ns_to_int64(&values->seconds[values->count], quotient));
    assert(fraction == 0);
    values->count++;
}

static void assert_seconds(QJSTemporalEpochNs epoch, int64_t seconds)
{
    QJSTemporalEpochNs expected;
    assert(!qjs_temporal_epoch_ns_multiply(&expected,
        qjs_temporal_epoch_ns_from_int64(seconds), 1000000000));
    assert(!qjs_temporal_epoch_ns_compare(epoch, expected));
}

int main(void)
{
    QJSTemporalZone zone;
    QJSTemporalISODateTime local = { { 1970, 1, 1 }, { 0 } };
    QJSTemporalEpochNs epoch, saved;
    Candidates candidates = { { 0 }, 0 };
    size_t count;
    int found;

    assert(!qjs_temporal_zone_parse(&zone, "Test/Gap", 8));
    assert(!qjs_temporal_zone_epoch(&zone, local, QJS_TEMPORAL_EARLIER, &epoch));
    assert_seconds(epoch, -9000);
    assert(!qjs_temporal_zone_start_of_day(&zone, local.date, &epoch));
    assert_seconds(epoch, 0);
    assert(!qjs_temporal_zone_epoch(&zone, local, QJS_TEMPORAL_LATER, &epoch));
    assert_seconds(epoch, 0);
    assert(qjs_temporal_zone_epoch(&zone, local,
        QJS_TEMPORAL_DISAMBIGUATION_REJECT, &epoch));

    assert(!qjs_temporal_zone_parse(&zone, "Test/Triple", 11));
    local.time.hour = 1;
    local.time.minute = 6;
    local.time.second = 40;
    assert(!qjs_temporal_zone_possible_epochs(&zone, local,
        collect, &candidates, &count));
    assert(count == 3 && candidates.count == 3);
    assert(candidates.seconds[0] == -3200 && candidates.seconds[1] == 400 &&
           candidates.seconds[2] == 4000);
    assert(!qjs_temporal_zone_epoch(&zone, local, QJS_TEMPORAL_LATER, &epoch));
    assert_seconds(epoch, 4000);
    assert(!qjs_temporal_zone_epoch(&zone, local, QJS_TEMPORAL_COMPATIBLE, &epoch));
    assert_seconds(epoch, -3200);
    assert(qjs_temporal_zone_epoch(&zone, local,
        QJS_TEMPORAL_DISAMBIGUATION_REJECT, &epoch));

    saved = epoch;
    assert(!qjs_temporal_zone_transition(&zone,
        qjs_temporal_epoch_ns_from_int64(-1), 0, &epoch, &found) && !found);
    assert(!qjs_temporal_zone_transition(&zone,
        qjs_temporal_epoch_ns_from_int64(-1), 1, &epoch, &found) && !found);
    transition_mode = 1;
    assert(!qjs_temporal_zone_transition(&zone,
        qjs_temporal_epoch_ns_from_int64(0), 0, &epoch, &found) && !found);
    assert(!qjs_temporal_zone_transition(&zone,
        qjs_temporal_epoch_ns_from_int64(0), 1, &epoch, &found) && !found);
    assert(!qjs_temporal_epoch_ns_compare(epoch, saved));
    return 0;
}
