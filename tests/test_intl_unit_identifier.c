/* Sanctioned units, every allowed pair, exact slices and invalid expressions.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/unit/identifier.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

int main(void)
{
    static const char *invalid[] = {"",
                                    "Meter",
                                    "metre",
                                    "feet",
                                    "kelvin",
                                    "acre ",
                                    " acre",
                                    "-per-acre",
                                    "acre-per-",
                                    "meter_per_second",
                                    "meter-per-second-per-hour",
                                    "meter-times-meter",
                                    "per",
                                    "METER",
                                    "meter-per-quarter",
                                    "meterpersecond",
                                    "meter-per-second/",
                                    "meter\xc2\xa0"};
    QJSIntlUnitIdentifier result;
    uint32_t count = qjs_intl_unit_definition_count(), i, j, meter;
    assert(count == 45 && !qjs_intl_unit_definition_at(count));
    for (i = 0; i < count; i++) {
        const QJSIntlUnitDefinition *unit = qjs_intl_unit_definition_at(i);
        assert(unit && unit->cldr_identifier &&
               qjs_intl_unit_identifier_find(bytes(unit->identifier)) == i);
        if (i)
            assert(strcmp(qjs_intl_unit_definition_at(i - 1)->identifier, unit->identifier) < 0);
        assert(!qjs_intl_unit_identifier_parse(bytes(unit->identifier), &result));
        assert(result.numerator == i && result.denominator == QJS_INTL_NO_UNIT);
        for (j = 0; j < count; j++) {
            char compound[96];
            int length = snprintf(compound, sizeof(compound), "%s-per-%s", unit->identifier,
                                  qjs_intl_unit_definition_at(j)->identifier);
            assert(length > 0 && (size_t)length < sizeof(compound));
            assert(qjs_intl_unit_identifier_find(bytes(compound)) == QJS_INTL_NO_UNIT);
            assert(!qjs_intl_unit_identifier_parse(bytes(compound), &result));
            assert(result.numerator == i && result.denominator == j);
        }
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        assert(qjs_intl_unit_identifier_find(bytes(invalid[i])) == QJS_INTL_NO_UNIT);
        assert(qjs_intl_unit_identifier_parse(bytes(invalid[i]), &result) ==
               QJS_INTL_INVALID_ARGUMENT);
        assert(result.numerator == QJS_INTL_NO_UNIT && result.denominator == QJS_INTL_NO_UNIT);
    }
    meter = qjs_intl_unit_identifier_find(bytes("meter"));
    assert(!strcmp(qjs_intl_unit_definition_at(meter)->cldr_identifier, "length-meter"));
    assert(!strcmp(qjs_intl_unit_definition_at(qjs_intl_unit_identifier_find(bytes("percent")))
                       ->cldr_identifier,
                   "concentr-percent"));
    assert(!strcmp(qjs_intl_unit_definition_at(qjs_intl_unit_identifier_find(bytes("degree")))
                       ->cldr_identifier,
                   "angle-degree"));
    assert(!qjs_intl_unit_identifier_parse((QJSIntlBytes){"meter!", 5}, &result));
    assert(result.numerator == meter && result.denominator == QJS_INTL_NO_UNIT);
    assert(qjs_intl_unit_identifier_find((QJSIntlBytes){"meter\0second", 12}) == QJS_INTL_NO_UNIT);
    assert(qjs_intl_unit_identifier_parse((QJSIntlBytes){"meter\0second", 12}, &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_identifier_parse((QJSIntlBytes){NULL, 5}, &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_identifier_parse(bytes("meter"), NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_unit_definition_at(UINT32_MAX));
    return 0;
}
