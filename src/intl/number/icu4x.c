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
#include "../currency/boundary.h"
#include "../icu4x/common.h"
#include "../common/format-builder.h"
#include "../common/pair-pattern.h"
#include "../common/unary-pattern.h"
#include "../common/range-compose.h"
#include "../../icu4x/decimal.h"
#include "../../icu4x/currency-number.h"
#include "../../icu4x/currency-format.h"
#include "../../icu4x/plural.h"
#include "../../icu4x/compact.h"
#include "../plural/rules.h"
#include <limits.h>
#include <string.h>

typedef struct NumberChoice {
    uint32_t source, default_system;
} NumberChoice;

typedef struct CurrencyNames {
    QJSICU4XCurrencyNameHandle *handle;
    QJSICU4XCurrencyName choices[6][3]; /* Category, then ordinary/exact 0/exact 1. */
} CurrencyNames;

typedef struct NumberPlural {
    QJSICU4XPlural *handle;
    QJSICU4XPluralData rules;
} NumberPlural;

typedef struct CompactNumbers {
    QJSICU4XCompactPatterns *handle;
    QJSIntlCompactScale *scales;
    QJSICU4XCompactPattern *choices; /* Row, category, ordinary/exact 0/exact 1. */
    size_t count;
} CompactNumbers;

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
    QJSICU4XCurrencySymbolHandle *currency_handle;
    char currency_code[3];
    CurrencyNames *names;
    NumberPlural *plural;
    CompactNumbers *compact;
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
        if (formatter->names) {
            qjs_icu4x_currency_name_close(formatter->names->handle);
            formatter->allocator.free(formatter->allocator.opaque, formatter->names);
        }
        if (formatter->plural) {
            qjs_icu4x_plural_close(formatter->plural->handle);
            formatter->allocator.free(formatter->allocator.opaque, formatter->plural);
        }
        if (formatter->compact) {
            CompactNumbers *compact = formatter->compact;
            qjs_icu4x_compact_patterns_close(compact->handle);
            if (compact->scales)
                formatter->allocator.free(formatter->allocator.opaque, compact->scales);
            if (compact->choices)
                formatter->allocator.free(formatter->allocator.opaque, compact->choices);
            formatter->allocator.free(formatter->allocator.opaque, compact);
        }
        qjs_icu4x_currency_symbol_close(formatter->currency_handle);
        if (formatter->tokens)
            formatter->allocator.free(formatter->allocator.opaque, formatter->tokens);
        if (formatter->storage)
            formatter->allocator.free(formatter->allocator.opaque, formatter->storage);
        formatter->allocator.free(formatter->allocator.opaque, formatter);
    }
}

/* Decode numeric symbols once into one engine-owned arena, using the existing
   bounded scalar writer. Currency text has its own retained payload below. */
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

static const QJSICU4XNumberCurrency *currency_override(uint32_t locale, QJSIntlBytes code)
{
    const QJSICU4XNumberCurrencyMap *map =
        &qjs_icu4x_number_currency_maps[qjs_icu4x_number_locales[locale].currencies];
    size_t low = 0, high = map->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const QJSICU4XNumberCurrency *row = &qjs_icu4x_number_currencies[map->begin + middle];
        int result = order(text(row->code), code);
        if (!result)
            return row;
        if (result < 0)
            low = middle + 1;
        else
            high = middle;
    }
    return NULL;
}

/* Public composite decimal payloads are preferred. The sparse supplement
   supplies only currency-specific fields omitted by the public data bake;
   it must never replace the requested digit repertoire with a default. */
