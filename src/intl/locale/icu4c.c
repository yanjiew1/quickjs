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
#include "icu4c-private.h"
#include "matching.h"
#include "identifier.h"
#include "../common/locale-grammar.h"
#include "../icu4c/identity.h"
#include <limits.h>
#include <unicode/uchar.h>
#include <unicode/ucol.h>
#include <unicode/uloc.h>
#include <unicode/uscript.h>

QJSIntlStatus qjs_intl_locale_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                   QJSIntlLocaleService **out)
{
    QJSIntlLocaleService *service;
    QJSIntlStatus error;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    /* The default is shared by every service. Collator has the narrowest ICU
       inventory; choose its installed candidates, not a generic metadata tag
       which a formatter might fail to match later. */
    error = qjs_icu4c_locales_open(allocator, ucol_countAvailable(), ucol_getAvailable,
                                   &service->defaults);
    if (!error)
        error = qjs_icu4c_locale_preferences_open(service);
    if (!error)
        error = qjs_icu4c_locale_registry_open(service);
    if (error) {
        qjs_intl_locale_close(service);
        return error;
    }
    *out = service;
    return QJS_INTL_OK;
}

void qjs_intl_locale_close(QJSIntlLocaleService *service)
{
    if (!service)
        return;
    qjs_icu4c_locale_registry_clear(service);
    qjs_icu4c_locale_preferences_clear(service);
    qjs_icu4c_locales_clear(&service->defaults);
    service->allocator.free(service->allocator.opaque, service);
}

void qjs_intl_locale_result_free(const QJSIntlLocaleService *service, char *result)
{
    if (result)
        service->allocator.free(service->allocator.opaque, result);
}

QJSIntlStatus qjs_intl_locale_profile_match(const QJSIntlLocaleService *service,
                                            QJSIntlBytes request, QJSIntlBytes *out)
{
    QJSIntlLocaleMatch match;
    QJSIntlStatus error;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !service)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_intl_lookup_matching_locale(service->defaults.tags, service->defaults.count,
                                            &request, 1, &match);
    if (!error && match.found)
        *out = service->defaults.tags[match.available_index];
    return error ? error : match.found ? QJS_INTL_OK : QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_locale_validate(const QJSIntlLocaleService *service, QJSIntlBytes input,
                                       int *out)
{
    if (out)
        *out = 0;
    if (!out || !service)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = qjs_intl_unicode_locale_well_formed(input.data, input.length);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_locale_canonicalize(const QJSIntlLocaleService *service, QJSIntlBytes input,
                                           char **out)
{
    return qjs_icu4c_locale_identity(service ? &service->allocator : NULL, input,
                                     QJS_ICU4C_CANONICALIZE, out);
}

QJSIntlStatus qjs_intl_locale_maximize(const QJSIntlLocaleService *service, QJSIntlBytes input,
                                       char **out)
{
    return qjs_icu4c_locale_identity(service ? &service->allocator : NULL, input,
                                     QJS_ICU4C_MAXIMIZE, out);
}

QJSIntlStatus qjs_intl_locale_minimize(const QJSIntlLocaleService *service, QJSIntlBytes input,
                                       char **out)
{
    return qjs_icu4c_locale_identity(service ? &service->allocator : NULL, input,
                                     QJS_ICU4C_MINIMIZE, out);
}

QJSIntlStatus qjs_intl_locale_canonicalize_uvalue(const QJSIntlLocaleService *service,
                                                  QJSIntlBytes key, QJSIntlBytes value, char **out)
{
    char *input = NULL, *canonical = NULL;
    QJSIntlLocaleComponents parts;
    QJSIntlBytes result;
    QJSIntlStatus error;
    size_t length;

    if (out)
        *out = NULL;
    if (!out || !service || !key.data || key.length != 2 || (value.length && !value.data) ||
        (value.length && !qjs_intl_unicode_type_well_formed(value.data, value.length)))
        return QJS_INTL_INVALID_ARGUMENT;
    if (value.length > SIZE_MAX - 10)
        return QJS_INTL_OVERFLOW;
    length = 8 + (value.length ? value.length + 1 : 0);
    input = service->allocator.malloc(service->allocator.opaque, length + 1);
    if (!input)
        return QJS_INTL_NO_MEMORY;
    memcpy(input, "und-u-", 6);
    input[6] = key.data[0];
    input[7] = key.data[1];
    if (value.length) {
        input[8] = '-';
        memcpy(input + 9, value.data, value.length);
    }
    input[length] = 0;
    error = qjs_intl_locale_canonicalize(service, (QJSIntlBytes){input, length}, &canonical);
    if (!error)
        error = qjs_intl_locale_components((QJSIntlBytes){canonical, strlen(canonical)}, &parts);
    if (!error) {
        char normalized[2] = {key.data[0], key.data[1]};
        size_t i;

        for (i = 0; i < 2; i++) {
            if (normalized[i] >= 'A' && normalized[i] <= 'Z')
                normalized[i] += 'a' - 'A';
        }
        result = qjs_intl_locale_keyword(&parts, normalized);
        error = qjs_icu4c_copy_bytes(&service->allocator, result, out);
    }
    if (canonical)
        service->allocator.free(service->allocator.opaque, canonical);
    service->allocator.free(service->allocator.opaque, input);
    return error;
}

QJSIntlStatus qjs_intl_locale_script_direction(const QJSIntlLocaleService *service,
                                               QJSIntlBytes script, unsigned int *out)
{
    char text[5];
    int32_t code;

    if (out)
        *out = QJS_INTL_DIRECTION_UNKNOWN;
    if (!out || !service || !script.data || script.length != 4)
        return QJS_INTL_INVALID_ARGUMENT;
    memcpy(text, script.data, 4);
    text[4] = 0;
    code = u_getPropertyValueEnum(UCHAR_SCRIPT, text);
    if (code == UCHAR_INVALID_CODE)
        return QJS_INTL_UNSUPPORTED;
    /* ICU's Boolean RTL property has no unknown category. These CLDR script
       identities have no general text ordering; false must not imply LTR. */
    if (code == USCRIPT_BRAILLE || code == USCRIPT_INHERITED || code == USCRIPT_COMMON ||
        code == USCRIPT_UNKNOWN || uscript_getUsage((UScriptCode)code) == USCRIPT_USAGE_NOT_ENCODED)
        return QJS_INTL_OK;
    *out =
        uscript_isRightToLeft((UScriptCode)code) ? QJS_INTL_DIRECTION_RTL : QJS_INTL_DIRECTION_LTR;
    return QJS_INTL_OK;
}
