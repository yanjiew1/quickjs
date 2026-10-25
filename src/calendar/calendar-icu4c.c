/* ICU4C calendar arithmetic behind the common ISO/calendar facade.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
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
#include "calendar-backend.h"
#include "data.h"
#include "eras.h"
#include "iso.h"
#include "lunisolar.h"
#include "astronomy.h"
#include "arithmetic-internal.h"
#include "../icu4c/once.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unicode/ucal.h>

/* Ordinals, nominal month codes and leap flags are distinct. In particular,
   Hebrew skips Adar I in ordinary years; Chinese/Dangi repeat a nominal month.
   A bounded year record keeps those ICU field details out of the facade. */
typedef struct ICUCalendarYear {
    int32_t year;
    int months;
    int64_t starts[14];
    int nominal[13], leap[13];
} ICUCalendarYear;

static int calendar_error(UErrorCode status)
{
    if (status == U_MEMORY_ALLOCATION_ERROR)
        return QJS_CAL_MEMORY;
    if (status == U_ILLEGAL_ARGUMENT_ERROR)
        return QJS_CAL_RANGE;
    return QJS_CAL_BACKEND;
}

static int lunar(QJSCalendarId id)
{
    return id == QJS_CAL_CHINESE || id == QJS_CAL_DANGI;
}

static int year_offset(QJSCalendarId id, int *out);

static UCalendar *open_calendar(QJSCalendarId id, UErrorCode *status)
{
    const char *type = qjs_calendar_identifier(id);
    char locale[80];
    UCalendar *cal;
    static const UChar utc[] = {'U', 'T', 'C', 0};

    /* ICU's C cutover setter rejects Gregorian subclasses. Their month/day
       arithmetic is Gregorian; labels and eras are adapted separately. */
    if (id == QJS_CAL_GREGORY || id == QJS_CAL_JAPANESE ||
        id == QJS_CAL_BUDDHIST || id == QJS_CAL_ROC)
        type = "gregorian";
    else if (id == QJS_CAL_ETHIOAA)
        type = "ethiopic-amete-alem";
    if (!type || id == QJS_CAL_ISO8601) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    snprintf(locale, sizeof(locale), "en_US_POSIX@calendar=%s", type);
    cal = ucal_open(utc, -1, locale, UCAL_DEFAULT, status);
    if (U_SUCCESS(*status)) {
        const char *actual_type = ucal_getType(cal, status);

        if (U_SUCCESS(*status) && (!actual_type || strcmp(actual_type, type)))
            *status = U_MISSING_RESOURCE_ERROR;
    }
    if (U_SUCCESS(*status) && (id == QJS_CAL_GREGORY || id == QJS_CAL_JAPANESE ||
        id == QJS_CAL_BUDDHIST || id == QJS_CAL_ROC))
        ucal_setGregorianChange(cal, -DBL_MAX, status);
    if (U_SUCCESS(*status))
        /* Strict ICU field limits reject valid Temporal and metadata years.
           Inputs are checked by this adapter, including exact year roundtrips. */
        ucal_setAttribute(cal, UCAL_LENIENT, 1);
    return cal;
}

/* ICU 79 adopted arithmetic extended years for Chinese, Dangi and ethioaa;
   older releases use historical storage epochs. Detect the actual convention
   on a mid-year date, rather than confusing the installed library with the
   headers used for compilation. Each calendar owns its initialization/error. */
typedef struct ICUYearConvention {
    QJSICUOnce once;
    QJSCalendarId id;
    int expected, legacy, offset, error;
} ICUYearConvention;

static ICUYearConvention conventions[] = {
    {QJS_ICU_ONCE_INIT, QJS_CAL_CHINESE, 2024, 2637, 0, 0},
    {QJS_ICU_ONCE_INIT, QJS_CAL_DANGI, 2024, 2333, 0, 0},
    {QJS_ICU_ONCE_INIT, QJS_CAL_ETHIOAA, 7516, -5500, 0, 0},
};

