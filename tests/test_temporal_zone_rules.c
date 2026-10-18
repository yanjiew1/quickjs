/* Common Temporal policy against deliberately close synthetic transitions.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include <assert.h>
#include <string.h>
#include "../src/temporal/time-zone.h"
#include "../src/timezone/timezone-backend.h"

struct QJSTimeZoneRules {
    int triple;
};
static const QJSTimeZoneRules gap_rules = {0}, triple_rules = {1};
static const QJSTimeZone gap_zone = {"Test/Gap", "Test/Gap", &gap_rules};
static const QJSTimeZone triple_zone = {"Test/Triple", "Test/Triple",
                                        &triple_rules};
static const QJSTimeZone utc_alias = {"Zulu/UTC", "UTC", NULL};
static int transition_mode;

size_t qjs_timezone_backend_count(void)
{
    return 3;
}

const QJSTimeZone *qjs_timezone_backend_at(size_t index)
{
    const QJSTimeZone *zones[] = {&gap_zone, &triple_zone, &utc_alias};
    return index < 3 ? zones[index] : NULL;
}

const char *qjs_timezone_backend_data_version(void)
{
    return "test";
}

static int folded_equal(const char *a, const char *b, size_t length)
{
    size_t i;

    if (strlen(a) != length)
        return 0;
    for (i = 0; i < length; i++) {
        unsigned char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z')
            x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z')
            y += 'a' - 'A';
        if (x != y)
            return 0;
    }
    return 1;
}

int qjs_timezone_backend_lookup(const QJSTimeZone **result,
                                const char *identifier, size_t length)
{
    size_t i;

    /* There is deliberately no UTC record in the test backend. */
    for (i = 0; i < qjs_timezone_backend_count(); i++) {
        const QJSTimeZone *zone = qjs_timezone_backend_at(i);
        if (folded_equal(zone->identifier, identifier, length)) {
            *result = zone;
            return 0;
        }
    }
    return QJS_TZ_RANGE;
}

int qjs_timezone_backend_offset(const QJSTimeZoneRules *zone, int64_t seconds,
                                int32_t *result)
{
    assert(zone == &gap_rules || zone == &triple_rules);
    if (zone->triple)
        *result = seconds < 0 ? 7200 : seconds < 1800 ? 3600 : 0;
    else
        *result = seconds < -3600 ? 1800 : seconds < 0 ? 0 : 7200;
    return 0;
}

int qjs_timezone_backend_info(const QJSTimeZoneRules *zone, int64_t seconds,
                              QJSTimeZoneInfo *result)
{
    qjs_timezone_backend_offset(zone, seconds, &result->offset_seconds);
    result->is_dst = 0;
    return 0;
}

