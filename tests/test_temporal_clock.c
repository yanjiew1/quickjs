/* Host clock normalization without relying on the machine's date setting.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
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
