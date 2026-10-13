/* Pure time-zone regression coverage without JS allocation. */
#include <assert.h>
#include <string.h>
#include "../src/temporal/time-zone.h"

static void test_start_of_day(void)
{
    static const struct {
        const char *identifier;
        int64_t epoch_nanoseconds;
    } cases[] = {
        { "UTC", INT64_C(0) },
        { "+05:30", -INT64_C(19800000000000) },
        { "-03:30", INT64_C(12600000000000) },
    };
    QJSTemporalISODate date = {1970, 1, 1};
    QJSTemporalISODateTime local;
    QJSTemporalZone zone;
    QJSTemporalEpochNs epoch, expected;
    QJSTemporalEpochNs unchanged = qjs_temporal_epoch_ns_from_int64(7);
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        assert(!qjs_temporal_zone_parse(&zone, cases[i].identifier,
                                       strlen(cases[i].identifier)));
        epoch = unchanged;
        assert(!qjs_temporal_zone_start_of_day(&zone, date, &epoch));
        expected = qjs_temporal_epoch_ns_from_int64(cases[i].epoch_nanoseconds);
        assert(!qjs_temporal_epoch_ns_compare(epoch, expected));
        assert(!qjs_temporal_zone_datetime(&zone, epoch, &local));
        assert(local.date.year == 1970 && local.date.month == 1 &&
               local.date.day == 1);
        assert(!local.time.hour && !local.time.minute && !local.time.second &&
               !local.time.millisecond && !local.time.microsecond &&
               !local.time.nanosecond);
    }
    assert(!qjs_temporal_zone_parse(&zone, "UTC", 3));
    epoch = unchanged;
    date = (QJSTemporalISODate){2024, 2, 30};
    assert(qjs_temporal_zone_start_of_day(&zone, date, &epoch) ==
           QJS_TEMPORAL_ERROR_RANGE);
    assert(!qjs_temporal_epoch_ns_compare(epoch, unchanged));
    /* Sao Paulo skipped midnight; the date starts at the transition. */
    assert(!qjs_temporal_zone_parse(&zone, "America/Sao_Paulo", 17));
    date = (QJSTemporalISODate){2018, 11, 4};
    assert(!qjs_temporal_zone_start_of_day(&zone, date, &epoch));
    expected = qjs_temporal_epoch_ns_from_int64(INT64_C(1541300400000000000));
    assert(!qjs_temporal_epoch_ns_compare(epoch, expected));
    assert(!qjs_temporal_zone_datetime(&zone, epoch, &local));
    assert(local.date.year == 2018 && local.date.month == 11 &&
           local.date.day == 4 && local.time.hour == 1 &&
           !local.time.minute && !local.time.second);
}

int main(void)
{
    QJSTemporalZone zone, other;
    QJSTemporalEpochNs epoch, candidates[2];
    QJSTemporalISODateTime datetime = {{2024,2,29},{12,0,0,0,0,1}}, local;
    int count, equal, found;
    test_start_of_day();
    assert(!qjs_temporal_zone_parse(&zone, "+05:30", 6));
    assert(!strcmp(zone.identifier, "+05:30"));
    assert(!qjs_temporal_zone_epoch(&zone, datetime, QJS_TEMPORAL_COMPATIBLE, &epoch));
    assert(!qjs_temporal_zone_datetime(&zone, epoch, &local));
    assert(local.date.year == 2024 && local.time.hour == 12 && local.time.nanosecond == 1);
    assert(!qjs_temporal_zone_possible_epochs(&zone, datetime, candidates, &count));
    assert(count == 1 && !qjs_temporal_epoch_ns_compare(epoch, candidates[0]));
    assert(qjs_temporal_zone_parse(&zone, "+00:00:01", 9));
    assert(!qjs_temporal_zone_parse(&zone, "-00:00", 6));
    assert(!strcmp(zone.identifier, "+00:00"));
    assert(!qjs_temporal_zone_parse(&other, "UTC", 3));
    assert(!qjs_temporal_zones_equal(&zone, &other, &equal) && !equal);
    assert(!qjs_temporal_zone_transition(&other, epoch, 1, candidates, &found) && !found);
    assert(!qjs_temporal_zone_parse(&zone, "America/New_York", 16));
    datetime = (QJSTemporalISODateTime){{2024,11,3},{1,30,0,0,0,0}};
    assert(!qjs_temporal_zone_possible_epochs(&zone, datetime, candidates, &count));
    assert(count == 2);
    assert(qjs_temporal_epoch_ns_compare(candidates[0], candidates[1]) < 0);
    assert(!qjs_temporal_zone_parse(&other, "US/Eastern", 10));
    assert(!qjs_temporal_zones_equal(&zone, &other, &equal) && equal);
    return 0;
}