static QJSIntlStatus currency_decimal(QJSIntlBytes locale, QJSIntlBytes system, QJSIntlBytes code,
                                      const QJSICU4XNumberRecord *source,
                                      const QJSICU4XNumberCurrency *override,
                                      QJSICU4XDecimalSymbols *symbols,
                                      QJSICU4XCurrencyDecimalHandle **handle)
{
    QJSICU4XCurrencyDecimal decimal;
    size_t i;
    QJSIntlStatus error;
    for (i = 0; i < 2; i++) {
        if (source->fields[18 + i] < QJS_ICU4X_NUMBER_EMPTY) {
            QJSIntlBytes field = text(source->fields[18 + i]);
            symbols->fields[i] = (QJSICU4XDecimalText){(const uint8_t *)field.data, field.length};
        }
    }
    error = qjs_icu4x_status(qjs_icu4x_currency_decimal_open(
        (const uint8_t *)locale.data, locale.length, (const uint8_t *)system.data, system.length,
        (const uint8_t *)code.data, code.length, handle));
    if (error == QJS_INTL_UNSUPPORTED) {
        if (override) {
            uint32_t refs[2] = {override->decimal, override->group};
            size_t i;
            for (i = 0; i < 2; i++) {
                if (refs[i] < QJS_ICU4X_NUMBER_EMPTY) {
                    QJSIntlBytes field = text(refs[i]);
                    symbols->fields[i] =
                        (QJSICU4XDecimalText){(const uint8_t *)field.data, field.length};
                }
            }
        }
        return QJS_INTL_OK;
    }
    if (!error)
        error = qjs_icu4x_status(qjs_icu4x_currency_decimal(*handle, &decimal));
    if (!error) {
        QJSICU4XDecimalText fields[2] = {decimal.decimal, decimal.group};
        uint32_t refs[2] = {override ? override->decimal : QJS_ICU4X_NUMBER_ABSENT,
                            override ? override->group : QJS_ICU4X_NUMBER_ABSENT};
        /* A composite public payload fills an absent per-code side from
           ordinary decimal symbols. Do not let that undo a general currency
           symbol on that side (currencyDecimal/currencyGroup). */
        for (i = 0; i < 2; i++)
            if (source->fields[18 + i] >= QJS_ICU4X_NUMBER_EMPTY ||
                refs[i] < QJS_ICU4X_NUMBER_EMPTY)
                symbols->fields[i] = fields[i];
        symbols->primary = decimal.primary;
        symbols->secondary = decimal.secondary;
        symbols->minimum = decimal.minimum;
    }
    return error;
}

static QJSIntlStatus currency_symbol(QJSIntlNumberFormatter *formatter, QJSIntlBytes locale,
                                     const QJSIntlNumberOptions *options)
{
    QJSICU4XCurrencySymbol symbol;
    QJSIntlStatus error;
    memcpy(formatter->currency_code, options->currency.data, 3);
    formatter->symbols.currency = (QJSIntlBytes){formatter->currency_code, 3};
    if (options->currency_display == QJS_INTL_NUMBER_CURRENCY_CODE)
        return QJS_INTL_OK;
    error = qjs_icu4x_status(qjs_icu4x_currency_symbol_open(
        (const uint8_t *)locale.data, locale.length, (const uint8_t *)options->currency.data, 3,
        options->currency_display == QJS_INTL_NUMBER_CURRENCY_NARROW, &formatter->currency_handle));
    if (!error)
        error = qjs_icu4x_status(qjs_icu4x_currency_symbol(formatter->currency_handle, &symbol));
    if (!error && symbol.found)
        formatter->symbols.currency = (QJSIntlBytes){(const char *)symbol.text, symbol.length};
    return error;
}

static int is_currency_token(uint32_t kind)
{
    return kind >= QJS_INTL_NUMBER_TOKEN_CURRENCY && kind <= QJS_INTL_NUMBER_TOKEN_CURRENCY_NARROW;
}

/* One selected cardinal rule owner serves compact affixes and outer names.
   Neither consumer rounds an amount or constructs an unrelated locale list. */
static QJSIntlStatus open_plural(QJSIntlNumberFormatter *formatter, QJSIntlBytes locale)
{
    NumberPlural *plural;
    QJSIntlPluralOperands probe = {"0", 1, 0};
    QJSIntlStatus error;
    unsigned category;
    if (formatter->plural)
        return QJS_INTL_OK;
    plural = formatter->allocator.malloc(formatter->allocator.opaque, sizeof(*plural));
    if (!plural)
        return QJS_INTL_NO_MEMORY;
    memset(plural, 0, sizeof(*plural));
    formatter->plural = plural;
    error = qjs_icu4x_status(
        qjs_icu4x_plural_open((const uint8_t *)locale.data, locale.length, 0, &plural->handle));
    if (!error)
        error = qjs_icu4x_status(qjs_icu4x_plural_data(plural->handle, &plural->rules));
    for (category = 0; !error && category < 6; category++) {
        if (plural->rules.mask & (1u << category)) {
            QJSICU4XRuleBytes rule = plural->rules.rules[category];
            int matches;
            error = qjs_intl_plural_rule_evaluate(
                (QJSIntlBytes){(const char *)rule.data, rule.length}, &probe, &matches);
        }
    }
    return error;
}

