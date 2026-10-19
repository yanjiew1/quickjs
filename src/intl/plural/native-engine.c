/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "native-engine.h"
#include "../../cutils/cutils.h"

static force_inline QJSIntlStatus plural_select(const QJSIntlPluralRuleSet *rules,
                                                 const QJSIntlPluralOperands *number,
                                                 uint32_t decimal_shift,
                                                 QJSIntlPluralCategory *out)
{
    unsigned int category;
    if (out)
        *out = QJS_INTL_PLURAL_OTHER;
    if (!out || !rules || !rules->ranges || !(rules->present & (1u << QJS_INTL_PLURAL_OTHER)))
        return QJS_INTL_INVALID_ARGUMENT;
    for (category = 0; category < QJS_INTL_PLURAL_CATEGORY_COUNT; category++) {
        if (rules->present & (1u << category)) {
            int matches;
            QJSIntlStatus status = decimal_shift
                ? qjs_intl_plural_rule_evaluate_shifted(rules->conditions[category], number,
                                                        decimal_shift, &matches)
                : qjs_intl_plural_rule_evaluate(rules->conditions[category], number, &matches);
            if (status != QJS_INTL_OK)
                return status;
            if (matches) {
                *out = (QJSIntlPluralCategory)category;
                return QJS_INTL_OK;
            }
        }
    }
    return QJS_INTL_DATA_ERROR; /* A validated empty other rule always matches. */
}

QJSIntlStatus qjs_intl_native_plural_select(const QJSIntlPluralRuleSet *rules,
                                            const QJSIntlPluralOperands *number,
                                            QJSIntlPluralCategory *out)
{
    return plural_select(rules, number, 0, out);
}

QJSIntlStatus qjs_intl_native_plural_select_shifted(const QJSIntlPluralRuleSet *rules,
                                                    const QJSIntlPluralOperands *number,
                                                    uint32_t decimal_shift,
                                                    QJSIntlPluralCategory *out)
{
    return plural_select(rules, number, decimal_shift, out);
}

QJSIntlStatus qjs_intl_native_plural_range(const QJSIntlPluralRuleSet *rules,
                                           QJSIntlPluralCategory start, QJSIntlPluralCategory end,
                                           QJSIntlPluralCategory *out)
{
    unsigned int value;
    if (out)
        *out = QJS_INTL_PLURAL_OTHER;
    if (!out || !rules || !rules->ranges || (unsigned int)start >= QJS_INTL_PLURAL_CATEGORY_COUNT ||
        (unsigned int)end >= QJS_INTL_PLURAL_CATEGORY_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    value = rules->ranges[(unsigned int)start * QJS_INTL_PLURAL_CATEGORY_COUNT + end];
    if (value == QJS_INTL_PLURAL_RANGE_ABSENT)
        value = end;
    if (value >= QJS_INTL_PLURAL_CATEGORY_COUNT)
        return QJS_INTL_DATA_ERROR;
    *out = (QJSIntlPluralCategory)value;
    return QJS_INTL_OK;
}
