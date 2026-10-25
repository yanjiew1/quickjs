/* CLDR date/time interval fallback without trimming formatted prefixes.
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
#include "range-layout.h"
#include "pattern-info.h"
#include "../common/range-compose.h"
#include <string.h>

static int same(const QJSIntlDatePattern *a, const QJSIntlDatePattern *b)
{
    size_t i;
    if (a->count != b->count)
        return 0;
    for (i = 0; i < a->count; i++) {
        const QJSIntlDatePatternToken *x = &a->tokens[i], *y = &b->tokens[i];
        if (x->symbol != y->symbol || x->width != y->width ||
            x->literal.length != y->literal.length ||
            (x->literal.length && memcmp(x->literal.data, y->literal.data, x->literal.length)))
            return 0;
    }
    return 1;
}

void qjs_intl_date_range_layout_clear(QJSIntlDateRangeLayout *layout)
{
    if (!layout)
        return;
    qjs_intl_date_pattern_clear(&layout->allocator, &layout->date);
    qjs_intl_date_pattern_clear(&layout->allocator, &layout->clock);
    qjs_intl_date_pattern_clear(&layout->allocator, &layout->connector);
    memset(layout, 0, sizeof(*layout));
}

QJSIntlStatus qjs_intl_date_range_layout_open(const QJSIntlAllocator *allocator,
                                              const QJSIntlDatePattern *date,
                                              const QJSIntlDatePattern *clock,
                                              QJSIntlBytes connector,
                                              const QJSIntlDatePattern *ordinary,
                                              QJSIntlDateRangeLayout *out)
{
    const unsigned int date_fields = (1U << QJS_INTL_DATETIME_DAY_PERIOD) - 1;
    QJSIntlDateRangeLayout result = {0};
    QJSIntlDatePattern combined = {0};
    QJSIntlDatePatternInfo d, t;
    QJSIntlStatus status;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !ordinary)
        return QJS_INTL_INVALID_ARGUMENT;
    result.allocator = *allocator;
    status = qjs_intl_date_pattern_info(date, &d);
    if (!status)
        status = qjs_intl_date_pattern_info(clock, &t);
    if (status)
        return status;
    if (!qjs_intl_datetime_component_mask(&d.components) ||
        (qjs_intl_datetime_component_mask(&d.components) & ~date_fields) || d.period_symbol ||
        !t.hour_symbol || (qjs_intl_datetime_component_mask(&t.components) & date_fields))
        return QJS_INTL_UNSUPPORTED;
    status = qjs_intl_date_combination_compile(allocator, connector, &result.connector);
    if (status)
        goto done;
    for (i = 0; i < result.connector.count; i++) {
        unsigned int symbol = result.connector.tokens[i].symbol;
        if (symbol && symbol != '0' && symbol != '1') {
            status = QJS_INTL_UNSUPPORTED;
            goto done;
        }
    }
    status = qjs_intl_date_pattern_combine(allocator, &result.connector, clock, date, &combined);
    if (!status && !same(&combined, ordinary))
        status = QJS_INTL_UNSUPPORTED;
    if (!status)
        status = qjs_intl_date_pattern_copy(allocator, date, &result.date);
    if (!status)
        status = qjs_intl_date_pattern_copy(allocator, clock, &result.clock);
done:
    qjs_intl_date_pattern_clear(allocator, &combined);
    if (status)
        qjs_intl_date_range_layout_clear(&result);
    else
        *out = result;
    return status;
}

QJSIntlStatus qjs_intl_date_range_layout_compose(const QJSIntlDateRangeLayout *layout,
                                                 const QJSIntlPairPattern *fallback,
                                                 const QJSIntlFormatted *date,
                                                 const QJSIntlFormatted clocks[2],
                                                 size_t maximum_output_length,
                                                 QJSIntlFormattedRange *out)
{
    /* Two slots plus at most three coalesced literals; the clock slot expands
       to the five real fallback fragments. Do not erase nested provenance. */
    QJSIntlRangePiece pieces[9] = {{0}};
    size_t i, count = 0;
    unsigned int first;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !layout || !fallback || !date || !clocks ||
        !layout->date.count || !layout->clock.count ||
        layout->connector.count > 5 || fallback->first > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    first = fallback->first;
    for (i = 0; i < layout->connector.count; i++) {
        const QJSIntlDatePatternToken *token = &layout->connector.tokens[i];
        if (token->symbol == '1') {
            pieces[count++] = (QJSIntlRangePiece){date, {0}, 0, QJS_INTL_RANGE_SHARED};
        } else if (token->symbol == '0') {
            pieces[count++] = (QJSIntlRangePiece){NULL, fallback->prefix, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED};
            pieces[count++] = (QJSIntlRangePiece){&clocks[first], {0}, 0,
                                                  first ? QJS_INTL_RANGE_END : QJS_INTL_RANGE_START};
            pieces[count++] = (QJSIntlRangePiece){NULL, fallback->separator, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED};
            pieces[count++] = (QJSIntlRangePiece){&clocks[1 - first], {0}, 0,
                                                  first ? QJS_INTL_RANGE_START : QJS_INTL_RANGE_END};
            pieces[count++] = (QJSIntlRangePiece){NULL, fallback->suffix, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED};
        } else {
            pieces[count++] = (QJSIntlRangePiece){NULL, token->literal, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED};
        }
    }
    return qjs_intl_range_compose(&layout->allocator, pieces, count, maximum_output_length, out);
}
