/* Exact duration mathematical values, large fields and fractional boundaries.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/duration/decimal.h"
#include "../src/temporal/duration.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static void check(const QJSTemporalDuration *duration, QJSTemporalUnit unit,
                  int fractional, const char *expected)
{
    QJSIntlMathematicalValue result;
    char buffer[64];
    size_t length = strlen(expected);
    assert(qjs_temporal_duration_is_valid(duration));
    assert(!qjs_intl_duration_component_decimal(duration, unit, fractional, buffer, sizeof(buffer), &result));
    assert(result.kind == QJS_INTL_FINITE && result.decimal.data == buffer);
    assert(result.decimal.length == length && !strcmp(buffer, expected));
    assert(!qjs_intl_duration_component_decimal(duration, unit, fractional, buffer, length + 1, &result));
    memset(buffer, 0x5a, sizeof(buffer));
    assert(qjs_intl_duration_component_decimal(duration, unit, fractional, buffer, length, &result) ==
           QJS_INTL_OVERFLOW);
    assert(!result.decimal.data && !result.decimal.length && buffer[0] == 0x5a);
}

int main(void)
{
    QJSTemporalDuration duration = {{0}};
    QJSIntlMathematicalValue result;
    char buffer[64];
    int sign;
    check(&duration, QJS_TEMPORAL_YEAR, 0, "0");
    check(&duration, QJS_TEMPORAL_SECOND, 1, "0.000000000");
    duration.fields[QJS_TEMPORAL_SECOND] = -0.0;
    check(&duration, QJS_TEMPORAL_SECOND, 1, "0.000000000");
    for (sign = 1; sign >= -1; sign -= 2) {
        duration = (QJSTemporalDuration){{0}};
        duration.fields[QJS_TEMPORAL_SECOND] = sign * 9007199254740991.0;
        duration.fields[QJS_TEMPORAL_MILLISECOND] = sign * 999;
        duration.fields[QJS_TEMPORAL_MICROSECOND] = sign * 999;
        duration.fields[QJS_TEMPORAL_NANOSECOND] = sign * 999;
        check(&duration, QJS_TEMPORAL_SECOND, 1,
              sign > 0 ? "9007199254740991.999999999" : "-9007199254740991.999999999");
        check(&duration, QJS_TEMPORAL_MILLISECOND, 1,
              sign > 0 ? "999.999999" : "-999.999999");
        check(&duration, QJS_TEMPORAL_MICROSECOND, 1,
              sign > 0 ? "999.999" : "-999.999");
    }
    duration = (QJSTemporalDuration){{0}};
    duration.fields[QJS_TEMPORAL_NANOSECOND] = 0x1p82;
    check(&duration, QJS_TEMPORAL_NANOSECOND, 0, "4835703278458516698824704");
    check(&duration, QJS_TEMPORAL_SECOND, 1, "4835703278458516.698824704");
    check(&duration, QJS_TEMPORAL_MILLISECOND, 1, "4835703278458516698.824704");
    check(&duration, QJS_TEMPORAL_MICROSECOND, 1, "4835703278458516698824.704");
    duration.fields[QJS_TEMPORAL_NANOSECOND] = 1;
    check(&duration, QJS_TEMPORAL_SECOND, 1, "0.000000001");
    duration.fields[QJS_TEMPORAL_MILLISECOND] = 1;
    check(&duration, QJS_TEMPORAL_SECOND, 1, "0.001000001");
    duration.fields[QJS_TEMPORAL_YEAR] = 4294967295.0;
    check(&duration, QJS_TEMPORAL_YEAR, 0, "4294967295");
    assert(qjs_intl_duration_component_decimal(&duration, QJS_TEMPORAL_YEAR, 1, buffer,
                                               sizeof(buffer), &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_duration_component_decimal(&duration, QJS_TEMPORAL_NANOSECOND, 1, buffer,
                                               sizeof(buffer), &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_duration_component_decimal(&duration, QJS_TEMPORAL_UNIT_COUNT, 0, buffer,
                                               sizeof(buffer), &result) == QJS_INTL_INVALID_ARGUMENT);
    duration.fields[QJS_TEMPORAL_SECOND] = NAN;
    assert(qjs_intl_duration_component_decimal(&duration, QJS_TEMPORAL_SECOND, 0, buffer,
                                               sizeof(buffer), &result) == QJS_INTL_INVALID_ARGUMENT);
    duration.fields[QJS_TEMPORAL_SECOND] = 0.5;
    assert(qjs_intl_duration_component_decimal(&duration, QJS_TEMPORAL_SECOND, 0, buffer,
                                               sizeof(buffer), &result) == QJS_INTL_INVALID_ARGUMENT);
    duration.fields[QJS_TEMPORAL_SECOND] = 0x1p126;
    assert(qjs_intl_duration_component_decimal(&duration, QJS_TEMPORAL_SECOND, 1, buffer,
                                               sizeof(buffer), &result) == QJS_INTL_OVERFLOW);
    assert(!result.decimal.data && !result.decimal.length);
    return 0;
}