static void initialize_convention(ICUYearConvention *convention)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *cal = open_calendar(convention->id, &status);
    int64_t day, offset;

    if (U_SUCCESS(status) &&
        qjs_iso_date_to_days(&day, (QJSISODate){2024, 6, 1}))
        status = U_INTERNAL_PROGRAM_ERROR;
    if (U_SUCCESS(status)) {
        ucal_setMillis(cal, (UDate)day * 86400000.0, &status);
        offset = (int64_t)ucal_get(cal, UCAL_EXTENDED_YEAR, &status) - convention->expected;
        if (U_SUCCESS(status) && offset != 0 && offset != convention->legacy)
            status = U_INVALID_FORMAT_ERROR;
        if (U_SUCCESS(status))
            convention->offset = (int)offset;
    }
    ucal_close(cal);
    if (U_FAILURE(status))
        convention->error = status == U_MEMORY_ALLOCATION_ERROR ? QJS_CAL_MEMORY : QJS_CAL_BACKEND;
}

static void initialize_chinese(void) { initialize_convention(&conventions[0]); }
static void initialize_dangi(void) { initialize_convention(&conventions[1]); }
static void initialize_ethioaa(void) { initialize_convention(&conventions[2]); }

static int year_offset(QJSCalendarId id, int *out)
{
    static void (*const initialize[])(void) = {
        initialize_chinese, initialize_dangi, initialize_ethioaa,
    };
    int index;

    switch (id) {
    case QJS_CAL_BUDDHIST: *out = -543; return 0;
    case QJS_CAL_ROC: *out = 1911; return 0;
    case QJS_CAL_CHINESE: index = 0; break;
    case QJS_CAL_DANGI: index = 1; break;
    case QJS_CAL_ETHIOAA: index = 2; break;
    default: *out = 0; return 0;
    }
    qjs_icu4c_once(&conventions[index].once, initialize[index]);
    if (conventions[index].error)
        return conventions[index].error;
    *out = conventions[index].offset;
    return 0;
}

static int arithmetic_year(UCalendar *cal, QJSCalendarId id, int32_t *out)
{
    UErrorCode status = U_ZERO_ERROR;
    int offset, error = year_offset(id, &offset);
    int64_t year;

    if (error)
        return error;
    year = (int64_t)ucal_get(cal, UCAL_EXTENDED_YEAR, &status) - offset;
    if (U_FAILURE(status))
        return calendar_error(status);
    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    *out = (int32_t)year;
    return 0;
}

static int epoch_day(UCalendar *cal, int64_t *out)
{
    UErrorCode status = U_ZERO_ERROR;
    UDate millis = ucal_getMillis(cal, &status);
    double day = millis / 86400000.0;

    if (U_FAILURE(status))
        return calendar_error(status);
    if (!isfinite(day) || day < -2147483648.0 || day > 2147483647.0 || floor(day) != day)
        return QJS_CAL_RANGE;
    *out = (int64_t)day;
    return 0;
}

