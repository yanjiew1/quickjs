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
#include "number.h"
#include "../icu4c/numbering.h"
#include "../icu4c/number-fields.h"
#include "../icu4c/skeleton.h"
#include "../unit/identifier.h"
#include <limits.h>
#include <string.h>
#include <unicode/ucurr.h>
#include <unicode/unum.h>
#include <unicode/unumberformatter.h>
#include <unicode/unumberrangeformatter.h>

struct QJSIntlNumberService {
    QJSIntlAllocator allocator;
    QJSICU4CLocales locales;
    QJSICU4CNumbering numbering;
};

struct QJSIntlNumberFormatter {
    QJSIntlAllocator allocator;
    QJSIntlNumberStyle style;
    size_t limit, numeric_limit;
    UNumberFormatter *single;
    UNumberRangeFormatter *range;
};

QJSIntlStatus qjs_intl_number_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                   QJSIntlNumberService **out)
{
    QJSIntlNumberService *service;
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
    error = qjs_icu4c_locales_open(allocator, unum_countAvailable(), unum_getAvailable,
                                   &service->locales);
    if (!error)
        error = qjs_icu4c_numbering_open(allocator, &service->locales, &service->numbering);
    if (error) {
        qjs_intl_number_close(service);
        return error;
    }
    *out = service;
    return QJS_INTL_OK;
}

