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
#include "relative.h"
#include "../icu4c/numbering.h"
#include "../icu4c/number-fields.h"
#include "../common/number-decimal.h"
#include "../icu4c/resource.h"
#include "../icu4c/skeleton.h"
#include "../plural/icu4c.h"
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unicode/uloc.h>
#include <unicode/unumberformatter.h>

struct QJSIntlRelativeService {
    QJSIntlAllocator allocator;
    QJSICU4CLocales locales;
    QJSICU4CNumbering numbering;
};

struct QJSIntlRelativeFormatter {
    QJSIntlAllocator allocator;
    QJSIntlRelativeOptions options;
    UNumberFormatter *number;
    QJSIntlPluralFormatter *plural;
    char *locale;
};

QJSIntlStatus qjs_intl_relative_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                     QJSIntlRelativeService **out)
{
    QJSIntlRelativeService *service;
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
    if (!error)
        error = qjs_icu4c_numbering_open(allocator, &service->locales, &service->numbering);
    if (error) {
        qjs_intl_relative_close(service);
        return error;
    }
    *out = service;
    return QJS_INTL_OK;
}

void qjs_intl_relative_close(QJSIntlRelativeService *service)
{
    if (service) {
        qjs_icu4c_numbering_clear(&service->numbering);
        qjs_icu4c_locales_clear(&service->locales);
        service->allocator.free(service->allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_relative_available(const QJSIntlRelativeService *service,
                                          const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!out || !count || !service)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->locales.tags;
    *count = service->locales.count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_relative_default_system(const QJSIntlRelativeService *service, size_t locale,
                                               QJSIntlBytes *out)
{
    return qjs_icu4c_numbering_default(service ? &service->numbering : NULL, locale, out);
}

QJSIntlStatus qjs_intl_relative_system_available(const QJSIntlRelativeService *service,
                                                 QJSIntlBytes system, int *out)
{
    return qjs_icu4c_numbering_supported(service ? &service->numbering : NULL, system, out);
}

QJSIntlStatus qjs_intl_relative_system_supported(const QJSIntlRelativeService *service,
                                                 size_t locale, QJSIntlBytes system, int *out)
{
    if (!service || locale >= service->locales.count) {
        if (out)
            *out = 0;
        return QJS_INTL_INVALID_ARGUMENT;
    }
    return qjs_intl_relative_system_available(service, system, out);
}

QJSIntlStatus qjs_intl_relative_formatter_open(const QJSIntlRelativeService *service, size_t locale,
                                               QJSIntlBytes system,
                                               const QJSIntlRelativeOptions *options,
                                               QJSIntlRelativeFormatter **out)
{
    QJSIntlRelativeFormatter *formatter = NULL;
    QJSIntlStatus error;
    UErrorCode status = U_ZERO_ERROR;
    QJSICU4CSkeleton tokens = {0};
    QJSIntlDecimalOptions digits;
    QJSIntlPluralOptions plural_options = {0};
    char *base = NULL, *selected = NULL;
    int supported;

    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->locales.count || !options ||
        (unsigned int)options->width >= QJS_INTL_RELATIVE_WIDTH_COUNT ||
        (unsigned int)options->numeric > QJS_INTL_RELATIVE_AUTO || !options->maximum_output_length)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!system.length)
        system = service->numbering.defaults[locale];
    error = qjs_icu4c_numbering_supported(&service->numbering, system, &supported);
    if (error || !supported)
        return error ? error : QJS_INTL_UNSUPPORTED;
    error = qjs_icu4c_locale_id(&service->allocator, service->locales.tags[locale], &base);
    if (!error)
        error = qjs_icu4c_locale_keyword(&service->allocator, base, "nu", system, &selected);
    if (error)
        goto done;
    digits = (QJSIntlDecimalOptions){1,
                                     0,
                                     3,
                                     1,
                                     21,
                                     1,
                                     QJS_INTL_ROUND_HALF_EXPAND,
                                     QJS_INTL_ROUND_FRACTION,
                                     QJS_INTL_TRAILING_ZERO_AUTO,
                                     options->maximum_output_length};
    error = qjs_icu4c_digit_skeleton(&tokens, &digits);
    if (error)
        goto done;
    qjs_icu4c_skeleton_token(&tokens, "group-auto sign-never");
    if (tokens.error) {
        error = tokens.error;
        goto done;
    }
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter) {
        error = QJS_INTL_NO_MEMORY;
        goto done;
    }
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = service->allocator;
    formatter->options = *options;
    formatter->locale = selected;
    selected = NULL;
    formatter->number =
        unumf_openForSkeletonAndLocale(tokens.text, tokens.length, formatter->locale, &status);
    error = qjs_icu4c_status(status);
    plural_options.digits = digits;
    if (!error)
        error = qjs_icu4c_plural_formatter_open(&service->allocator, service->locales.tags[locale],
                                                &plural_options, &formatter->plural);
    if (!error) {
        *out = formatter;
        formatter = NULL;
    }
done:
    qjs_intl_relative_formatter_close(formatter);
    if (base)
        service->allocator.free(service->allocator.opaque, base);
    if (selected)
        service->allocator.free(service->allocator.opaque, selected);
    return error;
}

