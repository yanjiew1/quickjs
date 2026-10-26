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
#include "unit-icu4x.h"
#include "../icu4x/unit-data.h"
#include "../icu4x/common.h"
#include "../../icu4x/unit.h"
#include "../../icu4x/plural.h"
#include <string.h>

typedef struct UnitPatterns {
    QJSIntlBytes locale;
    uint32_t group;
    QJSIntlUnitWidth width;
    QJSICU4XUnit *handle;
    QJSICU4XUnitPattern choices[6];
} UnitPatterns;

typedef struct UnitRules {
    QJSIntlBytes locale;
    QJSICU4XPlural *handle;
    QJSICU4XPluralData rules;
} UnitRules;

struct QJSIntlICU4XNumberUnit {
    QJSIntlAllocator allocator;
    QJSIntlNumberUnit composition;
    UnitPatterns *patterns;
    UnitRules *rules;
    size_t pattern_count, rule_count;
};

static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}

static QJSIntlStatus locale_index(const QJSIntlData *data, QJSIntlBytes name, uint32_t *out)
{
    uint32_t lo = 0, hi = data->locales.count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2, parent;
        QJSIntlBytes candidate;
        size_t shared;
        int order;
        qjs_intl_data_locale(data, mid, &candidate, &parent);
        shared = name.length < candidate.length ? name.length : candidate.length;
        order = shared ? memcmp(name.data, candidate.data, shared) : 0;
        if (!order)
            order = (name.length > candidate.length) - (name.length < candidate.length);
        if (!order) {
            *out = mid;
            return QJS_INTL_OK;
        }
        if (order < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_icu4x_number_unit_data_open(const QJSIntlAllocator *allocator,
                                              QJSIntlICU4XUnitData *out)
{
    QJSIntlICU4XUnitData view = {0};
    QJSIntlData data;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_icu4x_status(qjs_icu4x_unit_check_version((const uint8_t *)qjs_icu4x_unit_version,
                                                           strlen(qjs_icu4x_unit_version)));
    if (!status)
        status = qjs_intl_data_open(qjs_icu4x_unit_bytes, qjs_icu4x_unit_length, allocator, &data);
    if (!status)
        status = qjs_intl_unit_data_open(&data, allocator, &view.units);
    if (!status)
        status = qjs_intl_unit_grammar_data_open(&data, allocator, &view.grammar);
    if (!status)
        status = locale_index(&data, (QJSIntlBytes){"root", 4}, &view.root);
    if (!status)
        *out = view;
    return status;
}

void qjs_icu4x_number_unit_close(QJSIntlICU4XNumberUnit *unit)
{
    size_t i;
    if (!unit)
        return;
    for (i = 0; i < unit->pattern_count; i++)
        qjs_icu4x_unit_close(unit->patterns[i].handle);
    for (i = 0; i < unit->rule_count; i++)
        qjs_icu4x_plural_close(unit->rules[i].handle);
    if (unit->patterns)
        unit->allocator.free(unit->allocator.opaque, unit->patterns);
    if (unit->rules)
        unit->allocator.free(unit->allocator.opaque, unit->rules);
    unit->allocator.free(unit->allocator.opaque, unit);
}

static QJSIntlStatus prepare_source(QJSIntlICU4XNumberUnit *unit, const QJSIntlUnitOverlay *layer)
{
    QJSIntlStatus status;
    QJSIntlPluralOperands probe = {"0", 1, 0};
    QJSIntlUnitGroup group;
    UnitRules *rules;
    UnitPatterns *patterns;
    size_t i;
    unsigned category;

    for (i = 0; i < unit->rule_count; i++)
        if (equal(unit->rules[i].locale, layer->locale))
            break;
    if (i == unit->rule_count) {
        QJSIntlBytes locale = layer->locale;
        rules = &unit->rules[unit->rule_count++];
        rules->locale = locale;
        if (equal(locale, (QJSIntlBytes){"root", 4}))
            locale = (QJSIntlBytes){"und", 3};
        status = qjs_icu4x_status(
            qjs_icu4x_plural_open((const uint8_t *)locale.data, locale.length, 0, &rules->handle));
        if (!status)
            status = qjs_icu4x_status(qjs_icu4x_plural_data(rules->handle, &rules->rules));
        for (category = 0; !status && category < 6; category++) {
            if (rules->rules.mask & (1u << category)) {
                QJSICU4XRuleBytes rule = rules->rules.rules[category];
                int matches;
                status = qjs_intl_plural_rule_evaluate(
                    (QJSIntlBytes){(const char *)rule.data, rule.length}, &probe, &matches);
            }
        }
        if (status)
            return status;
    }
    for (i = 0; i < unit->pattern_count; i++) {
        patterns = &unit->patterns[i];
        if (patterns->group == layer->group && patterns->width == layer->width &&
            equal(patterns->locale, layer->locale))
            return QJS_INTL_OK;
    }
    patterns = &unit->patterns[unit->pattern_count++];
    patterns->locale = layer->locale;
    patterns->group = layer->group;
    patterns->width = layer->width;
    status = qjs_intl_unit_group_at(&unit->composition.data, layer->group, &group);
    if (!status)
        status = qjs_icu4x_status(qjs_icu4x_unit_open(
            (const uint8_t *)layer->locale.data, layer->locale.length,
            (const uint8_t *)group.name.data, group.name.length, layer->width, &patterns->handle));
    /* The companion explicitly covers public API gaps, not a different locale's
       rules or an arbitrary parent payload supplied by a convenience fallback. */
    if (status == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    for (category = 0; !status && category < 6; category++)
        status = qjs_icu4x_status(
            qjs_icu4x_unit_pattern(patterns->handle, category, &patterns->choices[category]));
    return status;
}

QJSIntlStatus qjs_icu4x_number_unit_open(const QJSIntlAllocator *allocator,
                                         const QJSIntlICU4XUnitData *data, QJSIntlBytes locale,
                                         QJSIntlUnitWidth width, QJSIntlBytes identifier,
                                         QJSIntlICU4XNumberUnit **out)
{
    QJSIntlNumberUnit composition;
    QJSIntlICU4XNumberUnit *unit;
    uint32_t index, heads[6];
    size_t capacity = 0, i;
    QJSIntlStatus status;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->free || !data ||
        (!locale.data && locale.length))
        return QJS_INTL_INVALID_ARGUMENT;
    status = locale_index(&data->units.data, locale, &index);
    if (!status)
        status = qjs_intl_number_unit_open(&data->units, &data->grammar, index, data->root, width,
                                           identifier, &composition);
    if (status)
        return status;
    heads[0] = composition.local.full;
    heads[1] = composition.local.numerator;
    heads[2] = composition.local.denominator;
    heads[3] = composition.root.full;
    heads[4] = composition.root.numerator;
    heads[5] = composition.root.denominator;
    /* Every reachable source is prepared before formatting. Nodes point only to
       smaller IDs; count an upper bound first, then deduplicate retained owners. */
    for (i = 0; i < 6; i++) {
        uint32_t head = heads[i];
        while (head) {
            QJSIntlUnitOverlay layer;
            status = qjs_intl_unit_overlay_at(&composition.data, head, &layer);
            if (status)
                return status;
            if (capacity == SIZE_MAX)
                return QJS_INTL_OVERFLOW;
            capacity++;
            head = layer.next;
        }
    }
    if (!capacity || capacity > SIZE_MAX / sizeof(*unit->patterns) ||
        capacity > SIZE_MAX / sizeof(*unit->rules))
        return QJS_INTL_OVERFLOW;
    unit = allocator->malloc(allocator->opaque, sizeof(*unit));
    if (!unit)
        return QJS_INTL_NO_MEMORY;
    memset(unit, 0, sizeof(*unit));
    unit->allocator = *allocator;
    unit->composition = composition;
    unit->patterns = allocator->malloc(allocator->opaque, capacity * sizeof(*unit->patterns));
    unit->rules = allocator->malloc(allocator->opaque, capacity * sizeof(*unit->rules));
    if (!unit->patterns || !unit->rules) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    memset(unit->patterns, 0, capacity * sizeof(*unit->patterns));
    memset(unit->rules, 0, capacity * sizeof(*unit->rules));
    for (i = 0; i < 6; i++) {
        uint32_t head = heads[i];
        while (head) {
            QJSIntlUnitOverlay layer;
            status = qjs_intl_unit_overlay_at(&composition.data, head, &layer);
            if (!status)
                status = prepare_source(unit, &layer);
            if (status)
                goto fail;
            head = layer.next;
        }
    }
    *out = unit;
    return QJS_INTL_OK;
fail:
    qjs_icu4x_number_unit_close(unit);
    return status;
}

typedef struct UnitQuantity {
    const QJSIntlICU4XNumberUnit *unit;
    const QJSIntlPluralOperands *number;
    uint32_t shift;
} UnitQuantity;

static QJSIntlStatus category(void *opaque, QJSIntlBytes locale, QJSIntlUnitCount *out)
{
    const UnitQuantity *quantity = opaque;
    size_t i;
    for (i = 0; i < quantity->unit->rule_count; i++) {
        const UnitRules *rules = &quantity->unit->rules[i];
        unsigned choice;
        if (!equal(rules->locale, locale))
            continue;
        for (choice = 0; choice < QJS_INTL_PLURAL_OTHER; choice++) {
            QJSICU4XRuleBytes rule = rules->rules.rules[choice];
            QJSIntlStatus status;
            int matches;
            if (!(rules->rules.mask & (1u << choice)))
                continue;
            status = qjs_intl_plural_rule_evaluate_shifted(
                (QJSIntlBytes){(const char *)rule.data, rule.length}, quantity->number,
                quantity->shift, &matches);
            if (status)
                return status;
            if (matches)
                break;
        }
        *out = QJS_INTL_UNIT_COUNT_ZERO + choice;
        return QJS_INTL_OK;
    }
    return QJS_INTL_UNSUPPORTED;
}

static QJSIntlStatus materialize(void *opaque, const QJSIntlUnitOverlay *layer,
                                 const QJSIntlUnitForm *form, QJSIntlUnitMaterializedPattern *out)
{
    const QJSIntlICU4XNumberUnit *unit = opaque;
    QJSIntlUnitPattern original;
    unsigned has_number = 1;
    QJSIntlStatus status;
    size_t i;
    if (form->field != QJS_INTL_UNIT_PATTERN || form->count < QJS_INTL_UNIT_COUNT_ZERO ||
        form->count > QJS_INTL_UNIT_COUNT_OTHER ||
        (form->case_name.length && !equal(form->case_name, (QJSIntlBytes){"nominative", 10})))
        return QJS_INTL_UNSUPPORTED;
    status = qjs_intl_unit_pattern_parse(form->text, &original);
    if (status == QJS_INTL_UNSUPPORTED) {
        original = (QJSIntlUnitPattern){form->text, {0}};
        has_number = 0;
    } else if (status) {
        return status;
    }
    for (i = 0; i < unit->pattern_count; i++) {
        const UnitPatterns *patterns = &unit->patterns[i];
        const QJSICU4XUnitPattern *prepared;
        QJSIntlBytes prefix, suffix;
        if (patterns->group != layer->group || patterns->width != layer->width ||
            !equal(patterns->locale, layer->locale) || !patterns->handle)
            continue;
        prepared = &patterns->choices[form->count - QJS_INTL_UNIT_COUNT_ZERO];
        prefix = (QJSIntlBytes){(const char *)prepared->prefix.data, prepared->prefix.length};
        suffix = (QJSIntlBytes){(const char *)prepared->suffix.data, prepared->suffix.length};
        /* Public get() can silently choose 'other' or another default case.
           Accept it only when the selected companion leaf proves the same
           decoded representation. Missing/blocked/case-specific forms retain
           their explicit source semantics instead of being invented here. */
        if (prepared->has_number != has_number || !equal(prefix, original.prefix) ||
            !equal(suffix, original.suffix))
            return QJS_INTL_UNSUPPORTED;
        *out = (QJSIntlUnitMaterializedPattern){{prefix, suffix}, has_number};
        return QJS_INTL_OK;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_icu4x_number_unit_format(const QJSIntlICU4XNumberUnit *unit,
                                           const QJSIntlPluralOperands *number, uint32_t shift,
                                           int exact, const QJSIntlFormatted *body, size_t limit,
                                           QJSIntlFormatted *out)
{
    QJSIntlUnitPatternRequest request = {0};
    UnitQuantity quantity = {unit, number, shift};
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !unit || exact < -1 || exact > 1 || (!number && (exact != -1 || shift)))
        return QJS_INTL_INVALID_ARGUMENT;
    request.resolve = number ? category : NULL;
    request.opaque = &quantity;
    request.category = number ? QJS_INTL_UNIT_COUNT_NONE : QJS_INTL_UNIT_COUNT_OTHER;
    request.exact = exact;
    request.materialize = materialize;
    request.materialize_opaque = (void *)unit;
    return qjs_intl_number_unit_format(&unit->composition, &unit->allocator, &request, body, limit,
                                       out);
}
