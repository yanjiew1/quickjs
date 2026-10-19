/* Native DisplayNames composes shared data owners, not other Intl services.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "display.h"
#include "calendar-names.h"
#include "language-format.h"
#include "text.h"
#include "../common/locale-grammar.h"
#include "../common/pair-pattern.h"
#include "../currency/data.h"
#include "../datetime/field-data.h"
#include <string.h>

struct QJSIntlDisplayService {
    QJSIntlAllocator allocator;
    QJSIntlNameData names;
    QJSIntlCurrencyData currency;
    QJSIntlDateFieldData date;
    QJSIntlLocaleMetadata metadata;
    QJSIntlBytes *available;
    uint32_t root;
};

struct QJSIntlDisplayFormatter {
    QJSIntlAllocator allocator;
    QJSIntlDisplayOptions options;
    QJSIntlNameData names;
    QJSIntlCurrencyData currency;
    QJSIntlDateFieldData date;
    QJSIntlCalendarNames calendars;
    uint32_t locale;
};

static QJSIntlStatus templates(const QJSIntlNameData *names, uint32_t locale)
{
    static const char *const keys[] = {"localePattern", "localeSeparator"};
    size_t i;
    for (i = 0; i < sizeof(keys) / sizeof(*keys); i++) {
        QJSIntlNameLabel label;
        QJSIntlPairPattern pattern;
        QJSIntlStatus status = qjs_intl_name_select(names, locale, QJS_INTL_NAME_PATTERN,
                                                   QJS_INTL_NAME_LONG, 0,
                                                   (QJSIntlBytes){keys[i], strlen(keys[i])}, &label);
        if (status)
            return status;
        if (!label.found)
            return QJS_INTL_DATA_ERROR;
        status = qjs_intl_pair_pattern_parse(label.entry.text, &pattern);
        if (status)
            return status == QJS_INTL_UNSUPPORTED ? QJS_INTL_DATA_ERROR : status;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_display_open(const QJSIntlAllocator *allocator, QJSIntlDisplayService **out)
{
    QJSIntlDisplayService *service;
    QJSIntlData data;
    QJSIntlStatus status;
    uint32_t i;
    size_t count = 0;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    status = qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, allocator, &data);
    if (!status)
        status = qjs_intl_name_data_open(&data, allocator, &service->names);
    if (!status)
        status = qjs_intl_currency_data_open(&data, allocator, &service->currency);
    if (!status)
        status = qjs_intl_date_field_data_open(&data, allocator, &service->date);
    if (!status)
        status = qjs_intl_locale_metadata_open(&data, allocator, &service->metadata);
    if (status)
        goto fail;
    if (data.locales.count < 2) {
        status = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    count = data.locales.count - 1;
    if (count > SIZE_MAX / sizeof(*service->available)) {
        status = QJS_INTL_OVERFLOW;
        goto fail;
    }
    service->available = allocator->malloc(allocator->opaque, count * sizeof(*service->available));
    if (!service->available) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    count = 0;
    for (i = 0; i < data.locales.count; i++) {
        QJSIntlBytes tag;
        uint32_t parent;
        qjs_intl_data_locale(&data, i, &tag, &parent);
        if (parent == UINT32_MAX) {
            service->root = i;
            continue;
        }
        /* Locale records may legitimately have no field for a particular code.
           Admit the prepared locale, not the global raw name-code inventory;
           require its inherited composition templates to be usable. */
        status = templates(&service->names, i);
        if (status)
            goto fail;
        service->available[count++] = tag;
    }
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_display_close(service);
    return status;
}

