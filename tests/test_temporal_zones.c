/* Pure time-zone regression coverage without JS allocation. */
#include <assert.h>
#include <string.h>
#include "../src/temporal/time-zone.h"

int main(void)
{
    QJSTemporalZone zone, other;
    QJSTemporalEpochNs epoch, candidates[2];
    QJSTemporalISODateTime datetime = {{2024,2,29},{12,0,0,0,0,1}}, local;
    int count, equal, found;
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
#ifdef CONFIG_ICU
    assert(!qjs_temporal_zone_parse(&zone, "America/New_York", 16));
    datetime = (QJSTemporalISODateTime){{2024,11,3},{1,30,0,0,0,0}};
    assert(!qjs_temporal_zone_possible_epochs(&zone, datetime, candidates, &count));
    assert(count == 2);
    assert(qjs_temporal_epoch_ns_compare(candidates[0], candidates[1]) < 0);
    assert(!qjs_temporal_zone_parse(&other, "US/Eastern", 10));
    assert(!qjs_temporal_zones_equal(&zone, &other, &equal) && equal);
#else
    assert(qjs_temporal_zone_parse(&zone, "America/New_York", 16));
    assert(!qjs_temporal_system_zone(&zone) && !strcmp(zone.identifier, "UTC"));
#endif
    return 0;
}
