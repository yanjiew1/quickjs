/*
 * Native Temporal support
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#ifndef QUICKJS_TEMPORAL_TYPES_H
#define QUICKJS_TEMPORAL_TYPES_H

#include "epoch.h"
#include "iso.h"

/* Ordered from the largest unit to the smallest unit. */
typedef enum QJSTemporalUnit {
    QJS_TEMPORAL_YEAR,
    QJS_TEMPORAL_MONTH,
    QJS_TEMPORAL_WEEK,
    QJS_TEMPORAL_DAY,
    QJS_TEMPORAL_HOUR,
    QJS_TEMPORAL_MINUTE,
    QJS_TEMPORAL_SECOND,
    QJS_TEMPORAL_MILLISECOND,
    QJS_TEMPORAL_MICROSECOND,
    QJS_TEMPORAL_NANOSECOND,
    QJS_TEMPORAL_UNIT_COUNT,
    QJS_TEMPORAL_UNIT_UNSET,
    QJS_TEMPORAL_UNIT_AUTO,
} QJSTemporalUnit;

typedef enum QJSTemporalCalendar {
    QJS_TEMPORAL_CAL_ISO8601,
    QJS_TEMPORAL_CAL_BUDDHIST,
    QJS_TEMPORAL_CAL_CHINESE,
    QJS_TEMPORAL_CAL_COPTIC,
    QJS_TEMPORAL_CAL_DANGI,
    QJS_TEMPORAL_CAL_ETHIOAA,
    QJS_TEMPORAL_CAL_ETHIOPIC,
    QJS_TEMPORAL_CAL_GREGORY,
    QJS_TEMPORAL_CAL_HEBREW,
    QJS_TEMPORAL_CAL_INDIAN,
    QJS_TEMPORAL_CAL_ISLAMIC_CIVIL,
    QJS_TEMPORAL_CAL_ISLAMIC_TBLA,
    QJS_TEMPORAL_CAL_ISLAMIC_UMALQURA,
    QJS_TEMPORAL_CAL_JAPANESE,
    QJS_TEMPORAL_CAL_PERSIAN,
    QJS_TEMPORAL_CAL_ROC,
    QJS_TEMPORAL_CAL_COUNT,
} QJSTemporalCalendar;

typedef enum QJSTemporalOverflow {
    QJS_TEMPORAL_OVERFLOW_CONSTRAIN,
    QJS_TEMPORAL_OVERFLOW_REJECT,
} QJSTemporalOverflow;

/* Duration slots are integral finite Numbers. Their accepted magnitudes
   exceed int64_t for some time fields, so keep the exact binary64 integer
   and normalize it using checked word arithmetic before arithmetic. */
typedef struct QJSTemporalDuration {
    double fields[QJS_TEMPORAL_UNIT_COUNT];
} QJSTemporalDuration;

typedef struct QJSTemporalDateDuration {
    int64_t years;
    int64_t months;
    int64_t weeks;
    int64_t days;
} QJSTemporalDateDuration;

typedef struct QJSTemporalInternalDuration {
    QJSTemporalDateDuration date;
    QJSTemporalEpochNs time;
} QJSTemporalInternalDuration;

typedef struct QJSTemporalDifferenceSettings {
    QJSTemporalUnit largest_unit;
    QJSTemporalUnit smallest_unit;
    QJSTemporalRoundingMode rounding_mode;
    uint32_t rounding_increment;
} QJSTemporalDifferenceSettings;

/* Formatting precision: -1 is auto, -2 omits seconds, 0..9 is fixed. */
typedef struct QJSTemporalStringPrecision {
    int precision;
    QJSTemporalUnit unit;
    uint32_t increment;
} QJSTemporalStringPrecision;

#endif /* QUICKJS_TEMPORAL_TYPES_H */
