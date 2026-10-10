/* Plain C shared calendar conversion and month arithmetic.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 * Era/MonthCode proposal 5833eae6c9079ffbb06a9ab7cc5128ebbf6e9ac7:
 * table-calendar-types, table-calendar-era-definitions, NonISOCalendarISOToDate,
 * ConstrainMonthCode and NonISOCalendarDateToISO.
 */
#include "internal.h"
#if defined(QJS_CAL_PERSIAN_AUTHORITY_VERIFIED) && !defined(QJS_CAL_USE_PERSIAN_AUTHORITY_TABLE)
#error Persian discovery requires the acquired authority table.
#endif
#if defined(QJS_CAL_DANGI_AUTHORITY_VERIFIED) && (!defined(QJS_CAL_ENABLE_LUNISOLAR_CANDIDATE) || !defined(QJS_CAL_HAVE_PUBLISHED_LUNISOLAR_DATA))
#error Dangi discovery requires the integrated primary provider and conversion seam.
#endif
#if defined(QJS_CAL_CHINESE_AUTHORITY_VERIFIED) && (!defined(QJS_CAL_ENABLE_LUNISOLAR_CANDIDATE) || !defined(QJS_CAL_HAVE_PUBLISHED_LUNISOLAR_DATA))
#error Chinese discovery requires the verified PMO provider and conversion seam.
#endif
#if defined(QJS_CAL_CHINESE_AUTHORITY_VERIFIED) && defined(QJS_CAL_LUNISOLAR_APPROXIMATION_REVIEW)
#error Chinese discovery cannot use the approximation review profile.
#endif
#include <limits.h>
#include <string.h>
#ifdef QJS_CAL_ENABLE_LUNISOLAR_CANDIDATE
#include "lunisolar.h"
#endif