static int read_year(UCalendar *cal, QJSCalendarId id, int32_t year,
                       ICUCalendarYear *out)
{
    UErrorCode status = U_ZERO_ERROR;
    ICUCalendarYear value = {0};
    int32_t actual_year;
    int error, field, offset, storage_year = year;
    int64_t cycles = 0;

    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    error = year_offset(id, &offset);
    if (error)
        return error;
    /* ICU's Indian month-start implementation truncates fractional negative
       Julian days toward zero. Move its calculation into a positive Gregorian
       cycle, then translate the result by the exact 400-year period. This is
       still ICU arithmetic, with no alternate calendar/data fallback. */
    if (id == QJS_CAL_INDIAN) {
        cycles = qjs_cal_floor_divide((int64_t)year - 2000, 400);
        storage_year = (int32_t)(year - cycles * 400);
    }
    ucal_clear(cal);
    ucal_set(cal, UCAL_EXTENDED_YEAR, storage_year + offset);
    ucal_set(cal, UCAL_ORDINAL_MONTH, 0);
    ucal_set(cal, UCAL_DATE, 1);
    error = epoch_day(cal, &value.starts[0]);
    if (error)
        return error;
    error = arithmetic_year(cal, id, &actual_year);
    if (error || actual_year != storage_year)
        return error ? error : QJS_CAL_RANGE;
    value.year = year;
    for (;;) {
        int index = value.months;

        if (index == 13)
            return QJS_CAL_BACKEND;
        field = ucal_get(cal, UCAL_MONTH, &status);
        value.nominal[index] = field + 1;
        value.leap[index] = ucal_get(cal, UCAL_IS_LEAP_MONTH, &status) != 0;
        if (id == QJS_CAL_HEBREW && field >= 5) {
            value.nominal[index] = field;
            value.leap[index] = field == 5;
        }
        ucal_add(cal, UCAL_MONTH, 1, &status);
        if (U_FAILURE(status))
            return calendar_error(status);
        error = epoch_day(cal, &value.starts[index + 1]);
        if (error)
            return error;
        if (value.starts[index + 1] <= value.starts[index] ||
            value.starts[index + 1] - value.starts[index] > 32)
            return QJS_CAL_BACKEND;
        value.months++;
        int64_t next_year = (int64_t)ucal_get(cal, UCAL_EXTENDED_YEAR, &status) - offset;
        if (U_FAILURE(status))
            return calendar_error(status);
        if (next_year != storage_year) {
            if (next_year != storage_year + 1)
                return QJS_CAL_BACKEND;
            break;
        }
    }
    if (value.months < 12)
        return QJS_CAL_BACKEND;
    if (lunar(id)) {
        QJSISODate first, last;
        int expected = 1, leap_count = 0, i;

        if (qjs_iso_date_from_days(&first, value.starts[0]) ||
            qjs_iso_date_from_days(&last, value.starts[value.months]) ||
            first.year != year || last.year != year + 1)
            return QJS_CAL_BACKEND;
        for (i = 0; i < value.months; i++) {
            int64_t days = value.starts[i + 1] - value.starts[i];

            if (days != 29 && days != 30)
                return QJS_CAL_BACKEND;
            if (value.leap[i]) {
                if (!i || value.nominal[i] != expected - 1 || ++leap_count > 1)
                    return QJS_CAL_BACKEND;
            } else if (value.nominal[i] != expected++) {
                return QJS_CAL_BACKEND;
            }
        }
        if (expected != 13 || value.months != 12 + leap_count)
            return QJS_CAL_BACKEND;
    }
    if (cycles) {
        int i;

        for (i = 0; i <= value.months; i++)
            value.starts[i] += cycles * 146097;
    }
    *out = value;
    return 0;
}

static int unpack_year(int32_t year, const QJSCalendarYear *packed, ICUCalendarYear *out)
{
    ICUCalendarYear value = {0};
    int i;

    value.year = year;
    value.months = packed->months;
    value.starts[0] = packed->first_day;
    for (i = 0; i < value.months; i++) {
        int ordinal = i + 1;

        value.nominal[i] = ordinal;
        if (packed->leap_month && ordinal > packed->leap_month)
            value.nominal[i]--;
        value.leap[i] = packed->leap_month && ordinal == packed->leap_month + 1;
        value.starts[i + 1] = value.starts[i] + 29 +
            ((packed->month_lengths >> (2 * i)) & 3);
    }
    if (value.starts[value.months] != packed->end_day)
        return QJS_CAL_BACKEND;
    *out = value;
    return 0;
}

static int approximate_year(QJSCalendarId id, int32_t year, ICUCalendarYear *out)
{
    QJSCalendarYear packed;
    int error = qjs_cal_lunisolar_approximate_year(id, year, &packed);

    return error ? error : unpack_year(year, &packed, out);
}

static int raw_year(QJSCalendarId id, int32_t year, ICUCalendarYear *out)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *cal = open_calendar(id, &status);
    int error;

    if (U_FAILURE(status)) {
        ucal_close(cal);
        return calendar_error(status);
    }
    error = read_year(cal, id, year, out);
    ucal_close(cal);
    return error;
}

/* A partial distant ICU year cannot be spliced into an independently chosen
   approximation: even adjacent successful queries can then overlap or leave
   gaps. Keep a complete ICU interval and join the common model at boundaries
   where both providers agree exactly. The anchors are well outside required
   PMO/KASI data intervals and track the shared model's mean-year extension. */
typedef struct ICULunarDomain {
    QJSICUOnce once;
    QJSCalendarId id;
    int32_t first, limit;
    int error;
} ICULunarDomain;

#define LUNAR_ANCHOR_SEARCH 32
static ICULunarDomain lunar_domains[] = {
    {QJS_ICU_ONCE_INIT, QJS_CAL_CHINESE, 0, 0, 0},
    {QJS_ICU_ONCE_INIT, QJS_CAL_DANGI, 0, 0, 0},
};

