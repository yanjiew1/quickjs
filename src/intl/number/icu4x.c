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
#include "icu4x-data.h"
#include "affix.h"
#include "body.h"
#include "exponent.h"
#include "round.h"
#include "../icu4x/common.h"
#include "../common/format-builder.h"
#include "../common/pair-pattern.h"
#include "../common/unary-pattern.h"
#include "../common/range-compose.h"
#include "../../icu4x/decimal.h"
#include "../../icu4x/currency-number.h"
#include <limits.h>
#include <string.h>

typedef struct NumberChoice {
    uint32_t source, default_system;
} NumberChoice;

struct QJSIntlNumberService {
    QJSIntlAllocator allocator;
    QJSICU4XInventory systems;
    QJSIntlBytes *available;
    NumberChoice *choices;
    size_t count;
};

struct QJSIntlNumberFormatter {
    QJSIntlAllocator allocator;
    QJSIntlNumberRounding rounding;
    QJSIntlNumberSign sign;
    QJSIntlNumberBody body;
    QJSIntlNumberAffixSymbols symbols;
    QJSIntlNumberPatternToken *tokens;
    size_t starts[4], counts[4]; /* Three outer sign forms, positive scientific affix. */
    uint16_t *storage;
    QJSIntlUTF16 exponent, exponent_minus;
    QJSIntlPairPattern range;
    QJSIntlUnaryPattern approximately;
};

static QJSIntlBytes text(uint32_t offset)
{
    const char *value = qjs_icu4x_number_strings + offset;
    return (QJSIntlBytes){value, strlen(value)};
}

static int order(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t common = a.length < b.length ? a.length : b.length;
    int result = common ? memcmp(a.data, b.data, common) : 0;
    return result ? result : (a.length > b.length) - (a.length < b.length);
}

static size_t system_index(QJSIntlBytes value)
{
    size_t low = 0, high = qjs_icu4x_number_system_count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int result = order(text(qjs_icu4x_number_systems[middle]), value);
        if (!result)
            return middle;
        if (result < 0)
            low = middle + 1;
        else
            high = middle;
    }
    return SIZE_MAX;
}

static int known_system(const QJSIntlNumberService *service, QJSIntlBytes value)
{
    size_t low = 0, high = service->systems.count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int result = order(service->systems.items[middle], value);
        if (!result)
            return 1;
        if (result < 0)
            low = middle + 1;
        else
            high = middle;
    }
    return 0;
}

static const QJSICU4XNumberRecord *record(uint32_t source, size_t system)
{
    return &qjs_icu4x_number_records[qjs_icu4x_number_rows[qjs_icu4x_number_locales[source].row +
                                                           system]];
}

