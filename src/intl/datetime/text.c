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
#include "text.h"
#include "name-select.h"
#include "../common/format-builder.h"
#include "../common/unary-pattern.h"
#include "../../calendar/calendar.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define MAX_TOKENS (2 * (QJS_INTL_DATETIME_COMPONENT_COUNT + 2) + 1)

typedef struct Piece {
    QJSIntlFormatted number;
    size_t cut;
    QJSIntlBytes text;
    QJSIntlUnaryPattern affix;
    QJSIntlPartType type;
} Piece;

static QJSIntlStatus numeric_value(const QJSIntlDatePatternToken *token,
                                  const QJSIntlDateNumericFields *local, QJSIntlDateHourCycle cycle,
                                  int64_t *out, QJSIntlPartType *part, unsigned int *digits)
{
    *digits = token->width == 2 ? 2 : 1;
    switch (token->symbol) {
    case 'y':
        *out = local->year <= 0 ? 1 - (int64_t)local->year : local->year;
        *part = QJS_INTL_PART_YEAR;
        break;
    case 'r':
        *out = local->related_year;
        *part = QJS_INTL_PART_RELATED_YEAR;
        *digits = 1;
        break;
    case 'M': case 'L':
        if (token->width > 2)
            return QJS_INTL_UNSUPPORTED;
        *out = local->month;
        *part = QJS_INTL_PART_MONTH;
        break;
    case 'd':
        *out = local->day;
        *part = QJS_INTL_PART_DAY;
        break;
    case 'h': case 'H': case 'K': case 'k':
        *out = local->hour;
        if (cycle == QJS_INTL_DATE_H11 || cycle == QJS_INTL_DATE_H12) {
            *out %= 12;
            if (!*out && cycle == QJS_INTL_DATE_H12)
                *out = 12;
        } else if (!*out && cycle == QJS_INTL_DATE_H24) {
            *out = 24;
        }
        *part = QJS_INTL_PART_HOUR;
        break;
    case 'm':
        *out = local->minute;
        *part = QJS_INTL_PART_MINUTE;
        break;
    case 's':
        *out = local->second;
        *part = QJS_INTL_PART_SECOND;
        break;
    case 'S':
        *out = local->millisecond / (token->width == 1 ? 100 : token->width == 2 ? 10 : 1);
        *digits = (unsigned int)token->width;
        *part = QJS_INTL_PART_FRACTIONAL_SECOND;
        break;
    default:
        return QJS_INTL_UNSUPPORTED;
    }
    return QJS_INTL_OK;
}

/* Find the last two code points in the entire FormatNumeric result. This also
   respects supplementary decimal digits and any locale pattern literals. */
static size_t two_digit_start(const QJSIntlFormatted *number)
{
    size_t at = number->length;
    unsigned int count;
    for (count = 0; count < 2 && at; count++) {
        uint16_t c = number->text[--at];
        if (c >= 0xdc00 && c <= 0xdfff && at &&
            number->text[at - 1] >= 0xd800 && number->text[at - 1] <= 0xdbff)
            --at;
    }
    return at;
}

static QJSIntlStatus prepare_numeric(const QJSIntlAllocator *allocator,
                                    const QJSIntlDatePatternToken *token,
                                    const QJSIntlDateNumericFields *local,
                                    QJSIntlDateHourCycle cycle,
                                    const QJSIntlDateNumberFormats *numbers,
                                    Piece *piece)
{
    const QJSIntlNumberFormatter *formatter;
    QJSIntlMathematicalValue mathematical = {0};
    char text[32];
    int64_t field;
    int length;
    unsigned int digits;
    QJSIntlStatus status = numeric_value(token, local, cycle, &field, &piece->type, &digits);

    if (status)
        return status;
    formatter = token->symbol == 'S' ? numbers->fraction[digits - 1] :
                digits == 2 ? numbers->two_digit : numbers->numeric;
    if (!formatter)
        return QJS_INTL_INVALID_ARGUMENT;
    length = snprintf(text, sizeof(text), "%" PRId64, field);
    if (length <= 0 || (size_t)length >= sizeof(text))
        return QJS_INTL_OVERFLOW;
    mathematical.decimal = (QJSIntlBytes){text, (size_t)length};
    status = qjs_intl_number_format(formatter, &mathematical, &piece->number);
    if (status)
        return status;
    if (!piece->number.text || !piece->number.length)
        return QJS_INTL_DATA_ERROR;
    if (digits == 2 && token->symbol != 'S')
        piece->cut = two_digit_start(&piece->number);
    return QJS_INTL_OK;
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && !memcmp(text.data, value, text.length);
}

