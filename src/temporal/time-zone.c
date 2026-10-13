/* Native Temporal time-zone conversions use the shared qjs_tz provider. */
/* Native Temporal time zones use the standalone qjs_tz system-first provider
 * with compiled fallback. Epoch arithmetic stays exact integer words.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
#include "time-zone.h"
#include "parse.h"
#include "format.h"
#include "../timezone/timezone.h"
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
#define NS_DAY INT64_C(86400000000000)
static int ascii_equal(const char *a, size_t length, const char *b)
{
    size_t i;
    if (length != strlen(b)) return 0;
    for (i = 0; i < length; i++) {
        unsigned char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return 0;
    }
    return 1;
}
static int zone_error(int error)
{
    return !error ? 0 : error == QJS_TZ_MEMORY ? QJS_TEMPORAL_ERROR_MEMORY :
        error == QJS_TZ_RANGE ? QJS_TEMPORAL_ERROR_RANGE : QJS_TEMPORAL_ERROR_BACKEND;
}
static int any_seconds(int64_t *seconds, uint64_t *fraction, QJSTemporalEpochNs value)
{
    QJSTemporalEpochNs quotient;
    uint64_t remainder;
    if (qjs_temporal_epoch_ns_divide(&quotient, &remainder, value, 1000000000) ||
        qjs_temporal_epoch_ns_to_int64(seconds, quotient)) return QJS_TEMPORAL_ERROR_RANGE;
    if (fraction) *fraction = remainder;
    return 0;
}
static int native_zone_open(const QJSTemporalZone *zone, const QJSTimeZone **output)
{
    if (zone->provider) return qjs_tz_provider_open(zone->provider,
        zone->identifier, strlen(zone->identifier), output);
    {
        QJSTimeZone *owned;
        int error = qjs_tz_open_embedded(&owned, zone->identifier,
                    strlen(zone->identifier), NULL);
        if (!error) *output = owned;
        return error;
    }
}
static void native_zone_close(const QJSTemporalZone *zone, const QJSTimeZone *backend)
{
    if (!zone->provider) qjs_tz_close((QJSTimeZone *)backend);
}
static int local_offsets(const QJSTemporalZone *zone, QJSTemporalEpochNs wall,
                         int64_t offsets[2])
{
    const QJSTimeZone *backend;
    int32_t values[2];
    int64_t seconds;
    int error = any_seconds(&seconds, NULL, wall);
    if (error) return error;
    error = zone_error(native_zone_open(zone, &backend));
    if (error) return error;
    error = zone_error(qjs_tz_local_offsets(backend, seconds, values));
    native_zone_close(zone, backend);
    if (!error) {
        offsets[0] = (int64_t)values[0] * 1000000000;
        offsets[1] = (int64_t)values[1] * 1000000000;
    }
    return error;
}
int qjs_temporal_zone_parse(QJSTemporalZone *result,
                           const char *identifier, size_t length)
{
    QJSTemporalZone zone = {0};
    QJSTemporalUTCOffset offset;
    size_t consumed;
    if (!length || length >= sizeof(zone.identifier) || memchr(identifier, 0, length))
        return QJS_TEMPORAL_ERROR_RANGE;
    if (identifier[0] == '+' || identifier[0] == '-') {
        if (qjs_temporal_parse_utc_offset_prefix(&offset, &consumed,
                identifier, length, 0) || consumed != length)
            return QJS_TEMPORAL_ERROR_RANGE;
        zone.is_offset = 1;
        zone.offset_nanoseconds = offset.nanoseconds;
        if (qjs_temporal_format_utc_offset(zone.identifier, sizeof(zone.identifier),
                                         offset.nanoseconds, 0) < 0)
            return QJS_TEMPORAL_ERROR_RANGE;
        *result = zone;
        return 0;
    }
    if (ascii_equal(identifier, length, "UTC")) {
        memcpy(zone.identifier, "UTC", 4);
        *result = zone;
        return 0;
    }
    {
        const char *id;
        if (qjs_tz_resolve(identifier, length, &id, NULL))
            return QJS_TEMPORAL_ERROR_RANGE;
        size_t size;
        if (!id) return QJS_TEMPORAL_ERROR_RANGE;
        size = strlen(id);
        if (size >= sizeof(zone.identifier)) return QJS_TEMPORAL_ERROR_BACKEND;
        memcpy(zone.identifier, id, size + 1);
        *result = zone;
        return 0;
    }
}
static int zone_offset_any(const QJSTemporalZone *zone,
                           QJSTemporalEpochNs epoch, int64_t *result)
{
    const QJSTimeZone *backend;
    int64_t seconds;
    int32_t offset;
    int error;
    if (zone->is_offset || !strcmp(zone->identifier, "UTC")) {
        *result = zone->is_offset ? zone->offset_nanoseconds : 0;
        return 0;
    }
    error = any_seconds(&seconds, NULL, epoch);
    if (error) return error;
    error = zone_error(native_zone_open(zone, &backend));
    if (error) return error;
    error = zone_error(qjs_tz_offset(backend, seconds, &offset));
    native_zone_close(zone, backend);
    if (!error) *result = (int64_t)offset * 1000000000;
    return error;
}
int qjs_temporal_zone_offset(const QJSTemporalZone *zone,
                            QJSTemporalEpochNs epoch, int64_t *result)
{
    if (!qjs_temporal_epoch_ns_is_valid(epoch)) return QJS_TEMPORAL_ERROR_RANGE;
    return zone_offset_any(zone, epoch, result);
}
int qjs_temporal_zone_datetime(const QJSTemporalZone *zone,
                              QJSTemporalEpochNs epoch,
                              QJSTemporalISODateTime *result)
{
    int64_t offset;
    QJSTemporalEpochNs local;
    int error = qjs_temporal_zone_offset(zone, epoch, &offset);
    if (error) return error;
    if (qjs_temporal_epoch_ns_add(&local, epoch,
                qjs_temporal_epoch_ns_from_int64(offset)) ||
        qjs_temporal_iso_datetime_from_epoch_ns(result, local))
        return QJS_TEMPORAL_ERROR_RANGE;
    return 0;
}
int qjs_temporal_zone_possible_epochs(const QJSTemporalZone *zone,
                         QJSTemporalISODateTime datetime,
                         QJSTemporalEpochNs result[2], int *count)
{
    QJSTemporalEpochNs wall, candidate, choices[2];
    int64_t offsets[2], actual;
    int i, n = 0, error;
    if (qjs_temporal_iso_datetime_to_epoch_ns(&wall, datetime))
        return QJS_TEMPORAL_ERROR_RANGE;
    if (zone->is_offset || !strcmp(zone->identifier, "UTC")) {
        offsets[0] = zone->is_offset ? zone->offset_nanoseconds : 0;
        offsets[1] = offsets[0];
    } else {
        error = local_offsets(zone, wall, offsets);
        if (error) return error;
    }
    for (i = 0; i < 2; i++) {
        if (i && offsets[0] == offsets[1]) break;
        if (qjs_temporal_epoch_ns_subtract(&candidate, wall,
                    qjs_temporal_epoch_ns_from_int64(offsets[i])))
            return QJS_TEMPORAL_ERROR_RANGE;
        error = zone_offset_any(zone, candidate, &actual);
        if (error) return error;
        if (actual == offsets[i]) {
            if (!qjs_temporal_epoch_ns_is_valid(candidate))
                return QJS_TEMPORAL_ERROR_RANGE;
            choices[n++] = candidate;
        }
    }
    if (n == 2 && qjs_temporal_epoch_ns_compare(choices[0], choices[1]) > 0) {
        candidate = choices[0]; choices[0] = choices[1]; choices[1] = candidate;
    }
    for (i = 0; i < n; i++) result[i] = choices[i];
    *count = n;
    return 0;
}
int qjs_temporal_zone_epoch(const QJSTemporalZone *zone,
                           QJSTemporalISODateTime datetime,
                           QJSTemporalDisambiguation disambiguation,
                           QJSTemporalEpochNs *result)
{
    QJSTemporalEpochNs choices[2], wall, epoch;
    int n, error;
    int64_t offsets[2], offset;
    if (disambiguation < QJS_TEMPORAL_COMPATIBLE ||
        disambiguation > QJS_TEMPORAL_DISAMBIGUATION_REJECT)
        return QJS_TEMPORAL_ERROR_RANGE;
    error = qjs_temporal_zone_possible_epochs(zone, datetime, choices, &n);
    if (error) return error;
    if (n) {
        if (n > 1 && disambiguation == QJS_TEMPORAL_DISAMBIGUATION_REJECT)
            return QJS_TEMPORAL_ERROR_RANGE;
        *result = choices[disambiguation == QJS_TEMPORAL_LATER ? n - 1 : 0];
        return 0;
    }
    if (disambiguation == QJS_TEMPORAL_DISAMBIGUATION_REJECT)
        return QJS_TEMPORAL_ERROR_RANGE;
    if (qjs_temporal_iso_datetime_to_epoch_ns(&wall, datetime))
        return QJS_TEMPORAL_ERROR_RANGE;
    error = local_offsets(zone, wall, offsets);
    if (error) return error;
    /* Gap: former and latter offsets bracket the skipped wall interval.
       Earlier moves by the gap to the former side; compatible uses later. */
    offset = disambiguation == QJS_TEMPORAL_EARLIER ? offsets[1] : offsets[0];
    if (qjs_temporal_epoch_ns_subtract(&epoch, wall,
                    qjs_temporal_epoch_ns_from_int64(offset)) ||
        !qjs_temporal_epoch_ns_is_valid(epoch))
        return QJS_TEMPORAL_ERROR_RANGE;
    *result = epoch;
    return 0;
}
int qjs_temporal_zone_start_of_day(const QJSTemporalZone *zone,
                                 QJSTemporalISODate date,
                                 QJSTemporalEpochNs *result)
{
    QJSTemporalISODateTime midnight = { date, {0} };
    QJSTemporalEpochNs choices[2];
    QJSTemporalEpochNs earlier, transition;
    int found;
    int n, error;
    error = qjs_temporal_zone_possible_epochs(zone, midnight, choices, &n);
    if (error) return error;
    if (n) { *result = choices[0]; return 0; }
    error = qjs_temporal_zone_epoch(zone, midnight, QJS_TEMPORAL_EARLIER, &earlier);
    if (error) return error;
    error = qjs_temporal_zone_transition(zone, earlier, 1, &transition, &found);
    if (error) return error;
    if (!found) return QJS_TEMPORAL_ERROR_BACKEND;
    *result = transition;
    return 0;
}
int qjs_temporal_zone_transition(const QJSTemporalZone *zone,
                                QJSTemporalEpochNs epoch, int next,
                                QJSTemporalEpochNs *result, int *found)
{
    const QJSTimeZone *backend;
    int64_t seconds, transition;
    uint64_t fraction;
    int exists, error;
    QJSTemporalEpochNs value;
    if (!qjs_temporal_epoch_ns_is_valid(epoch)) return QJS_TEMPORAL_ERROR_RANGE;
    if (zone->is_offset || !strcmp(zone->identifier, "UTC")) {
        *found = 0; return 0;
    }
    error = any_seconds(&seconds, &fraction, epoch);
    if (error) return error;
    error = zone_error(native_zone_open(zone, &backend));
    if (error) return error;
    /* TZif transitions are integer seconds. A previous query from within a
       second includes its start; exact queries are strict in both directions. */
    error = zone_error(qjs_tz_transition(backend, seconds, next,
                    !next && fraction != 0, &transition, &exists));
    native_zone_close(zone, backend);
    if (error) return error;
    if (exists) {
        if (qjs_temporal_epoch_ns_multiply(&value,
                qjs_temporal_epoch_ns_from_int64(transition), 1000000000) ||
            !qjs_temporal_epoch_ns_is_valid(value)) return QJS_TEMPORAL_ERROR_RANGE;
        *result = value;
    }
    *found = exists;
    return 0;
}
int qjs_temporal_zones_equal(const QJSTemporalZone *a,
                            const QJSTemporalZone *b, int *result)
{
    const char *first, *second;
    if (a->is_offset || b->is_offset) {
        *result = a->is_offset && b->is_offset &&
            a->offset_nanoseconds == b->offset_nanoseconds;
        return 0;
    }
    if (qjs_tz_resolve(a->identifier, strlen(a->identifier), NULL, &first) ||
        qjs_tz_resolve(b->identifier, strlen(b->identifier), NULL, &second))
        return QJS_TEMPORAL_ERROR_BACKEND;
    *result = !strcmp(first, second);
    return 0;
}
int qjs_temporal_system_zone(QJSTemporalZone *result)
{
    char identifier[256];
    int error = zone_error(qjs_tz_system_identifier(identifier, sizeof identifier));
    if (error) return error;
    return qjs_temporal_zone_parse(result, identifier, strlen(identifier));
}
int qjs_temporal_system_epoch(QJSTemporalEpochNs *result)
{
    QJSTemporalEpochNs seconds, fraction, epoch;
#ifdef _WIN32
    FILETIME filetime;
    uint64_t ticks;
    GetSystemTimeAsFileTime(&filetime);
    ticks = ((uint64_t)filetime.dwHighDateTime << 32) | filetime.dwLowDateTime;
    seconds = qjs_temporal_epoch_ns_from_int64((int64_t)(ticks / 10000000) - INT64_C(11644473600));
    fraction = qjs_temporal_epoch_ns_from_int64((int64_t)(ticks % 10000000) * 100);
#else
#ifdef CLOCK_REALTIME
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now)) return QJS_TEMPORAL_ERROR_BACKEND;
    seconds = qjs_temporal_epoch_ns_from_int64((int64_t)now.tv_sec);
    fraction = qjs_temporal_epoch_ns_from_int64(now.tv_nsec);
#else
    struct timeval now;
    if (gettimeofday(&now, NULL)) return QJS_TEMPORAL_ERROR_BACKEND;
    seconds = qjs_temporal_epoch_ns_from_int64((int64_t)now.tv_sec);
    fraction = qjs_temporal_epoch_ns_from_int64((int64_t)now.tv_usec * 1000);
#endif
#endif
    if (qjs_temporal_epoch_ns_multiply(&epoch, seconds, 1000000000) ||
        qjs_temporal_epoch_ns_add(&epoch, epoch, fraction)) return QJS_TEMPORAL_ERROR_RANGE;
    /* HostSystemUTCEpochNanoseconds clamps an out-of-range system clock. */
    if (!qjs_temporal_epoch_ns_is_valid(epoch)) {
        QJSTemporalEpochNs limit;
        qjs_temporal_epoch_ns_from_milliseconds(&limit,
            epoch.high >> 63 ? -8640000000000000.0 : 8640000000000000.0);
        epoch = limit;
    }
    *result = epoch;
    return 0;
}

