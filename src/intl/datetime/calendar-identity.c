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
#include "calendar-identity.h"

typedef struct Search {
    const QJSIntlLocaleMetadata *metadata;
    uint32_t target, index;
    int found;
} Search;

static QJSIntlStatus begin(const QJSIntlLocaleMetadata *metadata, QJSIntlBytes calendar, Search *search)
{
    *search = (Search){metadata, 0, 0, 0};
    return qjs_intl_locale_type_identity(metadata, (QJSIntlBytes){"u", 1},
                                         (QJSIntlBytes){"ca", 2}, calendar, &search->target);
}

static QJSIntlStatus consider(Search *search, QJSIntlBytes raw, uint32_t index)
{
    uint32_t identity;
    QJSIntlStatus status = qjs_intl_locale_type_identity(search->metadata, (QJSIntlBytes){"u", 1},
                                                       (QJSIntlBytes){"ca", 2}, raw, &identity);
    if (status == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    if (status)
        return status;
    if (identity != search->target)
        return QJS_INTL_OK;
    if (search->found)
        return QJS_INTL_DATA_ERROR;
    search->found = 1;
    search->index = index;
    return QJS_INTL_OK;
}

static QJSIntlStatus finish(const Search *search, uint32_t *out)
{
    if (!search->found)
        return QJS_INTL_UNSUPPORTED;
    *out = search->index;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_available_calendar_index(const QJSIntlLocaleMetadata *metadata,
                                                   const QJSIntlDateAvailableData *data,
                                                   QJSIntlBytes calendar, uint32_t *out)
{
    Search search;
    QJSIntlStatus status;
    uint32_t i;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes)
        return QJS_INTL_INVALID_ARGUMENT;
    status = begin(metadata, calendar, &search);
    for (i = 0; !status && i < data->calendar_count; i++) {
        QJSIntlBytes raw;
        status = qjs_intl_date_available_calendar_at(data, i, &raw);
        if (!status)
            status = consider(&search, raw, i);
    }
    return status ? status : finish(&search, out);
}

QJSIntlStatus qjs_intl_date_style_calendar_index(const QJSIntlLocaleMetadata *metadata,
                                               const QJSIntlDateStyleData *data,
                                               QJSIntlBytes calendar, uint32_t *out)
{
    Search search;
    QJSIntlStatus status;
    uint32_t i;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes)
        return QJS_INTL_INVALID_ARGUMENT;
    status = begin(metadata, calendar, &search);
    for (i = 0; !status && i < data->calendar_count; i++) {
        QJSIntlBytes raw;
        status = qjs_intl_date_style_calendar_at(data, i, &raw);
        if (!status)
            status = consider(&search, raw, i);
    }
    return status ? status : finish(&search, out);
}

QJSIntlStatus qjs_intl_date_combination_calendar_index(const QJSIntlLocaleMetadata *metadata,
                                                     const QJSIntlDateCombinationData *data,
                                                     QJSIntlBytes calendar, uint32_t *out)
{
    Search search;
    QJSIntlStatus status;
    uint32_t i;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes)
        return QJS_INTL_INVALID_ARGUMENT;
    status = begin(metadata, calendar, &search);
    for (i = 0; !status && i < data->calendar_count; i++) {
        QJSIntlBytes raw;
        status = qjs_intl_date_combination_calendar_at(data, i, &raw);
        if (!status)
            status = consider(&search, raw, i);
    }
    return status ? status : finish(&search, out);
}

QJSIntlStatus qjs_intl_date_name_calendar_index(const QJSIntlLocaleMetadata *metadata,
                                              const QJSIntlCalendarNameData *data,
                                              QJSIntlBytes calendar, uint32_t *out)
{
    Search search;
    QJSIntlStatus status;
    uint32_t i;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes)
        return QJS_INTL_INVALID_ARGUMENT;
    status = begin(metadata, calendar, &search);
    for (i = 0; !status && i < data->calendar_count; i++) {
        QJSIntlBytes raw;
        status = qjs_intl_calendar_name_calendar_at(data, i, &raw);
        if (!status)
            status = consider(&search, raw, i);
    }
    return status ? status : finish(&search, out);
}

QJSIntlStatus qjs_intl_date_interval_calendar_index(const QJSIntlLocaleMetadata *metadata,
                                                  const QJSIntlDateIntervalData *data,
                                                  QJSIntlBytes calendar, uint32_t *out)
{
    Search search;
    QJSIntlStatus status;
    uint32_t i;
    if (out)
        *out = 0;
    if (!out || !data || !data->section.bytes)
        return QJS_INTL_INVALID_ARGUMENT;
    status = begin(metadata, calendar, &search);
    for (i = 0; !status && i < data->calendar_count; i++) {
        QJSIntlBytes raw;
        status = qjs_intl_date_interval_calendar_at(data, i, &raw);
        if (!status)
            status = consider(&search, raw, i);
    }
    return status ? status : finish(&search, out);
}
