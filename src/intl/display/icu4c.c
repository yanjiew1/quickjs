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
#include "display.h"
#include "../icu4c/common.h"
#include "../icu4c/resource.h"
#include "../datetime/field.h"
#include <limits.h>
#include <string.h>
#include <unicode/ucurr.h>
#include <unicode/ucasemap.h>
#include <unicode/uchar.h>
#include <unicode/utf16.h>
#include <unicode/ubrk.h>
#include <unicode/udatpg.h>
#include <unicode/udata.h>
#include <unicode/ures.h>
#include <unicode/uldnames.h>
#include <unicode/uloc.h>

struct QJSIntlDisplayService {
    QJSIntlAllocator allocator;
    QJSICU4CLocales locales;
    char (*currencies)[4];
    size_t currency_count;
};

struct QJSIntlDisplayFormatter {
    QJSIntlAllocator allocator;
    QJSIntlDisplayOptions options;
    char *locale;
    ULocaleDisplayNames *names;
    UDateTimePatternGenerator *dates;
    int script_titlecase;
};

/* Presence is a resource fact, including a name whose text equals its code.
   Most codes resolve in the first locale; rare absent entries require the full
   inventory scan. Do this once at admission, not in supportedValuesOf queries. */
static QJSIntlStatus currency_present(const QJSIntlDisplayService *service, const char *code,
                                      int *out)
{
    UChar iso[4] = {(UChar)code[0], (UChar)code[1], (UChar)code[2], 0};
    size_t i;

    *out = 0;
    for (i = 0; i < service->locales.count; i++) {
        UErrorCode status = U_ZERO_ERROR;
        const UChar *name;
        int32_t length;
        char *locale;
        QJSIntlStatus error =
            qjs_icu4c_locale_id(&service->allocator, service->locales.tags[i], &locale);

        if (error)
            return error;
        name = ucurr_getName(iso, locale, UCURR_LONG_NAME, NULL, &length, &status);
        service->allocator.free(service->allocator.opaque, locale);
        if (U_FAILURE(status))
            return qjs_icu4c_status(status);
        if (name != iso) {
            *out = 1;
            break;
        }
    }
    return QJS_INTL_OK;
}

/* Currencies present in locale name data need not have an ISO metadata row:
   CLDR49's Chuvash XAD is one such entry. Collect the source-key union, then
   admit only codes with an actual plain name. A fixed three-letter bitset
   bounds scratch and deduplicates without a second growing catalogue. */