void qjs_intl_display_close(QJSIntlDisplayService *service)
{
    if (service) {
        QJSIntlAllocator allocator = service->allocator;
        if (service->available)
            allocator.free(allocator.opaque, service->available);
        allocator.free(allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_display_available(const QJSIntlDisplayService *service,
                                         const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!service || !out || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->available;
    *count = service->names.data.locales.count - 1;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_display_formatter_open(const QJSIntlDisplayService *service, size_t locale,
                                              const QJSIntlDisplayOptions *options,
                                              QJSIntlDisplayFormatter **out)
{
    QJSIntlDisplayFormatter result = {0}, *formatter;
    QJSIntlStatus status;
    if (out)
        *out = NULL;
    if (!out || !service || !options || locale >= service->names.data.locales.count - 1 ||
        (unsigned int)options->type >= QJS_INTL_DISPLAY_TYPE_COUNT ||
        (unsigned int)options->style > QJS_INTL_NAME_NARROW ||
        (options->dialect != 0 && options->dialect != 1) || !options->maximum_output_length ||
        options->maximum_output_length > SIZE_MAX / sizeof(uint16_t))
        return QJS_INTL_INVALID_ARGUMENT;
    result.allocator = service->allocator;
    result.options = *options;
    result.names = service->names;
    result.currency = service->currency;
    result.date = service->date;
    result.locale = (uint32_t)locale + (locale >= service->root);
    if (options->type == QJS_INTL_DISPLAY_CALENDAR) {
        status = qjs_intl_calendar_names_open(&service->metadata, &service->names,
                                             &service->allocator, &result.calendars);
        if (status)
            return status;
    }
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter) {
        qjs_intl_calendar_names_clear(&result.calendars);
        return QJS_INTL_NO_MEMORY;
    }
    *formatter = result;
    *out = formatter;
    return QJS_INTL_OK;
}

void qjs_intl_display_formatter_close(QJSIntlDisplayFormatter *formatter)
{
    if (formatter) {
        QJSIntlAllocator allocator = formatter->allocator;
        qjs_intl_calendar_names_clear(&formatter->calendars);
        allocator.free(allocator.opaque, formatter);
    }
}

static QJSIntlStatus simple_name(const QJSIntlDisplayFormatter *formatter, QJSIntlNameGroup group,
                                 QJSIntlBytes key, QJSIntlBytes *text, int *found)
{
    QJSIntlNameLabel label;
    QJSIntlStatus status = qjs_intl_name_select(&formatter->names, formatter->locale, group,
                                               formatter->options.style,
                                               group == QJS_INTL_NAME_SCRIPT, key, &label);
    if (!status) {
        *found = label.found;
        *text = label.entry.text;
    }
    return status;
}

static QJSIntlStatus currency_name(const QJSIntlDisplayFormatter *formatter, QJSIntlBytes code,
                                   QJSIntlBytes *text, int *found)
{
    QJSIntlCurrencyText label;
    uint32_t index;
    QJSIntlStatus status = qjs_intl_currency_code_find(&formatter->currency, code, &index);
    if (status == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    if (status)
        return status;
    status = qjs_intl_currency_field_at(&formatter->currency, formatter->locale, index,
                                       QJS_INTL_CURRENCY_NAME, &label);
    if (!status && (label.state == QJS_INTL_CURRENCY_VALUE || label.state == QJS_INTL_CURRENCY_EMPTY)) {
        *found = 1;
        *text = label.text;
    }
    return status;
}

static QJSIntlStatus date_name(const QJSIntlDisplayFormatter *formatter, QJSIntlBytes code,
                               QJSIntlBytes *text, int *found)
{
    static const QJSIntlDateFieldWidth widths[] = {
        QJS_INTL_DATE_FIELD_LONG, QJS_INTL_DATE_FIELD_SHORT, QJS_INTL_DATE_FIELD_NARROW
    };
    QJSIntlDateField field;
    QJSIntlDateFieldLabel label;
    QJSIntlStatus status = qjs_intl_date_field_from_code(code, &field);
    if (status)
        return status;
    status = qjs_intl_date_field_label_at(&formatter->date, formatter->locale, field,
                                         widths[formatter->options.style], 0, &label);
    if (!status && label.state == QJS_INTL_DATE_FIELD_VALUE) {
        *found = 1;
        *text = label.text;
    }
    return status;
}

QJSIntlStatus qjs_intl_display_format(const QJSIntlDisplayFormatter *formatter, QJSIntlBytes code,
                                      QJSIntlNameText *out)
{
    QJSIntlBytes text = {0}, key;
    QJSIntlStatus status;
    char normalized[4];
    size_t i;
    int found = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !code.data || !code.length || code.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    switch (formatter->options.type) {
    case QJS_INTL_DISPLAY_LANGUAGE:
        return qjs_intl_language_name_format(&formatter->names, formatter->locale,
                                             formatter->options.style, formatter->options.dialect,
                                             code, &formatter->allocator,
                                             formatter->options.maximum_output_length, out);
    case QJS_INTL_DISPLAY_REGION:
    case QJS_INTL_DISPLAY_SCRIPT:
        if (!qjs_intl_unicode_subtag_well_formed(formatter->options.type == QJS_INTL_DISPLAY_REGION ?
                                                QJS_INTL_REGION_SUBTAG : QJS_INTL_SCRIPT_SUBTAG,
                                                code.data, code.length))
            return QJS_INTL_INVALID_ARGUMENT;
        for (i = 0; i < code.length; i++) {
            unsigned int c = (uint8_t)code.data[i];
            normalized[i] = c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
        }
        status = simple_name(formatter, formatter->options.type == QJS_INTL_DISPLAY_REGION ?
                              QJS_INTL_NAME_REGION : QJS_INTL_NAME_SCRIPT,
                              (QJSIntlBytes){normalized, code.length}, &text, &found);
        break;
    case QJS_INTL_DISPLAY_CURRENCY:
        status = currency_name(formatter, code, &text, &found);
        break;
    case QJS_INTL_DISPLAY_CALENDAR:
        status = qjs_intl_calendar_names_find(&formatter->calendars, code, &key);
        if (status == QJS_INTL_UNSUPPORTED)
            return QJS_INTL_OK;
        if (!status)
            status = simple_name(formatter, QJS_INTL_NAME_CALENDAR, key, &text, &found);
        break;
    case QJS_INTL_DISPLAY_DATE_TIME_FIELD:
        status = date_name(formatter, code, &text, &found);
        break;
    default:
        return QJS_INTL_INVALID_ARGUMENT;
    }
    if (status || !found)
        return status;
    return qjs_intl_name_text_copy(&formatter->allocator, text,
                                  formatter->options.maximum_output_length, out);
}