void qjs_intl_number_close(QJSIntlNumberService *service)
{
    if (service) {
        qjs_icu4c_numbering_clear(&service->numbering);
        qjs_icu4c_locales_clear(&service->locales);
        service->allocator.free(service->allocator.opaque, service);
    }
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
    *out = service->locales.tags;
    *count = service->locales.count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_default_system(const QJSIntlNumberService *service, size_t locale,
                                             QJSIntlBytes *out)
{
    return qjs_icu4c_numbering_default(service ? &service->numbering : NULL, locale, out);
}

QJSIntlStatus qjs_intl_number_system_available(const QJSIntlNumberService *service,
                                               QJSIntlBytes name, int *out)
{
    return qjs_icu4c_numbering_supported(service ? &service->numbering : NULL, name, out);
}

QJSIntlStatus qjs_intl_number_system_supported(const QJSIntlNumberService *service, size_t locale,
                                               QJSIntlBytes name, int *out)
{
    if (!service || locale >= service->locales.count) {
        if (out)
            *out = 0;
        return QJS_INTL_INVALID_ARGUMENT;
    }
    return qjs_intl_number_system_available(service, name, out);
}

QJSIntlStatus qjs_intl_number_system_next(const QJSIntlNumberService *service, uint32_t *cursor,
                                          QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !cursor || !out || *cursor > service->numbering.count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (*cursor == service->numbering.count)
        return QJS_INTL_UNSUPPORTED;
    *out = service->numbering.names[(*cursor)++];
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_currency_supported(const QJSIntlNumberService *service,
                                                 QJSIntlBytes code, int *out)
{
    size_t i;

    if (out)
        *out = 0;
    if (!out || !service || (code.length && !code.data))
        return QJS_INTL_INVALID_ARGUMENT;
    if (code.length != 3)
        return QJS_INTL_OK;
    for (i = 0; i < 3; i++) {
        if (code.data[i] < 'A' || code.data[i] > 'Z')
            return QJS_INTL_OK;
    }
    *out = 1;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_currency_digits(const QJSIntlNumberService *service,
                                              QJSIntlBytes code, uint32_t *out)
{
    UErrorCode status = U_ZERO_ERROR;
    UChar iso[4];
    int supported;
    int32_t digits;
    size_t i;
    QJSIntlStatus error;

    if (out)
        *out = 0;
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_intl_number_currency_supported(service, code, &supported);
    if (error || !supported)
        return error ? error : QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < 3; i++)
        iso[i] = (UChar)code.data[i];
    iso[3] = 0;
    digits = ucurr_getDefaultFractionDigitsForUsage(iso, UCURR_USAGE_STANDARD, &status);
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    if (digits < 0 || digits > 100)
        return QJS_INTL_DATA_ERROR;
    *out = (uint32_t)digits;
    return QJS_INTL_OK;
}

static QJSIntlStatus skeleton(const QJSIntlNumberOptions *options, QJSICU4CSkeleton *s)
{
    static const char *const notations[] = {"", "scientific ", "engineering ", "compact-short "};
    static const char *const groups[] = {"group-off ", "group-auto ", "group-on-aligned ",
                                         "group-min2 "};
    static const char *const signs[] = {"sign-auto ", "sign-never ", "sign-always ",
                                        "sign-except-zero ", "sign-negative "};
    static const char *const accounting[] = {
        "sign-accounting ", "sign-never ", "sign-accounting-always ",
        "sign-accounting-except-zero ", "sign-accounting-negative "};
    static const char *const currencies[] = {"short ", "narrow ", "iso-code ", "full-name "};
    static const char *const units[] = {"short ", "narrow ", "full-name "};
    QJSIntlStatus error;

    if (!options || (unsigned int)options->style > QJS_INTL_NUMBER_UNIT ||
        (unsigned int)options->notation > QJS_INTL_NUMBER_COMPACT ||
        (unsigned int)options->grouping > QJS_INTL_NUMBER_GROUP_MIN2 ||
        (unsigned int)options->sign_display > QJS_INTL_NUMBER_SIGN_NEGATIVE ||
        options->compact_long > 1 || !options->maximum_output_length ||
        (unsigned int)options->currency_display > QJS_INTL_NUMBER_CURRENCY_NAME ||
        (unsigned int)options->currency_sign > QJS_INTL_NUMBER_CURRENCY_ACCOUNTING ||
        (unsigned int)options->unit_display > QJS_INTL_NUMBER_UNIT_LONG)
        return QJS_INTL_INVALID_ARGUMENT;
    if (options->style == QJS_INTL_NUMBER_PERCENT)
        qjs_icu4c_skeleton_token(s, "percent scale/100 ");
    if (options->style == QJS_INTL_NUMBER_CURRENCY) {
        size_t i;

        if (!options->currency.data || options->currency.length != 3)
            return QJS_INTL_INVALID_ARGUMENT;
        for (i = 0; i < 3; i++) {
            if (options->currency.data[i] < 'A' || options->currency.data[i] > 'Z')
                return QJS_INTL_INVALID_ARGUMENT;
        }
        qjs_icu4c_skeleton_token(s, "currency/");
        qjs_icu4c_skeleton_append(s, options->currency);
        qjs_icu4c_skeleton_token(s, " unit-width-");
        qjs_icu4c_skeleton_token(s, currencies[options->currency_display]);
    } else if (options->style == QJS_INTL_NUMBER_UNIT) {
        QJSIntlUnitIdentifier unit;
        const QJSIntlUnitDefinition *numerator, *denominator;

        error = qjs_intl_unit_identifier_parse(options->unit, &unit);
        if (error)
            return error;
        numerator = qjs_intl_unit_definition_at(unit.numerator);
        denominator = qjs_intl_unit_definition_at(unit.denominator);
        qjs_icu4c_skeleton_token(s, "measure-unit/");
        qjs_icu4c_skeleton_token(s, numerator->cldr_identifier);
        if (denominator) {
            qjs_icu4c_skeleton_token(s, " per-measure-unit/");
            qjs_icu4c_skeleton_token(s, denominator->cldr_identifier);
        }
        qjs_icu4c_skeleton_token(s, " unit-width-");
        qjs_icu4c_skeleton_token(s, units[options->unit_display]);
    }
    if (options->notation == QJS_INTL_NUMBER_COMPACT && options->compact_long)
        qjs_icu4c_skeleton_token(s, "compact-long ");
    else
        qjs_icu4c_skeleton_token(s, notations[options->notation]);
    error = qjs_icu4c_digit_skeleton(s, &options->digits);
    if (error)
        return error;
    qjs_icu4c_skeleton_token(s, groups[options->grouping]);
    qjs_icu4c_skeleton_token(s,
                             options->style == QJS_INTL_NUMBER_CURRENCY &&
                                     options->currency_sign == QJS_INTL_NUMBER_CURRENCY_ACCOUNTING
                                 ? accounting[options->sign_display]
                                 : signs[options->sign_display]);
    return s->error;
}

QJSIntlStatus qjs_intl_number_formatter_open(const QJSIntlNumberService *service, size_t locale,
                                             QJSIntlBytes system,
                                             const QJSIntlNumberOptions *options,
                                             QJSIntlNumberFormatter **out)
{
    QJSIntlNumberFormatter *formatter = NULL;
    QJSICU4CSkeleton tokens = {0};
    QJSIntlStatus error;
    UErrorCode status = U_ZERO_ERROR;
    char *base = NULL, *selected = NULL;
    int supported;

    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->locales.count)
        return QJS_INTL_INVALID_ARGUMENT;
    error = skeleton(options, &tokens);
    if (error)
        return error;
    if (!system.length)
        system = service->numbering.defaults[locale];
    error = qjs_icu4c_numbering_supported(&service->numbering, system, &supported);
    if (error || !supported)
        return error ? error : QJS_INTL_UNSUPPORTED;
    error = qjs_icu4c_locale_id(&service->allocator, service->locales.tags[locale], &base);
    if (!error)
        error = qjs_icu4c_locale_keyword(&service->allocator, base, "nu", system, &selected);
    if (error)
        goto done;
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = service->allocator;
    formatter->style = options->style;
    formatter->limit = options->maximum_output_length;
    formatter->numeric_limit = options->digits.maximum_output_length;
    formatter->single =
        unumf_openForSkeletonAndLocale(tokens.text, tokens.length, selected, &status);
    formatter->range = unumrf_openForSkeletonWithCollapseAndIdentityFallback(
        tokens.text, tokens.length, UNUM_RANGE_COLLAPSE_AUTO, UNUM_IDENTITY_FALLBACK_APPROXIMATELY,
        selected, NULL, &status);
    error = qjs_icu4c_status(status);
    if (!error) {
        *out = formatter;
        formatter = NULL;
    }
done:
    qjs_intl_number_formatter_close(formatter);
    if (base)
        service->allocator.free(service->allocator.opaque, base);
    if (selected)
        service->allocator.free(service->allocator.opaque, selected);
    return error;
}

void qjs_intl_number_formatter_close(QJSIntlNumberFormatter *formatter)
{
    if (formatter) {
        unumf_close(formatter->single);
        unumrf_close(formatter->range);
        formatter->allocator.free(formatter->allocator.opaque, formatter);
    }
}

static QJSIntlStatus decimal_text(const QJSIntlMathematicalValue *value, QJSIntlBytes *out)
{
    if (!value || (unsigned int)value->kind > QJS_INTL_NEGATIVE_ZERO)
        return QJS_INTL_INVALID_ARGUMENT;
    switch (value->kind) {
    case QJS_INTL_NAN:
        *out = (QJSIntlBytes){"NaN", 3};
        break;
    case QJS_INTL_POSITIVE_INFINITY:
        *out = (QJSIntlBytes){"Infinity", 8};
        break;
    case QJS_INTL_NEGATIVE_INFINITY:
        *out = (QJSIntlBytes){"-Infinity", 9};
        break;
    case QJS_INTL_NEGATIVE_ZERO:
        *out = (QJSIntlBytes){"-0", 2};
        break;
    default: {
        QJSIntlStatus error = qjs_intl_decimal_validate(value->decimal);

        if (error)
            return error;
        *out = value->decimal;
        break;
    }
    }
    return out->length > INT32_MAX ? QJS_INTL_OVERFLOW : QJS_INTL_OK;
}

static int zero_coefficient(QJSIntlBytes text)
{
    size_t i;

    for (i = 0; i < text.length && text.data[i] != 'e' && text.data[i] != 'E'; i++) {
        if (text.data[i] >= '1' && text.data[i] <= '9')
            return 0;
    }
    return 1;
}

static size_t code_points(const UChar *text, int32_t start, int32_t end)
{
    size_t count = 0;
    int32_t i;

    for (i = start; i < end; i++, count++) {
        if (text[i] >= 0xd800 && text[i] <= 0xdbff && i + 1 < end && text[i + 1] >= 0xdc00 &&
            text[i + 1] <= 0xdfff)
            i++;
    }
    return count;
}

static QJSIntlStatus numeric_bound(const QJSIntlNumberFormatter *formatter,
                                   const QJSIntlMathematicalValue *number,
                                   const UFormattedNumber *result)
{
    UErrorCode status = U_ZERO_ERROR;
    const UFormattedValue *value = unumf_resultAsValue(result, &status);
    UConstrainedFieldPosition *position = NULL;
    const UChar *text;
    int32_t length;
    size_t digits = 0, groups = 0;
    int decimal = 0;
    QJSIntlStatus error;

    text = ufmtval_getString(value, &length, &status);
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    if (length < 0)
        return QJS_INTL_DATA_ERROR;
    if ((size_t)length > formatter->limit)
        return QJS_INTL_OVERFLOW;
    if (number->kind != QJS_INTL_FINITE && number->kind != QJS_INTL_NEGATIVE_ZERO)
        return QJS_INTL_OK;
    position = ucfpos_open(&status);
    ucfpos_constrainCategory(position, UFIELD_CATEGORY_NUMBER, &status);
    while (ufmtval_nextPosition(value, position, &status)) {
        int32_t start, end, field = ucfpos_getField(position, &status);

        ucfpos_getIndexes(position, &start, &end, &status);
        if (U_FAILURE(status))
            break;
        if (start < 0 || end < start || end > length) {
            status = U_INVALID_FORMAT_ERROR;
            break;
        }
        if (field == UNUM_INTEGER_FIELD || field == UNUM_FRACTION_FIELD)
            digits += code_points(text, start, end);
        else if (field == UNUM_GROUPING_SEPARATOR_FIELD)
            groups += code_points(text, start, end);
        else if (field == UNUM_DECIMAL_SEPARATOR_FIELD)
            decimal = 1;
    }
    error = qjs_icu4c_status(status);
    if (!error) {
        if (groups > digits)
            error = QJS_INTL_DATA_ERROR;
        else if (digits - groups + (size_t)decimal > formatter->numeric_limit)
            error = QJS_INTL_OVERFLOW;
    }
    ucfpos_close(position);
    return error;
}

static QJSIntlStatus format_value(const QJSIntlNumberFormatter *formatter,
                                  const QJSIntlMathematicalValue *value, UFormattedNumber **out)
{
    QJSIntlBytes text;
    QJSIntlStatus error = decimal_text(value, &text);
    UErrorCode status = U_ZERO_ERROR;

    *out = NULL;
    if (error)
        return error;
    if (value->kind == QJS_INTL_FINITE && zero_coefficient(text)) {
        /* A zero coefficient is independent of even a huge exponent.
           Normalize it before ICU's narrower decimal exponent parser. */
        text = text.data[0] == '-' ? (QJSIntlBytes){"-0", 2} : (QJSIntlBytes){"0", 1};
    }
    *out = unumf_openResult(&status);
    unumf_formatDecimal(formatter->single, text.data, (int32_t)text.length, *out, &status);
    /* The decimal grammar and formatter have already been validated. ICU's
       digit/exponent ceilings report these statuses for valid huge inputs;
       classify that local size failure, without changing the global mapper. */
    if (status == U_ILLEGAL_ARGUMENT_ERROR || status == U_UNSUPPORTED_ERROR ||
        status == U_INPUT_TOO_LONG_ERROR)
        return QJS_INTL_OVERFLOW;
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    return numeric_bound(formatter, value, *out);
}

QJSIntlStatus qjs_intl_number_format(const QJSIntlNumberFormatter *formatter,
                                     const QJSIntlMathematicalValue *number, QJSIntlFormatted *out)
{
    UFormattedNumber *result = NULL;
    UErrorCode status = U_ZERO_ERROR;
    QJSIntlStatus error;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter)
        return QJS_INTL_INVALID_ARGUMENT;
    error = format_value(formatter, number, &result);
    if (!error) {
        const UFormattedValue *value = unumf_resultAsValue(result, &status);

        error = qjs_icu4c_status(status);
        if (!error)
            error = qjs_icu4c_number_parts(&formatter->allocator, value, number, formatter->style,
                                           formatter->limit, out);
    }
    unumf_closeResult(result);
    return error;
}

