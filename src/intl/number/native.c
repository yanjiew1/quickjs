/* Native standard decimal/percent NumberFormat service.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "number.h"
#include "affix.h"
#include "body.h"
#include "compact-data.h"
#include "compact-format.h"
#include "../plural/native-engine.h"
#include "symbol-data.h"
#include "../../cutils/cutils.h"
#include <limits.h>
#include <string.h>

enum { SCIENTIFIC_FORM = QJS_INTL_NUMBER_PATTERN_FORM_COUNT };

struct QJSIntlNumberService {
    QJSIntlAllocator allocator;
    QJSIntlNumberingData numbering;
    QJSIntlNumberPatternData patterns;
    QJSIntlNumberSymbolData symbols;
    QJSIntlCompactData compact;
    QJSIntlPluralData plurals;
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
    size_t offsets[SCIENTIFIC_FORM + 1], counts[SCIENTIFIC_FORM + 1];
    QJSIntlUTF16 exponent_separator, exponent_minus;
    QJSIntlCompactData compact;
    QJSIntlCompactMap compact_map;
    QJSIntlPluralRuleSet cardinal;
    QJSIntlCompactScale *scales;
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
    status = qjs_intl_compact_data_open(&service->numbering, allocator, &service->compact);
    if (status)
        goto fail;
    status = qjs_intl_plural_data_open(&data, allocator, &service->plurals);
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
        const uint16_t *owned[] = {
            formatter->body.decimal.data,       formatter->body.group.data,
            formatter->body.nan.data,           formatter->body.infinity.data,
            formatter->exponent_separator.data, formatter->exponent_minus.data};
        size_t i;
        for (i = 0; i < sizeof(owned) / sizeof(owned[0]); i++)
            if (owned[i])
                allocator.free(allocator.opaque, (void *)owned[i]);
        if (formatter->tokens)
            allocator.free(allocator.opaque, formatter->tokens);
        if (formatter->scales)
            allocator.free(allocator.opaque, formatter->scales);
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
    QJSIntlNumberPattern pattern, scientific = {0};
    QJSIntlNumberSymbols symbols;
    QJSIntlStatus status;
    uint32_t index, source;
    size_t i, total = 0, position = 0;
    int has_scientific;
    const uint8_t *at, *end, *next;
    if (out)
        *out = NULL;
    if (!out || !service || !options || locale >= service->numbering.data.locales.count - 1 ||
        (unsigned int)options->style > QJS_INTL_NUMBER_PERCENT ||
        (unsigned int)options->notation > QJS_INTL_NUMBER_COMPACT ||
        (unsigned int)options->grouping > QJS_INTL_NUMBER_GROUP_MIN2 ||
        (unsigned int)options->sign_display > QJS_INTL_NUMBER_SIGN_NEGATIVE ||
        options->compact_long > 1 || !options->maximum_output_length ||
        qjs_intl_decimal_options_validate(&options->digits) ||
        (!system_name.data && system_name.length))
        return QJS_INTL_INVALID_ARGUMENT;
    has_scientific = options->notation == QJS_INTL_NUMBER_SCIENTIFIC ||
                     options->notation == QJS_INTL_NUMBER_ENGINEERING;
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
    if (has_scientific) {
        status = qjs_intl_number_pattern_at(&service->patterns, source, index,
                                            QJS_INTL_PATTERN_SCIENTIFIC, &scientific);
        if (status)
            return status;
        if (scientific.state != QJS_INTL_NUMBER_PATTERN_VALUE)
            return QJS_INTL_DATA_ERROR;
    }
    status = qjs_intl_number_symbols_at(&service->symbols, source, index, &symbols);
    if (status)
        return status;
    for (i = 0; i < QJS_INTL_NUMBER_PATTERN_FORM_COUNT; i++) {
        if (pattern.forms[i].count > SIZE_MAX / sizeof(*formatter->tokens) - total)
            return QJS_INTL_OVERFLOW;
        total += pattern.forms[i].count;
    }
    if (has_scientific) {
        if (scientific.forms[QJS_INTL_NUMBER_PATTERN_POSITIVE].count >
            SIZE_MAX / sizeof(*formatter->tokens) - total)
            return QJS_INTL_OVERFLOW;
        total += scientific.forms[QJS_INTL_NUMBER_PATTERN_POSITIVE].count;
    }
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = service->allocator;
    formatter->rounding.digits = options->digits;
    formatter->rounding.notation = options->notation;
    formatter->rounding.percent = options->style == QJS_INTL_NUMBER_PERCENT;
    formatter->sign_display = options->sign_display;
    formatter->body.primary_grouping =
        has_scientific ? scientific.primary_grouping : pattern.primary_grouping;
    formatter->body.secondary_grouping =
        has_scientific ? scientific.secondary_grouping : pattern.secondary_grouping;
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
    for (i = 0; i < QJS_INTL_NUMBER_PATTERN_FORM_COUNT; i++) {
        uint32_t j;
        formatter->offsets[i] = position;
        formatter->counts[i] = pattern.forms[i].count;
        for (j = 0; j < pattern.forms[i].count; j++)
            qjs_intl_number_pattern_token_at(&service->patterns, &pattern.forms[i], j,
                                             &formatter->tokens[position++]);
    }
    if (has_scientific) {
        uint32_t j;
        formatter->offsets[SCIENTIFIC_FORM] = position;
        formatter->counts[SCIENTIFIC_FORM] =
            scientific.forms[QJS_INTL_NUMBER_PATTERN_POSITIVE].count;
        for (j = 0; j < formatter->counts[SCIENTIFIC_FORM]; j++) {
            QJSIntlNumberPatternToken *token = &formatter->tokens[position++];
            qjs_intl_number_pattern_token_at(
                &service->patterns, &scientific.forms[QJS_INTL_NUMBER_PATTERN_POSITIVE], j, token);
            if (token->kind != QJS_INTL_NUMBER_TOKEN_NUMBER &&
                token->kind != QJS_INTL_NUMBER_TOKEN_LITERAL) {
                status = QJS_INTL_DATA_ERROR;
                goto fail;
            }
        }
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
    if (!status && has_scientific)
        status = decode(&service->allocator, symbols.fields[QJS_INTL_SYMBOL_EXPONENTIAL].text,
                        &formatter->exponent_separator);
    if (!status && has_scientific)
        status = decode(&service->allocator, symbols.fields[QJS_INTL_SYMBOL_MINUS].text,
                        &formatter->exponent_minus);
    if (!status && options->notation == QJS_INTL_NUMBER_COMPACT) {
        QJSIntlCompactPattern entry;
        uint32_t j;
        size_t count = 0;
        formatter->compact = service->compact;
        status = qjs_intl_compact_map_at(&service->compact, source, index,
                                         options->compact_long ? QJS_INTL_COMPACT_DECIMAL_LONG
                                                               : QJS_INTL_COMPACT_DECIMAL_SHORT,
                                         &formatter->compact_map);
        if (status)
            goto fail;
        status = qjs_intl_plural_rules_at(&service->plurals, source, QJS_INTL_PLURAL_CARDINAL,
                                          &formatter->cardinal);
        if (status)
            goto fail;
        for (j = 0; j < formatter->compact_map.count; j++) {
            qjs_intl_compact_pattern_at(&service->compact, &formatter->compact_map, j, &entry);
            if (entry.count == QJS_INTL_COMPACT_OTHER && entry.variant == QJS_INTL_COMPACT_STANDARD)
                count++;
        }
        if (!count || count > SIZE_MAX / sizeof(*formatter->scales)) {
            status = QJS_INTL_DATA_ERROR;
            goto fail;
        }
        formatter->scales = service->allocator.malloc(service->allocator.opaque,
                                                      count * sizeof(*formatter->scales));
        if (!formatter->scales) {
            status = QJS_INTL_NO_MEMORY;
            goto fail;
        }
        count = 0;
        for (j = 0; j < formatter->compact_map.count; j++) {
            qjs_intl_compact_pattern_at(&service->compact, &formatter->compact_map, j, &entry);
            if (entry.count == QJS_INTL_COMPACT_OTHER && entry.variant == QJS_INTL_COMPACT_STANDARD)
                formatter->scales[count++] = (QJSIntlCompactScale){entry.magnitude, entry.exponent};
        }
        formatter->rounding.compact = formatter->scales;
        formatter->rounding.compact_count = count;
    }
    if (status)
        goto fail;
    *out = formatter;
    return QJS_INTL_OK;
fail:
    qjs_intl_number_formatter_close(formatter);
    return status;
}

/* Append the scientific separator and exponent before applying any positive
   notation literals (for example Hindi's brackets). The style's sign belongs
   outside that complete notation body. PartitionNotationSubPattern specifies
   ASCII decimal exponent text, including its explicit "0" case; the open
   ECMA-402 issue 627 tracks missing transliteration. Source E+000 padding
   does not override the specification's exponent rules. */