void qjs_intl_relative_formatter_close(QJSIntlRelativeFormatter *formatter)
{
    if (formatter) {
        unumf_close(formatter->number);
        qjs_intl_plural_formatter_close(formatter->plural);
        if (formatter->locale)
            formatter->allocator.free(formatter->allocator.opaque, formatter->locale);
        formatter->allocator.free(formatter->allocator.opaque, formatter);
    }
}

static QJSIntlStatus pattern(const QJSIntlRelativeFormatter *formatter, QJSIntlRelativeUnit unit,
                             const char *tense, const char *key, char **out, int *present)
{
    static const char *const units[] = {"second", "minute", "hour",    "day",
                                        "week",   "month",  "quarter", "year"};
    static const char *const widths[] = {"", "-short", "-narrow"};
    const QJSIntlAllocator *allocator = &formatter->allocator;
    char *root = NULL;
    int width;
    QJSIntlStatus error = QJS_INTL_OK;

    *out = NULL;
    *present = 0;
    for (width = formatter->options.width; width >= 0; width--) {
        char field[32], *candidate = NULL;
        const char *path[] = {"fields", field, tense ? "relativeTime" : "relative",
                              tense ? tense : key, key};
        UResourceBundle *resource = NULL;
        UErrorCode status = U_ZERO_ERROR;
        const char *actual;
        int length = snprintf(field, sizeof(field), "%s%s", units[unit], widths[width]);
        int blocked, is_root;

        if (length < 0 || (size_t)length >= sizeof(field)) {
            error = QJS_INTL_OVERFLOW;
            goto done;
        }
        error =
            qjs_icu4c_resource_path(allocator, formatter->locale, path, tense ? 5 : 4, &resource);
        if (error == QJS_INTL_UNSUPPORTED) {
            error = QJS_INTL_OK;
            continue;
        }
        if (error)
            goto done;
        actual = ures_getLocaleByType(resource, ULOC_ACTUAL_LOCALE, &status);
        if (U_FAILURE(status) || !actual) {
            error = U_FAILURE(status) ? qjs_icu4c_status(status) : QJS_INTL_DATA_ERROR;
            ures_close(resource);
            goto done;
        }
        is_root = !*actual || !strcmp(actual, "root");
        error = qjs_icu4c_resource_text(allocator, resource, &candidate, &blocked);
        ures_close(resource);
        if (error)
            goto done;
        if (blocked)
            goto done; /* An explicit block also discards a saved root value. */
        if (!is_root) {
            *out = candidate;
            *present = 1;
            goto done;
        }
        /* ICU resolves a root width alias to its root target. That generic
           text must not mask this locale's localized long/short fallback:
           af/day-short must inherit Afrikaans day, not root's English label.
           Keep the most specific root result only as the final fallback. */
        if (!root)
            root = candidate;
        else
            allocator->free(allocator->opaque, candidate);
    }
    if (root) {
        *out = root;
        *present = 1;
        root = NULL;
    }
done:
    if (root)
        allocator->free(allocator->opaque, root);
    return error;
}

