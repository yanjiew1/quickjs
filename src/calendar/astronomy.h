/* Portable calendar astronomy candidate. See astronomy.c for provenance.
 * SPDX-License-Identifier: Unicode-3.0
 */
#ifndef QUICKJS_CALENDAR_ASTRONOMY_H
#define QUICKJS_CALENDAR_ASTRONOMY_H
#include <stdint.h>
#include "calendar.h"

#define QJS_CAL_ASTRO_PI 3.14159265358979323846
#define QJS_CAL_ASTRO_SYNODIC_MONTH 29.530588853
#define QJS_CAL_ASTRO_DAY_MS 86400000.0
/* Original ICU solar evaluation interval, inclusive Jan 1 boundaries.
 * Outside it, solar phase has a continuous Gregorian mean-year extension. */
#define QJS_CAL_ASTRO_LINEAR_MIN_YEAR (-10000)
#define QJS_CAL_ASTRO_LINEAR_MAX_YEAR 10000
/* Numerical guard, not an assertion of physical or historical accuracy.
 * It includes metadata for the common internal year domain +/-1000000. */
#define QJS_CAL_ASTRO_MAX_ABS_MS 32000000000000000.0

/* Each failure leaves *result unchanged. UTC milliseconds since 1970-01-01.
 * after=1 includes an event at the starting instant; after=0 searches before.
 * The solar longitude model and the lunar model are ICU 78.3 approximations.
 * BACKEND reports nonfinite arithmetic or bounded-search failure. */
int qjs_calendar_astro_sun_longitude(double epoch_ms, double *result);
int qjs_calendar_astro_solar_time(double epoch_ms, double longitude,
                                int after, double *result);
int qjs_calendar_astro_new_moon(double epoch_ms, int after, double *result);

/* Index of the nearest MEAN new moon, not a phase converter. Within a day
 * containing a computed true new moon this labels the consecutive lunation.
 * Used only after event/day checks, with target-index verification. */
int qjs_calendar_astro_mean_lunation_number(double epoch_ms, int64_t *result);
double qjs_calendar_astro_mean_lunation_days(void);
#endif
