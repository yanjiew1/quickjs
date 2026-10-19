/* Native standard decimal/percent NumberFormat service.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "number.h"
#include "affix.h"
#include "body.h"
#include "symbol-data.h"
#include "../../cutils/cutils.h"
#include <limits.h>
#include <string.h>

struct QJSIntlNumberService {
    QJSIntlAllocator allocator;
    QJSIntlNumberingData numbering;
    QJSIntlNumberPatternData patterns;
    QJSIntlNumberSymbolData symbols;
    QJSIntlBytes *available;
    uint32_t root;
};

struct QJSIntlNumberFormatter {
    QJSIntlAllocator allocator;
    QJSIntlNumberRounding rounding;
    QJSIntlNumberSign sign_display;
    QJSIntlNumberBody body;
    QJSIntlNumberAffixSymbols symbols;
    QJSIntlNumberPatternToken *tokens;
    size_t offsets[3], counts[3];
};

void qjs_intl_number_close(QJSIntlNumberService *service)
{
    if (service) {
        QJSIntlAllocator allocator = service->allocator;
        if (service->available)
            allocator.free(allocator.opaque, service->available);
        allocator.free(allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_number_open(const QJSIntlAllocator *allocator, QJSIntlNumberService **out)
{
    QJSIntlNumberService *service;
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
    if (status)
        goto fail;
    status = qjs_intl_numbering_data_open(&data, allocator, &service->numbering);
    if (status)
        goto fail;
    status = qjs_intl_number_pattern_data_open(&service->numbering, allocator, &service->patterns);
    if (status)
        goto fail;
    status = qjs_intl_number_symbol_data_open(&service->numbering, allocator, &service->symbols);
    if (status)
        goto fail;
    if (data.locales.count < 2) {
        status = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    count = (size_t)data.locales.count - 1;
    if (count > SIZE_MAX / sizeof(*service->available)) {
        status = QJS_INTL_OVERFLOW;
        goto fail;
    }
    service->available = allocator->malloc(allocator->opaque,
                                           count * sizeof(*service->available));
    if (!service->available) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    count = 0;
    for (i = 0; i < data.locales.count; i++) {
        QJSIntlBytes tag;
        uint32_t parent;
        qjs_intl_data_locale(&data, i, &tag, &parent);
        if (parent == UINT32_MAX)
            service->root = i;
        else
            service->available[count++] = tag;
    }
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_number_close(service);
    return status;
}

QJSIntlStatus qjs_intl_number_available(const QJSIntlNumberService *service,
                                        const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!service || !out || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->available;
    *count = service->numbering.data.locales.count - 1;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_default_system(const QJSIntlNumberService *service, size_t locale,
                                             QJSIntlBytes *out)
{
    QJSIntlNumberingPreferences preferences;
    QJSIntlNumberingSystem system;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !service || locale >= service->numbering.data.locales.count - 1)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_numbering_preferences(&service->numbering, service->available[locale],
                                            &preferences);
    if (status)
        return status;
    status = qjs_intl_numbering_system_at(&service->numbering, preferences.standard, &system);
    if (status)
        return status;
    if (system.kind != QJS_INTL_NUMBERING_NUMERIC)
        return QJS_INTL_UNSUPPORTED;
    *out = system.name;
    return QJS_INTL_OK;
}

void qjs_intl_number_formatter_close(QJSIntlNumberFormatter *formatter)
{
    if (formatter) {
        QJSIntlAllocator allocator = formatter->allocator;
        const uint16_t *owned[] = {formatter->body.decimal.data, formatter->body.group.data,
                                   formatter->body.nan.data, formatter->body.infinity.data};
        size_t i;
        for (i = 0; i < sizeof(owned) / sizeof(owned[0]); i++)
            if (owned[i])
                allocator.free(allocator.opaque, (void *)owned[i]);
        if (formatter->tokens)
            allocator.free(allocator.opaque, formatter->tokens);
        allocator.free(allocator.opaque, formatter);
    }
}

/* The opened data owner already guarantees scalar UTF-8. Keep the bounded
   decoder here at the native-data boundary, outside the generic body engine. */
