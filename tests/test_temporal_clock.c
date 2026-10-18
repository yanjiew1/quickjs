/* Host clock normalization without relying on the machine's date setting.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include <assert.h>
#include "../src/temporal/clock.h"

int main(void)
{
    QJSTemporalEpochNs epoch, saved, expected;
    int64_t milliseconds;

    assert(!qjs_temporal_epoch_from_clock(&epoch, -1, 999999999));
    assert(!qjs_temporal_epoch_ns_compare(epoch,
        qjs_temporal_epoch_ns_from_int64(-1)));
    assert(!qjs_temporal_epoch_ns_to_milliseconds(&milliseconds, epoch));
    assert(milliseconds == -1);
    saved = epoch;
    assert(qjs_temporal_epoch_from_clock(&epoch, 0, -1));
    assert(!qjs_temporal_epoch_ns_compare(epoch, saved));
    assert(qjs_temporal_epoch_from_clock(&epoch, 0, 1000000000));
    assert(!qjs_temporal_epoch_ns_compare(epoch, saved));
    assert(!qjs_temporal_epoch_from_clock(&epoch, INT64_MAX, 999999999));
    assert(!qjs_temporal_epoch_ns_from_milliseconds(&expected, 8640000000000000.0));
    assert(!qjs_temporal_epoch_ns_compare(epoch, expected));
    assert(!qjs_temporal_epoch_from_clock(&epoch, INT64_MIN, 0));
    assert(!qjs_temporal_epoch_ns_from_milliseconds(&expected, -8640000000000000.0));
    assert(!qjs_temporal_epoch_ns_compare(epoch, expected));
    assert(!qjs_temporal_system_epoch(&epoch));
    assert(qjs_temporal_epoch_ns_is_valid(epoch));
    return 0;
}
