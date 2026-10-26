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
#include "../number/number.h"
#include "../plural/plural.h"
#include "../common/number-decimal.h"
#include "../icu4x/common.h"
#include "../icu4x/locale-data.h"
#include "../../icu4x/relative.h"
#include <math.h>
#include <string.h>

struct QJSIntlRelativeService {
    QJSIntlAllocator allocator;
    QJSIntlNumberService *number;
    QJSIntlPluralService *plural;
    const QJSIntlBytes *available; /* Borrow number's immutable inventory. */
    size_t count, *plural_indexes;
};

struct QJSIntlRelativeFormatter {
    QJSIntlAllocator allocator;
    QJSIntlRelativeOptions options;
    QJSIntlNumberFormatter *number;
    QJSIntlPluralFormatter *plural;
    QJSICU4XRelative *handle;
    /* Snapshot category metadata at construction; format calls neither build
       synthetic selector rules nor allocate foreign pattern payloads. */
    QJSICU4XRelativePattern patterns[QJS_INTL_RELATIVE_UNIT_COUNT][2][6];
};

static int compare_bytes(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t length = a.length < b.length ? a.length : b.length;
    int order = length ? memcmp(a.data, b.data, length) : 0;

    return order ? order : (a.length > b.length) - (a.length < b.length);
}

static size_t find_locale(const QJSIntlBytes *tags, size_t count, QJSIntlBytes tag)
{
    size_t low = 0, high = count;

    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (compare_bytes(tags[middle], tag) < 0)
            low = middle + 1;
        else
            high = middle;
    }
    return low < count && !compare_bytes(tags[low], tag) ? low : SIZE_MAX;
}

QJSIntlStatus qjs_intl_relative_open(const QJSIntlAllocator *allocator, const QJSIntlData *data,
                                     QJSIntlRelativeService **out)
{
    QJSIntlRelativeService *service;
    const QJSIntlBytes *plurals;
    size_t count, i;
    unsigned width;
    const char *version = qjs_icu4x_locale_strings + qjs_icu4x_locale_aliases.version;
    QJSIntlStatus error;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free || data)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_icu4x_status(
        qjs_icu4x_relative_check_version((const uint8_t *)version, strlen(version)));
    if (error)
        return error;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    error = qjs_intl_number_open(allocator, NULL, &service->number);
    if (!error)
        error = qjs_intl_plural_open(allocator, NULL, &service->plural);
    if (!error)
        error = qjs_intl_number_available(service->number, &service->available, &service->count);
    if (!error)
        error = qjs_intl_plural_available(service->plural, &plurals, &count);
    if (error)
        goto fail;
    if (!service->count || service->count > SIZE_MAX / sizeof(*service->plural_indexes)) {
        error = service->count ? QJS_INTL_OVERFLOW : QJS_INTL_DATA_ERROR;
        goto fail;
    }
    service->plural_indexes =
        allocator->malloc(allocator->opaque, service->count * sizeof(*service->plural_indexes));
    if (!service->plural_indexes) {
        error = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    for (i = 0; i < service->count; i++) {
        QJSIntlBytes tag = service->available[i];

        service->plural_indexes[i] = find_locale(plurals, count, tag);
        if (service->plural_indexes[i] == SIZE_MAX) {
            error = QJS_INTL_DATA_ERROR;
            goto fail;
        }
        /* Source-registry admission includes locales whose data inherits root.
           Check every width/unit through real public payload constructors. */
        for (width = 0; width < QJS_INTL_RELATIVE_WIDTH_COUNT; width++) {
            QJSICU4XRelative *handle = NULL;
            error = qjs_icu4x_status(
                qjs_icu4x_relative_open((const uint8_t *)tag.data, tag.length, width, &handle));
            qjs_icu4x_relative_close(handle);
            if (error)
                goto fail;
        }
    }
    *out = service;
    return QJS_INTL_OK;
fail:
    qjs_intl_relative_close(service);
    return error;
}

void qjs_intl_relative_close(QJSIntlRelativeService *service)
{
    if (!service)
        return;
    qjs_intl_number_close(service->number);
    qjs_intl_plural_close(service->plural);
    if (service->plural_indexes)
        service->allocator.free(service->allocator.opaque, service->plural_indexes);
    service->allocator.free(service->allocator.opaque, service);
}

QJSIntlStatus qjs_intl_relative_available(const QJSIntlRelativeService *service,
                                          const QJSIntlBytes **out, size_t *count)
{
    return qjs_intl_number_available(service ? service->number : NULL, out, count);
}

QJSIntlStatus qjs_intl_relative_default_system(const QJSIntlRelativeService *service, size_t locale,
                                               QJSIntlBytes *out)
{
    return qjs_intl_number_default_system(service ? service->number : NULL, locale, out);
}

QJSIntlStatus qjs_intl_relative_system_supported(const QJSIntlRelativeService *service,
                                                 size_t locale, QJSIntlBytes system, int *out)
{
    return qjs_intl_number_system_supported(service ? service->number : NULL, locale, system, out);
}

QJSIntlStatus qjs_intl_relative_system_available(const QJSIntlRelativeService *service,
                                                 QJSIntlBytes system, int *out)
{
    return qjs_intl_number_system_available(service ? service->number : NULL, system, out);
}