static QJSIntlStatus currency_inventory(QJSIntlDisplayService *service)
{
    enum { CODE_COUNT = 26 * 26 * 26 };
    unsigned char candidates[(CODE_COUNT + 7) / 8] = {0};
    QJSIntlStatus error = QJS_INTL_OK;
    size_t locale_index, count = 0, id;

    for (locale_index = 0; locale_index <= service->locales.count; locale_index++) {
        UErrorCode status = U_ZERO_ERROR;
        UResourceBundle *bundle = NULL, *table = NULL;
        char *locale = NULL;
        int32_t i, rows;

        if (locale_index < service->locales.count)
            error = qjs_icu4c_locale_id(&service->allocator, service->locales.tags[locale_index],
                                        &locale);
        if (error)
            return error;
        bundle = ures_openDirect(U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "curr",
                                 locale ? locale : "root", &status);
        if (locale)
            service->allocator.free(service->allocator.opaque, locale);
        if (U_SUCCESS(status))
            table = ures_getByKey(bundle, "Currencies", NULL, &status);
        int missing = status == U_MISSING_RESOURCE_ERROR;

        rows = U_SUCCESS(status) ? ures_getSize(table) : 0;
        for (i = 0; i < rows && U_SUCCESS(status); i++) {
            UResourceBundle *row = ures_getByIndex(table, i, NULL, &status);
            const char *code = U_SUCCESS(status) ? ures_getKey(row) : NULL;

            if (code && strlen(code) == 3 && code[0] >= 'A' && code[0] <= 'Z' && code[1] >= 'A' &&
                code[1] <= 'Z' && code[2] >= 'A' && code[2] <= 'Z') {
                id = ((size_t)(code[0] - 'A') * 26 + code[1] - 'A') * 26 + code[2] - 'A';
                candidates[id / 8] |= 1U << (id % 8);
            }
            ures_close(row);
        }
        ures_close(table);
        ures_close(bundle);
        if (!missing && U_FAILURE(status))
            return qjs_icu4c_status(status);
    }
    for (id = 0; id < CODE_COUNT; id++) {
        char code[4] = {(char)('A' + id / (26 * 26)), (char)('A' + id / 26 % 26),
                        (char)('A' + id % 26), 0};
        int present;

        if (!(candidates[id / 8] & (1U << (id % 8))))
            continue;
        error = currency_present(service, code, &present);
        if (error)
            return error;
        if (present)
            count++;
        else
            candidates[id / 8] &= ~(1U << (id % 8));
    }
    if (!count)
        return QJS_INTL_DATA_ERROR;
    service->currencies =
        service->allocator.malloc(service->allocator.opaque, count * sizeof(*service->currencies));
    if (!service->currencies)
        return QJS_INTL_NO_MEMORY;
    for (id = 0; id < CODE_COUNT; id++) {
        char *code;

        if (!(candidates[id / 8] & (1U << (id % 8))))
            continue;
        code = service->currencies[service->currency_count++];
        code[0] = (char)('A' + id / (26 * 26));
        code[1] = (char)('A' + id / 26 % 26);
        code[2] = (char)('A' + id % 26);
        code[3] = 0;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_display_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                    QJSIntlDisplayService **out)
{
    QJSIntlDisplayService *service;
    QJSIntlStatus error;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    error = qjs_icu4c_locales_open(allocator, uloc_countAvailable(), uloc_getAvailable,
                                   &service->locales);
    if (error)
        goto done;
    error = currency_inventory(service);
    if (!error) {
        *out = service;
        service = NULL;
    }
done:
    qjs_intl_display_close(service);
    return error;
}

void qjs_intl_display_close(QJSIntlDisplayService *service)
{
    if (service) {
        qjs_icu4c_locales_clear(&service->locales);
        if (service->currencies)
            service->allocator.free(service->allocator.opaque, service->currencies);
        service->allocator.free(service->allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_display_available(const QJSIntlDisplayService *service,
                                         const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!service || !out || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->locales.tags;
    *count = service->locales.count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_display_currency_next(const QJSIntlDisplayService *service, uint32_t *cursor,
                                             QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !cursor || !out || *cursor > service->currency_count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (*cursor == service->currency_count)
        return QJS_INTL_UNSUPPORTED;
    *out = (QJSIntlBytes){service->currencies[(*cursor)++], 3};
    return QJS_INTL_OK;
}

static QJSIntlStatus script_capitalization(QJSIntlDisplayFormatter *formatter)
{
    const char *const path[] = {"contextTransforms", "script"};
    UResourceBundle *resource = NULL;
    UErrorCode status = U_ZERO_ERROR;
    QJSIntlStatus error =
        qjs_icu4c_resource_path(&formatter->allocator, formatter->locale, path, 2, &resource);
    int32_t length;

    if (error == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    if (error)
        return error;
    if (ures_getType(resource) == URES_STRING) {
        const UChar *text = ures_getString(resource, &length, &status);

        if (U_SUCCESS(status) &&
            (length != 3 || text[0] != 0x2205 || text[1] != 0x2205 || text[2] != 0x2205))
            error = QJS_INTL_DATA_ERROR;
    } else {
        const int32_t *values = ures_getIntVector(resource, &length, &status);

        if (U_SUCCESS(status)) {
            if (length != 2 || (values[1] != 0 && values[1] != 1))
                error = QJS_INTL_DATA_ERROR;
            else
                formatter->script_titlecase = values[1];
        }
    }
    ures_close(resource);
    return error ? error : qjs_icu4c_status(status);
}

QJSIntlStatus qjs_intl_display_formatter_open(const QJSIntlDisplayService *service, size_t locale,
                                              const QJSIntlDisplayOptions *options,
                                              QJSIntlDisplayFormatter **out)
{
    QJSIntlDisplayFormatter *formatter;
    QJSIntlStatus error;
    UErrorCode status = U_ZERO_ERROR;
    UDisplayContext contexts[4];

    if (out)
        *out = NULL;
    if (!out || !service || !options || locale >= service->locales.count ||
        (unsigned int)options->type >= QJS_INTL_DISPLAY_TYPE_COUNT ||
        (unsigned int)options->style > QJS_INTL_NAME_NARROW || (unsigned int)options->dialect > 1 ||
        !options->maximum_output_length)
        return QJS_INTL_INVALID_ARGUMENT;
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = service->allocator;
    formatter->options = *options;
    error = qjs_icu4c_locale_id(&formatter->allocator, service->locales.tags[locale],
                                &formatter->locale);
    if (error)
        goto fail;
    contexts[0] = UDISPCTX_NO_SUBSTITUTE;
    contexts[1] = UDISPCTX_CAPITALIZATION_FOR_STANDALONE;
    contexts[2] = options->dialect ? UDISPCTX_DIALECT_NAMES : UDISPCTX_STANDARD_NAMES;
    contexts[3] =
        options->style == QJS_INTL_NAME_LONG ? UDISPCTX_LENGTH_FULL : UDISPCTX_LENGTH_SHORT;
    if (options->type == QJS_INTL_DISPLAY_DATE_TIME_FIELD)
        formatter->dates = udatpg_open(formatter->locale, &status);
    else if (options->type != QJS_INTL_DISPLAY_CURRENCY && options->type != QJS_INTL_DISPLAY_SCRIPT)
        formatter->names = uldn_openForContext(formatter->locale, contexts, 4, &status);
    error = qjs_icu4c_status(status);
    if (!error && options->type == QJS_INTL_DISPLAY_SCRIPT)
        error = script_capitalization(formatter);
    if (error)
        goto fail;
    *out = formatter;
    return QJS_INTL_OK;
fail:
    qjs_intl_display_formatter_close(formatter);
    return error;
}

void qjs_intl_display_formatter_close(QJSIntlDisplayFormatter *formatter)
{
    if (formatter) {
        uldn_close(formatter->names);
        udatpg_close(formatter->dates);
        if (formatter->locale)
            formatter->allocator.free(formatter->allocator.opaque, formatter->locale);
        formatter->allocator.free(formatter->allocator.opaque, formatter);
    }
}

static int32_t name(const QJSIntlDisplayFormatter *formatter, const char *code, UChar *out,
                    int32_t capacity, UErrorCode *status)
{
    static const UDateTimePatternField fields[] = {
        UDATPG_ERA_FIELD,    UDATPG_YEAR_FIELD,         UDATPG_QUARTER_FIELD,
        UDATPG_MONTH_FIELD,  UDATPG_WEEK_OF_YEAR_FIELD, UDATPG_WEEKDAY_FIELD,
        UDATPG_DAY_FIELD,    UDATPG_DAYPERIOD_FIELD,    UDATPG_HOUR_FIELD,
        UDATPG_MINUTE_FIELD, UDATPG_SECOND_FIELD,       UDATPG_ZONE_FIELD,
    };
    static const UDateTimePGDisplayWidth widths[] = {UDATPG_WIDE, UDATPG_ABBREVIATED,
                                                     UDATPG_NARROW};
    QJSIntlDateField field;
    const char *legacy;

    switch (formatter->options.type) {
    case QJS_INTL_DISPLAY_LANGUAGE:
        return uldn_localeDisplayName(formatter->names, code, out, capacity, status);
    case QJS_INTL_DISPLAY_REGION:
        return uldn_regionDisplayName(formatter->names, code, out, capacity, status);
    case QJS_INTL_DISPLAY_CALENDAR:
        legacy = uloc_toLegacyType("ca", code);
        if (!legacy) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
        return uldn_keyValueDisplayName(formatter->names, "calendar", legacy, out, capacity,
                                        status);
    case QJS_INTL_DISPLAY_DATE_TIME_FIELD:
        if (qjs_intl_date_field_from_code((QJSIntlBytes){code, strlen(code)}, &field)) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
        return udatpg_getFieldDisplayName(formatter->dates, fields[field],
                                          widths[formatter->options.style], out, capacity, status);
    default:
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
}

/* Script names shown alone use CLDR's stand-alone alternate. The public
   uldn_scriptDisplayName convenience API omits it; its standalone option only
   controls capitalization. Keep both source choice and casing in ICU data/API. */
static QJSIntlStatus script_name(const QJSIntlDisplayFormatter *formatter, const char *code,
                                 QJSIntlNameText *out)
{
    const char *const short_path[] = {"Scripts%short", code};
    const char *const standalone_path[] = {"Scripts%stand-alone", code};
    const char *const ordinary_path[] = {"Scripts", code};
    QJSICU4CResourcePath paths[] = {{short_path, 2}, {standalone_path, 2}, {ordinary_path, 2}};
    size_t first = formatter->options.style == QJS_INTL_NAME_LONG ? 1 : 0;
    UResourceBundle *resource = NULL;
    UCaseMap *casing = NULL;
    UBreakIterator *iterator = NULL;
    const UChar *source;
    UErrorCode status = U_ZERO_ERROR;
    QJSIntlStatus error;
    int32_t source_length, length, actual, at = 0;
    UChar32 first_code = 0;

    error = qjs_icu4c_resource_variants(
        &formatter->allocator, U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "lang", formatter->locale,
        paths + first, sizeof(paths) / sizeof(*paths) - first, &resource);
    if (error == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    if (error)
        return error;
    source = ures_getString(resource, &source_length, &status);
    if (U_FAILURE(status)) {
        error = qjs_icu4c_status(status);
        goto done;
    }
    if (source_length < 0) {
        error = QJS_INTL_DATA_ERROR;
        goto done;
    }
    if (source_length)
        U16_NEXT(source, at, source_length, first_code);
    length = source_length;
    if (formatter->script_titlecase && u_islower(first_code)) {
        casing = ucasemap_open(formatter->locale,
                               U_TITLECASE_NO_LOWERCASE | U_TITLECASE_NO_BREAK_ADJUSTMENT, &status);
        iterator = ubrk_open(UBRK_SENTENCE, formatter->locale, NULL, 0, &status);
        if (U_FAILURE(status)) {
            error = qjs_icu4c_status(status);
            goto done;
        }
        ucasemap_setBreakIterator(casing, iterator, &status);
        iterator = NULL; /* Adopted with the successful status above. */
        length = ucasemap_toTitle(casing, NULL, 0, source, source_length, &status);
        if (status == U_BUFFER_OVERFLOW_ERROR)
            status = U_ZERO_ERROR;
        if (U_FAILURE(status)) {
            error = qjs_icu4c_status(status);
            goto done;
        }
    }
    if (length < 0 || length == INT32_MAX ||
        (size_t)length > formatter->options.maximum_output_length ||
        (size_t)length + 1 > SIZE_MAX / sizeof(*out->text)) {
        error = QJS_INTL_OVERFLOW;
        goto done;
    }
    out->text = formatter->allocator.malloc(formatter->allocator.opaque,
                                            ((size_t)length + 1) * sizeof(*out->text));
    if (!out->text) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    if (casing) {
        actual = ucasemap_toTitle(casing, out->text, length + 1, source, source_length, &status);
        if (U_FAILURE(status) || actual != length) {
            error = U_FAILURE(status) ? qjs_icu4c_status(status) : QJS_INTL_DATA_ERROR;
            goto done;
        }
    } else if (length) {
        memcpy(out->text, source, (size_t)length * sizeof(*out->text));
    }
    out->text[length] = 0;
    out->length = (size_t)length;
    out->found = 1;
    error = QJS_INTL_OK;
done:
    ubrk_close(iterator);
    ucasemap_close(casing);
    ures_close(resource);
    return error;
}

QJSIntlStatus qjs_intl_display_format(const QJSIntlDisplayFormatter *formatter, QJSIntlBytes code,
                                      QJSIntlNameText *out)
{
    QJSIntlStatus error;
    UErrorCode status = U_ZERO_ERROR;
    QJSIntlNameText result = {0};
    const UChar *currency = NULL;
    char *text;
    int32_t length;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !code.data || !code.length)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_icu4c_copy_bytes(&formatter->allocator, code, &text);
    if (error)
        return error;
    if (formatter->options.type == QJS_INTL_DISPLAY_SCRIPT) {
        error = script_name(formatter, text, &result);
        if (error || !result.found)
            goto done;
        goto publish;
    }
    if (formatter->options.type == QJS_INTL_DISPLAY_CURRENCY) {
        UChar iso[4];
        size_t i;

        if (code.length != 3) {
            error = QJS_INTL_INVALID_ARGUMENT;
            goto done;
        }
        for (i = 0; i < 3; i++) {
            if (text[i] < 'A' || text[i] > 'Z') {
                error = QJS_INTL_INVALID_ARGUMENT;
                goto done;
            }
            iso[i] = (UChar)text[i];
        }
        iso[3] = 0;
        currency = ucurr_getName(iso, formatter->locale, UCURR_LONG_NAME, NULL, &length, &status);
        /* Root fallback can be a genuine (even code-identical) name and carry
           DEFAULT_WARNING. Only returning our input pointer means ICU supplied
           the ISO-code substitute instead of a resource-backed name. */
        if (currency == iso) {
            error = QJS_INTL_OK;
            goto done;
        }
    } else {
        length = name(formatter, text, NULL, 0, &status);
        /* With NO_SUBSTITUTE, ICU represents a missing name by a bogus string
           whose C extraction reports ILLEGAL_ARGUMENT. JS fallback stays core. */
        if (status == U_ILLEGAL_ARGUMENT_ERROR) {
            error = QJS_INTL_OK;
            goto done;
        }
        if (status == U_BUFFER_OVERFLOW_ERROR)
            status = U_ZERO_ERROR;
    }
    if (U_FAILURE(status)) {
        error = qjs_icu4c_status(status);
        goto done;
    }
    if (length < 0 || length == INT32_MAX ||
        (size_t)length > formatter->options.maximum_output_length ||
        (size_t)length + 1 > SIZE_MAX / sizeof(*result.text)) {
        error = QJS_INTL_OVERFLOW;
        goto done;
    }
    result.text = formatter->allocator.malloc(formatter->allocator.opaque,
                                              ((size_t)length + 1) * sizeof(*result.text));
    if (!result.text) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    if (formatter->options.type == QJS_INTL_DISPLAY_CURRENCY) {
        if (length)
            memcpy(result.text, currency, (size_t)length * sizeof(*result.text));
    } else {
        int32_t actual = name(formatter, text, result.text, length + 1, &status);

        if (U_FAILURE(status) || actual != length) {
            error = U_FAILURE(status) ? qjs_icu4c_status(status) : QJS_INTL_DATA_ERROR;
            goto done;
        }
    }
    result.length = (size_t)length;
    result.found = 1;
publish:
    *out = result;
    memset(&result, 0, sizeof(result));
    error = QJS_INTL_OK;
done:
    qjs_intl_name_text_clear(&formatter->allocator, &result);
    formatter->allocator.free(formatter->allocator.opaque, text);
    return error;
}

void qjs_intl_name_text_clear(const QJSIntlAllocator *allocator, QJSIntlNameText *out)
{
    if (!out)
        return;
    if (out->text && allocator && allocator->free)
        allocator->free(allocator->opaque, out->text);
    memset(out, 0, sizeof(*out));
}
