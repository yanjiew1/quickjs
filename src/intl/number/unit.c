/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "unit.h"
#include "../unit/identifier.h"
#include "../plural/native-engine.h"
#include <string.h>

QJSIntlStatus qjs_intl_number_unit_open(const QJSIntlUnitData *data, const QJSIntlPluralData *plurals,
                                        uint32_t locale, QJSIntlUnitWidth width, QJSIntlBytes name,
                                        QJSIntlNumberUnit *out)
{
    QJSIntlUnitIdentifier unit;
    const QJSIntlUnitDefinition *definition;
    QJSIntlStatus status;
    uint32_t group, head;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || !plurals || !plurals->section.bytes ||
        data->data.pack.bytes != plurals->data.pack.bytes)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_unit_identifier_parse(name, &unit);
    if (status)
        return status;
    if (unit.denominator != QJS_INTL_NO_UNIT)
        return QJS_INTL_UNSUPPORTED;
    definition = qjs_intl_unit_definition_at(unit.numerator);
    status = qjs_intl_unit_group_find(data, QJS_INTL_UNIT_GROUP_SINGLE,
                                      (QJSIntlBytes){definition->cldr_identifier,
                                                     strlen(definition->cldr_identifier)}, &group);
    if (!status)
        status = qjs_intl_unit_overlay_head(data, locale, width, group, &head);
    if (status)
        return status;
    if (!head)
        return QJS_INTL_UNSUPPORTED;
    *out = (QJSIntlNumberUnit){*data, *plurals, head};
    return QJS_INTL_OK;
}

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
            status = qjs_intl_plural_rules_at(quantity->data, mid, QJS_INTL_PLURAL_CARDINAL, &rules);
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

QJSIntlStatus qjs_intl_number_unit_format(const QJSIntlNumberUnit *unit,
                                          const QJSIntlAllocator *allocator,
                                          const QJSIntlPluralOperands *number, uint32_t shift,
                                          int exact, const QJSIntlFormatted *body, size_t limit,
                                          QJSIntlFormatted *out)
{
    QJSIntlSelectedUnitPattern selected;
    QJSIntlUnitPatternRequest request = {0};
    UnitQuantity quantity;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !unit || !unit->head || (!number && (exact != -1 || shift)))
        return QJS_INTL_INVALID_ARGUMENT;
    quantity = (UnitQuantity){&unit->plurals, number, shift};
    request.resolve = number ? category : NULL;
    request.opaque = &quantity;
    request.category = number ? QJS_INTL_UNIT_COUNT_NONE : QJS_INTL_UNIT_COUNT_OTHER;
    request.exact = exact;
    status = qjs_intl_unit_pattern_select(&unit->data, unit->head, &request, &selected);
    if (status)
        return status;
    return qjs_intl_unit_pattern_format(allocator, &selected.pattern, body, limit, out);
}
