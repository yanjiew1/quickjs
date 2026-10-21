/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "pattern-info.h"
#include <string.h>

int qjs_intl_date_pattern_fix_calendar_year(QJSIntlDatePattern *pattern)
{
    size_t i;
    int month = 0, changed = 0;

    if (!pattern || !pattern->tokens)
        return 0;

    /* Some CLDR calendar-date patterns mistakenly use week-year Y. Treat it
       as calendar-year y only with a month and no week fields. This workaround
       for source-data mistakes is not required by LDML or ECMA-402. Quoted
       letters are literal tokens, so they cannot enable or disable the rule. */
    for (i = 0; i < pattern->count; i++) {
        unsigned int symbol = pattern->tokens[i].symbol;

        if (symbol == 'w' || symbol == 'W')
            return 0;
        if (symbol == 'M' || symbol == 'L')
            month = 1;
    }
    if (!month)
        return 0;

    for (i = 0; i < pattern->count; i++) {
        if (pattern->tokens[i].symbol == 'Y') {
            pattern->tokens[i].symbol = 'y';
            changed = 1;
        }
    }

    return changed;
}

static int text_width(size_t count)
{
    return count == 5 ? QJS_INTL_DATETIME_NARROW :
           count == 4 ? QJS_INTL_DATETIME_LONG : QJS_INTL_DATETIME_SHORT;
}

static QJSIntlStatus component(unsigned int symbol, size_t count, int *field, int *value,
                               unsigned int *year)
{
    *year = 0;
    if (!count)
        return QJS_INTL_UNSUPPORTED;
    switch (symbol) {
    case 'G':
        if (count > 5)
            return QJS_INTL_UNSUPPORTED;
        *field = QJS_INTL_DATETIME_ERA;
        *value = text_width(count);
        break;
    case 'y': case 'r': case 'U':
        if (symbol == 'U' && count > 5)
            return QJS_INTL_UNSUPPORTED;
        *year = symbol == 'y' ? QJS_INTL_DATE_YEAR_CALENDAR :
                symbol == 'r' ? QJS_INTL_DATE_YEAR_RELATED : QJS_INTL_DATE_YEAR_CYCLIC;
        *field = QJS_INTL_DATETIME_YEAR;
        *value = symbol == 'y' && count == 2 ? QJS_INTL_DATETIME_TWO_DIGIT : QJS_INTL_DATETIME_NUMERIC;
        break;
    case 'M': case 'L':
        if (count > 5)
            return QJS_INTL_UNSUPPORTED;
        *field = QJS_INTL_DATETIME_MONTH;
        *value = count > 2 ? text_width(count) :
                 count == 2 ? QJS_INTL_DATETIME_TWO_DIGIT : QJS_INTL_DATETIME_NUMERIC;
        break;
    case 'E': case 'e': case 'c':
        if (count > 6 || (symbol != 'E' && count < 3))
            return QJS_INTL_UNSUPPORTED;
        *field = QJS_INTL_DATETIME_WEEKDAY;
        *value = text_width(count);
        break;
    case 'a': case 'b': case 'B':
        if (count > 5)
            return QJS_INTL_UNSUPPORTED;
        *field = symbol == 'a' ? -1 : QJS_INTL_DATETIME_DAY_PERIOD;
        *value = text_width(count);
        break;
    case 'd': case 'h': case 'H': case 'K': case 'k': case 'm': case 's':
        if (count > 2)
            return QJS_INTL_UNSUPPORTED;
        *field = symbol == 'd' ? QJS_INTL_DATETIME_DAY :
                 symbol == 'm' ? QJS_INTL_DATETIME_MINUTE :
                 symbol == 's' ? QJS_INTL_DATETIME_SECOND : QJS_INTL_DATETIME_HOUR;
        *value = count == 2 ? QJS_INTL_DATETIME_TWO_DIGIT : QJS_INTL_DATETIME_NUMERIC;
        break;
    case 'S':
        if (count > 3)
            return QJS_INTL_UNSUPPORTED;
        *field = QJS_INTL_DATETIME_FRACTION;
        *value = (int)count;
        break;
    case 'z':
        if (count > 4)
            return QJS_INTL_UNSUPPORTED;
        *field = QJS_INTL_DATETIME_ZONE;
        *value = count == 4 ? QJS_INTL_DATETIME_ZONE_LONG : QJS_INTL_DATETIME_ZONE_SHORT;
        break;
    case 'v': case 'O':
        if (count != 1 && count != 4)
            return QJS_INTL_UNSUPPORTED;
        *field = QJS_INTL_DATETIME_ZONE;
        *value = symbol == 'v' ?
                     (count == 4 ? QJS_INTL_DATETIME_ZONE_LONG_GENERIC : QJS_INTL_DATETIME_ZONE_SHORT_GENERIC) :
                     (count == 4 ? QJS_INTL_DATETIME_ZONE_LONG_OFFSET : QJS_INTL_DATETIME_ZONE_SHORT_OFFSET);
        break;
    default:
        return QJS_INTL_UNSUPPORTED;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_pattern_info(const QJSIntlDatePattern *pattern,
                                       QJSIntlDatePatternInfo *out)
{
    QJSIntlDatePatternInfo result = {0};
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !pattern || !pattern->tokens || !pattern->count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (pattern->order != QJS_INTL_DATE_INTERVAL_DEFAULT)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < pattern->count; i++) {
        const QJSIntlDatePatternToken *token = &pattern->tokens[i];
        unsigned int year;
        int field, value;
        QJSIntlStatus status;
        if (!token->symbol)
            continue;
        status = component(token->symbol, token->width, &field, &value, &year);
        if (status)
            return status;
        if (token->symbol == 'a' || token->symbol == 'b' || token->symbol == 'B') {
            if (result.period_symbol)
                return QJS_INTL_UNSUPPORTED;
            result.period_symbol = token->symbol;
            result.period_width = token->width;
        }
        if (year) {
            if ((result.year_kinds & year) ||
                (result.year_kinds && (year == QJS_INTL_DATE_YEAR_CALENDAR ||
                                      (result.year_kinds & QJS_INTL_DATE_YEAR_CALENDAR))))
                return QJS_INTL_UNSUPPORTED;
            result.year_kinds |= year;
        } else if (field >= 0 && result.components.values[field]) {
            return QJS_INTL_UNSUPPORTED;
        }
        if (field >= 0) {
            result.components.values[field] = value;
            if (field == QJS_INTL_DATETIME_HOUR)
                result.hour_symbol = token->symbol;
        }
    }
    *out = result;
    return QJS_INTL_OK;
}