static const char *const identifiers[QJS_CAL_COUNT] = {
    "iso8601","buddhist","chinese","coptic","dangi","ethioaa","ethiopic",
    "gregory","hebrew","indian","islamic-civil","islamic-tbla",
    "islamic-umalqura","japanese","persian","roc"
};
static int valid_calendar(QJSCalendarId calendar)
{
    return (unsigned)calendar < QJS_CAL_COUNT;
}
static int lunisolar_calendar(QJSCalendarId calendar)
{
    return calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI;
}
static int islamic_calendar(QJSCalendarId calendar)
{
    return calendar >= QJS_CAL_ISLAMIC_CIVIL && calendar <= QJS_CAL_ISLAMIC_UMALQURA;
}
const char *qjs_calendar_identifier(QJSCalendarId calendar)
{
    return valid_calendar(calendar) ? identifiers[calendar] : NULL;
}
int qjs_calendar_is_supported(QJSCalendarId calendar)
{
    /* Candidate conversion is callable for root validation without exposing
     * unproved calendar services through Intl/Temporal discovery. */
    return valid_calendar(calendar) &&
#ifndef QJS_CAL_CHINESE_AUTHORITY_VERIFIED
           calendar != QJS_CAL_CHINESE &&
#endif
#ifndef QJS_CAL_DANGI_AUTHORITY_VERIFIED
           calendar != QJS_CAL_DANGI &&
#endif
#ifndef QJS_CAL_PERSIAN_AUTHORITY_VERIFIED
           calendar != QJS_CAL_PERSIAN &&
#endif
           1;
}
static int identifier_equal(const char *text, size_t length, const char *name)
{
    size_t i;
    if (strlen(name) != length) return 0;
    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != (unsigned char)name[i]) return 0;
    }
    return 1;
}
int qjs_calendar_from_identifier(QJSCalendarId *result,
                                const char *text, size_t length)
{
    int i;
    QJSCalendarId calendar;
    if (!result || !text) return QJS_CAL_RANGE;
    if (identifier_equal(text, length, "ethiopic-amete-alem")) calendar = QJS_CAL_ETHIOAA;
    else if (identifier_equal(text, length, "islamicc")) calendar = QJS_CAL_ISLAMIC_CIVIL;
    else {
        for (i = 0; i < QJS_CAL_COUNT; i++)
            if (identifier_equal(text, length, identifiers[i])) break;
        if (i == QJS_CAL_COUNT) return QJS_CAL_RANGE;
        calendar = (QJSCalendarId)i;
    }
    if (!qjs_calendar_is_supported(calendar)) return QJS_CAL_UNSUPPORTED;
    *result = calendar;
    return QJS_CAL_OK;
}
static int64_t year_start(QJSCalendarId calendar, int32_t year)
{
    if (calendar == QJS_CAL_HEBREW) return qjs_cal_hebrew_year_start(year);
    if (islamic_calendar(calendar)) return qjs_cal_islamic_year_start(calendar, year);
    return qjs_cal_solar_year_start(calendar, year);
}
static int months_in_year(QJSCalendarId calendar, int32_t year)
{
    if (calendar == QJS_CAL_HEBREW) return 12 + qjs_cal_hebrew_leap(year);
    if (calendar == QJS_CAL_COPTIC || calendar == QJS_CAL_ETHIOAA ||
        calendar == QJS_CAL_ETHIOPIC) return 13;
    return 12;
}
static int month_length(QJSCalendarId calendar, int32_t year, int month)
{
    if (calendar == QJS_CAL_HEBREW) return qjs_cal_hebrew_month_length(year, month);
    if (islamic_calendar(calendar)) return qjs_cal_islamic_month_length(calendar, year, month);
    return qjs_cal_solar_month_length(calendar, year, month);
}
static void make_month_code(QJSCalendarId calendar, int32_t year, int month,
                           char code[5])
{
    int number = month, leap = 0;
    if (calendar == QJS_CAL_HEBREW && qjs_cal_hebrew_leap(year) && month >= 6) {
        number = month - 1;
        leap = month == 6;
    }
    code[0] = 'M'; code[1] = (char)('0' + number / 10);
    code[2] = (char)('0' + number % 10); code[3] = leap ? 'L' : 0;
    code[4] = 0;
}
int qjs_calendar_month_info(QJSCalendarId calendar, int32_t year, int month,
                           int *months, int *days, char month_code[5])
{
    int count, length;
    char code[5];
    if (!valid_calendar(calendar) || !months || !days || !month_code ||
        year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR) return QJS_CAL_RANGE;
    if (lunisolar_calendar(calendar)) {
#ifdef QJS_CAL_ENABLE_LUNISOLAR_CANDIDATE
        return qjs_calendar_lunisolar_month_info(calendar, year, month,
                                                months, days, month_code);
#else
        return QJS_CAL_UNSUPPORTED;
#endif
    }
    count = months_in_year(calendar, year);
    if (month < 1 || month > count) return QJS_CAL_RANGE;
    length = month_length(calendar, year, month);
    if (length < 5 || length > 31) return QJS_CAL_BACKEND;
    make_month_code(calendar, year, month, code);
    *months = count; *days = length; memcpy(month_code, code, sizeof(code));
    return QJS_CAL_OK;
}
int qjs_cal_to_epoch_day_unbounded(QJSCalendarId calendar, int32_t year,
                                  int month, int day, int64_t *result)
{
    int count, length, i, error;
    int64_t epoch;
    char code[5];
    if (!result) return QJS_CAL_RANGE;
    if (valid_calendar(calendar) && lunisolar_calendar(calendar)) {
#ifdef QJS_CAL_ENABLE_LUNISOLAR_CANDIDATE
        return qjs_calendar_lunisolar_to_epoch_day_unbounded(calendar, year, month, day, result);
#else
        return QJS_CAL_UNSUPPORTED;
#endif
    }
    error = qjs_calendar_month_info(calendar, year, month, &count, &length, code);
    if (error) return error;
    if (day < 1 || day > length) return QJS_CAL_RANGE;
    epoch = year_start(calendar, year) + day - 1;
    for (i = 1; i < month; i++) epoch += month_length(calendar, year, i);
    *result = epoch;
    return QJS_CAL_OK;
}
int qjs_calendar_to_epoch_day_unbounded(QJSCalendarId calendar, int32_t year,
                                       int month, int day, int64_t *result)
{
    return qjs_cal_to_epoch_day_unbounded(calendar, year, month, day, result);
}
int qjs_calendar_to_epoch_day(QJSCalendarId calendar, int32_t year,
                             int month, int day, int64_t *result)
{
    int64_t epoch;
    int error;
    if (!result) return QJS_CAL_RANGE;
    error = qjs_cal_to_epoch_day_unbounded(calendar, year, month, day, &epoch);
    if (error) return error;
    if (epoch < QJS_CAL_MIN_EPOCH_DAY || epoch > QJS_CAL_MAX_EPOCH_DAY)
        return QJS_CAL_RANGE;
    *result = epoch;
    return QJS_CAL_OK;
}
static void set_era(QJSCalendarId calendar, int64_t epoch, QJSCalendarDate *date)
{
    const char *name = NULL;
    int32_t year = date->year, era_year = year;
    switch (calendar) {
    case QJS_CAL_BUDDHIST: name = "be"; break;
    case QJS_CAL_COPTIC: name = "am"; break;
    case QJS_CAL_ETHIOAA: name = "aa"; break;
    case QJS_CAL_ETHIOPIC:
        name = year > 0 ? "am" : "aa";
        if (year <= 0) era_year += 5500;
        break;
    case QJS_CAL_GREGORY:
        name = year > 0 ? "ce" : "bce";
        if (year <= 0) era_year = 1 - year;
        break;
    case QJS_CAL_HEBREW: name = "am"; break;
    case QJS_CAL_INDIAN: name = "shaka"; break;
    case QJS_CAL_ISLAMIC_CIVIL: case QJS_CAL_ISLAMIC_TBLA: case QJS_CAL_ISLAMIC_UMALQURA:
        name = year > 0 ? "ah" : "bh";
        if (year <= 0) era_year = 1 - year;
        break;
    case QJS_CAL_PERSIAN: name = "ap"; break;
    case QJS_CAL_ROC:
        name = year > 0 ? "roc" : "broc";
        if (year <= 0) era_year = 1 - year;
        break;
    case QJS_CAL_JAPANESE:
        if (year < 1873) {
            name = year > 0 ? "ce" : "bce";
            if (year <= 0) era_year = 1 - year;
        } else if (epoch >= qjs_calendar_gregorian_to_epoch_day_unchecked(2019, 5, 1)) {
            name = "reiwa"; era_year = year - 2018;
        } else if (epoch >= qjs_calendar_gregorian_to_epoch_day_unchecked(1989, 1, 8)) {
            name = "heisei"; era_year = year - 1988;
        } else if (epoch >= qjs_calendar_gregorian_to_epoch_day_unchecked(1926, 12, 25)) {
            name = "showa"; era_year = year - 1925;
        } else if (epoch >= qjs_calendar_gregorian_to_epoch_day_unchecked(1912, 7, 30)) {
            name = "taisho"; era_year = year - 1911;
        } else { name = "meiji"; era_year = year - 1867; }
        break;
    default: break;
    }
    date->has_era = name != NULL;
    if (name) { strcpy(date->era, name); date->era_year = era_year; }
}
static int from_epoch_day(QJSCalendarId calendar, int64_t epoch,
                            QJSCalendarDate *result, int for_intl)
{
    QJSCalendarDate date = {0};
    int32_t low = QJS_CAL_MIN_YEAR, high = QJS_CAL_MAX_YEAR + 1, year;
    int64_t first, next, remaining;
    int count, month, days, sum = 0;
    int64_t minimum = for_intl ? QJS_CAL_INTL_MIN_EPOCH_DAY : QJS_CAL_MIN_EPOCH_DAY;
    int64_t maximum = for_intl ? QJS_CAL_INTL_MAX_EPOCH_DAY : QJS_CAL_MAX_EPOCH_DAY;
    if (!valid_calendar(calendar) || !result ||
        (for_intl != 2 && (epoch < minimum || epoch > maximum)))
        return QJS_CAL_RANGE;
    if (lunisolar_calendar(calendar)) {
#ifdef QJS_CAL_ENABLE_LUNISOLAR_CANDIDATE
        if (for_intl == 2)
            return qjs_calendar_lunisolar_from_epoch_day_unbounded(calendar, epoch, result);
        return for_intl ? qjs_calendar_lunisolar_from_epoch_day_for_intl(calendar, epoch, result) :
                          qjs_calendar_lunisolar_from_epoch_day(calendar, epoch, result);
#else
        return QJS_CAL_UNSUPPORTED;
#endif
    }
    /* Find the largest arithmetic year whose first day is <= epoch. The
     * bounded search avoids floating inverse guesses and unbounded repairs. */
    if (year_start(calendar, low) > epoch || year_start(calendar, high) <= epoch)
        return for_intl == 2 ? QJS_CAL_RANGE : QJS_CAL_BACKEND;
    while (high - low > 1) {
        int32_t mid = low + (high - low) / 2;
        if (year_start(calendar, mid) <= epoch) low = mid;
        else high = mid;
    }
    year = low; first = year_start(calendar, year);
    next = year_start(calendar, year + 1);
    if (next - first < 353 || next - first > 385) return QJS_CAL_BACKEND;
    count = months_in_year(calendar, year);
    for (month = 1; month <= count; month++) {
        days = month_length(calendar, year, month);
        if (days < 5 || days > 31) return QJS_CAL_BACKEND;
        sum += days;
    }
    if (sum != next - first) return QJS_CAL_BACKEND;
    remaining = epoch - first;
    for (month = 1; month <= count; month++) {
        days = month_length(calendar, year, month);
        if (remaining < days) break;
        remaining -= days;
    }
    if (month > count || remaining < 0) return QJS_CAL_BACKEND;
    date.year = year; date.month = month; date.day = (int32_t)remaining + 1;
    date.day_of_year = (int32_t)(epoch - first + 1);
    date.days_in_month = days; date.days_in_year = sum; date.months_in_year = count;
    date.in_leap_year = calendar == QJS_CAL_HEBREW ? count == 13 :
                       islamic_calendar(calendar) ? sum > 354 : sum > 365;
    make_month_code(calendar, year, month, date.month_code);
    set_era(calendar, epoch, &date);
    *result = date;
    return QJS_CAL_OK;
}
int qjs_calendar_from_epoch_day(QJSCalendarId calendar, int64_t epoch,
                                QJSCalendarDate *result)
{
    return from_epoch_day(calendar, epoch, result, 0);
}
int qjs_calendar_from_epoch_day_for_intl(QJSCalendarId calendar, int64_t epoch,
                                         QJSCalendarDate *result)
{
    return from_epoch_day(calendar, epoch, result, 1);
}
int qjs_calendar_year_has_supported_date(QJSCalendarId calendar, int32_t year,
                                          int *result)
{
    int64_t first, next;
    int error;
    if (!valid_calendar(calendar) || !result ||
        year < QJS_CAL_MIN_YEAR || year >= QJS_CAL_MAX_YEAR) return QJS_CAL_RANGE;
    if (lunisolar_calendar(calendar)) {
        /* M01 begins within the Gregorian year bearing the arithmetic year
         * number. Exclude remote years before asking the astronomy provider
         * for either year start. This does not assume a fixed lunar epoch. */
        if (qjs_calendar_gregorian_to_epoch_day_unchecked(year, 1, 1) >
                QJS_CAL_MAX_EPOCH_DAY ||
            qjs_calendar_gregorian_to_epoch_day_unchecked(year + 2, 1, 1) <=
                QJS_CAL_MIN_EPOCH_DAY) {
            *result = 0;
            return QJS_CAL_OK;
        }
    }
    error = qjs_cal_to_epoch_day_unbounded(calendar, year, 1, 1, &first);
    if (error) return error;
    error = qjs_cal_to_epoch_day_unbounded(calendar, year + 1, 1, 1, &next);
    if (error) return error;
    *result = first <= QJS_CAL_MAX_EPOCH_DAY && next > QJS_CAL_MIN_EPOCH_DAY;
    return QJS_CAL_OK;
}
int qjs_calendar_from_epoch_day_unbounded(QJSCalendarId calendar,
                                         int64_t epoch, QJSCalendarDate *result)
{
    return from_epoch_day(calendar, epoch, result, 2);
}
int qjs_calendar_year_from_era(QJSCalendarId calendar, const char *era,
                               int32_t era_year, int32_t *result)
{
    int64_t year = era_year;
    if (!valid_calendar(calendar) || !era || !result) return QJS_CAL_RANGE;
    switch (calendar) {
    case QJS_CAL_BUDDHIST: if (strcmp(era, "be")) return QJS_CAL_RANGE; break;
    case QJS_CAL_COPTIC: case QJS_CAL_HEBREW:
        if (strcmp(era, "am")) return QJS_CAL_RANGE;
        break;
    case QJS_CAL_ETHIOAA: if (strcmp(era, "aa")) return QJS_CAL_RANGE; break;
    case QJS_CAL_ETHIOPIC:
        if (!strcmp(era, "aa")) year -= 5500;
        else if (strcmp(era, "am")) return QJS_CAL_RANGE;
        break;
    case QJS_CAL_INDIAN: if (strcmp(era, "shaka")) return QJS_CAL_RANGE; break;
    case QJS_CAL_ISLAMIC_CIVIL: case QJS_CAL_ISLAMIC_TBLA: case QJS_CAL_ISLAMIC_UMALQURA:
        if (!strcmp(era, "bh")) year = 1 - year;
        else if (strcmp(era, "ah")) return QJS_CAL_RANGE;
        break;
    case QJS_CAL_PERSIAN: if (strcmp(era, "ap")) return QJS_CAL_RANGE; break;
    case QJS_CAL_ROC:
        if (!strcmp(era, "broc")) year = 1 - year;
        else if (strcmp(era, "roc")) return QJS_CAL_RANGE;
        break;
    case QJS_CAL_JAPANESE:
        if (!strcmp(era, "reiwa")) { year += 2018; break; }
        if (!strcmp(era, "heisei")) { year += 1988; break; }
        if (!strcmp(era, "showa")) { year += 1925; break; }
        if (!strcmp(era, "taisho")) { year += 1911; break; }
        if (!strcmp(era, "meiji")) { year += 1867; break; }
        /* Japanese dates before 1873 use the Gregorian epoch eras. */
        /* fall through */
    case QJS_CAL_GREGORY:
        if (!strcmp(era, "bce") || !strcmp(era, "bc")) year = 1 - year;
        else if (strcmp(era, "ce") && strcmp(era, "ad")) return QJS_CAL_RANGE;
        break;
    default: return QJS_CAL_RANGE;
    }
    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR) return QJS_CAL_RANGE;
    *result = (int32_t)year;
    return QJS_CAL_OK;
}
int qjs_calendar_month_ordinal(QJSCalendarId calendar, int32_t year,
                              const char *month_code, int constrain, int *result)
{
    int month, count, days, error;
    char code[5], fallback[5];
    size_t length;
    if (!result || !month_code || (constrain != 0 && constrain != 1))
        return QJS_CAL_RANGE;
    length = strlen(month_code);
    if ((length != 3 && length != 4) || month_code[0] != 'M' ||
        month_code[1] < '0' || month_code[1] > '9' ||
        month_code[2] < '0' || month_code[2] > '9' ||
        (length == 4 && month_code[3] != 'L')) return QJS_CAL_RANGE;
    error = qjs_calendar_month_info(calendar, year, 1, &count, &days, code);
    if (error) return error;
    for (month = 1; month <= count; month++) {
        error = qjs_calendar_month_info(calendar, year, month, &count, &days, code);
        if (error) return error;
        if (!strcmp(code, month_code)) { *result = month; return QJS_CAL_OK; }
    }
    if (!constrain || length != 4) return QJS_CAL_RANGE;
    if (calendar == QJS_CAL_HEBREW && !strcmp(month_code, "M05L"))
        strcpy(fallback, "M06");
    else if (lunisolar_calendar(calendar) && month_code[1] <= '1' &&
             ((month_code[1] == '0' && month_code[2] >= '1') ||
              (month_code[1] == '1' && month_code[2] <= '2'))) {
        memcpy(fallback, month_code, 3); fallback[3] = 0;
    } else return QJS_CAL_RANGE;
    return qjs_calendar_month_ordinal(calendar, year, fallback, 0, result);
}
int qjs_calendar_add_months(QJSCalendarId calendar, int32_t year, int month,
                           int64_t delta, int32_t *result_year, int *result_month)
{
    int count, days, error, new_month;
    int32_t low, high, new_year;
    int64_t index, target;
    char code[5];
    if (!result_year || !result_month) return QJS_CAL_RANGE;
    if (valid_calendar(calendar) && lunisolar_calendar(calendar)) {
#ifdef QJS_CAL_ENABLE_LUNISOLAR_CANDIDATE
        return qjs_calendar_lunisolar_add_months(calendar, year, month, delta,
                                                result_year, result_month);
#else
        return QJS_CAL_UNSUPPORTED;
#endif
    }
    error = qjs_calendar_month_info(calendar, year, month, &count, &days, code);
    if (error) return error;
    index = calendar == QJS_CAL_HEBREW ? qjs_cal_hebrew_months_before_year(year) :
                                       (int64_t)year * count;
    index += month - 1;
    if ((delta > 0 && index > INT64_MAX - delta) ||
        (delta < 0 && index < INT64_MIN - delta)) return QJS_CAL_RANGE;
    target = index + delta;
    if (calendar == QJS_CAL_HEBREW) {
        low = QJS_CAL_MIN_YEAR; high = QJS_CAL_MAX_YEAR + 1;
        if (target < qjs_cal_hebrew_months_before_year(low) ||
            target >= qjs_cal_hebrew_months_before_year(high)) return QJS_CAL_RANGE;
        while (high - low > 1) {
            int32_t mid = low + (high - low) / 2;
            if (qjs_cal_hebrew_months_before_year(mid) <= target) low = mid;
            else high = mid;
        }
        new_year = low;
        new_month = (int)(target - qjs_cal_hebrew_months_before_year(low)) + 1;
    } else {
        int64_t y = qjs_cal_floor_div(target, count);
        if (y < QJS_CAL_MIN_YEAR || y > QJS_CAL_MAX_YEAR) return QJS_CAL_RANGE;
        new_year = (int32_t)y; new_month = (int)qjs_cal_floor_mod(target, count) + 1;
    }
    *result_year = new_year; *result_month = new_month;
    return QJS_CAL_OK;
}
