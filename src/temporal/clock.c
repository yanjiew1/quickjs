/* Host UTC clock acquisition and range normalization.
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
#include "clock.h"
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

int qjs_temporal_epoch_from_clock(QJSTemporalEpochNs *result,
                                  int64_t seconds, int32_t nanoseconds)
{
    QJSTemporalEpochNs epoch;

    if (nanoseconds < 0 || nanoseconds >= 1000000000)
        return QJS_TEMPORAL_ERROR_BACKEND;
    if (qjs_temporal_epoch_ns_multiply(&epoch,
            qjs_temporal_epoch_ns_from_int64(seconds), 1000000000) ||
        qjs_temporal_epoch_ns_add(&epoch, epoch,
            qjs_temporal_epoch_ns_from_int64(nanoseconds)))
        return QJS_TEMPORAL_ERROR_RANGE;
    if (!qjs_temporal_epoch_ns_is_valid(epoch)) {
        double milliseconds = seconds < 0 ?
            -8640000000000000.0 : 8640000000000000.0;
        if (qjs_temporal_epoch_ns_from_milliseconds(&epoch, milliseconds))
            return QJS_TEMPORAL_ERROR_BACKEND;
    }
    *result = epoch;
    return 0;
}

int qjs_temporal_milliseconds_from_clock(int64_t *result,
                                         int64_t seconds, int32_t nanoseconds)
{
    const int64_t limit = INT64_C(8640000000000000);
    int64_t milliseconds;

    if (nanoseconds < 0 || nanoseconds >= 1000000000)
        return QJS_TEMPORAL_ERROR_BACKEND;
    if (seconds < -limit / 1000)
        milliseconds = -limit;
    else if (seconds > limit / 1000)
        milliseconds = limit;
    else {
        milliseconds = seconds * 1000 + nanoseconds / 1000000;
        if (milliseconds > limit)
            milliseconds = limit;
    }
    *result = milliseconds;
    return 0;
}

static int system_clock(int64_t *epoch_seconds, int32_t *fraction)
{
    int64_t seconds;
    int32_t nanoseconds;

#ifdef _WIN32
    FILETIME filetime;
    uint64_t ticks;

    GetSystemTimeAsFileTime(&filetime);
    ticks = ((uint64_t)filetime.dwHighDateTime << 32) | filetime.dwLowDateTime;
    seconds = (int64_t)(ticks / 10000000) - INT64_C(11644473600);
    nanoseconds = (int32_t)(ticks % 10000000) * 100;
#else
#ifdef CLOCK_REALTIME
    struct timespec now;

    if (clock_gettime(CLOCK_REALTIME, &now))
        return QJS_TEMPORAL_ERROR_BACKEND;
    seconds = (int64_t)now.tv_sec;
    nanoseconds = now.tv_nsec;
#else
    struct timeval now;

    if (gettimeofday(&now, NULL))
        return QJS_TEMPORAL_ERROR_BACKEND;
    seconds = (int64_t)now.tv_sec;
    nanoseconds = (int32_t)now.tv_usec * 1000;
#endif
#endif
    *epoch_seconds = seconds;
    *fraction = nanoseconds;
    return 0;
}

int qjs_temporal_system_epoch(QJSTemporalEpochNs *result)
{
    int64_t seconds;
    int32_t nanoseconds;
    int error = system_clock(&seconds, &nanoseconds);

    if (error)
        return error;
    return qjs_temporal_epoch_from_clock(result, seconds, nanoseconds);
}

int qjs_temporal_system_milliseconds(int64_t *result)
{
    int64_t seconds;
    int32_t nanoseconds;
    int error = system_clock(&seconds, &nanoseconds);

    if (error)
        return error;
    return qjs_temporal_milliseconds_from_clock(result, seconds, nanoseconds);
}
