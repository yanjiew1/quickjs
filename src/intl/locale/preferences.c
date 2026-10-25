/* RegionPreference, WeekInfoOfLocale, HourCyclesOfLocale, TextDirectionOfLocale.
 * ECMA402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "preferences.h"
#include "identifier.h"
#include <string.h>

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int digit(int c)
{
    return c >= '0' && c <= '9';
}
static int lower(int c)
{
    return c >= 'a' && c <= 'z';
}

static QJSIntlStatus copy_region(char out[4], QJSIntlBytes region)
{
    if (region.length != 2 && region.length != 3)
        return QJS_INTL_DATA_ERROR;
    memcpy(out, region.data, region.length);
    out[region.length] = 0;
    return QJS_INTL_OK;
}

static QJSIntlStatus subdivision_region(const QJSIntlLocaleService *service,
                                        const QJSIntlLocaleComponents *parts,
                                        const char key[2], char out[4])
{
    QJSIntlBytes subdivision = qjs_intl_locale_keyword(parts, key);
    QJSIntlLocaleComponents canonical_parts;
    size_t prefix, i;
    char tag[8], *canonical = NULL;
    QJSIntlStatus status;

    if (subdivision.length >= 2 && lower(subdivision.data[0]) &&
        lower(subdivision.data[1]))
        prefix = 2;
    else if (subdivision.length >= 3 && digit(subdivision.data[0]) &&
             digit(subdivision.data[1]) && digit(subdivision.data[2]))
        prefix = 3;
    else
        return QJS_INTL_OK;
    if (subdivision.length < prefix + 1 || subdivision.length > prefix + 4)
        return QJS_INTL_OK;
    for (i = prefix; i < subdivision.length; i++) {
        if (!lower(subdivision.data[i]) && !digit(subdivision.data[i]))
            return QJS_INTL_OK;
    }
    memcpy(tag, "und-", 4);
    memcpy(tag + 4, subdivision.data, prefix);
    status = qjs_intl_locale_canonicalize(service, (QJSIntlBytes){tag, prefix + 4},
                                          &canonical);
    if (!status)
        status = qjs_intl_locale_components(bytes(canonical), &canonical_parts);
    if (!status && canonical_parts.region.data)
        status = copy_region(out, canonical_parts.region);
    qjs_intl_locale_result_free(service, canonical);
    return status;
}

QJSIntlStatus qjs_intl_locale_region_preference(const QJSIntlLocaleService *service,
                                                QJSIntlBytes tag,
                                                QJSIntlRegionPreference *out)
{
    QJSIntlLocaleComponents parts, maximal_parts;
    QJSIntlRegionPreference result = {{0}, {0}};
    QJSIntlStatus status;
    char *maximal = NULL;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_locale_components(tag, &parts);
    if (status)
        return status;
    if (parts.region.data)
        status = copy_region(result.region, parts.region);
    else
        status = subdivision_region(service, &parts, "sd", result.region);
    if (!status && !result.region[0]) {
        status = qjs_intl_locale_maximize(service, tag, &maximal);
        if (!status)
            status = qjs_intl_locale_components(bytes(maximal), &maximal_parts);
        if (!status && maximal_parts.region.data)
            status = copy_region(result.region, maximal_parts.region);
    }
    qjs_intl_locale_result_free(service, maximal);
    if (status)
        return status;
    if (!result.region[0])
        memcpy(result.region, "001", 4);
    status = subdivision_region(service, &parts, "rg", result.override);
    if (!status)
        *out = result;
    return status;
}

QJSIntlStatus qjs_intl_locale_week_info(const QJSIntlLocaleService *service,
                                        QJSIntlBytes tag, QJSIntlBytes first_day,
                                        QJSIntlWeekInfo *out)
{
    static const char days[7][4] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    QJSIntlRegionPreference preference;
    QJSIntlWeekInfo result;
    QJSIntlStatus status;
    unsigned int i;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!first_day.data && first_day.length))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_locale_region_preference(service, tag, &preference);
    if (status)
        return status;
    status = QJS_INTL_UNSUPPORTED;
    if (preference.override[0])
        status =
            qjs_intl_locale_week_data(service, bytes(preference.override), &result);
    if (status == QJS_INTL_UNSUPPORTED)
        status = qjs_intl_locale_week_data(service, bytes(preference.region), &result);
    if (status == QJS_INTL_UNSUPPORTED)
        status = qjs_intl_locale_week_data(service, bytes("001"), &result);
    if (status)
        return status;
    if (first_day.length == 1 && first_day.data[0] >= '0' && first_day.data[0] <= '7')
        result.first_day = first_day.data[0] == '0' ? 7 : first_day.data[0] - '0';
    else if (first_day.length == 3) {
        for (i = 0; i < 7; i++) {
            if (!memcmp(first_day.data, days[i], 3)) {
                result.first_day = i + 1;
                break;
            }
        }
    }
    *out = result;
    return QJS_INTL_OK;
}

static QJSIntlStatus regional_list(const QJSIntlLocaleService *service,
                                   QJSIntlBytes tag, int hours, QJSIntlBytes *out,
                                   uint32_t *count)
{
    QJSIntlStatus (*query)(const QJSIntlLocaleService *, QJSIntlBytes,
                            QJSIntlBytes *, uint32_t *) =
        hours ? qjs_intl_locale_hour_data : qjs_intl_locale_calendar_data;
    QJSIntlRegionPreference preference;
    QJSIntlLocaleComponents parts;
    const char *regions[2];
    QJSIntlStatus status;
    size_t i;

    if (out)
        memset(out, 0, sizeof(*out));
    if (count)
        *count = 0;
    if (!out || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_locale_region_preference(service, tag, &preference);
    if (!status)
        status = qjs_intl_locale_components(tag, &parts);
    if (status)
        return status;
    regions[0] = preference.override[0] ? preference.override : preference.region;
    regions[1] = preference.override[0] ? preference.region : NULL;
    for (i = 0; i < 2 && regions[i]; i++) {
        char key[13];
        size_t length = strlen(regions[i]);
        if (parts.language.length > 8 || length > 3)
            return QJS_INTL_DATA_ERROR;
        memcpy(key, parts.language.data, parts.language.length);
        key[parts.language.length] = '-';
        memcpy(key + parts.language.length + 1, regions[i], length);
        status = query(service, (QJSIntlBytes){key, parts.language.length + 1 + length},
                        out, count);
        if (status == QJS_INTL_UNSUPPORTED)
            status = query(service, bytes(regions[i]), out, count);
        if (status != QJS_INTL_UNSUPPORTED)
            return status;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_locale_hour_cycles(const QJSIntlLocaleService *service,
                                          QJSIntlBytes tag, QJSIntlBytes *out,
                                          uint32_t *count)
{
    QJSIntlStatus status = regional_list(service, tag, 1, out, count);
    if (status == QJS_INTL_UNSUPPORTED) {
        *out = bytes("h23");
        *count = 1;
        return QJS_INTL_OK;
    }
    return status;
}

QJSIntlStatus qjs_intl_locale_calendar_preferences(const QJSIntlLocaleService *service,
                                                   QJSIntlBytes tag, QJSIntlBytes *out,
                                                   uint32_t *count)
{
    QJSIntlStatus status = regional_list(service, tag, 0, out, count);
    /* Fallback occurs after canonicalization and capability filtering, not
       through a second preference search after an unsupported first list. */
    return status == QJS_INTL_UNSUPPORTED ? QJS_INTL_OK : status;
}

QJSIntlStatus qjs_intl_locale_text_direction(const QJSIntlLocaleService *service,
                                             QJSIntlBytes tag, unsigned int *out)
{
    QJSIntlLocaleComponents parts;
    QJSIntlStatus status;
    char *maximal = NULL;

    if (out)
        *out = QJS_INTL_DIRECTION_UNKNOWN;
    if (!service || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_locale_components(tag, &parts);
    if (!status && !parts.script.data) {
        status = qjs_intl_locale_maximize(service, tag, &maximal);
        if (!status)
            status = qjs_intl_locale_components(bytes(maximal), &parts);
    }
    if (!status && parts.script.data) {
        status = qjs_intl_locale_script_direction(service, parts.script, out);
        if (status == QJS_INTL_UNSUPPORTED)
            status = QJS_INTL_OK;
    }
    qjs_intl_locale_result_free(service, maximal);
    return status;
}