static QJSIntlStatus currency_names(QJSIntlNumberFormatter *formatter, QJSIntlBytes locale,
                                    QJSIntlBytes code)
{
    CurrencyNames *names;
    QJSIntlStatus error;
    unsigned category, exact;

    names = formatter->allocator.malloc(formatter->allocator.opaque, sizeof(*names));
    if (!names)
        return QJS_INTL_NO_MEMORY;
    memset(names, 0, sizeof(*names));
    formatter->names = names;
    error = qjs_icu4x_status(qjs_icu4x_currency_name_open((const uint8_t *)locale.data,
                                                          locale.length, (const uint8_t *)code.data,
                                                          code.length, &names->handle));
    if (!error)
        error = open_plural(formatter, locale);
    /* Snapshot choices here, never allocate Rust selectors or reinterpret
       packed pattern data in the formatting hot path. */
    for (category = 0; !error && category < 6; category++)
        for (exact = 0; !error && exact < 3; exact++)
            error = qjs_icu4x_status(qjs_icu4x_currency_name(
                names->handle, category, (int)exact - 1, &names->choices[category][exact]));
    return error;
}

static QJSIntlStatus open_compact(QJSIntlNumberFormatter *formatter, QJSIntlBytes locale,
                                  QJSIntlBytes system, unsigned long_style)
{
    CompactNumbers *compact;
    QJSIntlStatus error;
    uint32_t count, row, category, exact;
    size_t items;

    compact = formatter->allocator.malloc(formatter->allocator.opaque, sizeof(*compact));
    if (!compact)
        return QJS_INTL_NO_MEMORY;
    memset(compact, 0, sizeof(*compact));
    formatter->compact = compact;
    error = qjs_icu4x_status(qjs_icu4x_compact_patterns_open(
        (const uint8_t *)locale.data, locale.length, (const uint8_t *)system.data, system.length,
        long_style, &compact->handle));
    if (!error)
        error = qjs_icu4x_status(qjs_icu4x_compact_patterns_count(compact->handle, &count));
    if (error)
        return error;
    items = count;
    if (items > SIZE_MAX / sizeof(*compact->scales) ||
        items > SIZE_MAX / 18 / sizeof(*compact->choices))
        return QJS_INTL_OVERFLOW;
    compact->count = count;
    if (count) {
        compact->scales = formatter->allocator.malloc(formatter->allocator.opaque,
                                                      count * sizeof(*compact->scales));
        compact->choices = formatter->allocator.malloc(formatter->allocator.opaque,
                                                       items * 18 * sizeof(*compact->choices));
        if (!compact->scales || !compact->choices)
            return QJS_INTL_NO_MEMORY;
        error = open_plural(formatter, locale);
    }
    for (row = 0; !error && row < count; row++) {
        QJSICU4XCompactScale scale;
        error = qjs_icu4x_status(qjs_icu4x_compact_patterns_scale(compact->handle, row, &scale));
        if (error)
            return error;
        if (scale.exponent > scale.magnitude || scale.magnitude > QJS_INTL_DECIMAL_EXPONENT_LIMIT ||
            (row && scale.magnitude <= compact->scales[row - 1].magnitude))
            return QJS_INTL_DATA_ERROR;
        compact->scales[row] = (QJSIntlCompactScale){scale.magnitude, scale.exponent};
        for (category = 0; !error && category < 6; category++)
            for (exact = 0; !error && exact < 3; exact++)
                error = qjs_icu4x_status(qjs_icu4x_compact_patterns_get(
                    compact->handle, row, category, (int)exact - 1,
                    &compact->choices[((size_t)row * 6 + category) * 3 + exact]));
    }
    formatter->rounding.compact = compact->scales;
    formatter->rounding.compact_count = count;
    return error;
}

