/* Portable Temporal civil arithmetic. */
#ifndef QUICKJS_TEMPORAL_CIVIL_H
#define QUICKJS_TEMPORAL_CIVIL_H

#include "types.h"
#include <stddef.h>

int qjs_temporal_iso_leap_year(int32_t year);
int qjs_temporal_iso_days_in_month(int32_t year, int32_t month);
int qjs_temporal_iso_date_compare(QJSTemporalISODate a,
                                   QJSTemporalISODate b);
int qjs_temporal_iso_datetime_compare(QJSTemporalISODateTime a,
                                       QJSTemporalISODateTime b);
int qjs_temporal_iso_date_within_limits(QJSTemporalISODate date);
int qjs_temporal_iso_datetime_within_limits(QJSTemporalISODateTime value);
int qjs_temporal_iso_year_month_within_limits(QJSTemporalISODate date);

/* Checked civil operations leave the required result untouched on error.
   Regulate accepts an unbalanced day/month, but not an unbalanced year.
   Add operations cover all int32 years; Temporal range is separate. */
int qjs_temporal_iso_regulate_date(QJSTemporalISODate *result,
                                  int64_t year, int64_t month, int64_t day,
                                  QJSTemporalOverflow overflow);
int qjs_temporal_iso_add_days(QJSTemporalISODate *result,
                              QJSTemporalISODate date, int64_t days);
int qjs_temporal_iso_date_add(QJSTemporalISODate *result,
                              QJSTemporalISODate date,
                              QJSTemporalDateDuration duration,
                              QJSTemporalOverflow overflow);
int qjs_temporal_iso_date_until(QJSTemporalDateDuration *result,
                                QJSTemporalISODate one,
                                QJSTemporalISODate two,
                                QJSTemporalUnit largest_unit);
int qjs_temporal_iso_day_of_week(QJSTemporalISODate date);
int qjs_temporal_iso_day_of_year(QJSTemporalISODate date);
int qjs_temporal_iso_week_of_year(QJSTemporalISODate date,
                                 int32_t *year_of_week);

int qjs_temporal_iso_datetime_add_time(QJSTemporalISODateTime *result,
                                       QJSTemporalISODateTime value,
                                       QJSTemporalEpochNs nanoseconds);
int qjs_temporal_iso_datetime_round(QJSTemporalISODateTime *result,
                                    QJSTemporalISODateTime value,
                                    uint64_t increment_nanoseconds,
                                    QJSTemporalRoundingMode mode);
/* Return written ASCII length, or -1 when the buffer is too small. */
int qjs_temporal_iso_date_format(char *buffer, size_t capacity,
                                 QJSTemporalISODate date);

#endif /* QUICKJS_TEMPORAL_CIVIL_H */
