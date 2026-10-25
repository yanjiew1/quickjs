/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

/* Plain civil relative duration arithmetic. */
#ifndef QUICKJS_TEMPORAL_RELATIVE_H
#define QUICKJS_TEMPORAL_RELATIVE_H
#include "calendar.h"
#include "time-zone.h"

/* All records/pointers are borrowed; NULL zone means fixed civil days. */
int qjs_temporal_relative_round(
    QJSTemporalInternalDuration duration,
    QJSTemporalEpochNs origin_epoch, QJSTemporalEpochNs dest_epoch,
    QJSTemporalISODateTime origin, const QJSTemporalZone *zone,
    QJSCalendarId calendar,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result);
int qjs_temporal_relative_total(
    QJSTemporalInternalDuration duration,
    QJSTemporalEpochNs origin_epoch, QJSTemporalEpochNs dest_epoch,
    QJSTemporalISODateTime origin, const QJSTemporalZone *zone,
    QJSCalendarId calendar, QJSTemporalUnit unit, double *result);
int qjs_temporal_plain_datetime_difference(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSCalendarId calendar, QJSTemporalUnit largest,
    QJSTemporalInternalDuration *result);
/* PlainDate rounding permits the boundary date at midnight, whose date
   is valid even when that midnight is outside PlainDateTime limits. */
int qjs_temporal_plain_relative_round(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSCalendarId calendar, QJSTemporalInternalDuration duration,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result);
int qjs_temporal_plain_datetime_difference_round(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSCalendarId calendar,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result);
int qjs_temporal_plain_datetime_difference_total(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSCalendarId calendar, QJSTemporalUnit unit, double *result);
#endif
