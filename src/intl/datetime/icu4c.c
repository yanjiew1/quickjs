/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "icu4c.h"
#include "../icu4c/numbering.h"
#include "../locale/preferences.h"
#include "../../calendar/calendar.h"
#include <string.h>
#include <unicode/ucal.h>
#include <unicode/udat.h>
#include <unicode/uloc.h>

typedef struct DateLocale {
    QJSIntlBytes calendar; /* Static known canonical calendar identifier. */
    QJSIntlDateHourPreferences hours;
} DateLocale;

struct QJSIntlDateTimeService {
    QJSIntlAllocator allocator;
    QJSICU4CLocales locales;
    QJSICU4CNumbering numbering;
    DateLocale *facts;
    unsigned int calendars;
};

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}

static QJSIntlBytes next_word(QJSIntlBytes list, size_t *at)
{
    size_t start = *at, end;
    while (*at < list.length && list.data[*at] != ' ')
        (*at)++;
    end = *at;
    if (*at < list.length)
        (*at)++;
    return (QJSIntlBytes){list.data + start, end - start};
}

static QJSIntlStatus hour_preferences(const QJSIntlLocaleService *identity, QJSIntlBytes locale,
                                      QJSIntlDateHourPreferences *out)
{
    static const char *const names[] = {"h11", "h12", "h23", "h24"};
    QJSIntlBytes list;
    uint32_t count, i;
    size_t at = 0;
    unsigned int seen = 0;
    QJSIntlStatus status = qjs_intl_locale_hour_cycles(identity, locale, &list, &count);
    if (status)
        return status;
    out->twelve = QJS_INTL_DATE_H12;
    out->twenty_four = QJS_INTL_DATE_H23;
    if (!count)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < count; i++) {
        QJSIntlBytes word = next_word(list, &at);
        unsigned int j, family;
        for (j = 0; j < sizeof(names) / sizeof(*names); j++)
            if (equal(word, bytes(names[j])))
                break;
        if (j == sizeof(names) / sizeof(*names))
            return QJS_INTL_DATA_ERROR;
        if (!i)
            out->preferred = j;
        family = j <= QJS_INTL_DATE_H12 ? 1 : 2;
        if (!(seen & family)) {
            if (family == 1)
                out->twelve = j;
            else
                out->twenty_four = j;
            seen |= family;
        }
    }
    return at == list.length ? QJS_INTL_OK : QJS_INTL_DATA_ERROR;
}

static QJSIntlBytes known_calendar(const char *legacy)
{
    const char *canonical = legacy ? uloc_toUnicodeLocaleType("calendar", legacy) : NULL;
    unsigned int i;

    if (!canonical)
        return (QJSIntlBytes){0};
    /* The proposal resolves the generic Islamic value only after negotiation. */
    if (!strcmp(canonical, "islamic"))
        canonical = "islamic-tbla";
    for (i = 0; i < QJS_CAL_COUNT; i++) {
        const char *name = qjs_calendar_identifier(i);

        if (!strcmp(name, canonical))
            return bytes(name);
    }
    return (QJSIntlBytes){0};
}

