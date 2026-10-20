/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "dayperiod.h"
#include <string.h>

QJSIntlStatus qjs_intl_dayperiod_resolve(const QJSIntlDayPeriodData *data,
                                       QJSIntlDayPeriodRuleSetKind kind, QJSIntlBytes locale,
                                       QJSIntlDayPeriodRuleList *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !locale.data || !locale.length ||
        (unsigned int)kind >= QJS_INTL_DAYPERIOD_KIND_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    for (;;) {
        QJSIntlDayPeriodRuleList result;
        uint32_t map;
        size_t length;
        QJSIntlStatus status = qjs_intl_dayperiod_find(data, kind, locale, &map);
        if (!status) {
            status = qjs_intl_dayperiod_map(data, map, &result.start, &result.count);
            if (!status)
                *out = result;
            return status;
        }
        if (status != QJS_INTL_UNSUPPORTED)
            return status;
        if (locale.length == 4 && !memcmp(locale.data, "root", 4))
            return QJS_INTL_UNSUPPORTED;
        length = locale.length;
        while (length && locale.data[length - 1] != '-')
            length--;
        if (length)
            locale.length = length - 1;
        else
            locale = (QJSIntlBytes){"root", 4};
    }
}

QJSIntlStatus qjs_intl_dayperiod_select(const QJSIntlDayPeriodData *data,
                                      const QJSIntlDayPeriodRuleList *rules, unsigned int symbol,
                                      uint32_t second, unsigned int fraction, QJSIntlDayPeriodCode *out)
{
    QJSIntlDayPeriodCode result;
    uint32_t i;
    if (out)
        *out = QJS_INTL_DAYPERIOD_AM;
    if (!out || second >= 86400 || fraction > 1 || (symbol != 'a' && symbol != 'b' && symbol != 'B'))
        return QJS_INTL_INVALID_ARGUMENT;
    result = second < 43200 ? QJS_INTL_DAYPERIOD_AM : QJS_INTL_DAYPERIOD_PM;
    if (symbol == 'b' && !fraction && (second == 0 || second == 43200))
        result = second ? QJS_INTL_DAYPERIOD_NOON : QJS_INTL_DAYPERIOD_MIDNIGHT;
    if (symbol != 'B' || !rules) {
        *out = result;
        return QJS_INTL_OK;
    }
    if (!data || !data->section.bytes || rules->start > data->rule_count ||
        rules->count > data->rule_count - rules->start)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < rules->count; i++) {
        QJSIntlDayPeriodRule rule;
        QJSIntlStatus status = qjs_intl_dayperiod_rule(data, rules->start + i, &rule);
        if (status)
            return status;
        if (rule.form == QJS_INTL_DAYPERIOD_AT) {
            if (!fraction && second == rule.start) {
                *out = rule.code;
                return QJS_INTL_OK;
            }
        } else if (rule.code >= QJS_INTL_DAYPERIOD_MORNING1 &&
                   (rule.start < rule.end ? second >= rule.start && second < rule.end :
                                           second >= rule.start || second < rule.end)) {
            /* Fixed AM/PM may coexist with a full variable partition. Keep
               the variable match, while still allowing a later exact point
               to win independently of physical rule order. */
            result = rule.code;
        }
    }
    *out = result;
    return QJS_INTL_OK;
}
