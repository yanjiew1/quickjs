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
    {
        const int64_t seconds[] = {
            INT64_MIN, -8640000000001LL, -8640000000000LL, -1, 0, 1,
            8640000000000LL, 8640000000001LL, INT64_MAX
        };
        const int32_t fractions[] = { 0, 1, 999999, 1000000, 999999999 };
        unsigned i, j;

        for (i = 0; i < sizeof(seconds) / sizeof(seconds[0]); i++) {
            for (j = 0; j < sizeof(fractions) / sizeof(fractions[0]); j++) {
                int64_t exact;
                assert(!qjs_temporal_epoch_from_clock(&epoch,
                    seconds[i], fractions[j]));
                assert(!qjs_temporal_epoch_ns_to_milliseconds(&exact, epoch));
                assert(!qjs_temporal_milliseconds_from_clock(&milliseconds,
                    seconds[i], fractions[j]));
                assert(milliseconds == exact);
            }
        }
        milliseconds = 42;
        assert(qjs_temporal_milliseconds_from_clock(&milliseconds, 0, -1));
        assert(milliseconds == 42);
        assert(qjs_temporal_milliseconds_from_clock(&milliseconds, 0, 1000000000));
        assert(milliseconds == 42);
        assert(!qjs_temporal_system_milliseconds(&milliseconds));
        assert(milliseconds >= -8640000000000000LL &&
               milliseconds <= 8640000000000000LL);
    }
    return 0;
}
