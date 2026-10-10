/*
 * Copyright (C) 1996-2012, International Business Machines Corporation
 * and others. All Rights Reserved.
 * Copyright (C) 2016 and later: Unicode, Inc. and others.
 * Original ICU modification history: 2003-nov-07 srl, Port from Java.
 * SPDX-License-Identifier: Unicode-3.0
 *
 * C translation of the solar longitude, lunar longitude/age, and event
 * interpolation portions of ICU 78.3 source/i18n/astro.cpp.
 * Upstream SHA-256:
 * cab130a5c3f9bd4708744f2c8b23439969c340a4076ba9fc5199fe97f25d2b1d
 * License and terms are preserved in LICENSE-ICU beside this source.
 *
 * The upstream algorithms cite Peter Duffett-Smith, Practical Astronomy
 * with your Calculator, pp. 86, 90, 142, and 147. This translation retains
 * ICU's constants, normalization, one-minute event tolerance, and ceil()
 * time corrections. It removes unrelated coordinate conversion/cache/ICU
 * classes and bounds Kepler iterations, interpolation, and restart depth.
 * Exhausted bounds and nonfinite intermediates fail explicitly.
 * This source candidate has not been compiled or executed by its author.
 */
#include "astronomy.h"
#include <math.h>
#include <stddef.h>

#define PI QJS_CAL_ASTRO_PI
#define PI2 (PI * 2.0)
#define DEG (PI / 180.0)
#define DAY_MS QJS_CAL_ASTRO_DAY_MS
#define TROPICAL_YEAR 365.242191
#define JULIAN_EPOCH_MS (-210866760000000.0)
#define JD_EPOCH 2447891.5
#define SUN_ETA_G (279.403303 * DEG)
#define SUN_OMEGA_G (282.768422 * DEG)
#define SUN_E 0.016713
#define MOON_L0 (318.351648 * DEG)
#define MOON_P0 (36.340410 * DEG)
#define MOON_N0 (318.510107 * DEG)
#define MOON_I (5.145366 * DEG)
#define EVENT_EPSILON_MS 60000.0
#define KEPLER_ITERATIONS 32
#define EVENT_ITERATIONS 64
#define EVENT_RESTARTS 16

static int valid_time(double time)
{
    return isfinite(time) && fabs(time) <= QJS_CAL_ASTRO_MAX_ABS_MS;
}

static double norm2pi(double angle)
{
    return angle - PI2 * floor(angle / PI2);
}

static double normpi(double angle)
{
    return norm2pi(angle + PI) - PI;
}

static double epoch_days(double time)
{
    /* Keep the same operation order as ICU's getJulianDay()-JD_EPOCH. */
    return (time - JULIAN_EPOCH_MS) / DAY_MS - JD_EPOCH;
}

static int sun_position(double time, double *longitude, double *anomaly)
{
    double day = epoch_days(time);
    double epoch_angle = norm2pi(PI2 / TROPICAL_YEAR * day);
    double mean = norm2pi(epoch_angle + SUN_ETA_G - SUN_OMEGA_G);
    double eccentric = mean;
    double delta, true_anomaly;
    int i;
    for (i = 0; i < KEPLER_ITERATIONS; i++) {
        delta = eccentric - SUN_E * sin(eccentric) - mean;
        eccentric -= delta / (1.0 - SUN_E * cos(eccentric));
        if (!isfinite(eccentric) || !isfinite(delta))
            return QJS_CAL_BACKEND;
        if (fabs(delta) <= 1e-5)
            break;
    }
    if (i == KEPLER_ITERATIONS)
        return QJS_CAL_BACKEND;
    true_anomaly = 2.0 * atan(tan(eccentric / 2.0) *
                                  sqrt((1.0 + SUN_E) / (1.0 - SUN_E)));
    *longitude = norm2pi(true_anomaly + SUN_OMEGA_G);
    *anomaly = mean;
    return isfinite(*longitude) ? QJS_CAL_OK : QJS_CAL_BACKEND;
}

static int moon_age(double time, double *age)
{
    double sun, sun_anomaly, day, mean_longitude, moon_anomaly;
    double evection, annual, a3, center, a4, longitude, variation;
    double node, x, y, ecliptic;
    int error = sun_position(time, &sun, &sun_anomaly);
    if (error)
        return error;
    day = epoch_days(time);
    mean_longitude = norm2pi(13.1763966 * DEG * day + MOON_L0);
    moon_anomaly = norm2pi(mean_longitude - 0.1114041 * DEG * day - MOON_P0);
    evection = 1.2739 * DEG * sin(2.0 * (mean_longitude - sun) - moon_anomaly);
    annual = 0.1858 * DEG * sin(sun_anomaly);
    a3 = 0.3700 * DEG * sin(sun_anomaly);
    moon_anomaly += evection - annual - a3;
    center = 6.2886 * DEG * sin(moon_anomaly);
    a4 = 0.2140 * DEG * sin(2.0 * moon_anomaly);
    longitude = mean_longitude + evection + center - annual + a4;
    variation = 0.6583 * DEG * sin(2.0 * (longitude - sun));
    longitude += variation;
    node = norm2pi(MOON_N0 - 0.0529539 * DEG * day);
    node -= 0.16 * DEG * sin(sun_anomaly);
    y = sin(longitude - node);
    x = cos(longitude - node);
    ecliptic = atan2(y * cos(MOON_I), x) + node;
    /* ICU then calculates unused equatorial coordinates; its moon age
     * depends only on this ecliptic longitude and the sun longitude. */
    *age = norm2pi(ecliptic - sun);
    return isfinite(*age) ? QJS_CAL_OK : QJS_CAL_BACKEND;
}

