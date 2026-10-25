/* Global Intl inventories coordinate executable service capabilities.
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
#include "../intl.h"
#include "../../../calendar/calendar.h"
#include "../../../intl/unit/identifier.h"
#include "../../../timezone/timezone.h"
#include <stdlib.h>

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int append(JSContext *ctx, JSValueConst array, uint32_t *index, QJSIntlBytes text)
{
    JSValue value;
    if (*index == UINT32_MAX) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    value = JS_NewStringLen(ctx, text.data, text.length);
    if (JS_IsException(value) || JS_DefinePropertyValueUint32(ctx, array, *index, value,
            JS_PROP_C_W_E | JS_PROP_THROW) < 0)
        return -1;
    (*index)++;
    return 0;
}

static int compare(const void *left, const void *right)
{
    const QJSIntlBytes *a = left, *b = right;
    size_t common = a->length < b->length ? a->length : b->length;
    int order = memcmp(a->data, b->data, common);
    return order ? order : (a->length > b->length) - (a->length < b->length);
}

static int calendars(JSContext *ctx, JSValueConst result)
{
    QJSIntlDateTimeService *service = js_intl_datetime_service(ctx);
    QJSIntlBytes names[QJS_CAL_COUNT];
    uint32_t index = 0;
    size_t count = 0, i;
    if (!service)
        return -1;
    for (i = 0; i < QJS_CAL_COUNT; i++) {
        QJSIntlBytes name = bytes(qjs_calendar_identifier(i));
        int supported;
        if (js_intl_error(ctx, qjs_intl_datetime_calendar_available(service, name, &supported),
                          "available calendars"))
            return -1;
        if (supported)
            names[count++] = name;
    }
    if (count > 1)
        qsort(names, count, sizeof(*names), compare);
    for (i = 0; i < count; i++)
        if (append(ctx, result, &index, names[i]))
            return -1;
    return 0;
}

static int currencies(JSContext *ctx, JSValueConst result)
{
    QJSIntlDisplayService *display = js_intl_display_service(ctx);
    QJSIntlNumberService *number = display ? js_intl_number_service(ctx) : NULL;
    uint32_t cursor = 0, index = 0;
    if (!number)
        return -1;
    for (;;) {
        QJSIntlBytes code;
        int supported;
        QJSIntlStatus status = qjs_intl_display_currency_next(display, &cursor, &code);
        if (status == QJS_INTL_UNSUPPORTED)
            return 0;
        if (js_intl_error(ctx, status, "available currency names") ||
            js_intl_error(ctx, qjs_intl_number_currency_supported(number, code, &supported),
                          "available currency formatting"))
            return -1;
        if (supported && append(ctx, result, &index, code))
            return -1;
    }
}

static int numbering_systems(JSContext *ctx, JSValueConst result)
{
    QJSIntlNumberService *number = js_intl_number_service(ctx);
    QJSIntlDateTimeService *date = number ? js_intl_datetime_service(ctx) : NULL;
    QJSIntlRelativeService *relative = date ? js_intl_relative_service(ctx) : NULL;
    uint32_t cursor = 0, index = 0;
    if (!relative)
        return -1;
    for (;;) {
        QJSIntlBytes name;
        int date_supported, relative_supported;
        QJSIntlStatus status = qjs_intl_number_system_next(number, &cursor, &name);
        if (status == QJS_INTL_UNSUPPORTED)
            return 0;
        if (js_intl_error(ctx, status, "available numeric systems") ||
            js_intl_error(ctx, qjs_intl_datetime_system_available(date, name, &date_supported),
                          "available date systems") ||
            js_intl_error(ctx, qjs_intl_relative_system_available(relative, name, &relative_supported),
                          "available relative systems"))
            return -1;
        if (date_supported && relative_supported && append(ctx, result, &index, name))
            return -1;
    }
}

static int collations(JSContext *ctx, JSValueConst result)
{
    QJSIntlCollationService *service = js_intl_collation_service(ctx);
    uint32_t cursor = 0, index = 0;

    if (!service)
        return -1;
    for (;;) {
        QJSIntlBytes name;
        QJSIntlStatus status = qjs_intl_collation_available_type_next(service, &cursor, &name);

        if (status == QJS_INTL_UNSUPPORTED)
            return 0;
        if (js_intl_error(ctx, status, "available collations") || append(ctx, result, &index, name))
            return -1;
    }
}

static int timezones(JSContext *ctx, JSValueConst result)
{
    uint32_t index = 0;
    size_t i, count = qjs_timezone_count();
    /* Filtering an already sorted unique identity list preserves that order.
       Primary non-geographic names and core UTC are part of the same facade. */
    for (i = 0; i < count; i++) {
        const QJSTimeZone *zone = qjs_timezone_at(i);
        if (qjs_timezone_is_primary(zone) &&
            append(ctx, result, &index, bytes(qjs_timezone_identifier(zone))))
            return -1;
    }
    return 0;
}

static int units(JSContext *ctx, JSValueConst result)
{
    uint32_t i, index = 0, count = qjs_intl_unit_definition_count();
    for (i = 0; i < count; i++) {
        const QJSIntlUnitDefinition *unit = qjs_intl_unit_definition_at(i);
        if (append(ctx, result, &index, bytes(unit->identifier)))
            return -1;
    }
    return 0;
}

JSValue js_intl_supported_values(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    static const struct {
        const char *name;
        int (*collect)(JSContext *, JSValueConst);
    } keys[] = {
        {"calendar", calendars},
        {"collation", collations},
        {"currency", currencies},
        {"numberingSystem", numbering_systems},
        {"timeZone", timezones},
        {"unit", units},
    };
    const char *key;
    size_t length, i;
    int status = 0;
    JSValue result;
    key = JS_ToCStringLen(ctx, &length, argc ? argv[0] : JS_UNDEFINED);
    if (!key)
        return JS_EXCEPTION;
    for (i = 0; i < countof(keys); i++)
        if (length == strlen(keys[i].name) && !memcmp(key, keys[i].name, length))
            break;
    JS_FreeCString(ctx, key);
    if (i == countof(keys))
        return JS_ThrowRangeError(ctx, "invalid Intl.supportedValuesOf key");
    result = JS_NewArray(ctx);
    if (JS_IsException(result))
        return result;
    if (keys[i].collect)
        status = keys[i].collect(ctx, result);
    if (status) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    return result;
}
