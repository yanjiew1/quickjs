/* CLDR local lateral alternatives precede parent lookup.
 * CLDR 7cdeb9e7fac52c59f22fc7248b05f989ee5689c2, reviewed 2026-10-19:
 * TR35 Lateral Inheritance / Empty Override and Unit Elements.
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
#include "pattern.h"
#include "../common/text.h"
#include "../../cutils/cutils.h"
#include <string.h>

QJSIntlStatus qjs_intl_unit_pattern_parse(QJSIntlBytes text, QJSIntlUnitPattern *out)
{
    return qjs_intl_unary_pattern_parse(text, out);
}

static QJSIntlStatus noun_trim(QJSIntlBytes text, QJSIntlBytes *out)
{
    size_t at = 0, first = text.length, last = 0;
    while (at < text.length) {
        const uint8_t *end;
        size_t start = at, remaining = text.length - at;
        int c = unicode_from_utf8((const uint8_t *)text.data + at,
                                   remaining < 4 ? (int)remaining : 4, &end);
        if (c <= 0 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return QJS_INTL_DATA_ERROR;
        at = (const char *)end - text.data;
        if (!qjs_intl_text_is_white_space(c)) {
            if (!last)
                first = start;
            last = at;
        }
    }
    *out = last ? (QJSIntlBytes){text.data + first, last - first} : (QJSIntlBytes){0};
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_unit_noun_parse(QJSIntlBytes text, QJSIntlBytes *out)
{
    QJSIntlUnitPattern pattern;
    QJSIntlBytes prefix, suffix;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_unit_pattern_parse(text, &pattern);
    if (status == QJS_INTL_UNSUPPORTED) {
        status = noun_trim(text, &suffix);
        prefix = (QJSIntlBytes){0};
    } else if (!status) {
        status = noun_trim(pattern.prefix, &prefix);
        if (!status)
            status = noun_trim(pattern.suffix, &suffix);
    }
    if (status)
        return status;
    if (!!prefix.length == !!suffix.length)
        return QJS_INTL_UNSUPPORTED;
    *out = prefix.length ? prefix : suffix;
    return QJS_INTL_OK;
}

static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}

static QJSIntlStatus case_choices(QJSIntlBytes requested, QJSIntlBytes *cases, unsigned int *count)
{
    QJSIntlBytes nominative = {"nominative", 10};
    size_t i;
    *count = 0;
    if (!requested.data && requested.length)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < requested.length; i++) {
        if (requested.data[i] < 'a' || requested.data[i] > 'z')
            return QJS_INTL_INVALID_ARGUMENT;
    }
    if (requested.length && !equal(requested, nominative))
        cases[(*count)++] = requested;
    cases[(*count)++] = nominative;
    cases[(*count)++] = (QJSIntlBytes){0};
    return QJS_INTL_OK;
}

static int category_valid(QJSIntlUnitCount category)
{
    return category >= QJS_INTL_UNIT_COUNT_ZERO && category <= QJS_INTL_UNIT_COUNT_OTHER;
}

typedef struct Selection {
    QJSIntlSelectedUnitPattern selected;
    QJSIntlBytes noun;
} Selection;

static QJSIntlStatus candidate(const QJSIntlUnitData *data, const QJSIntlUnitOverlay *layer,
                               QJSIntlUnitCount count, const QJSIntlBytes *cases,
                               unsigned int case_count, uint32_t shadow,
                               int noun, Selection *out)
{
    unsigned int j;
    for (j = 0; j < case_count; j++) {
        QJSIntlUnitForm form;
        QJSIntlUnitPattern pattern = {0};
        QJSIntlBytes text = {0};
        QJSIntlStatus status;
        if (shadow & (UINT32_C(1) << ((count - 1) * case_count + j)))
            continue;
        status = qjs_intl_unit_form_find(data, &layer->map, QJS_INTL_UNIT_PATTERN, count, cases[j], &form);
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (status)
            return status;
        if (form.state == QJS_INTL_UNIT_FORM_BLOCKED)
            continue;
        status = noun ? qjs_intl_unit_noun_parse(form.text, &text)
                      : qjs_intl_unit_pattern_parse(form.text, &pattern);
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (status)
            return status;
        out->selected = (QJSIntlSelectedUnitPattern){pattern, form, layer->locale, layer->width,
                                                      layer->group};
        out->noun = text;
        return QJS_INTL_OK;
    }
    return QJS_INTL_UNSUPPORTED;
}

static QJSIntlStatus select_pattern(const QJSIntlUnitData *data, uint32_t head,
                                     const QJSIntlUnitPatternRequest *request, int noun,
                                     Selection *out)
{
    QJSIntlBytes cases[3];
    unsigned int case_count = 0;
    uint32_t shadow = 0;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || head > data->node_count || !request ||
        (!request->case_name.data && request->case_name.length) ||
        (request->resolve ? request->category != QJS_INTL_UNIT_COUNT_NONE ||
                               request->exact < -1 || request->exact > 1
                         : !category_valid(request->category) || request->exact != -1))
        return QJS_INTL_INVALID_ARGUMENT;
    if (case_choices(request->case_name, cases, &case_count))
        return QJS_INTL_INVALID_ARGUMENT;
    while (head) {
        QJSIntlUnitOverlay layer;
        QJSIntlUnitGroup group;
        QJSIntlUnitCount category = request->category;
        QJSIntlStatus status;
        qjs_intl_unit_overlay_at(data, head, &layer);
        qjs_intl_unit_group_at(data, layer.group, &group);
        if (group.kind != QJS_INTL_UNIT_GROUP_SINGLE)
            return QJS_INTL_INVALID_ARGUMENT;
        if (request->resolve && request->exact >= 0) {
            status = candidate(data, &layer, request->exact + QJS_INTL_UNIT_COUNT_EXACT_ZERO,
                               cases, case_count, shadow, noun, out);
            if (status != QJS_INTL_UNSUPPORTED)
                return status;
        }
        if (request->resolve) {
            status = request->resolve(request->opaque, layer.locale, &category);
            if (status)
                return status;
            if (!category_valid(category))
                return QJS_INTL_DATA_ERROR;
        }
        status = candidate(data, &layer, category, cases, case_count, shadow, noun, out);
        if (status != QJS_INTL_UNSUPPORTED)
            return status;
        if (category != QJS_INTL_UNIT_COUNT_OTHER) {
            status = candidate(data, &layer, QJS_INTL_UNIT_COUNT_OTHER, cases, case_count, shadow, noun, out);
            if (status != QJS_INTL_UNSUPPORTED)
                return status;
        }
        /* Include even locally unselected categories: the next source may
           classify the same numeric value differently. Each present path is
           already an override, regardless of this adapter's eligibility. */
        for (i = 0; i < layer.map.count; i++) {
            QJSIntlUnitForm form;
            unsigned int j;
            qjs_intl_unit_form_at(data, &layer.map, i, &form);
            if (form.field != QJS_INTL_UNIT_PATTERN)
                continue;
            for (j = 0; j < case_count; j++) {
                if (equal(form.case_name, cases[j]))
                    shadow |= UINT32_C(1) << ((form.count - 1) * case_count + j);
            }
        }
        head = layer.next;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_unit_pattern_select(const QJSIntlUnitData *data, uint32_t head,
                                           const QJSIntlUnitPatternRequest *request,
                                           QJSIntlSelectedUnitPattern *out)
{
    Selection selected;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = select_pattern(data, head, request, 0, &selected);
    if (!status)
        *out = selected.selected;
    return status;
}