static int evaluate_angle(double time, int moon, double *angle)
{
    double anomaly;
    return moon ? moon_age(time, angle) : sun_position(time, angle, &anomaly);
}

static int event_time(double start, double desired, double period_days,
                      int moon, int after, double *result)
{
    double retry_start = start;
    int restart;
    for (restart = 0; restart <= EVENT_RESTARTS; restart++) {
        double last_angle, delta_angle, delta_time, last_delta_time, time;
        int error, iteration;
        if (!valid_time(retry_start))
            return QJS_CAL_BACKEND;
        error = evaluate_angle(retry_start, moon, &last_angle);
        if (error)
            return error;
        delta_angle = norm2pi(desired - last_angle);
        delta_time = (delta_angle + (after ? 0.0 : -PI2)) *
                      (period_days * DAY_MS) / PI2;
        last_delta_time = delta_time;
        time = retry_start + ceil(delta_time);
        /* ICU's zero-length interpolation would divide 0/0 at an exact
         * starting event. Preserve the documented inclusive forward case. */
        if (after && delta_time == 0.0) {
            if (retry_start < start ||
                retry_start - start > period_days * DAY_MS * 1.25)
                return QJS_CAL_BACKEND;
            *result = retry_start;
            return QJS_CAL_OK;
        }
        for (iteration = 0; iteration < EVENT_ITERATIONS; iteration++) {
            double angle, difference, factor;
            if (!valid_time(time))
                return QJS_CAL_BACKEND;
            error = evaluate_angle(time, moon, &angle);
            if (error)
                return error;
            difference = normpi(angle - last_angle);
            if (difference == 0.0)
                return QJS_CAL_BACKEND;
            factor = fabs(delta_time / difference);
            delta_time = normpi(desired - angle) * factor;
            if (!isfinite(delta_time))
                return QJS_CAL_BACKEND;
            if (fabs(delta_time) > fabs(last_delta_time)) {
                /* Equivalent to ICU's recursive one-eighth-period retry,
                 * with an explicit bound and no recursive stack growth. */
                double displacement = ceil(period_days * DAY_MS / 8.0);
                retry_start += after ? displacement : -displacement;
                break;
            }
            last_delta_time = delta_time;
            last_angle = angle;
            time += ceil(delta_time);
            if (fabs(delta_time) <= EVENT_EPSILON_MS) {
                /* A divergence retry must not return an event in the wrong
                 * direction or skip farther than the next event period. */
                if (!valid_time(time) ||
                    (after ? time < start : time >= start) ||
                    fabs(time - start) > period_days * DAY_MS * 1.25)
                    return QJS_CAL_BACKEND;
                *result = time;
                return QJS_CAL_OK;
            }
        }
        if (iteration == EVENT_ITERATIONS)
            return QJS_CAL_BACKEND;
    }
    return QJS_CAL_BACKEND;
}

int qjs_calendar_astro_sun_longitude(double epoch_ms, double *result)
{
    double longitude, anomaly;
    int error;
    if (!result || !valid_time(epoch_ms))
        return QJS_CAL_RANGE;
    error = sun_position(epoch_ms, &longitude, &anomaly);
    if (!error)
        *result = longitude;
    return error;
}

int qjs_calendar_astro_solar_time(double epoch_ms, double longitude,
                                int after, double *result)
{
    if (!result || !valid_time(epoch_ms) || !isfinite(longitude) ||
        (after != 0 && after != 1))
        return QJS_CAL_RANGE;
    return event_time(epoch_ms, norm2pi(longitude), TROPICAL_YEAR,
                      0, after, result);
}

int qjs_calendar_astro_new_moon(double epoch_ms, int after, double *result)
{
    if (!result || !valid_time(epoch_ms) || (after != 0 && after != 1))
        return QJS_CAL_RANGE;
    return event_time(epoch_ms, 0.0, QJS_CAL_ASTRO_SYNODIC_MONTH,
                      1, after, result);
}

double qjs_calendar_astro_mean_lunation_days(void)
{
    return 360.0 / (13.1763966 - 360.0 / TROPICAL_YEAR);
}

int qjs_calendar_astro_mean_lunation_number(double epoch_ms, int64_t *result)
{
    double cycles;
    if (!result || !valid_time(epoch_ms))
        return QJS_CAL_RANGE;
    /* The linear mean phase is unwrapped; rounding at computed new moons
     * avoids the secular drift of using SYNODIC_MONTH as an absolute index.
     * See the limits and validation requirement in the handoff document. */
    cycles = (13.1763966 * DEG - PI2 / TROPICAL_YEAR) * epoch_days(epoch_ms);
    cycles += MOON_L0 - SUN_ETA_G;
    cycles = floor(cycles / PI2 + 0.5);
    if (!isfinite(cycles) || cycles < -1000000000.0 || cycles > 1000000000.0)
        return QJS_CAL_BACKEND;
    *result = (int64_t)cycles;
    return QJS_CAL_OK;
}
