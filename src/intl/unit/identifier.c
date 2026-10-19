/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "identifier.h"
#include <string.h>

static const QJSIntlUnitDefinition definitions[] = {
#define QJS_INTL_UNIT(name, cldr) {name, cldr},
#include "identifiers.def"
#undef QJS_INTL_UNIT
};

uint32_t qjs_intl_unit_definition_count(void)
{
    return sizeof(definitions) / sizeof(definitions[0]);
}

const QJSIntlUnitDefinition *qjs_intl_unit_definition_at(uint32_t index)
{
    return index < qjs_intl_unit_definition_count() ? &definitions[index] : NULL;
}

uint32_t qjs_intl_unit_identifier_find(QJSIntlBytes input)
{
    uint32_t lo = 0, hi = qjs_intl_unit_definition_count();
    if (!input.data || !input.length)
        return QJS_INTL_NO_UNIT;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const char *candidate = definitions[mid].identifier;
        size_t length = strlen(candidate), shared = input.length < length ? input.length : length;
        int order = memcmp(input.data, candidate, shared);
        if (!order)
            order = (input.length > length) - (input.length < length);
        if (!order)
            return mid;
        if (order < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return QJS_INTL_NO_UNIT;
}

QJSIntlStatus qjs_intl_unit_identifier_parse(QJSIntlBytes input, QJSIntlUnitIdentifier *out)
{
    uint32_t numerator, denominator;
    size_t i;
    if (out)
        out->numerator = out->denominator = QJS_INTL_NO_UNIT;
    if (!out || !input.data || !input.length)
        return QJS_INTL_INVALID_ARGUMENT;
    numerator = qjs_intl_unit_identifier_find(input);
    if (numerator != QJS_INTL_NO_UNIT) {
        out->numerator = numerator;
        return QJS_INTL_OK;
    }
    /* The only sanctioned compound operator is one "-per-". Requiring two
       single identifiers also rejects a second operator in either operand. */
    for (i = 0; input.length >= 5 && i <= input.length - 5; i++) {
        if (!memcmp(input.data + i, "-per-", 5)) {
            numerator = qjs_intl_unit_identifier_find((QJSIntlBytes){input.data, i});
            denominator = qjs_intl_unit_identifier_find(
                (QJSIntlBytes){input.data + i + 5, input.length - i - 5});
            if (numerator == QJS_INTL_NO_UNIT || denominator == QJS_INTL_NO_UNIT)
                return QJS_INTL_INVALID_ARGUMENT;
            out->numerator = numerator;
            out->denominator = denominator;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_INVALID_ARGUMENT;
}

const char *qjs_intl_unit_identifier_cldr(const QJSIntlUnitIdentifier *unit)
{
    /* CLDR release-49-beta4, TR35 Unit Identifiers / Precomposed Compound Units.
       The legacy grouping prefix is not necessarily the numerator's category:
       liter is volume-liter, but liter-per-kilometer has a consumption prefix. */
    static const struct {
        const char *first, *second, *identifier;
    } compounds[] = {
        {"kilometer", "hour", "speed-kilometer-per-hour"},
        {"liter", "kilometer", "consumption-liter-per-kilometer"},
        {"meter", "second", "speed-meter-per-second"},
        {"mile", "gallon", "consumption-mile-per-gallon"},
        {"mile", "hour", "speed-mile-per-hour"},
    };
    const QJSIntlUnitDefinition *first, *second;
    size_t i;
    if (!unit || !(first = qjs_intl_unit_definition_at(unit->numerator)))
        return NULL;
    if (unit->denominator == QJS_INTL_NO_UNIT)
        return first->cldr_identifier;
    if (!(second = qjs_intl_unit_definition_at(unit->denominator)))
        return NULL;
    for (i = 0; i < sizeof(compounds) / sizeof(compounds[0]); i++) {
        if (!strcmp(first->identifier, compounds[i].first) &&
            !strcmp(second->identifier, compounds[i].second))
            return compounds[i].identifier;
    }
    return NULL;
}
