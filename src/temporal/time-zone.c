/* Native Temporal timezone arithmetic.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "time-zone.h"
#include <string.h>

static int zone_error(int error)
{
    switch (error) {
    case QJS_TZ_OK: return 0;
    case QJS_TZ_RANGE: return QJS_TEMPORAL_ERROR_RANGE;
    case QJS_TZ_UNSUPPORTED: return QJS_TEMPORAL_ERROR_UNSUPPORTED;
    case QJS_TZ_MEMORY: return QJS_TEMPORAL_ERROR_MEMORY;
    default: return QJS_TEMPORAL_ERROR_BACKEND;
    }
}

static int epoch_seconds(QJSTemporalEpochNs epoch, int64_t *seconds,
                         uint64_t *fraction)
{
    QJSTemporalEpochNs quotient;
    uint64_t remainder;

    if (qjs_temporal_epoch_ns_divide(&quotient, &remainder, epoch, 1000000000) ||
        qjs_temporal_epoch_ns_to_int64(seconds, quotient))
        return QJS_TEMPORAL_ERROR_RANGE;
    if (fraction)
        *fraction = remainder;
    return 0;
}

static int epoch_from_seconds(QJSTemporalEpochNs *result, int64_t seconds,
                              uint64_t fraction)
{
    QJSTemporalEpochNs epoch;

    if (qjs_temporal_epoch_ns_multiply(&epoch,
            qjs_temporal_epoch_ns_from_int64(seconds), 1000000000) ||
        qjs_temporal_epoch_ns_add(&epoch, epoch,
            qjs_temporal_epoch_ns_from_int64((int64_t)fraction)) ||
        !qjs_temporal_epoch_ns_is_valid(epoch))
        return QJS_TEMPORAL_ERROR_RANGE;
    *result = epoch;
    return 0;
}

int qjs_temporal_zone_parse(QJSTemporalZone *result,
                            const char *identifier, size_t length)
{
    return zone_error(qjs_timezone_parse_identifier(result, identifier, length));
}

int qjs_temporal_zone_offset(const QJSTemporalZone *zone,
                             QJSTemporalEpochNs epoch, int64_t *result)
{
    int64_t seconds;
    int32_t offset;
    int error;

    if (!qjs_temporal_epoch_ns_is_valid(epoch))
        return QJS_TEMPORAL_ERROR_RANGE;
    if (zone->is_offset) {
        *result = zone->offset_nanoseconds;
        return 0;
    }
    error = epoch_seconds(epoch, &seconds, NULL);
    if (!error)
        error = zone_error(qjs_timezone_offset(zone->named, seconds, &offset));
    if (error)
        return error;
    *result = (int64_t)offset * 1000000000;
    return 0;
}

int qjs_temporal_zone_datetime(const QJSTemporalZone *zone,
                               QJSTemporalEpochNs epoch,
                               QJSTemporalISODateTime *result)
{
    QJSTemporalEpochNs local;
    int64_t offset;
    int error = qjs_temporal_zone_offset(zone, epoch, &offset);

    if (error)
        return error;
    if (qjs_temporal_epoch_ns_add(&local, epoch,
            qjs_temporal_epoch_ns_from_int64(offset)) ||
        qjs_temporal_iso_datetime_from_epoch_ns(result, local))
        return QJS_TEMPORAL_ERROR_RANGE;
    return 0;
}

typedef struct EpochVisitor {
    QJSTemporalEpochVisit *visit;
    void *opaque;
    uint64_t fraction;
    int error;
} EpochVisitor;

static void visit_seconds(void *opaque, int64_t seconds)
{
    EpochVisitor *state = opaque;
    QJSTemporalEpochNs epoch;

    if (state->error)
        return;
    state->error = epoch_from_seconds(&epoch, seconds, state->fraction);
    if (!state->error)
        state->visit(state->opaque, epoch);
}

static int local_candidates(const QJSTemporalZone *zone,
                             QJSTemporalISODateTime datetime,
                             QJSTemporalEpochVisit *visit, void *opaque,
                             size_t *count, int64_t *before, int64_t *after,
                             int64_t *gap_end)
{
    QJSTemporalEpochNs wall, epoch;
    EpochVisitor state = { visit, opaque, 0, 0 };
    int64_t seconds, end;
    int32_t first, last;
    size_t n;
    int error;

    if (qjs_temporal_iso_datetime_to_epoch_ns(&wall, datetime))
        return QJS_TEMPORAL_ERROR_RANGE;
    if (zone->is_offset) {
        if (qjs_temporal_epoch_ns_subtract(&epoch, wall,
                qjs_temporal_epoch_ns_from_int64(zone->offset_nanoseconds)) ||
            !qjs_temporal_epoch_ns_is_valid(epoch))
            return QJS_TEMPORAL_ERROR_RANGE;
        visit(opaque, epoch);
        *count = 1;
        *before = *after = zone->offset_nanoseconds;
        return 0;
    }
    error = epoch_seconds(wall, &seconds, &state.fraction);
    if (!error)
        error = zone_error(qjs_timezone_candidates(zone->named, seconds,
            visit_seconds, &state, &n, &first, &last, &end));
    if (error || state.error)
        return error ? error : state.error;
    *count = n;
    *before = n ? 0 : (int64_t)first * 1000000000;
    *after = n ? 0 : (int64_t)last * 1000000000;
    if (!n)
        *gap_end = end;
    return 0;
}

int qjs_temporal_zone_possible_epochs(const QJSTemporalZone *zone,
                                      QJSTemporalISODateTime datetime,
                                      QJSTemporalEpochVisit *visit,
                                      void *opaque, size_t *count)
{
    int64_t before, after, gap_end;
    return local_candidates(zone, datetime, visit, opaque, count,
                             &before, &after, &gap_end);
}

typedef struct EpochChoices {
    QJSTemporalEpochNs first, last;
    int found;
} EpochChoices;

static void remember_epoch(void *opaque, QJSTemporalEpochNs epoch)
{
    EpochChoices *choices = opaque;
    if (!choices->found)
        choices->first = epoch;
    choices->last = epoch;
    choices->found = 1;
}

int qjs_temporal_zone_epoch(const QJSTemporalZone *zone,
                            QJSTemporalISODateTime datetime,
                            QJSTemporalDisambiguation disambiguation,
                            QJSTemporalEpochNs *result)
{
    EpochChoices choices = { { 0, 0 }, { 0, 0 }, 0 };
    QJSTemporalEpochNs wall, epoch;
    int64_t before, after, shift, gap_end;
    size_t count;
    int error;

    if (disambiguation < QJS_TEMPORAL_COMPATIBLE ||
        disambiguation > QJS_TEMPORAL_DISAMBIGUATION_REJECT)
        return QJS_TEMPORAL_ERROR_RANGE;
    error = local_candidates(zone, datetime, remember_epoch, &choices,
                             &count, &before, &after, &gap_end);
    if (error)
        return error;
    if (count) {
        if (count > 1 && disambiguation == QJS_TEMPORAL_DISAMBIGUATION_REJECT)
            return QJS_TEMPORAL_ERROR_RANGE;
        *result = disambiguation == QJS_TEMPORAL_LATER ? choices.last : choices.first;
        return 0;
    }
    if (disambiguation == QJS_TEMPORAL_DISAMBIGUATION_REJECT ||
        qjs_temporal_iso_datetime_to_epoch_ns(&wall, datetime))
        return QJS_TEMPORAL_ERROR_RANGE;
    /* Resolve again after moving the wall time by the gap. Subtracting
       one boundary offset directly would miss nearby additional changes. */
    shift = after - before;
    if (disambiguation == QJS_TEMPORAL_EARLIER)
        shift = -shift;
    if (qjs_temporal_epoch_ns_add(&epoch, wall,
            qjs_temporal_epoch_ns_from_int64(shift)) ||
        qjs_temporal_iso_datetime_from_epoch_ns(&datetime, epoch))
        return QJS_TEMPORAL_ERROR_RANGE;
    choices.found = 0;
    error = local_candidates(zone, datetime, remember_epoch, &choices,
                             &count, &before, &after, &gap_end);
    if (error)
        return error;
    if (!count)
        return QJS_TEMPORAL_ERROR_BACKEND;
    *result = disambiguation == QJS_TEMPORAL_EARLIER ? choices.first : choices.last;
    return 0;
}