static QJSIntlStatus era(const QJSIntlDateTextData *data, const QJSIntlDateCalendarFields *local,
                         uint32_t *calendar, QJSIntlBytes *key)
{
    const char *identifier;
    QJSIntlBytes raw, selected;
    QJSIntlStatus status;
    uint32_t source;
    if (!local->has_era)
        return QJS_INTL_UNSUPPORTED;
    if (!data->metadata || !data->eras ||
        !(identifier = qjs_calendar_identifier(local->era_calendar)))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_name_calendar_index(data->metadata, data->names, bytes(identifier), &source);
    if (!status)
        status = qjs_intl_calendar_name_calendar_at(data->names, source, &raw);
    if (!status)
        status = qjs_intl_calendar_era_find(data->eras, raw, bytes(local->era_code), key);
    if (!status)
        status = qjs_intl_calendar_name_calendar_at(data->names, data->calendar, &selected);
    if (!status) {
        /* ISO has explicit name aliases/overrides but no era arithmetic of its
           own. Pre-adoption Japanese, in contrast, needs Gregorian labels. */
        *calendar = equal(selected, "iso8601") ? data->calendar : source;
    }
    return status;
}

static QJSIntlStatus leap_affix(const QJSIntlDateTextData *data,
                                const QJSIntlDatePatternToken *token, QJSIntlUnaryPattern *out)
{
    QJSIntlCalendarNameGroup group = {0};
    QJSIntlCalendarNameEntry entry;
    QJSIntlCalendarNameNode origin;
    QJSIntlStatus status;
    uint32_t index, head;
    group.calendar = data->calendar;
    group.family = QJS_INTL_CAL_NAME_MONTH_PATTERN;
    /* Numeric M/L share the numeric/all template, independently of name
       context. Apply it after digit formatting, including two-digit clipping. */
    group.context = token->width <= 2 ? QJS_INTL_CAL_NAME_NUMERIC :
                    token->symbol == 'L' ? QJS_INTL_CAL_NAME_STAND_ALONE : QJS_INTL_CAL_NAME_FORMAT;
    group.width = token->width <= 2 ? QJS_INTL_CAL_NAME_ALL :
                  token->width == 4 ? QJS_INTL_CAL_NAME_WIDE :
                  token->width == 5 ? QJS_INTL_CAL_NAME_NARROW : QJS_INTL_CAL_NAME_ABBREVIATED;
    status = qjs_intl_calendar_name_find_group(data->names, &group, &index);
    if (!status)
        status = qjs_intl_calendar_name_head(data->names, data->locale, index, &head);
    if (!status)
        status = qjs_intl_calendar_name_resolve(data->names, head, bytes("leap"),
                                               QJS_INTL_CAL_NAME_NO_YEAR, (QJSIntlBytes){0},
                                               &entry, &origin);
    if (status)
        return status;
    if (entry.blocked || !entry.text.length)
        return QJS_INTL_UNSUPPORTED;
    return qjs_intl_unary_pattern_parse(entry.text, out);
}

