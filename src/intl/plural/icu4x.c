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
#include "plural.h"
#include "icu4x.h"
#include "../icu4x/common.h"
#include "../icu4x/locale-data.h"
#include "../../icu4x/plural.h"
#include "../../icu4x/decimal.h"
#include <limits.h>
#include <string.h>

struct QJSIntlPluralService {
    QJSIntlAllocator allocator;
    QJSICU4XLocales locales;
};
struct QJSIntlPluralFormatter {
    QJSIntlAllocator allocator;
    QJSIntlPluralOptions options;
    QJSICU4XPlural *handle;
    QJSICU4XPluralData data;
    QJSICU4XCompact *compact;
};

static int enumerate_locales(void *opaque, QJSICU4XEmit *emit)
{
    const char *version = qjs_icu4x_locale_strings + qjs_icu4x_locale_aliases.version;
    int status = qjs_icu4x_plural_check_version((const uint8_t *)version, strlen(version));
    size_t i;
    unsigned int kind;

    if (status)
        return status;
    for (i = 0; i < qjs_icu4x_locale_registry_count; i++) {
        const char *tag = qjs_icu4x_locale_strings + qjs_icu4x_locale_registry[i];
        size_t length = strlen(tag);
        for (kind = 0; kind < 2; kind++) {
            QJSICU4XPlural *handle = NULL;
            status = qjs_icu4x_plural_open((const uint8_t *)tag, length, kind, &handle);
            qjs_icu4x_plural_close(handle);
            if (status)
                return status;
        }
        status = emit(opaque, (const uint8_t *)tag, length);
        if (status)
            return status;
    }
    return QJS_ICU4X_OK;
}

QJSIntlStatus qjs_intl_plural_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                   QJSIntlPluralService **out)
{
    QJSIntlPluralService *service;
    QJSIntlStatus error;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    error = qjs_icu4x_locales_open(allocator, enumerate_locales, &service->locales);
    if (error) {
        qjs_intl_plural_close(service);
        return error;
    }
    *out = service;
    return QJS_INTL_OK;
}