int qjs_temporal_zone_start_of_day(const QJSTemporalZone *zone,
                                   QJSTemporalISODate date,
                                   QJSTemporalEpochNs *result)
{
    QJSTemporalISODateTime midnight = { date, { 0 } };
    EpochChoices choices = { { 0, 0 }, { 0, 0 }, 0 };
    int64_t before, after, gap_end;
    size_t count;
    int error;

    error = local_candidates(zone, midnight, remember_epoch, &choices,
                             &count, &before, &after, &gap_end);
    if (error)
        return error;
    if (count) {
        *result = choices.first;
        return 0;
    }
    return epoch_from_seconds(result, gap_end, 0);
}

int qjs_temporal_zone_transition(const QJSTemporalZone *zone,
                                 QJSTemporalEpochNs epoch, int next,
                                 QJSTemporalEpochNs *result, int *found)
{
    QJSTemporalEpochNs transition;
    int64_t seconds, when;
    uint64_t fraction;
    int exists, error;

    if (!qjs_temporal_epoch_ns_is_valid(epoch))
        return QJS_TEMPORAL_ERROR_RANGE;
    if (zone->is_offset) {
        *found = 0;
        return 0;
    }
    error = epoch_seconds(epoch, &seconds, &fraction);
    if (!error)
        error = zone_error(qjs_timezone_transition(zone->named, seconds, next,
                            !next && fraction != 0, &when, &exists));
    if (error)
        return error;
    if (exists) {
        error = epoch_from_seconds(&transition, when, 0);
        if (error) {
            *found = 0;
            return 0;
        }
        *result = transition;
    }
    *found = exists;
    return 0;
}

int qjs_temporal_zones_equal(const QJSTemporalZone *a,
                             const QJSTemporalZone *b, int *result)
{
    if (a->is_offset || b->is_offset)
        *result = a->is_offset && b->is_offset &&
                  a->offset_nanoseconds == b->offset_nanoseconds;
    else
        *result = !strcmp(qjs_timezone_primary_identifier(a->named),
                          qjs_timezone_primary_identifier(b->named));
    return 0;
}
