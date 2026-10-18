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
    QJSTemporalCalendar calendar,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result);
int qjs_temporal_relative_total(
    QJSTemporalInternalDuration duration,
    QJSTemporalEpochNs origin_epoch, QJSTemporalEpochNs dest_epoch,
    QJSTemporalISODateTime origin, const QJSTemporalZone *zone,
    QJSTemporalCalendar calendar, QJSTemporalUnit unit, double *result);
int qjs_temporal_plain_datetime_difference(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar, QJSTemporalUnit largest,
    QJSTemporalInternalDuration *result);
/* PlainDate rounding permits the boundary date at midnight, whose date
   is valid even when that midnight is outside PlainDateTime limits. */
int qjs_temporal_plain_relative_round(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar, QJSTemporalInternalDuration duration,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result);
int qjs_temporal_plain_datetime_difference_round(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result);
int qjs_temporal_plain_datetime_difference_total(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar, QJSTemporalUnit unit, double *result);
#endif