QJSIntlStatus qjs_intl_unit_noun_select(const QJSIntlUnitData *data, uint32_t head,
                                        const QJSIntlUnitPatternRequest *request,
                                        QJSIntlSelectedUnitNoun *out)
{
    Selection selected;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !request || request->exact != -1)
        return QJS_INTL_INVALID_ARGUMENT;
    status = select_pattern(data, head, request, 1, &selected);
    if (!status) {
        const QJSIntlSelectedUnitPattern *source = &selected.selected;
        *out = (QJSIntlSelectedUnitNoun){selected.noun, source->form, source->locale,
                                         source->width, source->group};
    }
    return status;
}

QJSIntlStatus qjs_intl_unit_template_select(const QJSIntlUnitData *data, uint32_t head,
                                            QJSIntlUnitField field, QJSIntlBytes case_name,
                                            QJSIntlSelectedUnitTemplate *out)
{
    QJSIntlBytes cases[3];
    unsigned int case_count = 0, shadow = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || head > data->node_count ||
        (field != QJS_INTL_UNIT_PER_PATTERN && field != QJS_INTL_UNIT_COMPOUND_PATTERN) ||
        (field == QJS_INTL_UNIT_PER_PATTERN && case_name.length) ||
        case_choices(case_name, cases, &case_count))
        return QJS_INTL_INVALID_ARGUMENT;
    if (field == QJS_INTL_UNIT_PER_PATTERN) {
        cases[0] = (QJSIntlBytes){0};
        case_count = 1;
    }
    while (head) {
        QJSIntlUnitOverlay layer;
        QJSIntlUnitGroup group;
        unsigned int i;
        qjs_intl_unit_overlay_at(data, head, &layer);
        qjs_intl_unit_group_at(data, layer.group, &group);
        if (group.kind != (field == QJS_INTL_UNIT_PER_PATTERN ? QJS_INTL_UNIT_GROUP_SINGLE
                                                              : QJS_INTL_UNIT_GROUP_COMPOUND))
            return QJS_INTL_INVALID_ARGUMENT;
        for (i = 0; i < case_count; i++) {
            QJSIntlSelectedUnitTemplate selected = {0};
            QJSIntlUnitForm form;
            QJSIntlStatus status;
            if (shadow & (1u << i))
                continue;
            status = qjs_intl_unit_form_find(data, &layer.map, field, QJS_INTL_UNIT_COUNT_NONE,
                                             cases[i], &form);
            if (status == QJS_INTL_UNSUPPORTED)
                continue;
            if (status)
                return status;
            shadow |= 1u << i;
            if (form.state == QJS_INTL_UNIT_FORM_BLOCKED)
                continue;
            status = field == QJS_INTL_UNIT_PER_PATTERN
                         ? qjs_intl_unit_pattern_parse(form.text, &selected.pattern.per_unit)
                         : qjs_intl_pair_pattern_parse(form.text, &selected.pattern.division);
            if (status == QJS_INTL_UNSUPPORTED)
                continue;
            if (status)
                return status;
            selected.form = form;
            selected.locale = layer.locale;
            selected.width = layer.width;
            selected.group = layer.group;
            *out = selected;
            return QJS_INTL_OK;
        }
        head = layer.next;
    }
    return QJS_INTL_UNSUPPORTED;
}
