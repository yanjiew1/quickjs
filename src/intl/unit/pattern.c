/* CLDR local lateral alternatives precede parent lookup.
 * CLDR 7cdeb9e7fac52c59f22fc7248b05f989ee5689c2, reviewed 2026-10-19:
 * TR35 Lateral Inheritance / Empty Override and Unit Elements.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "pattern.h"
#include <string.h>

QJSIntlStatus qjs_intl_unit_pattern_parse(QJSIntlBytes text, QJSIntlUnitPattern *out)
{
    size_t i, number = 0;
    int seen = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!text.data && text.length) || text.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < text.length; i++) {
        if (text.data[i] == '}')
            return QJS_INTL_DATA_ERROR;
        if (text.data[i] != '{')
            continue;
        if (seen || text.length - i < 3 || text.data[i + 1] != '0' || text.data[i + 2] != '}')
            return QJS_INTL_DATA_ERROR;
        seen = 1;
        number = i;
        i += 2;
    }
    if (!seen)
        return QJS_INTL_UNSUPPORTED;
    out->prefix = (QJSIntlBytes){text.data, number};
    out->suffix = (QJSIntlBytes){text.data + number + 3, text.length - number - 3};
    return QJS_INTL_OK;
}

static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}

static int category_valid(QJSIntlUnitCount category)
{
    return category >= QJS_INTL_UNIT_COUNT_ZERO && category <= QJS_INTL_UNIT_COUNT_OTHER;
}

static QJSIntlStatus candidate(const QJSIntlUnitData *data, const QJSIntlUnitOverlay *layer,
                               QJSIntlUnitCount count, const QJSIntlBytes *cases,
                               unsigned int case_count, uint32_t shadow,
                               QJSIntlSelectedUnitPattern *out)
{
    unsigned int j;
    for (j = 0; j < case_count; j++) {
        QJSIntlUnitForm form;
        QJSIntlUnitPattern pattern;
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
        status = qjs_intl_unit_pattern_parse(form.text, &pattern);
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (status)
            return status;
        *out = (QJSIntlSelectedUnitPattern){pattern, form, layer->locale, layer->width, layer->group};
        return QJS_INTL_OK;
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_unit_pattern_select(const QJSIntlUnitData *data, uint32_t head,
                                           const QJSIntlUnitPatternRequest *request,
                                           QJSIntlSelectedUnitPattern *out)
{
    QJSIntlBytes cases[3], nominative = {"nominative", 10};
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
    for (i = 0; i < request->case_name.length; i++) {
        if (request->case_name.data[i] < 'a' || request->case_name.data[i] > 'z')
            return QJS_INTL_INVALID_ARGUMENT;
    }
    if (request->case_name.length && !equal(request->case_name, nominative))
        cases[case_count++] = request->case_name;
    cases[case_count++] = nominative;
    cases[case_count++] = (QJSIntlBytes){0};
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
                               cases, case_count, shadow, out);
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
        status = candidate(data, &layer, category, cases, case_count, shadow, out);
        if (status != QJS_INTL_UNSUPPORTED)
            return status;
        if (category != QJS_INTL_UNIT_COUNT_OTHER) {
            status = candidate(data, &layer, QJS_INTL_UNIT_COUNT_OTHER, cases, case_count, shadow, out);
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