void qjs_intl_number_close(QJSIntlNumberService *service)
{
    if (service) {
        qjs_icu4x_inventory_clear(&service->systems);
        if (service->available)
            service->allocator.free(service->allocator.opaque, service->available);
        if (service->choices)
            service->allocator.free(service->allocator.opaque, service->choices);
        service->allocator.free(service->allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_number_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                   QJSIntlNumberService **out)
{
    QJSIntlNumberService *service;
    QJSIntlBytes version = text(qjs_icu4x_number_version);
    QJSIntlStatus error;
    size_t i;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_icu4x_status(
        qjs_icu4x_decimal_check_version((const uint8_t *)version.data, version.length));
    if (error)
        return error;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    error = qjs_icu4x_inventory_open(allocator, qjs_icu4x_decimal_systems, &service->systems);
    if (error)
        goto fail;
    if (qjs_icu4x_number_locale_count > UINT32_MAX ||
        qjs_icu4x_number_locale_count > SIZE_MAX / sizeof(*service->available) ||
        qjs_icu4x_number_locale_count > SIZE_MAX / sizeof(*service->choices)) {
        error = QJS_INTL_OVERFLOW;
        goto fail;
    }
    service->available = allocator->malloc(allocator->opaque, qjs_icu4x_number_locale_count *
                                                                  sizeof(*service->available));
    service->choices = allocator->malloc(allocator->opaque,
                                         qjs_icu4x_number_locale_count * sizeof(*service->choices));
    if (!service->available || !service->choices) {
        error = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    /* Explicit symbol rows omit root-inheriting locales. Admit declared source
       candidates only after the real public constructor resolves their data. */
    for (i = 0; i < qjs_icu4x_number_locale_count; i++) {
        QJSIntlBytes tag = text(qjs_icu4x_number_locales[i].tag);
        QJSICU4XDecimal *handle = NULL;
        QJSICU4XDecimalSymbols symbols;
        size_t system;
        if (!strcmp(tag.data, "root"))
            continue;
        error = qjs_icu4x_status(
            qjs_icu4x_decimal_open((const uint8_t *)tag.data, tag.length, NULL, 0, &handle));
        if (!error)
            error = qjs_icu4x_status(qjs_icu4x_decimal_symbols(handle, &symbols));
        system = error ? SIZE_MAX
                       : system_index((QJSIntlBytes){(const char *)symbols.fields[6].data,
                                                     symbols.fields[6].length});
        qjs_icu4x_decimal_close(handle);
        if (!error &&
            (system == SIZE_MAX || !known_system(service, text(qjs_icu4x_number_systems[system]))))
            error = QJS_INTL_DATA_ERROR;
        if (error)
            goto fail;
        service->available[service->count] = tag;
        service->choices[service->count++] = (NumberChoice){(uint32_t)i, (uint32_t)system};
    }
    if (!service->count) {
        error = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_number_close(service);
    return error;
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
    *count = service->count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_default_system(const QJSIntlNumberService *service, size_t locale,
                                             QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out || locale >= service->count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = text(qjs_icu4x_number_systems[service->choices[locale].default_system]);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_system_available(const QJSIntlNumberService *service,
                                               QJSIntlBytes value, int *out)
{
    if (out)
        *out = 0;
    if (!service || !out || (!value.data && value.length))
        return QJS_INTL_INVALID_ARGUMENT;
    *out = system_index(value) != SIZE_MAX;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_system_supported(const QJSIntlNumberService *service, size_t locale,
                                               QJSIntlBytes value, int *out)
{
    if (out)
        *out = 0;
    if (!service || locale >= service->count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_number_system_available(service, value, out);
}

QJSIntlStatus qjs_intl_number_system_next(const QJSIntlNumberService *service, uint32_t *cursor,
                                          QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !cursor || !out || *cursor > qjs_icu4x_number_system_count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (*cursor == qjs_icu4x_number_system_count)
        return QJS_INTL_UNSUPPORTED;
    *out = text(qjs_icu4x_number_systems[(*cursor)++]);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_currency_digits(const QJSIntlNumberService *service,
                                              QJSIntlBytes code, uint32_t *out)
{
    if (out)
        *out = 0;
    if (!service || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_icu4x_status(
        qjs_icu4x_currency_digits((const uint8_t *)code.data, code.length, out));
}

QJSIntlStatus qjs_intl_number_currency_supported(const QJSIntlNumberService *service,
                                                 QJSIntlBytes code, int *out)
{
    size_t i;
    if (out)
        *out = 0;
    if (!service || !out || (!code.data && code.length))
        return QJS_INTL_INVALID_ARGUMENT;
    if (code.length != 3)
        return QJS_INTL_OK;
    for (i = 0; i < 3; i++)
        if (code.data[i] < 'A' || code.data[i] > 'Z')
            return QJS_INTL_OK;
    *out = 1;
    return QJS_INTL_OK;
}

void qjs_intl_number_formatter_close(QJSIntlNumberFormatter *formatter)
{
    if (formatter) {
        if (formatter->tokens)
            formatter->allocator.free(formatter->allocator.opaque, formatter->tokens);
        if (formatter->storage)
            formatter->allocator.free(formatter->allocator.opaque, formatter->storage);
        formatter->allocator.free(formatter->allocator.opaque, formatter);
    }
}

/* Decode once into one engine-owned arena, using the existing bounded scalar
   writer. No Rust payload/string pointer survives formatter construction. */
static QJSIntlStatus open_symbols(QJSIntlNumberFormatter *formatter,
                                  const QJSICU4XDecimalSymbols *symbols,
                                  const QJSICU4XNumberRecord *source)
{
    QJSIntlBytes fields[6] = {{(const char *)symbols->fields[0].data, symbols->fields[0].length},
                              {(const char *)symbols->fields[1].data, symbols->fields[1].length},
                              text(source->fields[7]),
                              text(source->fields[6]),
                              text(source->fields[4]),
                              text(source->fields[2])};
    QJSIntlUTF16 *targets[6] = {&formatter->body.decimal, &formatter->body.group,
                                &formatter->body.nan,     &formatter->body.infinity,
                                &formatter->exponent,     &formatter->exponent_minus};
    QJSIntlFormatBuilder builder = {.limit = SIZE_MAX / sizeof(uint16_t)};
    QJSIntlStatus error;
    size_t i, start;

    for (i = 0; i < 6; i++) {
        error = qjs_intl_format_builder_text(&builder, fields[i], NULL, NULL);
        if (error)
            return error;
    }
    if (!builder.length)
        return QJS_INTL_DATA_ERROR;
    formatter->storage =
        formatter->allocator.malloc(formatter->allocator.opaque, builder.length * sizeof(uint16_t));
    if (!formatter->storage)
        return QJS_INTL_NO_MEMORY;
    builder.result.text = formatter->storage;
    builder.limit = builder.length;
    builder.length = 0;
    for (i = 0; i < 6; i++) {
        start = builder.length;
        error = qjs_intl_format_builder_text(&builder, fields[i], NULL, NULL);
        if (error)
            return error;
        *targets[i] = (QJSIntlUTF16){formatter->storage + start, builder.length - start};
    }
    memcpy(formatter->body.digits, symbols->digits, sizeof(formatter->body.digits));
    formatter->body.primary_grouping = symbols->primary;
    formatter->body.secondary_grouping = symbols->secondary;
    formatter->body.minimum_grouping_digits = symbols->minimum;
    formatter->symbols = (QJSIntlNumberAffixSymbols){text(source->fields[2]),
                                                     text(source->fields[1]),
                                                     text(source->fields[0]),
                                                     text(source->fields[5]),
                                                     {0},
                                                     {0},
                                                     {0}};
    return QJS_INTL_OK;
}

static QJSIntlStatus open_affixes(QJSIntlNumberFormatter *formatter,
                                  const QJSICU4XNumberRecord *source, unsigned style)
{
    uint32_t outer = source->patterns[style == QJS_INTL_NUMBER_PERCENT ? 2 : 0];
    uint32_t science = source->patterns[1];
    const QJSICU4XNumberAffix *forms[4];
    size_t i, j, count = 0, at = 0;

    if (outer >= QJS_ICU4X_NUMBER_EMPTY || science >= QJS_ICU4X_NUMBER_EMPTY)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < 3; i++)
        forms[i] = &qjs_icu4x_number_patterns[outer].forms[i];
    forms[3] = &qjs_icu4x_number_patterns[science].forms[0];
    for (i = 0; i < 4; i++) {
        if (forms[i]->count > SIZE_MAX / sizeof(*formatter->tokens) - count)
            return QJS_INTL_OVERFLOW;
        count += forms[i]->count;
    }
    formatter->tokens = formatter->allocator.malloc(formatter->allocator.opaque,
                                                    count * sizeof(*formatter->tokens));
    if (!formatter->tokens)
        return QJS_INTL_NO_MEMORY;
    for (i = 0; i < 4; i++) {
        formatter->starts[i] = at;
        formatter->counts[i] = forms[i]->count;
        for (j = 0; j < forms[i]->count; j++) {
            const QJSICU4XNumberToken *token = &qjs_icu4x_number_tokens[forms[i]->begin + j];
            formatter->tokens[at++] = (QJSIntlNumberPatternToken){token->kind, text(token->text)};
        }
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_formatter_open(const QJSIntlNumberService *service, size_t locale,
                                             QJSIntlBytes system,
                                             const QJSIntlNumberOptions *options,
                                             QJSIntlNumberFormatter **out)
{
    QJSIntlNumberFormatter *formatter;
    const QJSICU4XNumberRecord *source;
    QJSICU4XDecimal *handle = NULL;
    QJSICU4XDecimalSymbols symbols;
    QJSIntlStatus error;
    QJSIntlBytes tag;
    size_t index, i;

    if (out)
        *out = NULL;
    if (!out || !service || !options || locale >= service->count ||
        (unsigned)options->style > QJS_INTL_NUMBER_UNIT ||
        (unsigned)options->notation > QJS_INTL_NUMBER_COMPACT ||
        (unsigned)options->grouping > QJS_INTL_NUMBER_GROUP_MIN2 ||
        (unsigned)options->sign_display > QJS_INTL_NUMBER_SIGN_NEGATIVE ||
        options->compact_long > 1 ||
        (unsigned)options->currency_display > QJS_INTL_NUMBER_CURRENCY_NAME ||
        (unsigned)options->currency_sign > QJS_INTL_NUMBER_CURRENCY_ACCOUNTING ||
        (unsigned)options->unit_display > QJS_INTL_NUMBER_UNIT_LONG ||
        !options->maximum_output_length ||
        options->maximum_output_length > SIZE_MAX / sizeof(uint16_t) ||
        qjs_intl_decimal_options_validate(&options->digits) || (!system.data && system.length))
        return QJS_INTL_INVALID_ARGUMENT;
    /* Private implementation stage: these styles are added next; no incomplete
       ICU4X profile is exposed to JavaScript or substituted with another style. */
    if (options->style > QJS_INTL_NUMBER_PERCENT || options->notation == QJS_INTL_NUMBER_COMPACT)
        return QJS_INTL_UNSUPPORTED;
    if (!system.length)
        system = text(qjs_icu4x_number_systems[service->choices[locale].default_system]);
    index = system_index(system);
    if (index == SIZE_MAX)
        return QJS_INTL_UNSUPPORTED;
    source = record(service->choices[locale].source, index);
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = service->allocator;
    formatter->rounding = (QJSIntlNumberRounding){
        options->digits, options->notation, options->style == QJS_INTL_NUMBER_PERCENT, NULL, 0};
    formatter->sign = options->sign_display;
    formatter->body.grouping = options->grouping;
    formatter->body.maximum_output_length = options->maximum_output_length;
    tag = service->available[locale];
    if (known_system(service, system)) {
        error = qjs_icu4x_status(qjs_icu4x_decimal_open((const uint8_t *)tag.data, tag.length,
                                                        (const uint8_t *)system.data, system.length,
                                                        &handle));
        if (!error)
            error = qjs_icu4x_status(qjs_icu4x_decimal_symbols(handle, &symbols));
    } else {
        /* A required digit system may be absent from the library. Use its
           matching CLDR numeric metadata; the locale default could
           have different separators (e.g. fa defaults to arabext, not latn). */
        memset(&symbols, 0, sizeof(symbols));
        memcpy(symbols.digits, qjs_icu4x_number_digits[index], sizeof(symbols.digits));
        symbols.primary = source->grouping[0];
        symbols.secondary = source->grouping[1];
        symbols.minimum = source->grouping[2];
        for (i = 0; i < 2; i++) {
            QJSIntlBytes field = text(source->fields[10 + i]);
            symbols.fields[i] = (QJSICU4XDecimalText){(const uint8_t *)field.data, field.length};
        }
        error = QJS_INTL_OK;
    }
    if (!error)
        error = open_symbols(formatter, &symbols, source);
    qjs_icu4x_decimal_close(handle);
    if (!error)
        error = open_affixes(formatter, source, options->style);
    if (!error)
        error = qjs_intl_pair_pattern_parse(text(source->fields[8]), &formatter->range);
    if (!error)
        error = qjs_intl_unary_pattern_parse(text(source->fields[9]), &formatter->approximately);
    if (!error && formatter->approximately.prefix.length &&
        formatter->approximately.suffix.length) {
        /* FormatApproximately inserts one sign record. A circumfix source
           template cannot express that operation; use the selected atomic
           sign as a prefix (ILND policy), rather than emit two sign records. */
        formatter->approximately = (QJSIntlUnaryPattern){text(source->fields[3]), {0}};
    }
    if (error) {
        qjs_intl_number_formatter_close(formatter);
        return error;
    }
    *out = formatter;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_format(const QJSIntlNumberFormatter *formatter,
                                     const QJSIntlMathematicalValue *value, QJSIntlFormatted *out)
{
    QJSIntlRoundedNumber rounded = {0};
    QJSIntlFormatted body = {0}, exponent = {0}, notation = {0};
    QJSIntlStatus error;
    unsigned form;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !value)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_intl_number_round(&formatter->allocator, &formatter->rounding, value, &rounded);
    if (!error)
        error = qjs_intl_number_sign_pattern(formatter->sign, &rounded, &form);
    if (!error)
        error = qjs_intl_number_body_format(
            &formatter->allocator, &formatter->body, rounded.kind,
            (QJSIntlBytes){rounded.number.text, rounded.number.length}, &body);
    if (!error && formatter->rounding.notation != QJS_INTL_NUMBER_STANDARD &&
        (rounded.kind == QJS_INTL_FINITE || rounded.kind == QJS_INTL_NEGATIVE_ZERO)) {
        error = qjs_intl_number_exponent_format(&formatter->allocator, &body,
                                                formatter->body.digits, formatter->exponent,
                                                formatter->exponent_minus, rounded.exponent,
                                                formatter->body.maximum_output_length, &exponent);
        if (!error)
            error = qjs_intl_number_affix_format(
                &formatter->allocator, formatter->tokens + formatter->starts[3],
                formatter->counts[3], &formatter->symbols, &exponent, 0,
                formatter->body.maximum_output_length, &notation);
        if (!error) {
            qjs_intl_formatted_clear(&formatter->allocator, &body);
            body = notation;
            memset(&notation, 0, sizeof(notation));
        }
    }
    if (!error)
        error = qjs_intl_number_affix_format(
            &formatter->allocator, formatter->tokens + formatter->starts[form],
            formatter->counts[form], &formatter->symbols, &body,
            form == QJS_INTL_NUMBER_PATTERN_PLUS, formatter->body.maximum_output_length, out);
    qjs_intl_formatted_clear(&formatter->allocator, &body);
    qjs_intl_formatted_clear(&formatter->allocator, &exponent);
    qjs_intl_formatted_clear(&formatter->allocator, &notation);
    qjs_intl_number_round_clear(&formatter->allocator, &rounded);
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

QJSIntlStatus qjs_intl_number_format_range(const QJSIntlNumberFormatter *formatter,
                                           const QJSIntlMathematicalValue *start,
                                           const QJSIntlMathematicalValue *end,
                                           QJSIntlFormattedRange *out)
{
    QJSIntlFormatted first = {0}, last = {0};
    QJSIntlStatus error;
    QJSIntlRangePiece pieces[5];
    const QJSIntlFormatted *bodies[2] = {&first, &last};
    size_t count;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !start || !end || start->kind == QJS_INTL_NAN ||
        end->kind == QJS_INTL_NAN)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_intl_number_format(formatter, start, &first);
    if (!error)
        error = qjs_intl_number_format(formatter, end, &last);
    if (!error) {
        if (first.length == last.length &&
            !memcmp(first.text, last.text, first.length * sizeof(uint16_t))) {
            pieces[0] =
                (QJSIntlRangePiece){NULL, formatter->approximately.prefix,
                                    QJS_INTL_PART_APPROXIMATELY_SIGN, QJS_INTL_RANGE_SHARED};
            pieces[1] = (QJSIntlRangePiece){&first, {0}, 0, QJS_INTL_RANGE_SHARED};
            pieces[2] =
                (QJSIntlRangePiece){NULL, formatter->approximately.suffix,
                                    QJS_INTL_PART_APPROXIMATELY_SIGN, QJS_INTL_RANGE_SHARED};
            count = 3;
        } else {
            unsigned first_source = formatter->range.first;
            pieces[0] = (QJSIntlRangePiece){NULL, formatter->range.prefix, QJS_INTL_PART_LITERAL,
                                            QJS_INTL_RANGE_SHARED};
            pieces[1] =
                (QJSIntlRangePiece){bodies[first_source],
                                    {0},
                                    0,
                                    first_source ? QJS_INTL_RANGE_END : QJS_INTL_RANGE_START};
            pieces[2] = (QJSIntlRangePiece){NULL, formatter->range.separator, QJS_INTL_PART_LITERAL,
                                            QJS_INTL_RANGE_SHARED};
            pieces[3] =
                (QJSIntlRangePiece){bodies[1 - first_source],
                                    {0},
                                    0,
                                    first_source ? QJS_INTL_RANGE_START : QJS_INTL_RANGE_END};
            pieces[4] = (QJSIntlRangePiece){NULL, formatter->range.suffix, QJS_INTL_PART_LITERAL,
                                            QJS_INTL_RANGE_SHARED};
            count = 5;
        }
        error = qjs_intl_range_compose(&formatter->allocator, pieces, count,
                                       formatter->body.maximum_output_length, out);
    }
    qjs_intl_formatted_clear(&formatter->allocator, &first);
    qjs_intl_formatted_clear(&formatter->allocator, &last);
    return error;
}