static void initialize_lunar_domain(ICULunarDomain *domain)
{
    ICUCalendarYear before, after, approximate;
    int side, attempt, error = QJS_CAL_BACKEND;

    for (side = 0; side < 2; side++) {
        for (attempt = 0; attempt < LUNAR_ANCHOR_SEARCH; attempt++) {
            int32_t boundary = side ? QJS_CAL_ASTRO_LINEAR_MAX_YEAR - attempt :
                                      QJS_CAL_ASTRO_LINEAR_MIN_YEAR + attempt;

            error = raw_year(domain->id, boundary - 1, &before);
            if (!error)
                error = raw_year(domain->id, boundary, &after);
            if (error) {
                domain->error = error;
                return;
            }
            error = approximate_year(domain->id, boundary, &approximate);
            if (error) {
                domain->error = error;
                return;
            }
            if (before.starts[before.months] == after.starts[0] &&
                after.starts[0] == approximate.starts[0]) {
                if (side)
                    domain->limit = boundary;
                else
                    domain->first = boundary;
                break;
            }
        }
        if (attempt == LUNAR_ANCHOR_SEARCH) {
            domain->error = QJS_CAL_BACKEND;
            return;
        }
    }
}

static void initialize_chinese_domain(void) { initialize_lunar_domain(&lunar_domains[0]); }
static void initialize_dangi_domain(void) { initialize_lunar_domain(&lunar_domains[1]); }

static int calendar_year(QJSCalendarId id, int32_t year, ICUCalendarYear *out)
{
    QJSCalendarYear official;
    int error;

    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    /* ICU's astronomical result can disagree with mandatory PMO/KASI records
       (for example Chinese 1987's leap month). This explicitly approved data
       exception is shared with formatting; all other retained years use ICU. */
    error = qjs_calendar_official_year(id, year, &official);
    if (error != QJS_CAL_UNSUPPORTED)
        return error ? error : unpack_year(year, &official, out);
    if (lunar(id) && (year < QJS_CAL_ASTRO_LINEAR_MIN_YEAR + LUNAR_ANCHOR_SEARCH ||
                     year >= QJS_CAL_ASTRO_LINEAR_MAX_YEAR - LUNAR_ANCHOR_SEARCH)) {
        int index = id == QJS_CAL_DANGI;
        ICULunarDomain *domain = &lunar_domains[index];

        qjs_icu4c_once(&domain->once,
                       index ? initialize_dangi_domain : initialize_chinese_domain);
        if (domain->error)
            return domain->error;
        if (year < domain->first || year >= domain->limit)
            return approximate_year(id, year, out);
    }
    /* Allocation/resource failures in ICU's retained interval remain visible. */
    return raw_year(id, year, out);
}

static int lunar_year_at(QJSCalendarId id, int64_t day, ICUCalendarYear *out)
{
    QJSISODate iso;
    ICUCalendarYear value;
    int32_t year;
    int error;

    if (qjs_iso_date_from_days(&iso, day) || iso.year < QJS_CAL_MIN_YEAR ||
        iso.year > QJS_CAL_MAX_YEAR + 1)
        return QJS_CAL_RANGE;
    year = iso.year > QJS_CAL_MAX_YEAR ? QJS_CAL_MAX_YEAR : iso.year;
    error = calendar_year(id, year, &value);
    if (error)
        return error;
    if (day < value.starts[0]) {
        if (year == QJS_CAL_MIN_YEAR)
            return QJS_CAL_RANGE;
        error = calendar_year(id, year - 1, &value);
        if (error)
            return error;
    }
    if (day < value.starts[0] || day >= value.starts[value.months])
        return QJS_CAL_RANGE;
    *out = value;
    return 0;
}

static int lunar_year_record(QJSCalendarId id, int32_t year, QJSCalendarYearLayout *out)
{
    ICUCalendarYear value;
    QJSCalendarYearLayout result = {0};
    int i, error = calendar_year(id, year, &value);

    if (error)
        return error;
    result.first_day = value.starts[0];
    result.end_day = value.starts[value.months];
    result.months = value.months;
    for (i = 0; i < value.months; i++) {
        int64_t length = value.starts[i + 1] - value.starts[i];

        if (length != 29 && length != 30)
            return QJS_CAL_BACKEND;
        result.lengths[i] = (unsigned char)length;
        if (value.leap[i])
            result.leap_month = value.nominal[i];
    }
    *out = result;
    return 0;
}