int qjs_timezone_backend_candidates(const QJSTimeZoneRules *zone, int64_t local,
                                    QJSTimeZoneVisit *visit, void *opaque,
                                    size_t *count, int32_t *before,
                                    int32_t *after, int64_t *gap_end)
{
    const int offsets[] = {7200, 3600, 1800, 0};
    size_t n = 0;
    int i;

    /* Descending offsets produce ascending instants. Evaluate each against
       the rule active at that instant, independently of common policy. */
    for (i = 0; i < 4; i++) {
        int32_t actual;
        int64_t epoch = local - offsets[i];
        qjs_timezone_backend_offset(zone, epoch, &actual);
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

int qjs_timezone_backend_transition(const QJSTimeZoneRules *zone, int64_t epoch,
                                    int next, int inclusive, int64_t *when,
                                    int *found)
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
    assert(
        !qjs_temporal_epoch_ns_divide(&quotient, &fraction, epoch, 1000000000));
    assert(!qjs_temporal_epoch_ns_to_int64(&values->seconds[values->count],
                                           quotient));
    assert(fraction == 0);
    values->count++;
}

static void assert_seconds(QJSTemporalEpochNs epoch, int64_t seconds)
{
    QJSTemporalEpochNs expected;
    assert(!qjs_temporal_epoch_ns_multiply(
        &expected, qjs_temporal_epoch_ns_from_int64(seconds), 1000000000));
    assert(!qjs_temporal_epoch_ns_compare(epoch, expected));
}

int main(void)
{
    QJSTemporalZone zone, utc, alias, numeric;
    QJSTemporalISODateTime local = {{1970, 1, 1}, {0}};
    QJSTemporalEpochNs epoch, saved;
    Candidates candidates = {{0}, 0};
    size_t count;
    int found, equal;

    assert(!qjs_temporal_zone_parse(&utc, "uTc", 3));
    assert(!qjs_temporal_zone_parse(&alias, "zulu/utc", 8));
    assert(!strcmp(alias.identifier, "Zulu/UTC"));
    assert(!qjs_temporal_zones_equal(&utc, &alias, &equal) && equal);
    assert(!qjs_temporal_zone_parse(&numeric, "+00:00", 6));
    assert(!qjs_temporal_zones_equal(&alias, &numeric, &equal) && !equal);
    assert(qjs_timezone_backend_count() == 3 && qjs_timezone_count() == 4);
    assert(!strcmp(qjs_timezone_identifier(qjs_timezone_at(0)), "Test/Gap"));
    assert(!strcmp(qjs_timezone_identifier(qjs_timezone_at(1)), "Test/Triple"));
    assert(qjs_timezone_at(2) == utc.named);
    assert(qjs_timezone_at(3) == alias.named && !qjs_timezone_at(4));
    assert(!qjs_temporal_zone_epoch(&alias, local, QJS_TEMPORAL_COMPATIBLE,
                                    &epoch));
    assert_seconds(epoch, 0);
    saved = qjs_temporal_epoch_ns_from_int64(123);
    epoch = saved;
    assert(!qjs_temporal_zone_transition(&alias, saved, 1, &epoch, &found));
    assert(!found && !qjs_temporal_epoch_ns_compare(epoch, saved));

    assert(!qjs_temporal_zone_parse(&zone, "Test/Gap", 8));
    assert(
        !qjs_temporal_zone_epoch(&zone, local, QJS_TEMPORAL_EARLIER, &epoch));
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
    assert(!qjs_temporal_zone_possible_epochs(&zone, local, collect,
                                              &candidates, &count));
    assert(count == 3 && candidates.count == 3);
    assert(candidates.seconds[0] == -3200 && candidates.seconds[1] == 400 &&
           candidates.seconds[2] == 4000);
    assert(!qjs_temporal_zone_epoch(&zone, local, QJS_TEMPORAL_LATER, &epoch));
    assert_seconds(epoch, 4000);
    assert(!qjs_temporal_zone_epoch(&zone, local, QJS_TEMPORAL_COMPATIBLE,
                                    &epoch));
    assert_seconds(epoch, -3200);
    assert(qjs_temporal_zone_epoch(&zone, local,
                                   QJS_TEMPORAL_DISAMBIGUATION_REJECT, &epoch));

    saved = epoch;
    assert(!qjs_temporal_zone_transition(&zone,
                                         qjs_temporal_epoch_ns_from_int64(-1),
                                         0, &epoch, &found) &&
           !found);
    assert(!qjs_temporal_zone_transition(&zone,
                                         qjs_temporal_epoch_ns_from_int64(-1),
                                         1, &epoch, &found) &&
           !found);
    transition_mode = 1;
    assert(!qjs_temporal_zone_transition(
               &zone, qjs_temporal_epoch_ns_from_int64(0), 0, &epoch, &found) &&
           !found);
    assert(!qjs_temporal_zone_transition(
               &zone, qjs_temporal_epoch_ns_from_int64(0), 1, &epoch, &found) &&
           !found);
    assert(!qjs_temporal_epoch_ns_compare(epoch, saved));
    return 0;
}
