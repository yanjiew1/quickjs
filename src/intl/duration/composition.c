/* Normative duration composition over the Number and List service contracts.
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
#include "composition.h"
#include "digital.h"
#include "decimal.h"
#include "../../temporal/duration.h"
#include <string.h>

struct QJSIntlDurationFormatter {
    QJSIntlAllocator allocator;
    QJSIntlNumberFormatter *numbers[QJS_TEMPORAL_UNIT_COUNT];
    QJSIntlListFormatter *list;
    QJSIntlDurationDigital digital;
    QJSIntlDurationOptions options;
};

void qjs_intl_duration_formatter_close(QJSIntlDurationFormatter *formatter)
{
    if (formatter) {
        QJSIntlAllocator allocator = formatter->allocator;
        unsigned int unit;

        for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++)
            qjs_intl_number_formatter_close(formatter->numbers[unit]);
        qjs_intl_list_formatter_close(formatter->list);
        qjs_intl_duration_digital_clear(&allocator, &formatter->digital);
        allocator.free(allocator.opaque, formatter);
    }
}

static int numeric(QJSIntlDurationUnitStyle style)
{
    return style == QJS_INTL_DURATION_UNIT_NUMERIC || style == QJS_INTL_DURATION_UNIT_2DIGIT;
}

static int has_fraction(const QJSIntlDurationOptions *options, unsigned int unit)
{
    return unit + 1 < QJS_TEMPORAL_UNIT_COUNT &&
           options->units[unit + 1].style == QJS_INTL_DURATION_UNIT_FRACTIONAL;
}

static QJSIntlNumberOptions number_options(const QJSIntlDurationOptions *options, unsigned int unit)
{
    static const char *const names[] = {"year", "month", "week", "day", "hour", "minute", "second",
                                       "millisecond", "microsecond", "nanosecond"};
    QJSIntlNumberOptions result = {0};
    QJSIntlDurationUnitStyle style = options->units[unit].style;
    int fraction = has_fraction(options, unit);
    result.style = numeric(style) ? QJS_INTL_NUMBER_DECIMAL : QJS_INTL_NUMBER_UNIT;
    result.grouping = numeric(style) ? QJS_INTL_NUMBER_GROUP_OFF : QJS_INTL_NUMBER_GROUP_AUTO;
    result.sign_display = QJS_INTL_NUMBER_SIGN_AUTO;
    result.maximum_output_length = options->maximum_output_length;
    result.digits = (QJSIntlDecimalOptions){.minimum_integer_digits = style == QJS_INTL_DURATION_UNIT_2DIGIT ? 2 : 1,
                                           .minimum_fraction_digits = fraction && options->fractional_digits >= 0 ?
                                               options->fractional_digits : 0,
                                           .maximum_fraction_digits = fraction ?
                                               (options->fractional_digits >= 0 ? options->fractional_digits : 9) : 3,
                                           .minimum_significant_digits = 1, .maximum_significant_digits = 21,
                                           .rounding_increment = 1,
                                           .rounding_mode = fraction ? QJS_INTL_ROUND_TRUNC : QJS_INTL_ROUND_HALF_EXPAND,
                                           .rounding_type = QJS_INTL_ROUND_FRACTION,
                                           .maximum_output_length = options->maximum_output_length};
    result.unit = (QJSIntlBytes){names[unit], strlen(names[unit])};
    result.unit_display = style == QJS_INTL_DURATION_UNIT_LONG ? QJS_INTL_NUMBER_UNIT_LONG :
                          style == QJS_INTL_DURATION_UNIT_NARROW ? QJS_INTL_NUMBER_UNIT_NARROW : QJS_INTL_NUMBER_UNIT_SHORT;
    return result;
}

QJSIntlStatus qjs_intl_duration_composition_open(const QJSIntlAllocator *allocator,
                                               const QJSIntlNumberService *number_service, size_t number_locale,
                                               const QJSIntlListService *list_service, size_t list_locale,
                                               QJSIntlBytes pattern, QJSIntlBytes system,
                                               const QJSIntlDurationOptions *options,
                                               QJSIntlDurationFormatter **out)
{
    QJSIntlDurationFormatter *formatter;
    QJSIntlListStyle list_style;
    QJSIntlStatus status;
    unsigned int unit;

    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free ||
        !number_service || !list_service || qjs_intl_duration_options_validate(options))
        return QJS_INTL_INVALID_ARGUMENT;
    formatter = allocator->malloc(allocator->opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = *allocator;
    formatter->options = *options;
    status = qjs_intl_duration_digital(&formatter->allocator, pattern, &formatter->digital);
    if (status)
        goto fail;
    list_style = options->style == QJS_INTL_DURATION_LONG ? QJS_INTL_LIST_LONG :
                 options->style == QJS_INTL_DURATION_NARROW ? QJS_INTL_LIST_NARROW : QJS_INTL_LIST_SHORT;
    status = qjs_intl_list_formatter_open(list_service, list_locale,
                                         QJS_INTL_LIST_UNIT, list_style, &formatter->list);
    if (status)
        goto fail;
    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        QJSIntlNumberOptions number;

        if (options->units[unit].style == QJS_INTL_DURATION_UNIT_FRACTIONAL)
            continue;
        number = number_options(options, unit);
        status = qjs_intl_number_formatter_open(number_service, number_locale,
                                                 system, &number, &formatter->numbers[unit]);
        if (status)
            goto fail;
    }
    *out = formatter;
    return QJS_INTL_OK;
fail:
    qjs_intl_duration_formatter_close(formatter);
    return status;
}

static int nonzero(const QJSTemporalDuration *duration, unsigned int unit, int fractional)
{
    unsigned int last = fractional ? QJS_TEMPORAL_NANOSECOND : unit;
    for (; unit <= last; unit++)
        if (duration->fields[unit] != 0)
            return 1;
    return 0;
}

static QJSIntlStatus component(const QJSIntlDurationFormatter *formatter, const QJSTemporalDuration *duration,
                               unsigned int unit, int *show_sign, QJSIntlFormatted *number,
                               QJSIntlDurationPart **parts, QJSIntlDurationComponent *out)
{
    QJSIntlMathematicalValue value;
    QJSIntlStatus status;
    char buffer[64];
    size_t i;
    status = qjs_intl_duration_component_decimal(duration, unit, has_fraction(&formatter->options, unit),
                                                buffer, sizeof(buffer), &value);
    if (status)
        return status;
    if (*show_sign && !nonzero(duration, unit, has_fraction(&formatter->options, unit)) &&
        qjs_temporal_duration_sign(duration) < 0)
        value.kind = QJS_INTL_NEGATIVE_ZERO;
    /* Later components use the unsigned pattern. HALF_EXPAND and TRUNC are
       sign-symmetric, so format their magnitude through the borrowed slice. */
    if (!*show_sign && value.decimal.length && value.decimal.data[0] == '-') {
        value.decimal.data++;
        value.decimal.length--;
    }
    status = qjs_intl_number_format(formatter->numbers[unit], &value, number);
    if (status)
        return status;
    if (number->part_count > SIZE_MAX / sizeof(**parts))
        return QJS_INTL_OVERFLOW;
    if (number->part_count) {
        *parts = formatter->allocator.malloc(formatter->allocator.opaque, number->part_count * sizeof(**parts));
        if (!*parts)
            return QJS_INTL_NO_MEMORY;
    }
    for (i = 0; i < number->part_count; i++)
        (*parts)[i] = (QJSIntlDurationPart){number->parts[i].start, number->parts[i].end, number->parts[i].type, unit};
    *out = (QJSIntlDurationComponent){{number->text, number->length}, *parts, number->part_count};
    *show_sign = 0;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_duration_format(const QJSIntlDurationFormatter *formatter, const QJSTemporalDuration *duration,
                                      QJSIntlDurationFormatted *out)
{
    QJSIntlFormatted numbers[QJS_TEMPORAL_UNIT_COUNT] = {{0}}, list = {0};
    QJSIntlDurationPart *parts[QJS_TEMPORAL_UNIT_COUNT] = {0};
    QJSIntlDurationComponent components[QJS_TEMPORAL_UNIT_COUNT], group[3];
    QJSIntlDurationFormatted digital = {0};
    QJSIntlUTF16 items[QJS_TEMPORAL_UNIT_COUNT];
    QJSIntlStatus status = QJS_INTL_OK;
    size_t count = 0, i;
    unsigned int unit;
    int show_sign = 1;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !formatter || !duration || !qjs_temporal_duration_is_valid(duration))
        return QJS_INTL_INVALID_ARGUMENT;
    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        QJSIntlDurationUnitOptions options = formatter->options.units[unit];
        if (numeric(options.style)) {
            int shown[3] = {0};
            QJSIntlBytes separators[2];
            unsigned int current;
            size_t group_count = 0;
            shown[0] = unit == QJS_TEMPORAL_HOUR &&
                (duration->fields[QJS_TEMPORAL_HOUR] != 0 || options.display == QJS_INTL_DURATION_ALWAYS);
            shown[2] = nonzero(duration, QJS_TEMPORAL_SECOND, 1) ||
                formatter->options.units[QJS_TEMPORAL_SECOND].display == QJS_INTL_DURATION_ALWAYS;
            shown[1] = unit <= QJS_TEMPORAL_MINUTE && ((shown[0] && shown[2]) ||
                duration->fields[QJS_TEMPORAL_MINUTE] != 0 ||
                formatter->options.units[QJS_TEMPORAL_MINUTE].display == QJS_INTL_DURATION_ALWAYS);
            for (current = QJS_TEMPORAL_HOUR; current <= QJS_TEMPORAL_SECOND; current++) {
                if (!shown[current - QJS_TEMPORAL_HOUR])
                    continue;
                if (group_count)
                    separators[group_count - 1] = current == QJS_TEMPORAL_MINUTE ?
                        formatter->digital.hour_minute : formatter->digital.minute_second;
                status = component(formatter, duration, current, &show_sign, &numbers[current],
                                    &parts[current], &group[group_count++]);
                if (status)
                    goto done;
            }
            if (group_count) {
                status = qjs_intl_duration_join_digital(&formatter->allocator,
                    group, group_count, separators, formatter->options.maximum_output_length, &digital);
                if (status)
                    goto done;
                components[count++] = (QJSIntlDurationComponent){{digital.text, digital.length}, digital.parts, digital.part_count};
            }
            break;
        }
        if (options.display == QJS_INTL_DURATION_ALWAYS || nonzero(duration, unit, has_fraction(&formatter->options, unit))) {
            status = component(formatter, duration, unit, &show_sign, &numbers[unit], &parts[unit], &components[count++]);
            if (status)
                goto done;
        }
        if (has_fraction(&formatter->options, unit))
            break;
    }
    for (i = 0; i < count; i++)
        items[i] = components[i].text;
    status = qjs_intl_list_format(formatter->list, items, count, &list);
    if (!status)
        status = qjs_intl_duration_join_parts(&formatter->allocator, &list, components, count,
                                             formatter->options.maximum_output_length, out);
done:
    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        qjs_intl_number_result_clear(&formatter->allocator, &numbers[unit]);
        if (parts[unit])
            formatter->allocator.free(formatter->allocator.opaque, parts[unit]);
    }
    qjs_intl_duration_result_clear(&formatter->allocator, &digital);
    qjs_intl_list_result_clear(&formatter->allocator, &list);
    return status;
}
