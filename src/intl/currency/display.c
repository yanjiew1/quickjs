/* LDML currency display names and symbols; no number formatting or rounding.
 * CLDR 7cdeb9e7fac52c59f22fc7248b05f989ee5689c2, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "display.h"
#include <string.h>

static int currency_text(const QJSIntlCurrencyData *data, uint32_t locale, uint32_t code,
                         QJSIntlCurrencyField field, QJSIntlBytes *out)
{
    QJSIntlCurrencyText value;
    qjs_intl_currency_field_at(data, locale, code, field, &value);
    if ((value.state != QJS_INTL_CURRENCY_VALUE && value.state != QJS_INTL_CURRENCY_EMPTY) ||
        (field >= QJS_INTL_CURRENCY_NAME_EXACT_ZERO &&
         field <= QJS_INTL_CURRENCY_NAME_OTHER && value.from_root))
        return 0;
    *out = value.text;
    return 1;
}

QJSIntlStatus qjs_intl_currency_symbol(const QJSIntlCurrencyData *data, uint32_t locale,
                                        QJSIntlBytes code, unsigned int narrow, QJSIntlBytes *out)
{
    QJSIntlStatus status;
    uint32_t index;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count || narrow > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_currency_code_find(data, code, &index);
    if (status != QJS_INTL_OK && status != QJS_INTL_UNSUPPORTED)
        return status;
    if (!status && ((narrow && currency_text(data, locale, index, QJS_INTL_CURRENCY_NARROW, out)) ||
                    currency_text(data, locale, index, QJS_INTL_CURRENCY_SYMBOL, out)))
        return QJS_INTL_OK;
    *out = code;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_currency_name(const QJSIntlCurrencyData *data, uint32_t locale,
                                      QJSIntlBytes code, QJSIntlPluralCategory category, int exact,
                                      QJSIntlBytes *out)
{
    QJSIntlStatus status;
    uint32_t index;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        (unsigned int)category >= QJS_INTL_PLURAL_CATEGORY_COUNT || exact < -1 || exact > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_currency_code_find(data, code, &index);
    if (status != QJS_INTL_OK && status != QJS_INTL_UNSUPPORTED)
        return status;
    if (!status &&
        ((exact >= 0 && currency_text(data, locale, index,
                                     QJS_INTL_CURRENCY_NAME_EXACT_ZERO + exact, out)) ||
         currency_text(data, locale, index, QJS_INTL_CURRENCY_NAME_ZERO + category, out) ||
         (category != QJS_INTL_PLURAL_OTHER &&
          currency_text(data, locale, index, QJS_INTL_CURRENCY_NAME_OTHER, out)) ||
         currency_text(data, locale, index, QJS_INTL_CURRENCY_NAME, out)))
        return QJS_INTL_OK;
    *out = code;
    return QJS_INTL_OK;
}

