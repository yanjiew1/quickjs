/* Fixed date-field code semantics shared by bindings and data consumers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "field.h"
#include <string.h>

QJSIntlStatus qjs_intl_date_field_from_code(QJSIntlBytes code, QJSIntlDateField *out)
{
    static const struct {
        const char *code;
        QJSIntlDateField field;
    } fields[] = {
        {"era", QJS_INTL_DATE_ERA}, {"year", QJS_INTL_DATE_YEAR},
        {"quarter", QJS_INTL_DATE_QUARTER}, {"month", QJS_INTL_DATE_MONTH},
        {"weekOfYear", QJS_INTL_DATE_WEEK_OF_YEAR}, {"weekday", QJS_INTL_DATE_WEEKDAY},
        {"day", QJS_INTL_DATE_DAY}, {"dayPeriod", QJS_INTL_DATE_DAY_PERIOD},
        {"hour", QJS_INTL_DATE_HOUR}, {"minute", QJS_INTL_DATE_MINUTE},
        {"second", QJS_INTL_DATE_SECOND}, {"timeZoneName", QJS_INTL_DATE_TIME_ZONE_NAME}
    };
    size_t i;
    if (out)
        *out = 0;
    if (!out || !code.data || !code.length || code.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < sizeof(fields) / sizeof(*fields); i++) {
        if (code.length == strlen(fields[i].code) && !memcmp(code.data, fields[i].code, code.length)) {
            *out = fields[i].field;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_INVALID_ARGUMENT;
}
