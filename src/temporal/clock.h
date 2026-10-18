/* Host UTC clock, shared by Date and Temporal without timezone lookup. */
#ifndef QUICKJS_TEMPORAL_CLOCK_H
#define QUICKJS_TEMPORAL_CLOCK_H

#include "types.h"

/* Normalize floor seconds plus 0..999999999 nanoseconds and clamp to
   Temporal's Instant range. Errors preserve the output. */
int qjs_temporal_epoch_from_clock(QJSTemporalEpochNs *result,
                                  int64_t seconds, int32_t nanoseconds);
/* Equivalent to flooring the clamped exact epoch, without constructing or
   dividing a two-word value. Errors preserve the output. */
int qjs_temporal_milliseconds_from_clock(int64_t *result,
                                         int64_t seconds, int32_t nanoseconds);
int qjs_temporal_system_epoch(QJSTemporalEpochNs *result);
int qjs_temporal_system_milliseconds(int64_t *result);

#endif /* QUICKJS_TEMPORAL_CLOCK_H */