static void month_code(const ICUCalendarYear *year, int month, char out[5])
{
    int nominal = year->nominal[month - 1];

    out[0] = 'M';
    out[1] = '0' + nominal / 10;
    out[2] = '0' + nominal % 10;
    out[3] = year->leap[month - 1] ? 'L' : 0;
    out[4] = 0;
}

int qjs_calendar_backend_is_supported(QJSCalendarId id)
{
    return id > QJS_CAL_ISO8601 && id < QJS_CAL_COUNT;
}

int qjs_calendar_backend_era_at(QJSCalendarId id, size_t index, const char **out)
{
    return qjs_cal_era_at(id, index, out);
}

int qjs_calendar_backend_month_info(QJSCalendarId id, int32_t year, int month,
                                    int *months, int *days, char code[5])
{
    ICUCalendarYear record;
    int error = calendar_year(id, year, &record);

    if (error)
        return error;
    if (month < 1 || month > record.months)
        return QJS_CAL_RANGE;
    *months = record.months;
    *days = (int)(record.starts[month] - record.starts[month - 1]);
    month_code(&record, month, code);
    return 0;
}

int qjs_calendar_backend_to_epoch_day(QJSCalendarId id, int32_t year,
                                      int month, int day, int64_t *out)
{
    ICUCalendarYear record;
    int error = calendar_year(id, year, &record);

    if (error)
        return error;
    if (month < 1 || month > record.months || day < 1 ||
        day > record.starts[month] - record.starts[month - 1])
        return QJS_CAL_RANGE;
    *out = record.starts[month - 1] + day - 1;
    return 0;
}

int qjs_calendar_backend_from_epoch_day(QJSCalendarId id, int64_t day,
                                        QJSCalendarDate *out)
{
    UErrorCode status = U_ZERO_ERROR;
    ICUCalendarYear record;
    QJSCalendarDate value = {0};
    int error, month;

    if (lunar(id)) {
        /* Do not ask ICU to decode a day after it has rejected that year's
           metadata. ISO-year bracketing also keeps approximate years coherent. */
        error = lunar_year_at(id, day, &record);
        if (!error)
            value.year = record.year;
    } else {
        UCalendar *cal = open_calendar(id, &status);
        int64_t storage_day = day, cycles = 0;
        double millis;

        if (id == QJS_CAL_INDIAN) {
            QJSISODate iso;

            if (qjs_iso_date_from_days(&iso, day)) {
                ucal_close(cal);
                return QJS_CAL_RANGE;
            }
            cycles = qjs_cal_floor_divide((int64_t)iso.year - 2000, 400);
            storage_day -= cycles * 146097;
        }
        millis = (double)storage_day * 86400000.0;

        if (U_FAILURE(status)) {
            ucal_close(cal);
            return calendar_error(status);
        }
        ucal_setMillis(cal, millis, &status);
        if (U_SUCCESS(status) && ucal_getMillis(cal, &status) != millis)
            status = U_ILLEGAL_ARGUMENT_ERROR; /* ICU can clamp extreme inputs. */
        error = U_FAILURE(status) ? calendar_error(status) : arithmetic_year(cal, id, &value.year);
        if (!error && id == QJS_CAL_INDIAN) {
            int64_t year = value.year + cycles * 400;

            if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
                error = QJS_CAL_RANGE;
            else
                value.year = (int32_t)year;
        }
        if (!error)
            error = read_year(cal, id, value.year, &record);
        ucal_close(cal);
    }
    if (error)
        return error;
    if (day < record.starts[0] || day >= record.starts[record.months])
        return QJS_CAL_RANGE;
    for (month = 1; day >= record.starts[month]; month++)
        ;
    value.month = month;
    value.day = (int32_t)(day - record.starts[month - 1] + 1);
    value.day_of_year = (int32_t)(day - record.starts[0] + 1);
    value.days_in_month = (int32_t)(record.starts[month] - record.starts[month - 1]);
    value.days_in_year = (int32_t)(record.starts[record.months] - record.starts[0]);
    value.months_in_year = record.months;
    value.in_leap_year = lunar(id) || id == QJS_CAL_HEBREW ? record.months == 13 :
        id == QJS_CAL_ISLAMIC_CIVIL || id == QJS_CAL_ISLAMIC_TBLA ||
        id == QJS_CAL_ISLAMIC_UMALQURA ? value.days_in_year > 354 : value.days_in_year > 365;
    month_code(&record, month, value.month_code);
    error = qjs_cal_set_era(id, day, &value);
    if (error)
        return error;
    *out = value;
    return 0;
}

