/* Temporal minimum timezone capability and exact boundary conversion.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include <assert.h>
#include <string.h>
#include "../src/temporal/time-zone.h"
#include "../src/timezone/timezone-backend.h"

static void store_epoch(void *opaque, QJSTemporalEpochNs epoch)
{
    *(QJSTemporalEpochNs *)opaque = epoch;
}

int main(void)
{
    QJSTemporalZone utc, offset, saved;
    QJSTemporalISODateTime datetime;
    QJSTemporalEpochNs zero = { 0, 0 }, result, limit;
    int64_t ns;
    size_t count;
    int found, equal;
    QJSTimeZoneInfo info;

    assert(!qjs_temporal_zone_parse(&utc, "uTc", 3));
    assert(!strcmp(utc.identifier, "UTC"));
    assert(qjs_timezone_backend_count() == 0);
    assert(qjs_timezone_backend_at(0) == NULL);
    assert(qjs_timezone_count() == 1);
    assert(qjs_timezone_at(0) == utc.named);
    assert(qjs_timezone_at(1) == NULL);
    assert(!strcmp(qjs_timezone_data_version(), "minimal"));
    assert(qjs_timezone_is_primary(utc.named));
    assert(!qjs_timezone_info(utc.named, 0, &info));
    assert(!info.offset_seconds && !info.is_dst);
    saved = utc;
    assert(qjs_temporal_zone_parse(&utc, "Europe/Paris", 12));
    assert(!memcmp(&saved, &utc, sizeof(utc)));
    assert(qjs_temporal_zone_parse(&utc, "UTC\0", 4));
    assert(!memcmp(&saved, &utc, sizeof(utc)));
    assert(!qjs_temporal_zone_parse(&offset, "-00:00", 6));
    assert(!strcmp(offset.identifier, "+00:00"));
    assert(!qjs_temporal_zones_equal(&utc, &offset, &equal) && !equal);
    assert(!qjs_temporal_zone_parse(&offset, "+0530", 5));
    assert(!strcmp(offset.identifier, "+05:30"));
    assert(!qjs_temporal_zone_offset(&offset, zero, &ns));
    assert(ns == INT64_C(19800000000000));
    assert(!qjs_temporal_zone_datetime(&offset, zero, &datetime));
    assert(datetime.date.year == 1970 && datetime.date.month == 1 &&
           datetime.date.day == 1 && datetime.time.hour == 5 &&
           datetime.time.minute == 30);
    assert(!qjs_temporal_zone_epoch(&offset, datetime,
        QJS_TEMPORAL_DISAMBIGUATION_REJECT, &result));
    assert(!qjs_temporal_epoch_ns_compare(result, zero));
    assert(!qjs_temporal_zone_possible_epochs(&offset, datetime,
        store_epoch, &result, &count));
    assert(count == 1 && !qjs_temporal_epoch_ns_compare(result, zero));
    result = qjs_temporal_epoch_ns_from_int64(123);
    assert(!qjs_temporal_zone_transition(&utc, zero, 1, &result, &found));
    assert(!found && result.low == 123 && !result.high);
    assert(qjs_temporal_zone_parse(&offset, "+24:00", 6));
    assert(qjs_temporal_zone_parse(&offset, "+01:00:00", 9));

    /* Offset subtraction, rather than the local date, decides Instant's
       range. Its minimum can produce a civil date on the preceding day. */
    assert(!qjs_temporal_epoch_ns_from_milliseconds(&limit, -8640000000000000.0));
    assert(!qjs_temporal_zone_parse(&offset, "-01:00", 6));
    assert(!qjs_temporal_zone_datetime(&offset, limit, &datetime));
    assert(datetime.date.year == -271821 && datetime.date.month == 4 &&
           datetime.date.day == 19 && datetime.time.hour == 23);
    assert(!qjs_temporal_zone_epoch(&offset, datetime,
        QJS_TEMPORAL_COMPATIBLE, &result));
    assert(!qjs_temporal_epoch_ns_compare(result, limit));
    assert(!qjs_temporal_zone_datetime(&utc,
        qjs_temporal_epoch_ns_from_int64(-1), &datetime));
    assert(datetime.date.year == 1969 && datetime.date.month == 12 &&
           datetime.date.day == 31 && datetime.time.nanosecond == 999);
    return 0;
}
