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
