/* Fixed date-field code semantics shared by bindings and data consumers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