static QJSIntlStatus decode(const QJSIntlAllocator *allocator, QJSIntlBytes text, QJSIntlUTF16 *out)
{
    const uint8_t *at, *end, *next;
    uint16_t *result;
    size_t length = 0;
    if (!text.data || !text.length || text.length > INT_MAX ||
        text.length > SIZE_MAX / sizeof(*result))
        return QJS_INTL_DATA_ERROR;
    at = (const uint8_t *)text.data;
    end = at + text.length;
    result = allocator->malloc(allocator->opaque, text.length * sizeof(*result));
    if (!result)
        return QJS_INTL_NO_MEMORY;
    while (at < end) {
        int c = unicode_from_utf8(at, (int)(end - at), &next);
        if (c <= 0 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) {
            allocator->free(allocator->opaque, result);
            return QJS_INTL_DATA_ERROR;
        }
        if (c > 0xffff) {
            c -= 0x10000;
            result[length++] = 0xd800 + (c >> 10);
            result[length++] = 0xdc00 + (c & 1023);
        } else
            result[length++] = c;
        at = next;
    }
    *out = (QJSIntlUTF16){result, length};
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_formatter_open(const QJSIntlNumberService *service, size_t locale,
                                             QJSIntlBytes system_name,
                                             const QJSIntlNumberOptions *options,
                                             QJSIntlNumberFormatter **out)
{
    QJSIntlNumberFormatter *formatter;
    QJSIntlNumberingSystem system;
    QJSIntlNumberPattern pattern;
    QJSIntlNumberSymbols symbols;
    QJSIntlStatus status;
    uint32_t index, source;
    size_t i, total = 0, position = 0;
    const uint8_t *at, *end, *next;
    if (out)
        *out = NULL;
    if (!out || !service || !options || locale >= service->numbering.data.locales.count - 1 ||
        (unsigned int)options->style > QJS_INTL_NUMBER_PERCENT ||
        (unsigned int)options->grouping > QJS_INTL_NUMBER_GROUP_MIN2 ||
        (unsigned int)options->sign_display > QJS_INTL_NUMBER_SIGN_NEGATIVE ||
        !options->maximum_output_length || qjs_intl_decimal_options_validate(&options->digits) ||
        (!system_name.data && system_name.length))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!system_name.length) {
        status = qjs_intl_number_default_system(service, locale, &system_name);
        if (status)
            return status;
    }
    status = qjs_intl_numbering_system_find(&service->numbering, system_name, &index);
    if (status)
        return status;
    qjs_intl_numbering_system_at(&service->numbering, index, &system);
    if (system.kind != QJS_INTL_NUMBERING_NUMERIC)
        return QJS_INTL_UNSUPPORTED;
    source = (uint32_t)locale + (locale >= service->root);
    status = qjs_intl_number_pattern_at(&service->patterns, source, index,
                                        options->style == QJS_INTL_NUMBER_PERCENT
                                            ? QJS_INTL_PATTERN_PERCENT
                                            : QJS_INTL_PATTERN_DECIMAL,
                                        &pattern);
    if (status)
        return status;
    if (pattern.state != QJS_INTL_NUMBER_PATTERN_VALUE)
        return QJS_INTL_DATA_ERROR;
    status = qjs_intl_number_symbols_at(&service->symbols, source, index, &symbols);
    if (status)
        return status;
    for (i = 0; i < 3; i++) {
        if (pattern.forms[i].count > SIZE_MAX / sizeof(*formatter->tokens) - total)
            return QJS_INTL_OVERFLOW;
        total += pattern.forms[i].count;
    }
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = service->allocator;
    formatter->rounding.digits = options->digits;
    formatter->rounding.percent = options->style == QJS_INTL_NUMBER_PERCENT;
    formatter->sign_display = options->sign_display;
    formatter->body.primary_grouping = pattern.primary_grouping;
    formatter->body.secondary_grouping = pattern.secondary_grouping;
    formatter->body.minimum_grouping_digits = pattern.minimum_grouping_digits;
    formatter->body.grouping = options->grouping;
    formatter->body.maximum_output_length = options->maximum_output_length;
    formatter->symbols = (QJSIntlNumberAffixSymbols){
        symbols.fields[QJS_INTL_SYMBOL_MINUS].text,
        symbols.fields[QJS_INTL_SYMBOL_PLUS].text,
        symbols.fields[QJS_INTL_SYMBOL_PERCENT].text,
        symbols.fields[QJS_INTL_SYMBOL_PER_MILLE].text,
        {0},
    };
    formatter->tokens =
        service->allocator.malloc(service->allocator.opaque, total * sizeof(*formatter->tokens));
    if (!formatter->tokens) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    for (i = 0; i < 3; i++) {
        uint32_t j;
        formatter->offsets[i] = position;
        formatter->counts[i] = pattern.forms[i].count;
        for (j = 0; j < pattern.forms[i].count; j++)
            qjs_intl_number_pattern_token_at(&service->patterns, &pattern.forms[i], j,
                                             &formatter->tokens[position++]);
    }
    at = (const uint8_t *)system.value.data;
    end = at + system.value.length;
    for (i = 0; i < 10; i++) {
        int c = unicode_from_utf8(at, (int)(end - at), &next);
        if (c <= 0 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) {
            status = QJS_INTL_DATA_ERROR;
            goto fail;
        }
        formatter->body.digits[i] = c;
        at = next;
    }
    if (at != end) {
        status = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    status = decode(&service->allocator, symbols.fields[QJS_INTL_SYMBOL_DECIMAL].text,
                    &formatter->body.decimal);
    if (!status)
        status = decode(&service->allocator, symbols.fields[QJS_INTL_SYMBOL_GROUP].text,
                        &formatter->body.group);
    if (!status)
        status = decode(&service->allocator, symbols.fields[QJS_INTL_SYMBOL_NAN].text,
                        &formatter->body.nan);
    if (!status)
        status = decode(&service->allocator, symbols.fields[QJS_INTL_SYMBOL_INFINITY].text,
                        &formatter->body.infinity);
    if (status)
        goto fail;
    *out = formatter;
    return QJS_INTL_OK;
fail:
    qjs_intl_number_formatter_close(formatter);
    return status;
}

QJSIntlStatus qjs_intl_number_format(const QJSIntlNumberFormatter *formatter,
                                     const QJSIntlMathematicalValue *value, QJSIntlFormatted *out)
{
    QJSIntlRoundedNumber rounded = {0};
    QJSIntlFormatted body = {0};
    QJSIntlStatus status;
    unsigned int form;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !value)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_number_round(&formatter->allocator, &formatter->rounding, value, &rounded);
    if (!status)
        status = qjs_intl_number_sign_pattern(formatter->sign_display, &rounded, &form);
    if (!status)
        status = qjs_intl_number_body_format(
            &formatter->allocator, &formatter->body, rounded.kind,
            (QJSIntlBytes){rounded.number.text, rounded.number.length}, &body);
    if (!status)
        status = qjs_intl_number_affix_format(
            &formatter->allocator, formatter->tokens + formatter->offsets[form],
            formatter->counts[form], &formatter->symbols, &body,
            form == QJS_INTL_NUMBER_PATTERN_PLUS, formatter->body.maximum_output_length, out);
    qjs_intl_number_body_clear(&formatter->allocator, &body);
    qjs_intl_number_round_clear(&formatter->allocator, &rounded);
    return status;
}

void qjs_intl_number_result_clear(const QJSIntlAllocator *allocator, QJSIntlFormatted *out)
{
    qjs_intl_number_body_clear(allocator, out);
}