static QJSIntlStatus append_exponent(const QJSIntlNumberFormatter *formatter,
                                     const QJSIntlFormatted *body, int32_t exponent,
                                     QJSIntlFormatted *out)
{
    QJSIntlUTF16 separator = formatter->exponent_separator;
    QJSIntlUTF16 minus = formatter->exponent_minus;
    uint32_t magnitude = exponent < 0 ? (uint32_t)-(int64_t)exponent : (uint32_t)exponent;
    uint32_t reversed[10];
    size_t count = 0, digit_length = 0, length = body->length, i, at, part;
    size_t extra_parts = exponent < 0 ? 3 : 2;
    do {
        uint32_t c = '0' + magnitude % 10;
        reversed[count++] = c;
        digit_length += c > 0xffff ? 2 : 1;
        magnitude /= 10;
    } while (magnitude);
    if (separator.length > formatter->body.maximum_output_length - length)
        return QJS_INTL_OVERFLOW;
    length += separator.length;
    if (exponent < 0) {
        if (minus.length > formatter->body.maximum_output_length - length)
            return QJS_INTL_OVERFLOW;
        length += minus.length;
    }
    if (digit_length > formatter->body.maximum_output_length - length)
        return QJS_INTL_OVERFLOW;
    length += digit_length;
    if (length > SIZE_MAX / sizeof(uint16_t) ||
        body->part_count > SIZE_MAX / sizeof(QJSIntlPart) - extra_parts)
        return QJS_INTL_OVERFLOW;
    out->text = formatter->allocator.malloc(formatter->allocator.opaque, length * sizeof(uint16_t));
    if (!out->text)
        return QJS_INTL_NO_MEMORY;
    out->parts = formatter->allocator.malloc(
        formatter->allocator.opaque, (body->part_count + extra_parts) * sizeof(QJSIntlPart));
    if (!out->parts) {
        qjs_intl_number_result_clear(&formatter->allocator, out);
        return QJS_INTL_NO_MEMORY;
    }
    out->length = length;
    out->part_count = body->part_count + extra_parts;
    memcpy(out->text, body->text, body->length * sizeof(uint16_t));
    memcpy(out->parts, body->parts, body->part_count * sizeof(QJSIntlPart));
    at = body->length;
    part = body->part_count;
    memcpy(out->text + at, separator.data, separator.length * sizeof(uint16_t));
    out->parts[part++] = (QJSIntlPart){at, at + separator.length, QJS_INTL_PART_EXPONENT_SEPARATOR};
    at += separator.length;
    if (exponent < 0) {
        memcpy(out->text + at, minus.data, minus.length * sizeof(uint16_t));
        out->parts[part++] =
            (QJSIntlPart){at, at + minus.length, QJS_INTL_PART_EXPONENT_MINUS_SIGN};
        at += minus.length;
    }
    out->parts[part] = (QJSIntlPart){at, length, QJS_INTL_PART_EXPONENT_INTEGER};
    for (i = count; i; i--) {
        uint32_t c = reversed[i - 1];
        if (c > 0xffff) {
            c -= 0x10000;
            out->text[at++] = 0xd800 + (c >> 10);
            out->text[at++] = 0xdc00 + (c & 1023);
        } else
            out->text[at++] = c;
    }
    return QJS_INTL_OK;
}

