/* Duration unit defaults, transitions and resolved-record validation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/duration/options.h"
#include <assert.h>
#include <string.h>

static QJSIntlDurationOptions defaults(QJSIntlDurationStyle base)
{
    QJSIntlDurationOptions options = {.style = base, .fractional_digits = -1, .maximum_output_length = 1024};
    QJSIntlDurationUnitStyle previous = QJS_INTL_DURATION_UNIT_UNSET;
    unsigned int unit;
    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        assert(!qjs_intl_duration_unit_options(unit, base, QJS_INTL_DURATION_UNIT_UNSET,
                                               QJS_INTL_DURATION_DISPLAY_UNSET, previous, 0, &options.units[unit]));
        if (unit >= QJS_TEMPORAL_HOUR && unit < QJS_TEMPORAL_NANOSECOND)
            previous = options.units[unit].style;
    }
    assert(!qjs_intl_duration_options_validate(&options));
    return options;
}

int main(void)
{
    QJSIntlDurationOptions options;
    QJSIntlDurationUnitOptions result, saved;
    unsigned int style, unit;
    for (style = QJS_INTL_DURATION_LONG; style <= QJS_INTL_DURATION_DIGITAL; style++) {
        options = defaults(style);
        for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
            if (style != QJS_INTL_DURATION_DIGITAL) {
                assert((unsigned int)options.units[unit].style == style);
                assert(options.units[unit].display == QJS_INTL_DURATION_AUTO);
            } else if (unit < QJS_TEMPORAL_HOUR) {
                assert(options.units[unit].style == QJS_INTL_DURATION_UNIT_SHORT);
                assert(options.units[unit].display == QJS_INTL_DURATION_AUTO);
            } else if (unit <= QJS_TEMPORAL_SECOND) {
                assert(options.units[unit].style == (unit == QJS_TEMPORAL_HOUR ?
                       QJS_INTL_DURATION_UNIT_NUMERIC : QJS_INTL_DURATION_UNIT_2DIGIT));
                assert(options.units[unit].display == QJS_INTL_DURATION_ALWAYS);
            } else {
                assert(options.units[unit].style == QJS_INTL_DURATION_UNIT_FRACTIONAL);
                assert(options.units[unit].display == QJS_INTL_DURATION_AUTO);
            }
        }
    }
    for (style = QJS_INTL_DURATION_LONG; style <= QJS_INTL_DURATION_NARROW; style++) {
        QJSIntlDurationUnitStyle previous = QJS_INTL_DURATION_UNIT_UNSET;
        options = defaults(style);
        for (unit = QJS_TEMPORAL_HOUR; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
            assert(!qjs_intl_duration_unit_options(unit, style,
                                                   unit == QJS_TEMPORAL_HOUR ? QJS_INTL_DURATION_UNIT_NUMERIC :
                                                   QJS_INTL_DURATION_UNIT_UNSET,
                                                   QJS_INTL_DURATION_DISPLAY_UNSET, previous, 0, &options.units[unit]));
            assert(options.units[unit].style == (unit == QJS_TEMPORAL_HOUR ? QJS_INTL_DURATION_UNIT_NUMERIC :
                   unit <= QJS_TEMPORAL_SECOND ? QJS_INTL_DURATION_UNIT_2DIGIT : QJS_INTL_DURATION_UNIT_FRACTIONAL));
            assert(options.units[unit].display == (unit <= QJS_TEMPORAL_SECOND ? QJS_INTL_DURATION_ALWAYS :
                                                   QJS_INTL_DURATION_AUTO));
            previous = options.units[unit].style;
        }
        assert(!qjs_intl_duration_options_validate(&options));
    }
    assert(!qjs_intl_duration_unit_options(QJS_TEMPORAL_HOUR, QJS_INTL_DURATION_SHORT,
                                           QJS_INTL_DURATION_UNIT_LONG, QJS_INTL_DURATION_DISPLAY_UNSET,
                                           QJS_INTL_DURATION_UNIT_SHORT, 0, &result));
    assert(result.style == QJS_INTL_DURATION_UNIT_LONG && result.display == QJS_INTL_DURATION_ALWAYS);
    /* Effective TwoDigitHours is distinct from raw CLDR data. When true the
       normative override is unconditional, even for explicit text hours. */
    assert(!qjs_intl_duration_unit_options(QJS_TEMPORAL_HOUR, QJS_INTL_DURATION_LONG,
                                           QJS_INTL_DURATION_UNIT_LONG, QJS_INTL_DURATION_DISPLAY_UNSET,
                                           QJS_INTL_DURATION_UNIT_LONG, 1, &result));
    assert(result.style == QJS_INTL_DURATION_UNIT_2DIGIT);
    assert(!qjs_intl_duration_unit_options(QJS_TEMPORAL_MINUTE, QJS_INTL_DURATION_LONG,
                                           QJS_INTL_DURATION_UNIT_NUMERIC, QJS_INTL_DURATION_AUTO,
                                           QJS_INTL_DURATION_UNIT_NUMERIC, 0, &result));
    assert(result.style == QJS_INTL_DURATION_UNIT_2DIGIT && result.display == QJS_INTL_DURATION_AUTO);
    assert(!qjs_intl_duration_unit_options(QJS_TEMPORAL_MILLISECOND, QJS_INTL_DURATION_SHORT,
                                           QJS_INTL_DURATION_UNIT_NUMERIC, QJS_INTL_DURATION_DISPLAY_UNSET,
                                           QJS_INTL_DURATION_UNIT_LONG, 0, &result));
    assert(result.style == QJS_INTL_DURATION_UNIT_FRACTIONAL && result.display == QJS_INTL_DURATION_AUTO);
    saved = result;
    assert(qjs_intl_duration_unit_options(QJS_TEMPORAL_MILLISECOND, QJS_INTL_DURATION_SHORT,
                                          QJS_INTL_DURATION_UNIT_NUMERIC, QJS_INTL_DURATION_ALWAYS,
                                          QJS_INTL_DURATION_UNIT_LONG, 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &saved, sizeof(result)));
    assert(qjs_intl_duration_unit_options(QJS_TEMPORAL_YEAR, QJS_INTL_DURATION_SHORT,
                                          QJS_INTL_DURATION_UNIT_NUMERIC, QJS_INTL_DURATION_AUTO,
                                          QJS_INTL_DURATION_UNIT_UNSET, 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_duration_unit_options(QJS_TEMPORAL_MINUTE, QJS_INTL_DURATION_SHORT,
                                          QJS_INTL_DURATION_UNIT_LONG, QJS_INTL_DURATION_AUTO,
                                          QJS_INTL_DURATION_UNIT_NUMERIC, 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_duration_unit_options(QJS_TEMPORAL_NANOSECOND, QJS_INTL_DURATION_SHORT,
                                          QJS_INTL_DURATION_UNIT_LONG, QJS_INTL_DURATION_AUTO,
                                          QJS_INTL_DURATION_UNIT_FRACTIONAL, 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    options = defaults(QJS_INTL_DURATION_DIGITAL);
    options.units[QJS_TEMPORAL_MINUTE].style = QJS_INTL_DURATION_UNIT_NUMERIC;
    assert(qjs_intl_duration_options_validate(&options) == QJS_INTL_INVALID_ARGUMENT);
    options = defaults(QJS_INTL_DURATION_DIGITAL);
    options.units[QJS_TEMPORAL_NANOSECOND].display = QJS_INTL_DURATION_ALWAYS;
    assert(qjs_intl_duration_options_validate(&options) == QJS_INTL_INVALID_ARGUMENT);
    options = defaults(QJS_INTL_DURATION_LONG);
    options.fractional_digits = 10;
    assert(qjs_intl_duration_options_validate(&options) == QJS_INTL_INVALID_ARGUMENT);
    options.fractional_digits = -2;
    assert(qjs_intl_duration_options_validate(&options) == QJS_INTL_INVALID_ARGUMENT);
    options.fractional_digits = 9;
    assert(!qjs_intl_duration_options_validate(&options));
    options.maximum_output_length = 0;
    assert(qjs_intl_duration_options_validate(&options) == QJS_INTL_INVALID_ARGUMENT);
    return 0;
}
