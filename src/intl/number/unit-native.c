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
#include "unit-native.h"
#include "../plural/native-engine.h"
#include <string.h>

typedef struct UnitQuantity {
    const QJSIntlPluralData *data;
    const QJSIntlPluralOperands *number;
    uint32_t shift;
} UnitQuantity;

static QJSIntlStatus category(void *opaque, QJSIntlBytes source, QJSIntlUnitCount *out)
{
    UnitQuantity *quantity = opaque;
    const QJSIntlData *data = &quantity->data->data;
    QJSIntlPluralRuleSet rules;
    QJSIntlPluralCategory result;
    QJSIntlStatus status;
    uint32_t lo = 0, hi = data->locales.count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2, parent;
        QJSIntlBytes tag;
        size_t shared;
        int order;
        qjs_intl_data_locale(data, mid, &tag, &parent);
        shared = source.length < tag.length ? source.length : tag.length;
        order = shared ? memcmp(source.data, tag.data, shared) : 0;
        if (!order)
            order = (source.length > tag.length) - (source.length < tag.length);
        if (!order) {
            status =
                qjs_intl_plural_rules_at(quantity->data, mid, QJS_INTL_PLURAL_CARDINAL, &rules);
            if (!status)
                status = qjs_intl_native_plural_select_shifted(&rules, quantity->number,
                                                               quantity->shift, &result);
            if (!status)
                *out = result + QJS_INTL_UNIT_COUNT_ZERO;
            return status;
        }
        if (order < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_number_unit_native_open(const QJSIntlUnitData *data,
                                               const QJSIntlPluralData *plurals,
                                               const QJSIntlUnitGrammarData *grammar,
                                               uint32_t locale, uint32_t root,
                                               QJSIntlUnitWidth width, QJSIntlBytes name,
                                               QJSIntlNativeNumberUnit *out)
{
    QJSIntlNativeNumberUnit configured = {0};
    QJSIntlStatus status;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !plurals || !plurals->section.bytes ||
        data->data.pack.bytes != plurals->data.pack.bytes)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_number_unit_open(data, grammar, locale, root, width, name,
                                       &configured.composition);
    if (!status) {
        configured.plurals = *plurals;
        *out = configured;
    }
    return status;
}

QJSIntlStatus qjs_intl_number_unit_native_format(const QJSIntlNativeNumberUnit *unit,
                                                 const QJSIntlAllocator *allocator,
                                                 const QJSIntlPluralOperands *number,
                                                 uint32_t shift, int exact,
                                                 const QJSIntlFormatted *body, size_t limit,
                                                 QJSIntlFormatted *out)
{
    QJSIntlUnitPatternRequest request = {0};
    UnitQuantity quantity;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !unit || exact < -1 || exact > 1 || (!number && (exact != -1 || shift)))
        return QJS_INTL_INVALID_ARGUMENT;
    quantity = (UnitQuantity){&unit->plurals, number, shift};
    request.resolve = number ? category : NULL;
    request.opaque = &quantity;
    request.category = number ? QJS_INTL_UNIT_COUNT_NONE : QJS_INTL_UNIT_COUNT_OTHER;
    request.exact = exact;
    return qjs_intl_number_unit_format(&unit->composition, allocator, &request, body, limit, out);
}