QJSIntlStatus qjs_intl_relative_format(const QJSIntlRelativeFormatter *formatter, double number,
                                       QJSIntlRelativeUnit unit, QJSIntlRelativeFormatted *out)
{
    QJSIntlNumberDecimal shortest = {0}, real = {0};
    QJSIntlFormatted body = {0};
    UFormattedNumber *formatted = NULL;
    UErrorCode status = U_ZERO_ERROR;
    QJSIntlStatus error;
    static const char *const categories[] = {"zero", "one", "two", "few", "many", "other"};
    char *text = NULL;
    const char *category;
    QJSIntlPluralCategory selected;
    int present;
    const QJSIntlAllocator *allocator;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !isfinite(number) ||
        (unsigned int)unit >= QJS_INTL_RELATIVE_UNIT_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    allocator = &formatter->allocator;
    if (formatter->options.numeric == QJS_INTL_RELATIVE_AUTO) {
        const char *key;

        error = qjs_intl_number_to_intl_decimal(allocator, number, &shortest);
        if (error)
            goto done;
        key = number == 0 ? "0" : shortest.value.decimal.data;
        error = pattern(formatter, unit, NULL, key, &text, &present);
        if (error)
            goto done;
        if (present) {
            error = qjs_intl_relative_literal_format(allocator, (QJSIntlBytes){text, strlen(text)},
                                                     formatter->options.maximum_output_length, out);
            goto done;
        }
    }
    /* ICU's RelativeDateTimeFormatter public API accepts only double. That
       loses exact large integer digits and uses epsilon-based lexical matching.
       Keep normative pattern composition in the common layer, while ICU owns
       decimal formatting, localized digits and source patterns. The shared
       exact plural evaluator uses only ICU rules, avoiding truncated i operands
       (for example French 1e18 must not select the singular pattern). */
    error = qjs_intl_number_to_real_decimal(allocator, fabs(number), &real);
    if (error)
        goto done;
    formatted = unumf_openResult(&status);
    unumf_formatDecimal(formatter->number, real.value.decimal.data,
                        (int32_t)real.value.decimal.length, formatted, &status);
    error = qjs_icu4c_status(status);
    if (!error)
        error = qjs_intl_plural_select(formatter->plural, &real.value, &selected);
    if (error)
        goto done;
    category = categories[selected];
    error =
        pattern(formatter, unit, signbit(number) ? "past" : "future", category, &text, &present);
    if (!error && !present && strcmp(category, "other"))
        error =
            pattern(formatter, unit, signbit(number) ? "past" : "future", "other", &text, &present);
    if (error)
        goto done;
    if (!present) {
        error = QJS_INTL_DATA_ERROR;
        goto done;
    }
    if (strstr(text, "{0}")) {
        const UFormattedValue *value = unumf_resultAsValue(formatted, &status);

        error = qjs_icu4c_status(status);
        if (!error)
            error = qjs_icu4c_number_parts(allocator, value, &real.value, QJS_INTL_NUMBER_DECIMAL,
                                           formatter->options.maximum_output_length, &body);
        if (error)
            goto done;
    }
    error = qjs_intl_relative_pattern_format(allocator, (QJSIntlBytes){text, strlen(text)}, &body,
                                             unit, formatter->options.maximum_output_length, out);
done:
    if (text)
        allocator->free(allocator->opaque, text);
    unumf_closeResult(formatted);
    qjs_intl_formatted_clear(allocator, &body);
    qjs_intl_number_decimal_clear(allocator, &shortest);
    qjs_intl_number_decimal_clear(allocator, &real);
    return error;
}