QJSIntlStatus qjs_intl_relative_formatter_open(const QJSIntlRelativeService *service, size_t locale,
                                               QJSIntlBytes system,
                                               const QJSIntlRelativeOptions *options,
                                               QJSIntlRelativeFormatter **out)
{
    QJSIntlRelativeFormatter *formatter;
    QJSIntlNumberOptions number = {0};
    QJSIntlPluralOptions plural = {0};
    QJSIntlBytes tag;
    QJSIntlStatus error;
    unsigned unit, past, category;

    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->count || !options ||
        (unsigned)options->width >= QJS_INTL_RELATIVE_WIDTH_COUNT ||
        (unsigned)options->numeric > QJS_INTL_RELATIVE_AUTO || !options->maximum_output_length)
        return QJS_INTL_INVALID_ARGUMENT;
    formatter = service->allocator.malloc(service->allocator.opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = service->allocator;
    formatter->options = *options;
    tag = service->available[locale];
    error = qjs_icu4x_status(qjs_icu4x_relative_open((const uint8_t *)tag.data, tag.length,
                                                     options->width, &formatter->handle));
    if (error)
        goto fail;
    number.grouping = QJS_INTL_NUMBER_GROUP_AUTO;
    number.sign_display = QJS_INTL_NUMBER_SIGN_NEVER;
    number.digits = (QJSIntlDecimalOptions){1,
                                            0,
                                            3,
                                            1,
                                            21,
                                            1,
                                            QJS_INTL_ROUND_HALF_EXPAND,
                                            QJS_INTL_ROUND_FRACTION,
                                            QJS_INTL_TRAILING_ZERO_AUTO,
                                            options->maximum_output_length};
    number.maximum_output_length = options->maximum_output_length;
    plural.digits = number.digits;
    error = qjs_intl_number_formatter_open(service->number, locale, system, &number,
                                           &formatter->number);
    if (!error)
        error = qjs_intl_plural_formatter_open(service->plural, service->plural_indexes[locale],
                                               &plural, &formatter->plural);
    if (error)
        goto fail;
    for (unit = 0; unit < QJS_INTL_RELATIVE_UNIT_COUNT; unit++) {
        for (past = 0; past < 2; past++) {
            for (category = 0; category < 6; category++) {
                error = qjs_icu4x_status(
                    qjs_icu4x_relative_pattern(formatter->handle, unit, past, category,
                                               &formatter->patterns[unit][past][category]));
                if (error)
                    goto fail;
            }
        }
    }
    *out = formatter;
    return QJS_INTL_OK;
fail:
    qjs_intl_relative_formatter_close(formatter);
    return error;
}

void qjs_intl_relative_formatter_close(QJSIntlRelativeFormatter *formatter)
{
    if (formatter) {
        qjs_icu4x_relative_close(formatter->handle);
        qjs_intl_number_formatter_close(formatter->number);
        qjs_intl_plural_formatter_close(formatter->plural);
        formatter->allocator.free(formatter->allocator.opaque, formatter);
    }
}

QJSIntlStatus qjs_intl_relative_format(const QJSIntlRelativeFormatter *formatter, double value,
                                       QJSIntlRelativeUnit unit, QJSIntlRelativeFormatted *out)
{
    QJSIntlNumberDecimal real = {0};
    QJSIntlFormatted body = {0};
    QJSIntlPluralCategory category;
    const QJSIntlAllocator *allocator;
    const QJSICU4XRelativePattern *pattern;
    QJSIntlStatus error;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !isfinite(value) || (unsigned)unit >= QJS_INTL_RELATIVE_UNIT_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    allocator = &formatter->allocator;
    if (formatter->options.numeric == QJS_INTL_RELATIVE_AUTO && value >= INT8_MIN &&
        value <= INT8_MAX && trunc(value) == value) {
        QJSICU4XRelativeText text;
        int status = qjs_icu4x_relative_literal(formatter->handle, unit, (int32_t)value, &text);
        /* Literal lookup uses the original Number, before rounding. Both zero
           signs select offset zero; the original sign still chooses tense. */
        if (!status)
            return qjs_intl_relative_literal_format(
                allocator, (QJSIntlBytes){(const char *)text.data, text.length},
                formatter->options.maximum_output_length, out);
        if (status != QJS_ICU4X_UNSUPPORTED)
            return qjs_icu4x_status(status);
    }
    error = qjs_intl_number_to_real_decimal(allocator, fabs(value), &real);
    if (!error)
        error = qjs_intl_plural_select(formatter->plural, &real.value, &category);
    if (error)
        goto done;
    pattern = &formatter->patterns[unit][signbit(value) != 0][category];
    if (pattern->has_number)
        error = qjs_intl_number_format(formatter->number, &real.value, &body);
    if (!error)
        error = qjs_intl_relative_affixes_format(
            allocator, (QJSIntlBytes){(const char *)pattern->prefix.data, pattern->prefix.length},
            (QJSIntlBytes){(const char *)pattern->suffix.data, pattern->suffix.length},
            pattern->has_number ? &body : NULL, unit, formatter->options.maximum_output_length,
            out);
done:
    qjs_intl_number_result_clear(allocator, &body);
    qjs_intl_number_decimal_clear(allocator, &real);
    return error;
}
