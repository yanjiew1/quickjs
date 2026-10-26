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
#include "unit.h"
#include "../unit/identifier.h"
#include <string.h>

static QJSIntlStatus overlay(const QJSIntlUnitData *data, uint32_t locale, QJSIntlUnitWidth width,
                             QJSIntlUnitGroupKind kind, const char *name, uint32_t *out)
{
    QJSIntlStatus status;
    uint32_t group;
    *out = 0;
    if (!name)
        return QJS_INTL_OK;
    status = qjs_intl_unit_group_find(data, kind, (QJSIntlBytes){name, strlen(name)}, &group);
    if (status == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    if (!status)
        status = qjs_intl_unit_overlay_head(data, locale, width, group, out);
    return status;
}

static QJSIntlStatus composition(const QJSIntlUnitData *data, const QJSIntlUnitGrammarData *grammar,
                                 uint32_t locale, QJSIntlUnitWidth width,
                                 const QJSIntlUnitIdentifier *unit, QJSIntlNumberUnitComposition *out)
{
    const QJSIntlUnitDefinition *first = qjs_intl_unit_definition_at(unit->numerator);
    const QJSIntlUnitDefinition *second = qjs_intl_unit_definition_at(unit->denominator);
    QJSIntlStatus status = overlay(data, locale, width, QJS_INTL_UNIT_GROUP_SINGLE,
                                    qjs_intl_unit_identifier_cldr(unit), &out->full);
    if (status || !second)
        return status;
    status = overlay(data, locale, width, QJS_INTL_UNIT_GROUP_SINGLE, first->cldr_identifier,
                       &out->numerator);
    if (!status)
        status = overlay(data, locale, width, QJS_INTL_UNIT_GROUP_SINGLE, second->cldr_identifier,
                           &out->denominator);
    if (!status)
        status = overlay(data, locale, width, QJS_INTL_UNIT_GROUP_COMPOUND, "per", &out->division);
    if (!status)
        status = qjs_intl_unit_grammar_at(grammar, locale, QJS_INTL_UNIT_GRAMMAR_PLURAL, &out->plural);
    if (!status)
        status = qjs_intl_unit_grammar_at(grammar, locale, QJS_INTL_UNIT_GRAMMAR_CASE, &out->case_rule);
    return status;
}

QJSIntlStatus qjs_intl_number_unit_open(const QJSIntlUnitData *data,
                                        const QJSIntlUnitGrammarData *grammar, uint32_t locale,
                                        uint32_t root, QJSIntlUnitWidth width, QJSIntlBytes name,
                                        QJSIntlNumberUnit *out)
{
    QJSIntlUnitIdentifier unit;
    QJSIntlNumberUnit configured = {0};
    QJSIntlBytes root_name;
    QJSIntlStatus status;
    uint32_t parent;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !grammar || !grammar->section.bytes ||
        data->data.pack.bytes != grammar->data.pack.bytes || locale >= data->data.locales.count ||
        (unsigned int)width >= QJS_INTL_UNIT_WIDTH_COUNT ||
        qjs_intl_data_locale(&data->data, root, &root_name, &parent) || parent != UINT32_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_unit_identifier_parse(name, &unit);
    if (status)
        return status;
    configured.compound = unit.denominator != QJS_INTL_NO_UNIT;
    status = composition(data, grammar, locale, width, &unit, &configured.local);
    if (!status)
        status = composition(data, grammar, root, width, &unit, &configured.root);
    if (status)
        return status;
    if (!configured.local.full && !(configured.local.numerator && configured.local.denominator))
        return QJS_INTL_UNSUPPORTED;
    configured.data = *data;
    configured.active = 1;
    *out = configured;
    return QJS_INTL_OK;
}

typedef struct UnitSource {
    const QJSIntlUnitPatternRequest *request;
    QJSIntlStatus error;
} UnitSource;

static QJSIntlStatus category(void *opaque, QJSIntlBytes source, QJSIntlUnitCount *out)
{
    UnitSource *quantity = opaque;
    quantity->error = quantity->request->resolve(quantity->request->opaque, source, out);
    return quantity->error;
}

static int token(QJSIntlBytes value, const char *text)
{
    return value.length == strlen(text) && !memcmp(value.data, text, value.length);
}

static QJSIntlStatus derived_request(const QJSIntlUnitPatternRequest *whole,
                                     QJSIntlBytes plural, QJSIntlBytes case_name,
                                     QJSIntlUnitPatternRequest *out)
{
    static const char *categories[] = {"zero", "one", "two", "few", "many", "other"};
    size_t i;
    *out = *whole;
    if (!token(case_name, "compound"))
        out->case_name = case_name;
    if (token(plural, "compound"))
        return QJS_INTL_OK;
    for (i = 0; i < sizeof(categories) / sizeof(categories[0]); i++) {
        if (token(plural, categories[i])) {
            out->resolve = NULL;
            out->category = QJS_INTL_UNIT_COUNT_ZERO + i;
            out->exact = -1;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_DATA_ERROR;
}

static QJSIntlStatus format_composition(const QJSIntlNumberUnit *unit,
                                        const QJSIntlNumberUnitComposition *plan,
                                        const QJSIntlUnitPatternRequest *whole,
                                        const QJSIntlAllocator *allocator,
                                        const QJSIntlFormatted *body, size_t limit,
                                        QJSIntlFormatted *out)
{
    QJSIntlSelectedUnitPattern numerator;
    QJSIntlSelectedUnitNoun denominator;
    QJSIntlSelectedUnitTemplate layout;
    QJSIntlUnitPatternRequest request;
    QJSIntlBytes prefix[4] = {0}, suffix[4] = {0}, noun;
    QJSIntlUnitAffix before = {prefix, 2}, after = {suffix, 2};
    const UnitSource *quantity = whole->opaque;
    QJSIntlStatus status;
    if (plan->full) {
        status = qjs_intl_unit_pattern_select(&unit->data, plan->full, whole, &numerator);
        if (!status)
            return qjs_intl_unit_pattern_format(allocator, &numerator.pattern, body, limit, out);
        if (status != QJS_INTL_UNSUPPORTED || quantity->error)
            return status;
    }
    if (!unit->compound || !plan->numerator || !plan->denominator)
        return QJS_INTL_UNSUPPORTED;
    status = derived_request(whole, plan->plural.first, plan->case_rule.first, &request);
    if (!status)
        status = qjs_intl_unit_pattern_select(&unit->data, plan->numerator, &request, &numerator);
    /* TR35 patternTimes rejects an extracted component whose numeric
       placeholder is in the middle. Retry a coherent root expression instead
       of dropping either side of the name or fabricating a denominator. */
    if (!status)
        status = qjs_intl_unit_noun_parse(numerator.form.text, &noun);
    if (status)
        return status;
    status = qjs_intl_unit_template_select(&unit->data, plan->denominator, QJS_INTL_UNIT_PER_PATTERN,
                                           (QJSIntlBytes){0}, &layout);
    if (!status) {
        prefix[0] = layout.pattern.per_unit.prefix;
        prefix[1] = numerator.pattern.prefix;
        suffix[0] = numerator.pattern.suffix;
        suffix[1] = layout.pattern.per_unit.suffix;
    } else {
        if (status != QJS_INTL_UNSUPPORTED)
            return status;
        status = qjs_intl_unit_template_select(&unit->data, plan->division,
                                               QJS_INTL_UNIT_COMPOUND_PATTERN, whole->case_name, &layout);
        if (!status) {
            status = derived_request(whole, plan->plural.second, plan->case_rule.second, &request);
            /* A derived noun category is grammatical, never exact numeric 0/1. */
            request.exact = -1;
        }
        if (!status)
            status = qjs_intl_unit_noun_select(&unit->data, plan->denominator, &request, &denominator);
        if (status)
            return status;
        /* One division needs at most four borrowed fragments on either side
           of the number. Operand order comes from the selected template. */
        prefix[0] = layout.pattern.division.prefix;
        if (layout.pattern.division.first == 0) {
            prefix[1] = numerator.pattern.prefix;
            suffix[0] = numerator.pattern.suffix;
            suffix[1] = layout.pattern.division.separator;
            suffix[2] = denominator.noun;
            suffix[3] = layout.pattern.division.suffix;
            after.count = 4;
        } else {
            prefix[1] = denominator.noun;
            prefix[2] = layout.pattern.division.separator;
            prefix[3] = numerator.pattern.prefix;
            before.count = 4;
            suffix[0] = numerator.pattern.suffix;
            suffix[1] = layout.pattern.division.suffix;
        }
    }
    return qjs_intl_unit_affixes_format(allocator, &before, &after, body, limit, out);
}

QJSIntlStatus qjs_intl_number_unit_format(const QJSIntlNumberUnit *unit,
                                          const QJSIntlAllocator *allocator,
                                          const QJSIntlUnitPatternRequest *input,
                                          const QJSIntlFormatted *body, size_t limit,
                                          QJSIntlFormatted *out)
{
    QJSIntlUnitPatternRequest request;
    UnitSource quantity;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !unit || !unit->active || !input ||
        (input->resolve
             ? input->category != QJS_INTL_UNIT_COUNT_NONE || input->exact < -1 || input->exact > 1
             : input->category < QJS_INTL_UNIT_COUNT_ZERO ||
                   input->category > QJS_INTL_UNIT_COUNT_OTHER || input->exact != -1))
        return QJS_INTL_INVALID_ARGUMENT;
    quantity = (UnitSource){input, QJS_INTL_OK};
    request = *input;
    request.resolve = input->resolve ? category : NULL;
    request.opaque = &quantity;
    status = format_composition(unit, &unit->local, &request, allocator, body, limit, out);
    /* Missing source cardinal rules are a dependency error, not ineligible
       unit text. Do not hide that failure by substituting root selection. */
    if (status == QJS_INTL_UNSUPPORTED && unit->compound && !quantity.error)
        status = format_composition(unit, &unit->root, &request, allocator, body, limit, out);
    return status;
}