QJSIntlStatus qjs_intl_number_format_range(const QJSIntlNumberFormatter *formatter,
                                           const QJSIntlMathematicalValue *first,
                                           const QJSIntlMathematicalValue *last,
                                           QJSIntlFormattedRange *out)
{
    UFormattedNumber *a = NULL, *b = NULL;
    UFormattedNumberRange *range = NULL;
    UErrorCode status = U_ZERO_ERROR;
    QJSIntlStatus error;
    QJSICU4CNumberFields map = {0};
    QJSIntlFormattedRange formatted = {0};
    QJSIntlBytes first_text, last_text;
    const UFormattedValue *a_value, *b_value;
    const UChar *a_text, *b_text;
    int32_t a_length, b_length;
    size_t start, end;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !first || !last || first->kind == QJS_INTL_NAN ||
        last->kind == QJS_INTL_NAN)
        return QJS_INTL_INVALID_ARGUMENT;
    error = decimal_text(first, &first_text);
    if (!error)
        error = decimal_text(last, &last_text);
    if (!error)
        error = format_value(formatter, first, &a);
    if (!error)
        error = format_value(formatter, last, &b);
    if (error)
        goto done;
    a_value = unumf_resultAsValue(a, &status);
    b_value = unumf_resultAsValue(b, &status);
    a_text = ufmtval_getString(a_value, &a_length, &status);
    b_text = ufmtval_getString(b_value, &b_length, &status);
    if (U_FAILURE(status)) {
        error = qjs_icu4c_status(status);
        goto done;
    }
    if (a_length < 0 || b_length < 0) {
        error = QJS_INTL_DATA_ERROR;
        goto done;
    }
    /* ECMA-402 compares displayed strings, whereas ICU identity compares
       quantities. Hidden signs may make distinct inputs render identically. */
    if (a_length == b_length &&
        (!a_length || !memcmp(a_text, b_text, (size_t)a_length * sizeof(*a_text))))
        last_text = first_text;
    range = unumrf_openResult(&status);
    unumrf_formatDecimalRange(formatter->range, first_text.data, (int32_t)first_text.length,
                              last_text.data, (int32_t)last_text.length, range, &status);
    const UFormattedValue *value = unumrf_resultAsValue(range, &status);
    error = qjs_icu4c_status(status);
    if (!error)
        error = qjs_icu4c_number_fields(&formatter->allocator, value, formatter->limit, &map);
    if (error)
        goto done;
    if (map.length > SIZE_MAX / sizeof(*formatted.parts) ||
        map.length > SIZE_MAX / sizeof(*formatted.text)) {
        error = QJS_INTL_OVERFLOW;
        goto done;
    }
    if (map.length) {
        formatted.text = formatter->allocator.malloc(formatter->allocator.opaque,
                                                     map.length * sizeof(*formatted.text));
        formatted.parts = formatter->allocator.malloc(formatter->allocator.opaque,
                                                      map.length * sizeof(*formatted.parts));
        if (!formatted.text || !formatted.parts) {
            error = QJS_INTL_NO_MEMORY;
            goto done;
        }
        memcpy(formatted.text, map.text, map.length * sizeof(*formatted.text));
    }
    formatted.length = map.length;
    for (start = 0; start < map.length; start = end) {
        unsigned int source = map.flags[start] & (QJS_ICU4C_FIELD_START | QJS_ICU4C_FIELD_END);
        const QJSIntlMathematicalValue *number = source == QJS_ICU4C_FIELD_END ? last : first;
        QJSIntlPartType type =
            qjs_icu4c_number_part_type(map.fields[start], number, formatter->style);

        for (end = start + 1; end < map.length && map.fields[end] == map.fields[start] &&
                              map.flags[end] == map.flags[start];
             end++)
            ;
        formatted.parts[formatted.part_count++] =
            (QJSIntlRangePart){start, end, type,
                               source == QJS_ICU4C_FIELD_START ? QJS_INTL_RANGE_START
                               : source == QJS_ICU4C_FIELD_END ? QJS_INTL_RANGE_END
                                                               : QJS_INTL_RANGE_SHARED};
    }
    *out = formatted;
    memset(&formatted, 0, sizeof(formatted));
done:
    qjs_intl_formatted_range_clear(&formatter->allocator, &formatted);
    qjs_icu4c_number_fields_clear(&formatter->allocator, &map);
    unumf_closeResult(a);
    unumf_closeResult(b);
    unumrf_closeResult(range);
    return error;
}

void qjs_intl_number_result_clear(const QJSIntlAllocator *allocator, QJSIntlFormatted *out)
{
    qjs_intl_formatted_clear(allocator, out);
}

void qjs_intl_number_range_result_clear(const QJSIntlAllocator *allocator,
                                        QJSIntlFormattedRange *out)
{
    qjs_intl_formatted_range_clear(allocator, out);
}
