/* CLDR standard/accounting patterns, alpha variants and legacy spacing.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "currency-pattern.h"
#include "../currency/boundary.h"
#include <string.h>

static int equals(QJSIntlBytes value, const char *text)
{
    size_t length = strlen(text);
    return value.length == length && !memcmp(value.data, text, length);
}

static int currency_token(QJSIntlNumberTokenKind kind)
{
    return kind >= QJS_INTL_NUMBER_TOKEN_CURRENCY && kind <= QJS_INTL_NUMBER_TOKEN_CURRENCY_NARROW;
}

static int needs_alpha(const QJSIntlNumberPatternData *data, const QJSIntlNumberAffix *form,
                       const QJSIntlCurrencyBoundary *boundary)
{
    QJSIntlNumberPatternToken token;
    uint32_t i, number = form->count;
    for (i = 0; i < form->count; i++) {
        qjs_intl_number_pattern_token_at(data, form, i, &token);
        if (token.kind == QJS_INTL_NUMBER_TOKEN_NUMBER) {
            number = i;
            break;
        }
    }
    for (i = 0; i < form->count; i++) {
        qjs_intl_number_pattern_token_at(data, form, i, &token);
        if (currency_token(token.kind) &&
            ((i < number && boundary->before_number) || (i > number && boundary->after_number)))
            return 1;
    }
    return 0;
}

static QJSIntlStatus spacing(const QJSIntlNumberLayouts *layouts, unsigned int start,
                            unsigned int matches, QJSIntlBytes *out)
{
    const QJSIntlNumberLayoutText *fields = layouts->fields + start;
    if (fields[0].state != QJS_INTL_LAYOUT_VALUE || fields[1].state != QJS_INTL_LAYOUT_VALUE ||
        fields[2].state != QJS_INTL_LAYOUT_VALUE)
        return QJS_INTL_OK;
    if (!equals(fields[0].text, "[[:^S:]&[:^Z:]]") || !equals(fields[1].text, "[:digit:]"))
        return QJS_INTL_UNSUPPORTED;
    if (matches)
        *out = fields[2].text;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_currency_spacing(const QJSIntlNumberLayouts *layouts,
                                              const QJSIntlCurrencyBoundary *boundary,
                                              QJSIntlBytes *before, QJSIntlBytes *after)
{
    QJSIntlBytes before_text = {0}, after_text = {0};
    QJSIntlStatus status;
    if (before)
        *before = before_text;
    if (after)
        *after = after_text;
    if (!layouts || !boundary || !before || !after)
        return QJS_INTL_INVALID_ARGUMENT;
    status = spacing(layouts, QJS_INTL_LAYOUT_CURRENCY_BEFORE_MATCH,
                     boundary->spacing_after_number, &before_text);
    if (!status)
        status = spacing(layouts, QJS_INTL_LAYOUT_CURRENCY_AFTER_MATCH,
                         boundary->spacing_before_number, &after_text);
    if (!status) {
        *before = before_text;
        *after = after_text;
    }
    return status;
}

QJSIntlStatus qjs_intl_number_currency_pattern(const QJSIntlNumberPatternData *patterns,
                                              const QJSIntlCurrencyData *currencies,
                                              const QJSIntlNumberLayoutData *layouts,
                                              uint32_t locale, uint32_t system, QJSIntlBytes code,
                                              QJSIntlBytes symbol, unsigned int accounting,
                                              QJSIntlCurrencyPattern *out)
{
    QJSIntlCurrencyPattern result = {0};
    QJSIntlNumberPattern alpha;
    QJSIntlNumberLayouts spacing_layouts;
    QJSIntlCurrencyBoundary boundary;
    QJSIntlCurrencyText override;
    QJSIntlStatus status;
    uint32_t index, i;
    int has_override = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !patterns || !currencies || !layouts || accounting > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_currency_boundary(symbol, &boundary);
    if (status)
        return status;
    status = qjs_intl_number_pattern_at(patterns, locale, system,
                                         accounting ? QJS_INTL_PATTERN_CURRENCY_ACCOUNTING
                                                    : QJS_INTL_PATTERN_CURRENCY_STANDARD,
                                         &result.pattern);
    if (status)
        return status;
    if (result.pattern.state != QJS_INTL_NUMBER_PATTERN_VALUE)
        return QJS_INTL_UNSUPPORTED;
    status = qjs_intl_currency_code_find(currencies, code, &index);
    if (status != QJS_INTL_OK && status != QJS_INTL_UNSUPPORTED)
        return status;
    if (!status && !accounting) {
        status = qjs_intl_currency_field_at(currencies, locale, index, QJS_INTL_CURRENCY_PATTERN,
                                             &override);
        if (status)
            return status;
        if (override.state == QJS_INTL_CURRENCY_VALUE) {
            uint32_t minimum = result.pattern.minimum_grouping_digits;
            status = qjs_intl_number_pattern_override(patterns, override.text, &result.pattern);
            if (status)
                return status;
            result.pattern.minimum_grouping_digits = minimum;
            has_override = 1;
        }
    }
    if (!has_override) {
        status = qjs_intl_number_pattern_at(patterns, locale, system,
                                             accounting ? QJS_INTL_PATTERN_CURRENCY_ACCOUNTING_ALPHA
                                                        : QJS_INTL_PATTERN_CURRENCY_STANDARD_ALPHA,
                                             &alpha);
        if (status)
            return status;
        if (alpha.state == QJS_INTL_NUMBER_PATTERN_VALUE) {
            for (i = 0; i < QJS_INTL_NUMBER_PATTERN_FORM_COUNT; i++)
                if (needs_alpha(patterns, &result.pattern.forms[i], &boundary))
                    result.pattern.forms[i] = alpha.forms[i];
            /* Modern alpha variants replace the legacy spacing mechanism for
               the whole family, including symbols choosing its ordinary form. */
            *out = result;
            return QJS_INTL_OK;
        }
    }
    status = qjs_intl_number_layouts_at(layouts, locale, system, &spacing_layouts);
    if (!status)
        status = qjs_intl_number_currency_spacing(&spacing_layouts, &boundary,
                                                   &result.before_currency, &result.after_currency);
    if (!status)
        *out = result;
    return status;
}