static int is_one(const QJSIntlDecimal *number)
{
    size_t i;
    if (number->digits[0] != '1' || (int64_t)number->length + number->exponent != 1)
        return 0;
    for (i = 1; i < number->length; i++)
        if (number->digits[i] != '0')
            return 0;
    return 1;
}

static int compact_candidate(const QJSIntlNumberFormatter *formatter, uint32_t magnitude,
                             QJSIntlCompactCount count, const QJSIntlCompactPattern *baseline,
                             QJSIntlCompactPattern *out)
{
    QJSIntlStatus status =
        qjs_intl_compact_pattern_find(&formatter->compact, &formatter->compact_map, magnitude,
                                      count, QJS_INTL_COMPACT_STANDARD, out);
    /* CLDR requires a common divisor for one threshold/family. Some pinned
       inherited forms disagree (e.g. vec's count-one raw "0"). Keep the
       selected scale: incompatible or unavailable forms use OTHER rather
       than dropping the magnitude or rounding at a second scale. */
    return !status && out->state == QJS_INTL_NUMBER_PATTERN_VALUE &&
           out->exponent == baseline->exponent;
}

static QJSIntlStatus compact_pattern(const QJSIntlNumberFormatter *formatter,
                                   const QJSIntlRoundedNumber *number,
                                   QJSIntlCompactPattern *out)
{
    size_t lo = 0, hi = formatter->rounding.compact_count;
    QJSIntlCompactPattern baseline, pattern;
    QJSIntlPluralOperands operands;
    QJSIntlPluralCategory category;
    QJSIntlStatus status;
    uint32_t magnitude;
    int exact = -1;
    if (!number->exponent)
        return QJS_INTL_UNSUPPORTED; /* No notation affix at exponent zero. */
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (number->notation_magnitude >= 0 &&
            formatter->scales[mid].magnitude <= (uint64_t)number->notation_magnitude)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (!lo)
        return QJS_INTL_DATA_ERROR;
    magnitude = formatter->scales[lo - 1].magnitude;
    qjs_intl_compact_pattern_find(&formatter->compact, &formatter->compact_map, magnitude,
                                  QJS_INTL_COMPACT_OTHER, QJS_INTL_COMPACT_STANDARD, &baseline);
    if (baseline.mode == QJS_INTL_COMPACT_FALLBACK)
        return QJS_INTL_UNSUPPORTED;
    if (number->number.rounded.length == 1 && number->number.rounded.digits[0] == '0')
        exact = QJS_INTL_COMPACT_EXACT_ZERO;
    else if (is_one(&number->number.rounded))
        exact = QJS_INTL_COMPACT_EXACT_ONE;
    if (exact < 0 || !compact_candidate(formatter, magnitude, exact, &baseline, &pattern)) {
        /* This plural context chooses the compact affix from the visible
           rounded mantissa, not the expanded quantity used for outer units. */
        operands = (QJSIntlPluralOperands){number->number.text, number->number.length, 0};
        status = qjs_intl_native_plural_select(&formatter->cardinal, &operands, &category);
        if (status)
            return status;
        if (!compact_candidate(formatter, magnitude, category + QJS_INTL_COMPACT_ZERO, &baseline,
                               &pattern))
            pattern = baseline;
    }
    *out = pattern;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_format(const QJSIntlNumberFormatter *formatter,
                                     const QJSIntlMathematicalValue *value, QJSIntlFormatted *out)
{
    QJSIntlRoundedNumber rounded = {0};
    QJSIntlFormatted body = {0}, exponent_body = {0}, notation_body = {0};
    QJSIntlCompactPattern compact = {0};
    QJSIntlStatus status;
    unsigned int form;
    int has_compact = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !value)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_number_round(&formatter->allocator, &formatter->rounding, value, &rounded);
    if (!status)
        status = qjs_intl_number_sign_pattern(formatter->sign_display, &rounded, &form);
    if (!status && formatter->rounding.notation == QJS_INTL_NUMBER_COMPACT &&
        (rounded.kind == QJS_INTL_FINITE || rounded.kind == QJS_INTL_NEGATIVE_ZERO)) {
        status = compact_pattern(formatter, &rounded, &compact);
        has_compact = status == QJS_INTL_OK;
        if (status == QJS_INTL_UNSUPPORTED)
            status = QJS_INTL_OK;
    }
    /* Literal compact forms omit the numeric body entirely (e.g. "mille").
       Its unused padded/localized digits must not consume the output bound. */
    if (!status && (!has_compact || compact.mode != QJS_INTL_COMPACT_LITERAL))
        status = qjs_intl_number_body_format(
            &formatter->allocator, &formatter->body, rounded.kind,
            (QJSIntlBytes){rounded.number.text, rounded.number.length}, &body);
    if (!status &&
        (formatter->rounding.notation == QJS_INTL_NUMBER_SCIENTIFIC ||
         formatter->rounding.notation == QJS_INTL_NUMBER_ENGINEERING) &&
        (rounded.kind == QJS_INTL_FINITE || rounded.kind == QJS_INTL_NEGATIVE_ZERO)) {
        status = append_exponent(formatter, &body, rounded.exponent, &exponent_body);
        if (!status)
            status = qjs_intl_number_affix_format(
                &formatter->allocator, formatter->tokens + formatter->offsets[SCIENTIFIC_FORM],
                formatter->counts[SCIENTIFIC_FORM], &formatter->symbols, &exponent_body, 0,
                formatter->body.maximum_output_length, &notation_body);
        if (!status) {
            qjs_intl_number_body_clear(&formatter->allocator, &body);
            body = notation_body;
            memset(&notation_body, 0, sizeof(notation_body));
        }
    }
    if (!status && has_compact) {
        status = qjs_intl_number_compact_format(
            &formatter->allocator, &formatter->compact, &compact, formatter->symbols.minus,
            formatter->symbols.plus, &body, formatter->body.maximum_output_length, &notation_body);
        if (!status) {
            qjs_intl_number_body_clear(&formatter->allocator, &body);
            body = notation_body;
            memset(&notation_body, 0, sizeof(notation_body));
        }
    }
    if (!status)
        status = qjs_intl_number_affix_format(
            &formatter->allocator, formatter->tokens + formatter->offsets[form],
            formatter->counts[form], &formatter->symbols, &body,
            form == QJS_INTL_NUMBER_PATTERN_PLUS, formatter->body.maximum_output_length, out);
    qjs_intl_number_body_clear(&formatter->allocator, &body);
    qjs_intl_number_body_clear(&formatter->allocator, &exponent_body);
    qjs_intl_number_body_clear(&formatter->allocator, &notation_body);
    qjs_intl_number_round_clear(&formatter->allocator, &rounded);
    return status;
}

void qjs_intl_number_result_clear(const QJSIntlAllocator *allocator, QJSIntlFormatted *out)
{
    qjs_intl_number_body_clear(allocator, out);
}