int qjs_calendar_backend_year_from_era(QJSCalendarId id, const char *era,
                                       int32_t era_year, int32_t *out)
{
    return qjs_cal_year_from_era(id, era, era_year, out);
}

int qjs_calendar_backend_month_ordinal(QJSCalendarId id, int32_t year,
                                       int nominal, int leap, int constrain, int *out)
{
    ICUCalendarYear record;
    int error, month;
    int maximum = id == QJS_CAL_COPTIC || id == QJS_CAL_ETHIOPIC || id == QJS_CAL_ETHIOAA ? 13 : 12;

    if (nominal < 1 || nominal > maximum || (leap != 0 && leap != 1) ||
        (constrain != 0 && constrain != 1) ||
        (leap && !lunar(id) && !(id == QJS_CAL_HEBREW && nominal == 5)))
        return QJS_CAL_RANGE;
    error = calendar_year(id, year, &record);
    if (error)
        return error;
    for (month = 0; month < record.months; month++) {
        if (record.nominal[month] == nominal && record.leap[month] == leap) {
            *out = month + 1;
            return 0;
        }
    }
    if (!leap || !constrain)
        return QJS_CAL_RANGE;
    if (id == QJS_CAL_HEBREW)
        nominal = 6; /* Missing M05L constrains to Adar, M06. */
    for (month = 0; month < record.months; month++) {
        if (record.nominal[month] == nominal && !record.leap[month]) {
            *out = month + 1;
            return 0;
        }
    }
    return QJS_CAL_RANGE;
}

int qjs_calendar_backend_add_months(QJSCalendarId id, int32_t year,
                                    int month, int64_t delta,
                                    int32_t *out_year, int *out_month)
{
    UErrorCode status = U_ZERO_ERROR;
    ICUCalendarYear record;
    UCalendar *cal;
    int32_t target_year;
    int target_month, error;
    int64_t target_day, cycles = 0, storage_day, storage_delta = delta;

    if (lunar(id))
        return qjs_cal_lunisolar_add_months(id, year, month, delta,
                                            lunar_year_record, out_year, out_month);
    error = calendar_year(id, year, &record);

    if (error)
        return error;
    if (month < 1 || month > record.months || delta < -26000026 || delta > 26000026)
        return QJS_CAL_RANGE;
    storage_day = record.starts[month - 1];
    if (id == QJS_CAL_INDIAN) {
        int64_t delta_cycles = storage_delta / 4800;

        cycles = qjs_cal_floor_divide((int64_t)year - 2000, 400);
        storage_day -= cycles * 146097;
        storage_delta -= delta_cycles * 4800;
        cycles += delta_cycles;
    }
    cal = open_calendar(id, &status);
    if (U_SUCCESS(status)) {
        ucal_setMillis(cal, (UDate)storage_day * 86400000.0, &status);
        ucal_add(cal, UCAL_MONTH, (int32_t)storage_delta, &status);
    }
    error = U_FAILURE(status) ? calendar_error(status) : arithmetic_year(cal, id, &target_year);
    target_month = !error ? ucal_get(cal, UCAL_ORDINAL_MONTH, &status) + 1 : 0;
    if (!error)
        error = epoch_day(cal, &target_day);
    if (!error && id == QJS_CAL_INDIAN) {
        int64_t year = target_year + cycles * 400;

        if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
            error = QJS_CAL_RANGE;
        else {
            target_year = (int32_t)year;
            target_day += cycles * 146097;
        }
    }
    if (!error)
        error = read_year(cal, id, target_year, &record);
    ucal_close(cal);
    if (error)
        return error;
    if (U_FAILURE(status) || target_month < 1 || target_month > record.months ||
        record.starts[target_month - 1] != target_day)
        return QJS_CAL_RANGE;
    *out_year = target_year;
    *out_month = target_month;
    return 0;
}