static QJSIntlStatus named(const QJSIntlDateTextData *data, const QJSIntlDateCalendarFields *local,
                           unsigned int subsecond, const QJSIntlDatePatternToken *token, Piece *out)
{
    static const char *const weekdays[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
    QJSIntlDateNameSelection selected;
    QJSIntlBytes code;
    QJSIntlStatus status;
    uint32_t calendar = data->calendar;
    switch (token->symbol) {
    case 'G':
        status = era(data, local, &calendar, &code);
        if (status)
            return status;
        out->type = QJS_INTL_PART_ERA;
        break;
    case 'M': case 'L':
        code = bytes(local->month_key);
        out->type = QJS_INTL_PART_MONTH;
        break;
    case 'E': case 'e': case 'c':
        code = bytes(weekdays[local->weekday]);
        out->type = QJS_INTL_PART_WEEKDAY;
        break;
    case 'U':
        if (!local->has_cyclic_year)
            return QJS_INTL_UNSUPPORTED;
        code = bytes(local->cyclic_key);
        out->type = QJS_INTL_PART_YEAR_NAME;
        break;
    case 'a': case 'b': case 'B': {
        QJSIntlDayPeriodCode primary, fallback = local->numeric.hour < 12 ?
                                                QJS_INTL_DAYPERIOD_AM : QJS_INTL_DAYPERIOD_PM;
        QJSIntlDatePeriodNameSelection period;
        uint32_t second = (local->numeric.hour * 60 + local->numeric.minute) * 60 + local->numeric.second;
        status = qjs_intl_dayperiod_select(data->periods, data->period_rules, token->symbol,
                                           second, subsecond, data->use_midnight, &primary);
        if (!status)
            status = qjs_intl_date_period_name_select(data->names, data->locale, calendar,
                                                     token->symbol, token->width, primary, fallback, &period);
        if (status)
            return status;
        selected = period.selected;
        out->type = QJS_INTL_PART_DAY_PERIOD;
        goto selected_name;
    }
    default:
        return QJS_INTL_UNSUPPORTED;
    }
    status = qjs_intl_date_name_select(data->names, data->locale, calendar, token->symbol,
                                      token->width, code, local->in_leap_year, &selected);
    if (status)
        return status;
selected_name:
    if (selected.state != QJS_INTL_DATE_NAME_TEXT)
        return QJS_INTL_UNSUPPORTED;
    out->text = selected.entry.text;
    /* Chinese/Dangi have a repeated nominal month. Hebrew's inserted Adar has
       an independent month key/name, and must not receive a lunar prefix. */
    if ((token->symbol == 'M' || token->symbol == 'L') && local->has_cyclic_year && local->leap_month)
        return leap_affix(data, token, &out->affix);
    return QJS_INTL_OK;
}

static void small_key(char out[3], unsigned int number)
{
    if (number < 10) {
        out[0] = '0' + number;
        out[1] = 0;
    } else {
        out[0] = '0' + number / 10;
        out[1] = '0' + number % 10;
        out[2] = 0;
    }
}

static QJSIntlStatus probe_name(const QJSIntlDateTextData *data,
                                const QJSIntlDateCalendarFields *local,
                                const QJSIntlDatePatternToken *token, unsigned int fraction)
{
    Piece piece = {0};
    return named(data, local, fraction, token, &piece);
}

static QJSIntlStatus admit_eras(const QJSIntlDateTextData *data, QJSCalendarId calendar,
                                const QJSIntlDatePatternToken *token)
{
    QJSIntlDateCalendarFields local = {0};
    size_t i;
    local.has_era = 1;
    for (i = 0; ; i++) {
        const char *code;
        int error;
        QJSIntlStatus status;
        local.era_calendar = calendar;
        if (calendar == QJS_CAL_ISO8601 || calendar == QJS_CAL_GREGORY) {
            if (i == 2)
                return QJS_INTL_OK;
            code = i ? "bce" : "ce";
            local.era_calendar = QJS_CAL_GREGORY;
        } else {
            error = qjs_calendar_era_at(calendar, i, &code);
            if (error == QJS_CAL_UNSUPPORTED)
                return i ? QJS_INTL_OK : QJS_INTL_UNSUPPORTED;
            if (error)
                return error == QJS_CAL_MEMORY ? QJS_INTL_NO_MEMORY : QJS_INTL_DATA_ERROR;
            if (calendar == QJS_CAL_JAPANESE && (!strcmp(code, "ce") || !strcmp(code, "bce")))
                local.era_calendar = QJS_CAL_GREGORY;
        }
        if (strlen(code) >= sizeof(local.era_code))
            return QJS_INTL_DATA_ERROR;
        strcpy(local.era_code, code);
        status = probe_name(data, &local, token, 0);
        if (status)
            return status;
    }
}

static QJSIntlStatus probe_clock(const QJSIntlDateTextData *data,
                                 const QJSIntlDatePatternToken *token, uint32_t second)
{
    QJSIntlDateCalendarFields local = {0};
    QJSIntlStatus status;
    local.numeric.hour = second / 3600;
    local.numeric.minute = second / 60 % 60;
    local.numeric.second = second % 60;
    status = probe_name(data, &local, token, 0);
    return status ? status : probe_name(data, &local, token, 1);
}

static QJSIntlStatus admit_periods(const QJSIntlDateTextData *data,
                                   const QJSIntlDatePatternToken *token)
{
    QJSIntlStatus status = probe_clock(data, token, 0);
    uint32_t i;
    if (!status)
        status = probe_clock(data, token, 43200);
    if (status || token->symbol != 'B' || !data->period_rules)
        return status;
    /* Rule boundaries and the AM/PM boundary partition every possible clock
       outcome. A fractional probe removes an exact-point override, exposing
       the variable-range or fallback result underneath it. */
    for (i = 0; i < data->period_rules->count; i++) {
        QJSIntlDayPeriodRule rule;
        status = qjs_intl_dayperiod_rule(data->periods, data->period_rules->start + i, &rule);
        if (!status)
            status = probe_clock(data, token, rule.start % 86400);
        if (!status)
            status = probe_clock(data, token, rule.end % 86400);
        if (status)
            return status;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus admit_field(const QJSIntlDateTextData *data, QJSCalendarId calendar,
                                 const QJSIntlDatePatternToken *token)
{
    QJSIntlDateCalendarFields local = {0};
    QJSIntlStatus status;
    unsigned int i, leap;
    if (token->symbol == 'G')
        return admit_eras(data, calendar, token);
    if (token->symbol == 'a' || token->symbol == 'b' || token->symbol == 'B')
        return admit_periods(data, token);
    if (token->symbol == 'E' || token->symbol == 'e' || token->symbol == 'c') {
        for (i = 0; i < 7; i++) {
            local.weekday = i;
            status = probe_name(data, &local, token, 0);
            if (status)
                return status;
        }
        return QJS_INTL_OK;
    }
    if (token->symbol == 'U') {
        if (calendar != QJS_CAL_CHINESE && calendar != QJS_CAL_DANGI)
            return QJS_INTL_UNSUPPORTED;
        local.has_cyclic_year = 1;
        for (i = 1; i <= 60; i++) {
            small_key(local.cyclic_key, i);
            status = probe_name(data, &local, token, 0);
            if (status)
                return status;
        }
        return QJS_INTL_OK;
    }
    if (token->symbol == 'M' || token->symbol == 'L') {
        unsigned int last = calendar == QJS_CAL_COPTIC || calendar == QJS_CAL_ETHIOAA ||
                            calendar == QJS_CAL_ETHIOPIC || calendar == QJS_CAL_HEBREW ? 13 : 12;
        if (token->width <= 2) {
            QJSIntlUnaryPattern affix;
            return leap_affix(data, token, &affix);
        }
        for (i = 1; i <= last; i++) {
            small_key(local.month_key, i);
            for (leap = 0; leap <= 1; leap++) {
                if (calendar == QJS_CAL_HEBREW && i == 6 && !leap)
                    continue;
                local.in_leap_year = leap;
                status = probe_name(data, &local, token, 0);
                if (status)
                    return status;
            }
        }
        if (calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI) {
            QJSIntlUnaryPattern affix;
            return leap_affix(data, token, &affix);
        }
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_names_admit(const QJSIntlDatePattern *pattern,
                                       QJSCalendarId calendar, const QJSIntlDateTextData *data,
                                       QJSIntlDateNameAdmission *cache)
{
    static const char symbols[] = "GMLEecUabB";
    QJSIntlDatePatternInfo info;
    QJSIntlStatus status;
    size_t i;
    if ((unsigned int)calendar >= QJS_CAL_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_pattern_info(pattern, &info);
    if (status)
        return status;
    if ((info.year_kinds & QJS_INTL_DATE_YEAR_RELATED) &&
        calendar != QJS_CAL_CHINESE && calendar != QJS_CAL_DANGI)
        return QJS_INTL_UNSUPPORTED;
    if (cache) {
        if (cache->data && (cache->data != data || cache->calendar != calendar))
            return QJS_INTL_INVALID_ARGUMENT;
        cache->data = data;
        cache->calendar = calendar;
    }
    for (i = 0; i < pattern->count; i++) {
        const QJSIntlDatePatternToken *token = &pattern->tokens[i];
        const char *found;
        size_t field;
        if (!token->symbol)
            continue;
        if ((token->symbol == 'M' || token->symbol == 'L') && token->width <= 2 &&
            calendar != QJS_CAL_CHINESE && calendar != QJS_CAL_DANGI)
            continue;
        found = strchr(symbols, token->symbol);
        if (!found)
            continue;
        if (!data || !data->names || !data->names->section.bytes ||
            data->locale >= data->names->section.count || data->calendar >= data->names->calendar_count)
            return QJS_INTL_INVALID_ARGUMENT;
        field = found - symbols;
        if (token->width >= 7)
            return QJS_INTL_UNSUPPORTED;
        if (cache && cache->fields[field][token->width]) {
            if (cache->fields[field][token->width] == 2)
                return QJS_INTL_UNSUPPORTED;
            continue;
        }
        status = admit_field(data, calendar, token);
        if (cache && (status == QJS_INTL_OK || status == QJS_INTL_UNSUPPORTED))
            cache->fields[field][token->width] = status ? 2 : 1;
        if (status)
            return status;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus emit(QJSIntlFormatBuilder *builder, const QJSIntlDatePattern *pattern,
                          const Piece *pieces)
{
    size_t i;
    for (i = 0; i < pattern->count; i++) {
        const QJSIntlDatePatternToken *token = &pattern->tokens[i];
        const Piece *piece = &pieces[i];
        size_t start = builder->length;
        QJSIntlStatus status;
        QJSIntlPart part = {0, piece->number.length - piece->cut, piece->type};
        QJSIntlFormatted number = {piece->number.text ? piece->number.text + piece->cut : NULL,
                                  part.end, &part, 1};
        if (piece->number.text && (piece->affix.prefix.length || piece->affix.suffix.length)) {
            /* The marker belongs to the month part, not a separate literal.
               Suppress the digit body's part while retaining its UTF-16 text. */
            QJSIntlFormatted digits = number;
            digits.part_count = 0;
            status = qjs_intl_format_builder_text(builder, piece->affix.prefix, NULL, NULL);
            if (!status)
                status = qjs_intl_format_builder_body(builder, &digits);
            if (!status)
                status = qjs_intl_format_builder_text(builder, piece->affix.suffix, NULL, NULL);
            if (!status)
                status = qjs_intl_format_builder_part(builder, start, builder->length, QJS_INTL_PART_MONTH, 0);
        } else if (piece->number.text) {
            status = qjs_intl_format_builder_body(builder, &number);
        } else {
            status = qjs_intl_format_builder_text(builder, piece->affix.prefix, NULL, NULL);
            if (!status)
                status = qjs_intl_format_builder_text(builder, token->symbol ? piece->text : token->literal,
                                                       NULL, NULL);
            if (!status)
                status = qjs_intl_format_builder_text(builder, piece->affix.suffix, NULL, NULL);
            if (!status)
                status = qjs_intl_format_builder_part(builder, start, builder->length,
                                                       token->symbol ? piece->type : QJS_INTL_PART_LITERAL, 0);
        }
        if (status)
            return status;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_text_format(const QJSIntlAllocator *allocator,
                                       const QJSIntlDatePattern *pattern,
                                       const QJSIntlDateCalendarFields *local,
                                       unsigned int subsecond, QJSIntlDateHourCycle cycle,
                                       const QJSIntlDateNumberFormats *numbers,
                                       const QJSIntlDateTextData *data,
                                       size_t maximum_output_length, QJSIntlFormatted *out)
{
    Piece pieces[MAX_TOKENS] = {0};
    QJSIntlFormatBuilder builder = {0};
    QJSIntlDatePatternInfo info;
    QJSIntlStatus status;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !local || !numbers || !data ||
        !data->names || !data->names->section.bytes || data->locale >= data->names->section.count ||
        data->calendar >= data->names->calendar_count || !maximum_output_length ||
        (unsigned int)cycle > QJS_INTL_DATE_H24 || subsecond > 1 ||
        (local->numeric.millisecond && !subsecond) || local->weekday > 6 ||
        local->in_leap_year > 1 || local->leap_month > 1 || local->has_era > 1 ||
        local->has_cyclic_year > 1 || !local->numeric.month || local->numeric.month > 13 ||
        !local->numeric.day || local->numeric.day > 31 || local->numeric.hour > 23 ||
        local->numeric.minute > 59 || local->numeric.second > 59 || local->numeric.millisecond > 999)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_pattern_info(pattern, &info);
    if (status)
        return status;
    if (pattern->count > MAX_TOKENS)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < pattern->count; i++) {
        unsigned int symbol = pattern->tokens[i].symbol;
        if (!symbol)
            continue;
        status = prepare_numeric(allocator, &pattern->tokens[i], &local->numeric, cycle,
                                 numbers, &pieces[i]);
        if (status == QJS_INTL_UNSUPPORTED)
            status = named(data, local, subsecond, &pattern->tokens[i], &pieces[i]);
        else if (!status && (symbol == 'M' || symbol == 'L') &&
                 local->has_cyclic_year && local->leap_month)
            status = leap_affix(data, &pattern->tokens[i], &pieces[i].affix);
        if (status)
            goto done;
    }
    builder.limit = maximum_output_length;
    status = emit(&builder, pattern, pieces);
    if (status)
        goto done;
    if (builder.length > SIZE_MAX / sizeof(uint16_t)) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    if (builder.length)
        builder.result.text = allocator->malloc(allocator->opaque, builder.length * sizeof(uint16_t));
    if (builder.count)
        builder.result.parts = allocator->malloc(allocator->opaque, builder.count * sizeof(QJSIntlPart));
    if ((builder.length && !builder.result.text) || (builder.count && !builder.result.parts)) {
        status = QJS_INTL_NO_MEMORY;
        goto done;
    }
    builder.result.length = builder.length;
    builder.result.part_count = builder.count;
    builder.length = builder.count = 0;
    status = emit(&builder, pattern, pieces);
    if (!status) {
        *out = builder.result;
        memset(&builder.result, 0, sizeof(builder.result));
    }
done:
    for (i = 0; i < MAX_TOKENS; i++)
        qjs_intl_formatted_clear(allocator, &pieces[i].number);
    qjs_intl_formatted_clear(allocator, &builder.result);
    return status;
}
