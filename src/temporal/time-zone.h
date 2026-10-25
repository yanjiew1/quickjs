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

/* Exact Temporal operations over a compile-time selected timezone backend. */
#ifndef QUICKJS_TEMPORAL_TIME_ZONE_H
#define QUICKJS_TEMPORAL_TIME_ZONE_H

#include "types.h"
#include "../timezone/identifier.h"

typedef enum QJSTemporalDisambiguation {
    QJS_TEMPORAL_COMPATIBLE,
    QJS_TEMPORAL_EARLIER,
    QJS_TEMPORAL_LATER,
    QJS_TEMPORAL_DISAMBIGUATION_REJECT,
} QJSTemporalDisambiguation;

/* Named handles borrow immutable backend data. Offset zones are whole
   minutes; named offsets retain historical seconds. No record owns memory. */
typedef QJSTimeZoneIdentifier QJSTemporalZone;

int qjs_temporal_zone_parse(QJSTemporalZone *result,
                            const char *identifier, size_t length);
int qjs_temporal_zone_offset(const QJSTemporalZone *zone,
                             QJSTemporalEpochNs epoch, int64_t *result);
int qjs_temporal_zone_datetime(const QJSTemporalZone *zone,
                               QJSTemporalEpochNs epoch,
                               QJSTemporalISODateTime *result);
/* Visitors receive borrowed numeric values. Stage results until success:
   an out-of-range candidate can fail after earlier candidates were visited. */
typedef void QJSTemporalEpochVisit(void *opaque, QJSTemporalEpochNs epoch);
int qjs_temporal_zone_possible_epochs(const QJSTemporalZone *zone,
                                      QJSTemporalISODateTime datetime,
                                      QJSTemporalEpochVisit *visit,
                                      void *opaque, size_t *count);
int qjs_temporal_zone_epoch(const QJSTemporalZone *zone,
                            QJSTemporalISODateTime datetime,
                            QJSTemporalDisambiguation disambiguation,
                            QJSTemporalEpochNs *result);
int qjs_temporal_zone_start_of_day(const QJSTemporalZone *zone,
                                   QJSTemporalISODate date,
                                   QJSTemporalEpochNs *result);
int qjs_temporal_zone_transition(const QJSTemporalZone *zone,
                                 QJSTemporalEpochNs epoch, int next,
                                 QJSTemporalEpochNs *result, int *found);
/* Pure equality over already resolved zones. */
int qjs_temporal_zones_equal(const QJSTemporalZone *a,
                             const QJSTemporalZone *b);

#endif /* QUICKJS_TEMPORAL_TIME_ZONE_H */
