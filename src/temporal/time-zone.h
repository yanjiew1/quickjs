/* Pure C Temporal time zones and exact epoch conversion. */
#ifndef QUICKJS_TEMPORAL_TIME_ZONE_H
#define QUICKJS_TEMPORAL_TIME_ZONE_H
#include "calendar.h"
#include "../timezone/timezone.h"
typedef enum QJSTemporalDisambiguation {
    QJS_TEMPORAL_COMPATIBLE, QJS_TEMPORAL_EARLIER,
    QJS_TEMPORAL_LATER, QJS_TEMPORAL_DISAMBIGUATION_REJECT
} QJSTemporalDisambiguation;
/* Offset zones carry whole minutes. Named IDs preserve canonical alias
   spelling, not merely the primary zone (Temporal.timeZoneId requires it). */
struct QJSTzProvider;
typedef struct QJSTemporalZone {
    char identifier[256];
    int64_t offset_nanoseconds;
    int is_offset;
    /* Non-owning immutable named-zone cache, bound by the engine runtime.
       NULL keeps pure native calls deterministic with embedded data only. */
    struct QJSTzProvider *provider;
} QJSTemporalZone;
int qjs_temporal_zone_parse(QJSTemporalZone *result,
                           const char *identifier, size_t length);
int qjs_temporal_zone_offset(const QJSTemporalZone *zone,
                            QJSTemporalEpochNs epoch, int64_t *result);
int qjs_temporal_zone_datetime(const QJSTemporalZone *zone,
                              QJSTemporalEpochNs epoch,
                              QJSTemporalISODateTime *result);
int qjs_temporal_zone_possible_epochs(const QJSTemporalZone *zone,
                         QJSTemporalISODateTime datetime,
                         QJSTemporalEpochNs result[2], int *count);
int qjs_temporal_zone_epoch(const QJSTemporalZone *zone,
                           QJSTemporalISODateTime datetime,
                           QJSTemporalDisambiguation disambiguation,
                           QJSTemporalEpochNs *result);
int qjs_temporal_zone_start_of_day(const QJSTemporalZone *zone,
                                 QJSTemporalISODate date,
                                 QJSTemporalEpochNs *result);
/* next: true strictly next, false strictly previous. found false is not
   an error (UTC/offset zones and dates beyond the last transition). */
int qjs_temporal_zone_transition(const QJSTemporalZone *zone,
                                QJSTemporalEpochNs epoch, int next,
                                QJSTemporalEpochNs *result, int *found);
int qjs_temporal_zones_equal(const QJSTemporalZone *a,
                            const QJSTemporalZone *b, int *result);
int qjs_temporal_system_zone(QJSTemporalZone *result);
int qjs_temporal_system_epoch(QJSTemporalEpochNs *result);
#endif
