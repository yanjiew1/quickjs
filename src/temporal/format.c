/*
 * Native Temporal support
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#include <stdio.h>
#include <string.h>
#include "format.h"

static void qjs_temporal_format_fraction(char output[10], unsigned value)
{
    int i;

    for (i = 8; i >= 0; i--) {
        output[i] = '0' + value % 10;
        value /= 10;
    }
    output[9] = '\0';
}

int qjs_temporal_format_date_time(char *output, size_t capacity,
                                 QJSTemporalISODateTime datetime,
                                 int precision)
{
    char buffer[64], fraction[10];
    QJSTemporalISODate date = datetime.date;
    QJSTemporalISOTime time = datetime.time;
    int length, digits, value;

    if (!qjs_temporal_iso_date_is_valid(date) ||
        !qjs_temporal_iso_time_is_valid(time) ||
        precision < -2 || precision > 9)
        return -1;
    if (date.year >= 0 && date.year <= 9999) {
        length = snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d",
                          date.year, date.month, date.day,
                          time.hour, time.minute);
    } else {
        int64_t year = date.year;
        char sign = year < 0 ? '-' : '+';
        if (year < 0)
            year = -year;
        length = snprintf(buffer, sizeof(buffer), "%c%06lld-%02d-%02dT%02d:%02d",
                          sign, (long long)year, date.month, date.day,
                          time.hour, time.minute);
    }
    if (length < 0 || (size_t)length >= sizeof(buffer))
        return -1;
    if (precision != -2) {
        int count = snprintf(buffer + length, sizeof(buffer) - length,
                             ":%02d", time.second);
        if (count < 0 || (size_t)count >= sizeof(buffer) - length)
            return -1;
        length += count;
        value = time.millisecond * 1000000 + time.microsecond * 1000 +
                time.nanosecond;
        qjs_temporal_format_fraction(fraction, value);
        digits = precision;
        if (precision == -1) {
            digits = 9;
            while (digits > 0 && fraction[digits - 1] == '0')
                digits--;
        }
        if (digits) {
            buffer[length++] = '.';
            memcpy(buffer + length, fraction, digits);
            length += digits;
            buffer[length] = '\0';
        }
    }
    if ((size_t)length >= capacity)
        return -1;
    memcpy(output, buffer, length + 1);
    return length;
}

int qjs_temporal_format_utc_offset(char *output, size_t capacity,
                                  int64_t nanoseconds, int round_to_minutes)
{
    char buffer[32], fraction[10];
    uint64_t magnitude, seconds;
    int length, digits, hour, minute, second;
    char sign;

    if (nanoseconds <= -INT64_C(86400000000000) ||
        nanoseconds >= INT64_C(86400000000000))
        return -1;
    sign = nanoseconds < 0 ? '-' : '+';
    magnitude = nanoseconds < 0 ? (uint64_t)-nanoseconds :
                                 (uint64_t)nanoseconds;
    if (round_to_minutes) {
        uint64_t minutes = (magnitude + UINT64_C(30000000000)) /
                           UINT64_C(60000000000);
        if (!minutes)
            sign = '+';
        hour = minutes / 60;
        minute = minutes % 60;
        length = snprintf(buffer, sizeof(buffer), "%c%02d:%02d",
                          sign, hour, minute);
    } else {
        seconds = magnitude / UINT64_C(1000000000);
        hour = seconds / 3600;
        minute = seconds / 60 % 60;
        second = seconds % 60;
        length = snprintf(buffer, sizeof(buffer), "%c%02d:%02d",
                          sign, hour, minute);
        if (second || magnitude % UINT64_C(1000000000)) {
            int count = snprintf(buffer + length, sizeof(buffer) - length,
                                 ":%02d", second);
            if (count < 0 || (size_t)count >= sizeof(buffer) - length)
                return -1;
            length += count;
            qjs_temporal_format_fraction(fraction,
                        (unsigned)(magnitude % UINT64_C(1000000000)));
            digits = 9;
            while (digits && fraction[digits - 1] == '0')
                digits--;
            if (digits) {
                buffer[length++] = '.';
                memcpy(buffer + length, fraction, digits);
                length += digits;
                buffer[length] = '\0';
            }
        }
    }
    if (length < 0 || (size_t)length >= capacity)
        return -1;
    memcpy(output, buffer, length + 1);
    return length;
}