void qjs_intl_plural_close(QJSIntlPluralService *service)
{
    if (service) {
        qjs_icu4x_locales_clear(&service->locales);
        service->allocator.free(service->allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_plural_available(const QJSIntlPluralService *service,
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

void qjs_intl_plural_formatter_close(QJSIntlPluralFormatter *formatter)
{
    if (formatter) {
        qjs_icu4x_plural_close(formatter->handle);
        qjs_icu4x_compact_close(formatter->compact);
        formatter->allocator.free(formatter->allocator.opaque, formatter);
    }
}

QJSIntlStatus qjs_icu4x_plural_formatter_open(const QJSIntlAllocator *allocator,
                                              QJSIntlBytes locale,
                                              const QJSIntlPluralOptions *options,
                                              QJSIntlPluralFormatter **out)
{
    QJSIntlPluralFormatter *formatter;
    QJSIntlPluralOperands probe = {"0", 1, 0};
    QJSIntlStatus error;
    unsigned int category;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free || !options ||
        (unsigned)options->type > QJS_INTL_PLURAL_ORDINAL ||
        (unsigned)options->notation > QJS_INTL_PLURAL_COMPACT || options->compact_long > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_intl_decimal_options_validate(&options->digits);
    if (error)
        return error;
    formatter = allocator->malloc(allocator->opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = *allocator;
    formatter->options = *options;
    error = qjs_icu4x_status(qjs_icu4x_plural_open((const uint8_t *)locale.data, locale.length,
                                                   options->type, &formatter->handle));
    if (!error)
        error = qjs_icu4x_status(qjs_icu4x_plural_data(formatter->handle, &formatter->data));
    // Admit complete rule grammar once, including branches a numeric probe
    // would short-circuit. The snapshot is immutable for this formatter's life.
    for (category = 0; !error && category < QJS_INTL_PLURAL_CATEGORY_COUNT; category++) {
        QJSICU4XRuleBytes rule = formatter->data.rules[category];
        int matches;
        if (formatter->data.mask & (1u << category))
            error = qjs_intl_plural_rule_evaluate(
                (QJSIntlBytes){(const char *)rule.data, rule.length}, &probe, &matches);
    }
    if (!error && options->notation == QJS_INTL_PLURAL_COMPACT)
        error =
            qjs_icu4x_status(qjs_icu4x_compact_open((const uint8_t *)locale.data, locale.length,
                                                    options->compact_long, &formatter->compact));
    if (error) {
        qjs_intl_plural_formatter_close(formatter);
        return error;
    }
    *out = formatter;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_plural_formatter_open(const QJSIntlPluralService *service, size_t locale,
                                             const QJSIntlPluralOptions *options,
                                             QJSIntlPluralFormatter **out)
{
    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->locales.count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_icu4x_plural_formatter_open(&service->allocator, service->locales.tags[locale],
                                           options, out);
}

QJSIntlStatus qjs_intl_plural_categories(const QJSIntlPluralFormatter *formatter, unsigned int *out)
{
    if (out)
        *out = 0;
    if (!out || !formatter)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = formatter->data.mask;
    return QJS_INTL_OK;
}

typedef struct Resolution {
    QJSIntlDecimalResult rounded;
    QJSIntlBytes text;
    QJSIntlPluralCategory selected;
} Resolution;

static QJSIntlStatus resolve(const QJSIntlPluralFormatter *formatter,
                             const QJSIntlMathematicalValue *value, Resolution *out)
{
    QJSIntlDecimal decimal = {0};
    QJSIntlStatus error;
    int32_t exponent = 0;
    unsigned int category;

    memset(out, 0, sizeof(*out));
    out->selected = QJS_INTL_PLURAL_OTHER;
    if (!value || (unsigned)value->kind > QJS_INTL_NEGATIVE_ZERO)
        return QJS_INTL_INVALID_ARGUMENT;
    switch (value->kind) {
    case QJS_INTL_NAN:
        out->text = (QJSIntlBytes){"NaN", 3};
        return QJS_INTL_OK;
    case QJS_INTL_POSITIVE_INFINITY:
        out->text = (QJSIntlBytes){"Infinity", 8};
        return QJS_INTL_OK;
    case QJS_INTL_NEGATIVE_INFINITY:
        out->text = (QJSIntlBytes){"-Infinity", 9};
        return QJS_INTL_OK;
    default:
        break;
    }
    error = qjs_intl_decimal_from_value(&formatter->allocator, value, &decimal);
    if (!error)
        error = qjs_intl_decimal_format_numeric(&formatter->allocator, &decimal,
                                                &formatter->options.digits, &out->rounded);
    qjs_intl_decimal_clear(&formatter->allocator, &decimal);
    if (error)
        return error;
    out->text = (QJSIntlBytes){out->rounded.text, out->rounded.length};
    if (out->text.length > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    if (!(out->rounded.rounded.length == 1 && out->rounded.rounded.digits[0] == '0')) {
        int64_t magnitude =
            (int64_t)out->rounded.rounded.length + out->rounded.rounded.exponent - 1;
        if (magnitude < INT32_MIN || magnitude > INT32_MAX)
            return QJS_INTL_OVERFLOW;
        if (formatter->options.notation == QJS_INTL_PLURAL_SCIENTIFIC) {
            exponent = (int32_t)magnitude;
        } else if (formatter->options.notation == QJS_INTL_PLURAL_ENGINEERING) {
            int64_t remainder = magnitude % 3;
            int64_t adjusted = magnitude - (remainder < 0 ? remainder + 3 : remainder);
            if (adjusted < INT32_MIN || adjusted > INT32_MAX)
                return QJS_INTL_OVERFLOW;
            exponent = (int32_t)adjusted;
        } else if (formatter->options.notation == QJS_INTL_PLURAL_COMPACT) {
            error = qjs_icu4x_status(
                qjs_icu4x_compact_exponent(formatter->compact, (int32_t)magnitude, &exponent));
            if (error)
                return error;
        }
    }
    // The unscaled rounded text supplies n/i/v/w/f/t. Notation adds c/e only;
    // consulting compact metadata must not apply a second rounding operation.
    for (category = 0; category < QJS_INTL_PLURAL_OTHER; category++) {
        QJSICU4XRuleBytes rule = formatter->data.rules[category];
        QJSIntlPluralOperands operands = {out->text.data, out->text.length, exponent};
        int matches;
        if (!(formatter->data.mask & (1u << category)))
            continue;
        error = qjs_intl_plural_rule_evaluate((QJSIntlBytes){(const char *)rule.data, rule.length},
                                              &operands, &matches);
        if (error)
            return error;
        if (matches) {
            out->selected = (QJSIntlPluralCategory)category;
            break;
        }
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_plural_select(const QJSIntlPluralFormatter *formatter,
                                     const QJSIntlMathematicalValue *value,
                                     QJSIntlPluralCategory *out)
{
    Resolution result = {0};
    QJSIntlStatus error;

    if (out)
        *out = QJS_INTL_PLURAL_OTHER;
    if (!out || !formatter)
        return QJS_INTL_INVALID_ARGUMENT;
    error = resolve(formatter, value, &result);
    if (!error)
        *out = result.selected;
    qjs_intl_decimal_result_clear(&formatter->allocator, &result.rounded);
    return error;
}

QJSIntlStatus qjs_intl_plural_select_range(const QJSIntlPluralFormatter *formatter,
                                           const QJSIntlMathematicalValue *first,
                                           const QJSIntlMathematicalValue *last,
                                           QJSIntlPluralCategory *out)
{
    Resolution a = {0}, b = {0};
    QJSIntlStatus error;

    if (out)
        *out = QJS_INTL_PLURAL_OTHER;
    if (!out || !formatter || !first || !last || first->kind == QJS_INTL_NAN ||
        last->kind == QJS_INTL_NAN)
        return QJS_INTL_INVALID_ARGUMENT;
    error = resolve(formatter, first, &a);
    if (!error)
        error = resolve(formatter, last, &b);
    if (!error) {
        if (a.text.length == b.text.length && !memcmp(a.text.data, b.text.data, a.text.length))
            *out = a.selected;
        else
            *out = (QJSIntlPluralCategory)formatter->data.ranges[a.selected * 6 + b.selected];
    }
    qjs_intl_decimal_result_clear(&formatter->allocator, &a.rounded);
    qjs_intl_decimal_result_clear(&formatter->allocator, &b.rounded);
    return error;
}