static QJSIntlStatus select_plural(const QJSIntlNumberFormatter *formatter,
                                   const QJSIntlRoundedNumber *rounded, uint32_t shift,
                                   unsigned *category, int *exact)
{
    size_t i;
    *category = QJS_INTL_PLURAL_OTHER;
    *exact = -1;
    if (rounded->kind == QJS_INTL_FINITE || rounded->kind == QJS_INTL_NEGATIVE_ZERO) {
        const QJSIntlDecimal *number = &rounded->number.rounded;
        QJSIntlPluralOperands operands = {rounded->number.text, rounded->number.length,
                                          (int32_t)shift};
        int64_t exponent = (int64_t)number->exponent + shift;
        /* Standard and scientific/engineering labels use the visible rounded
           mantissa. Preserve fraction zeros for category evaluation, while
           explicit 0/1 selectors use mathematical equality. This follows the
           living draft's scaled-x ordering and native policy; ICU's high-level
           formatter can instead choose a label from the expanded quantity.
           Compact outer names virtually expand it; compact affixes use shift 0. */
        if (number->length == 1 && number->digits[0] == '0')
            *exact = 0;
        else if (number->digits[0] == '1' && exponent <= 0 &&
                 (uint64_t)number->length == (uint64_t)(1 - exponent)) {
            for (i = 1; i < number->length && number->digits[i] == '0'; i++)
                ;
            if (i == number->length)
                *exact = 1;
        }
        for (*category = 0; *category < QJS_INTL_PLURAL_OTHER; (*category)++) {
            QJSICU4XRuleBytes rule = formatter->plural->rules.rules[*category];
            QJSIntlStatus error;
            int matches;
            if (!(formatter->plural->rules.mask & (1u << *category)))
                continue;
            error = qjs_intl_plural_rule_evaluate_shifted(
                (QJSIntlBytes){(const char *)rule.data, rule.length}, &operands, shift, &matches);
            if (error)
                return error;
            if (matches)
                break;
        }
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus format_currency_name(const QJSIntlNumberFormatter *formatter,
                                          const QJSIntlRoundedNumber *rounded,
                                          const QJSIntlFormatted *body, QJSIntlFormatted *out)
{
    const CurrencyNames *names = formatter->names;
    const QJSICU4XCurrencyName *choice;
    QJSIntlNumberPatternToken tokens[5];
    QJSIntlNumberAffixSymbols symbols = {0};
    unsigned category = QJS_INTL_PLURAL_OTHER;
    int exact = -1;
    size_t i, count = 0;

    uint32_t shift =
        formatter->rounding.notation == QJS_INTL_NUMBER_COMPACT ? (uint32_t)rounded->exponent : 0;
    QJSIntlStatus error = select_plural(formatter, rounded, shift, &category, &exact);
    if (error)
        return error;
    choice = &names->choices[category][exact + 1];
    symbols.currency = (QJSIntlBytes){(const char *)choice->name.data, choice->name.length};
    for (i = 0; i < 3; i++) {
        if (choice->literals[i].length)
            tokens[count++] = (QJSIntlNumberPatternToken){
                QJS_INTL_NUMBER_TOKEN_LITERAL,
                {(const char *)choice->literals[i].data, choice->literals[i].length}};
        if (i < 2)
            tokens[count++] = (QJSIntlNumberPatternToken){
                (choice->first ^ i) ? QJS_INTL_NUMBER_TOKEN_CURRENCY : QJS_INTL_NUMBER_TOKEN_NUMBER,
                {0}};
    }
    return qjs_intl_number_affix_format(&formatter->allocator, tokens, count, &symbols, body, 0,
                                        formatter->body.maximum_output_length, out);
}

static QJSIntlStatus compact_text(QJSIntlFormatBuilder *builder, QJSICU4XDecimalText text)
{
    size_t start = builder->length, first = SIZE_MAX, last = 0;
    QJSIntlStatus error = qjs_intl_format_builder_text(
        builder, (QJSIntlBytes){(const char *)text.data, text.length}, &first, &last);
    if (error)
        return error;
    /* Affix-internal spaces belong to the compact name; only its surrounding
       Unicode whitespace becomes literal parts, matching native composition. */
    if (first == SIZE_MAX)
        return qjs_intl_format_builder_part(builder, start, builder->length, QJS_INTL_PART_LITERAL,
                                            0);
    error = qjs_intl_format_builder_part(builder, start, first, QJS_INTL_PART_LITERAL, 0);
    if (!error)
        error = qjs_intl_format_builder_part(builder, first, last, QJS_INTL_PART_COMPACT, 0);
    if (!error)
        error =
            qjs_intl_format_builder_part(builder, last, builder->length, QJS_INTL_PART_LITERAL, 0);
    return error;
}

static QJSIntlStatus emit_compact(QJSIntlFormatBuilder *builder,
                                  const QJSICU4XCompactPattern *pattern,
                                  const QJSIntlFormatted *body)
{
    QJSIntlStatus error = compact_text(builder, pattern->prefix);
    if (!error && pattern->has_number)
        error = qjs_intl_format_builder_body(builder, body);
    if (!error)
        error = compact_text(builder, pattern->suffix);
    return error;
}

static QJSIntlStatus format_compact(const QJSIntlNumberFormatter *formatter,
                                    const QJSIntlRoundedNumber *rounded,
                                    const QJSIntlFormatted *body, QJSIntlFormatted *out)
{
    const CompactNumbers *compact = formatter->compact;
    const QJSICU4XCompactPattern *pattern;
    QJSIntlFormatBuilder builder = {.limit = formatter->body.maximum_output_length};
    QJSIntlStatus error;
    unsigned category;
    int exact;
    size_t low = 0, high = compact->count;

    /* Rows with the same exponent can have distinct words. Select by final
       magnitude after rounding, never by exponent alone. */
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (rounded->notation_magnitude >= 0 &&
            compact->scales[middle].magnitude <= (uint64_t)rounded->notation_magnitude)
            low = middle + 1;
        else
            high = middle;
    }
    if (!low || compact->scales[low - 1].exponent != (uint32_t)rounded->exponent)
        return QJS_INTL_DATA_ERROR;
    error = select_plural(formatter, rounded, 0, &category, &exact);
    if (error)
        return error;
    pattern = &compact->choices[((low - 1) * 6 + category) * 3 + (size_t)(exact + 1)];
    error = emit_compact(&builder, pattern, body);
    if (error)
        return error;
    if (!builder.length || !builder.count)
        return QJS_INTL_DATA_ERROR;
    builder.result.text =
        formatter->allocator.malloc(formatter->allocator.opaque, builder.length * sizeof(uint16_t));
    if (!builder.result.text)
        return QJS_INTL_NO_MEMORY;
    builder.result.parts = formatter->allocator.malloc(formatter->allocator.opaque,
                                                       builder.count * sizeof(QJSIntlPart));
    if (!builder.result.parts) {
        qjs_intl_formatted_clear(&formatter->allocator, &builder.result);
        return QJS_INTL_NO_MEMORY;
    }
    builder.length = builder.count = 0;
    error = emit_compact(&builder, pattern, body);
    if (error) {
        qjs_intl_formatted_clear(&formatter->allocator, &builder.result);
        return error;
    }
    builder.result.length = builder.length;
    builder.result.part_count = builder.count;
    *out = builder.result;
    return QJS_INTL_OK;
}

static int alpha_needed(const QJSICU4XNumberAffix *form, const QJSIntlCurrencyBoundary *boundary)
{
    size_t i, number = form->count;
    for (i = 0; i < form->count; i++) {
        if (qjs_icu4x_number_tokens[form->begin + i].kind == QJS_INTL_NUMBER_TOKEN_NUMBER) {
            number = i;
            break;
        }
    }
    /* Standard currency selection tests the symbol's closest character even
       across literal spacing. Compact notation has a separate adjacency rule. */
    for (i = 0; i < form->count; i++) {
        if (is_currency_token(qjs_icu4x_number_tokens[form->begin + i].kind) &&
            ((i < number && boundary->before_number) || (i > number && boundary->after_number)))
            return 1;
    }
    return 0;
}

static int equals(QJSIntlBytes value, const char *expected)
{
    size_t length = strlen(expected);
    return value.length == length && !memcmp(value.data, expected, length);
}

static QJSIntlStatus currency_spacing(const QJSICU4XNumberRecord *source, size_t start,
                                      unsigned matches, QJSIntlBytes *out)
{
    size_t i;
    for (i = 0; i < 3; i++)
        if (source->fields[start + i] >= QJS_ICU4X_NUMBER_EMPTY)
            return QJS_INTL_OK;
    if (!equals(text(source->fields[start]), "[[:^S:]&[:^Z:]]") ||
        !equals(text(source->fields[start + 1]), "[:digit:]"))
        return QJS_INTL_UNSUPPORTED;
    if (matches)
        *out = text(source->fields[start + 2]);
    return QJS_INTL_OK;
}

static QJSIntlStatus currency_forms(QJSIntlNumberFormatter *formatter,
                                    const QJSICU4XNumberRecord *source,
                                    const QJSICU4XNumberCurrency *override,
                                    const QJSIntlNumberOptions *options,
                                    const QJSICU4XNumberAffix **forms)
{
    unsigned accounting = options->currency_sign == QJS_INTL_NUMBER_CURRENCY_ACCOUNTING;
    uint32_t base = source->patterns[3 + accounting], alpha = source->patterns[5 + accounting];
    QJSIntlCurrencyBoundary boundary;
    QJSIntlStatus error;
    size_t i;

    if (!formatter->symbols.currency.length &&
        source->patterns[7 + accounting] < QJS_ICU4X_NUMBER_EMPTY) {
        base = source->patterns[7 + accounting];
        alpha = QJS_ICU4X_NUMBER_ABSENT;
    } else if (!accounting && override && override->pattern < QJS_ICU4X_NUMBER_EMPTY) {
        base = override->pattern;
        alpha = QJS_ICU4X_NUMBER_ABSENT;
    }
    if (base >= QJS_ICU4X_NUMBER_EMPTY)
        return QJS_INTL_DATA_ERROR;
    formatter->body.primary_grouping = qjs_icu4x_number_patterns[base].grouping[0];
    formatter->body.secondary_grouping = qjs_icu4x_number_patterns[base].grouping[1];
    error = qjs_intl_currency_boundary(formatter->symbols.currency, &boundary);
    if (error)
        return error;
    for (i = 0; i < 3; i++) {
        forms[i] = &qjs_icu4x_number_patterns[base].forms[i];
        if (alpha < QJS_ICU4X_NUMBER_EMPTY && alpha_needed(forms[i], &boundary))
            forms[i] = &qjs_icu4x_number_patterns[alpha].forms[i];
    }
    /* An available modern alpha family replaces legacy spacing even when its
       ordinary form wins. Explicit spacing is filtered against body digits later. */
    if (alpha < QJS_ICU4X_NUMBER_EMPTY)
        return QJS_INTL_OK;
    error = currency_spacing(source, 12, boundary.spacing_after_number,
                             &formatter->symbols.before_currency);
    if (!error)
        error = currency_spacing(source, 15, boundary.spacing_before_number,
                                 &formatter->symbols.after_currency);
    return error;
}

static QJSIntlStatus open_affixes(QJSIntlNumberFormatter *formatter,
                                  const QJSICU4XNumberRecord *source,
                                  const QJSIntlNumberOptions *options,
                                  const QJSICU4XNumberCurrency *override)
{
    uint32_t outer = source->patterns[options->style == QJS_INTL_NUMBER_PERCENT ? 2 : 0];
    uint32_t science = source->patterns[1];
    const QJSICU4XNumberAffix *forms[4];
    size_t i, j, count = 0, at = 0;
    QJSIntlStatus error;

    if (outer >= QJS_ICU4X_NUMBER_EMPTY || science >= QJS_ICU4X_NUMBER_EMPTY)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < 3; i++)
        forms[i] = &qjs_icu4x_number_patterns[outer].forms[i];
    formatter->body.primary_grouping = qjs_icu4x_number_patterns[outer].grouping[0];
    formatter->body.secondary_grouping = qjs_icu4x_number_patterns[outer].grouping[1];
    if (options->style == QJS_INTL_NUMBER_CURRENCY &&
        options->currency_display != QJS_INTL_NUMBER_CURRENCY_NAME) {
        error = currency_forms(formatter, source, override, options, forms);
        if (error)
            return error;
    }
    forms[3] = &qjs_icu4x_number_patterns[science].forms[0];
    if (options->notation == QJS_INTL_NUMBER_SCIENTIFIC ||
        options->notation == QJS_INTL_NUMBER_ENGINEERING) {
        formatter->body.primary_grouping = qjs_icu4x_number_patterns[science].grouping[0];
        formatter->body.secondary_grouping = qjs_icu4x_number_patterns[science].grouping[1];
    }
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
    const QJSICU4XNumberCurrency *override = NULL;
    QJSICU4XDecimal *handle = NULL;
    QJSICU4XCurrencyDecimalHandle *currency_handle = NULL;
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
    if (options->style == QJS_INTL_NUMBER_UNIT ||
        (options->notation == QJS_INTL_NUMBER_COMPACT &&
         options->style == QJS_INTL_NUMBER_CURRENCY &&
         options->currency_display != QJS_INTL_NUMBER_CURRENCY_NAME))
        return QJS_INTL_UNSUPPORTED;
    if (options->style == QJS_INTL_NUMBER_CURRENCY) {
        int supported;
        error = qjs_intl_number_currency_supported(service, options->currency, &supported);
        if (error || !supported)
            return error ? error : QJS_INTL_INVALID_ARGUMENT;
        override = currency_override(service->choices[locale].source, options->currency);
    }
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
    if (!error && options->style == QJS_INTL_NUMBER_CURRENCY)
        error = currency_decimal(tag, system, options->currency, source, override, &symbols,
                                 &currency_handle);
    if (!error)
        error = open_symbols(formatter, &symbols, source);
    qjs_icu4x_decimal_close(handle);
    qjs_icu4x_currency_decimal_close(currency_handle);
    if (!error && options->style == QJS_INTL_NUMBER_CURRENCY) {
        if (options->currency_display == QJS_INTL_NUMBER_CURRENCY_NAME)
            error = currency_names(formatter, tag, options->currency);
        else
            error = currency_symbol(formatter, tag, options);
    }
    if (!error)
        error = open_affixes(formatter, source, options, override);
    if (!error && options->notation == QJS_INTL_NUMBER_COMPACT)
        error = open_compact(formatter, tag, system, options->compact_long);
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
    QJSIntlFormatted body = {0}, exponent = {0}, notation = {0}, signed_body = {0};
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
    if (!error &&
        (formatter->rounding.notation == QJS_INTL_NUMBER_SCIENTIFIC ||
         formatter->rounding.notation == QJS_INTL_NUMBER_ENGINEERING) &&
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
    if (!error && formatter->compact && rounded.exponent &&
        (rounded.kind == QJS_INTL_FINITE || rounded.kind == QJS_INTL_NEGATIVE_ZERO)) {
        error = format_compact(formatter, &rounded, &body, &notation);
        if (!error) {
            qjs_intl_formatted_clear(&formatter->allocator, &body);
            body = notation;
            memset(&notation, 0, sizeof(notation));
        }
    }
    if (!error) {
        QJSIntlNumberAffixSymbols symbols = formatter->symbols;
        if (!body.length || (symbols.before_currency.length &&
                             !qjs_icu4x_is_decimal_digit(qjs_intl_number_body_edge(&body, 1))))
            symbols.before_currency = (QJSIntlBytes){0};
        if (!body.length || (symbols.after_currency.length &&
                             !qjs_icu4x_is_decimal_digit(qjs_intl_number_body_edge(&body, 0))))
            symbols.after_currency = (QJSIntlBytes){0};
        error = qjs_intl_number_affix_format(
            &formatter->allocator, formatter->tokens + formatter->starts[form],
            formatter->counts[form], &symbols, &body, form == QJS_INTL_NUMBER_PATTERN_PLUS,
            formatter->body.maximum_output_length, formatter->names ? &signed_body : out);
    }
    if (!error && formatter->names)
        error = format_currency_name(formatter, &rounded, &signed_body, out);
    qjs_intl_formatted_clear(&formatter->allocator, &body);
    qjs_intl_formatted_clear(&formatter->allocator, &exponent);
    qjs_intl_formatted_clear(&formatter->allocator, &notation);
    qjs_intl_formatted_clear(&formatter->allocator, &signed_body);
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