void qjs_intl_datetime_close(QJSIntlDateTimeService *service)
{
    if (service) {
        if (service->facts)
            service->allocator.free(service->allocator.opaque, service->facts);
        qjs_icu4c_numbering_clear(&service->numbering);
        qjs_icu4c_locales_clear(&service->locales);
        service->allocator.free(service->allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_datetime_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                     QJSIntlDateTimeService **out)
{
    QJSIntlDateTimeService *service;
    QJSIntlLocaleService *identity = NULL;
    QJSIntlStatus error;
    UEnumeration *calendars = NULL;
    UErrorCode status = U_ZERO_ERROR;
    const char *name;
    size_t i;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    error = qjs_icu4c_locales_open(allocator, udat_countAvailable(), udat_getAvailable,
                                   &service->locales);
    if (!error)
        error = qjs_icu4c_numbering_open(allocator, &service->locales, &service->numbering);
    if (!error)
        error = qjs_intl_locale_open(allocator, NULL, &identity);
    if (error)
        goto done;
    if (service->locales.count > SIZE_MAX / sizeof(*service->facts)) {
        error = QJS_INTL_OVERFLOW;
        goto done;
    }
    service->facts =
        allocator->malloc(allocator->opaque, service->locales.count * sizeof(*service->facts));
    if (!service->facts) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    calendars = ucal_getKeywordValuesForLocale("calendar", "en", 0, &status);
    while (U_SUCCESS(status) && (name = uenum_next(calendars, NULL, &status))) {
        QJSIntlBytes canonical = known_calendar(name);
        unsigned int kind;

        for (kind = 0; canonical.length && kind < QJS_CAL_COUNT; kind++)
            if (equal(canonical, bytes(qjs_calendar_identifier(kind))))
                service->calendars |= 1U << kind;
    }
    uenum_close(calendars);
    error = qjs_icu4c_status(status);
    if (error)
        goto done;
    service->calendars |= 1U << QJS_CAL_ISO8601;
    for (i = 0; i < service->locales.count; i++) {
        char *locale = NULL;
        UCalendar *calendar = NULL;

        error = qjs_icu4c_locale_id(allocator, service->locales.tags[i], &locale);
        if (!error) {
            static const UChar utc[] = {'U', 'T', 'C'};

            calendar = ucal_open(utc, 3, locale, UCAL_DEFAULT, &status);
            if (U_SUCCESS(status))
                service->facts[i].calendar = known_calendar(ucal_getType(calendar, &status));
            error = qjs_icu4c_status(status);
            if (!error && !service->facts[i].calendar.length)
                error = QJS_INTL_DATA_ERROR;
        }
        ucal_close(calendar);
        if (locale)
            allocator->free(allocator->opaque, locale);
        if (!error)
            error = hour_preferences(identity, service->locales.tags[i], &service->facts[i].hours);
        if (error)
            goto done;
    }
    *out = service;
    service = NULL;
done:
    qjs_intl_locale_close(identity);
    qjs_intl_datetime_close(service);
    return error;
}

QJSIntlStatus qjs_intl_datetime_available(const QJSIntlDateTimeService *service,
                                          const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!out || !count || !service)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->locales.tags;
    *count = service->locales.count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_datetime_default_calendar(const QJSIntlDateTimeService *service,
                                                 size_t locale, QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !service || locale >= service->locales.count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->facts[locale].calendar;
    return QJS_INTL_OK;
}

static QJSIntlStatus calendar_query(const QJSIntlDateTimeService *service, QJSIntlBytes calendar,
                                    int aliases, int *out)
{
    unsigned int i;

    if (out)
        *out = 0;
    if (!out || !service || (calendar.length && !calendar.data))
        return QJS_INTL_INVALID_ARGUMENT;
    if (aliases && equal(calendar, bytes("islamic")))
        calendar = bytes("islamic-tbla");
    for (i = 0; i < QJS_CAL_COUNT; i++) {
        if (equal(calendar, bytes(qjs_calendar_identifier(i)))) {
            *out = (service->calendars >> i) & 1;
            break;
        }
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_datetime_calendar_supported(const QJSIntlDateTimeService *service,
                                                   QJSIntlBytes calendar, int *out)
{
    return calendar_query(service, calendar, 1, out);
}

QJSIntlStatus qjs_intl_datetime_calendar_available(const QJSIntlDateTimeService *service,
                                                   QJSIntlBytes calendar, int *out)
{
    return calendar_query(service, calendar, 0, out);
}

QJSIntlStatus qjs_intl_datetime_default_system(const QJSIntlDateTimeService *service, size_t locale,
                                               QJSIntlBytes *out)
{
    return qjs_icu4c_numbering_default(service ? &service->numbering : NULL, locale, out);
}

QJSIntlStatus qjs_intl_datetime_system_available(const QJSIntlDateTimeService *service,
                                                 QJSIntlBytes system, int *out)
{
    return qjs_icu4c_numbering_supported(service ? &service->numbering : NULL, system, out);
}

QJSIntlStatus qjs_intl_datetime_system_supported(const QJSIntlDateTimeService *service,
                                                 size_t locale, QJSIntlBytes system, int *out)
{
    if (!service || locale >= service->locales.count) {
        if (out)
            *out = 0;
        return QJS_INTL_INVALID_ARGUMENT;
    }
    return qjs_intl_datetime_system_available(service, system, out);
}

QJSIntlStatus qjs_intl_datetime_hour_preferences(const QJSIntlDateTimeService *service,
                                                 size_t locale, QJSIntlDateHourPreferences *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !service || locale >= service->locales.count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->facts[locale].hours;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_datetime_formatter_open(const QJSIntlDateTimeService *service, size_t locale,
                                               QJSIntlBytes calendar, QJSIntlBytes system,
                                               const QJSTimeZoneIdentifier *zone,
                                               const QJSIntlDateTimeOptions *options,
                                               QJSIntlDateTimeFormatter **out)
{
    QJSIntlStatus error;
    int supported;

    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->locales.count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!calendar.length)
        calendar = service->facts[locale].calendar;
    if (equal(calendar, bytes("islamic")))
        calendar = bytes("islamic-tbla");
    error = calendar_query(service, calendar, 0, &supported);
    if (error || !supported)
        return error ? error : QJS_INTL_UNSUPPORTED;
    if (!system.length)
        system = service->numbering.defaults[locale];
    error = qjs_icu4c_numbering_supported(&service->numbering, system, &supported);
    if (error || !supported)
        return error ? error : QJS_INTL_UNSUPPORTED;
    return qjs_icu4c_datetime_create(&service->allocator, service->locales.tags[locale], calendar,
                                     system, zone, options, out);
}
