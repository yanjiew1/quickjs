/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "options.h"

static int numeric(QJSIntlDurationUnitStyle style)
{
    return style == QJS_INTL_DURATION_UNIT_NUMERIC || style == QJS_INTL_DURATION_UNIT_2DIGIT;
}

static int follows(QJSIntlDurationUnitStyle style, QJSIntlDurationDisplay display,
                   QJSIntlDurationUnitStyle previous)
{
    if (style == QJS_INTL_DURATION_UNIT_FRACTIONAL && display == QJS_INTL_DURATION_ALWAYS)
        return 0;
    if (previous == QJS_INTL_DURATION_UNIT_FRACTIONAL)
        return style == QJS_INTL_DURATION_UNIT_FRACTIONAL;
    return !numeric(previous) || numeric(style) || style == QJS_INTL_DURATION_UNIT_FRACTIONAL;
}

QJSIntlStatus qjs_intl_duration_unit_options(QJSTemporalUnit unit, QJSIntlDurationStyle base,
                                            QJSIntlDurationUnitStyle requested,
                                            QJSIntlDurationDisplay display,
                                            QJSIntlDurationUnitStyle previous,
                                            int two_digit_hours, QJSIntlDurationUnitOptions *out)
{
    QJSIntlDurationUnitOptions result;
    QJSIntlDurationDisplay default_display = QJS_INTL_DURATION_ALWAYS;
    QJSIntlDurationUnitStyle style = requested;
    int largest = unit < QJS_TEMPORAL_HOUR;
    int fractional = unit > QJS_TEMPORAL_SECOND;
    if (!out || (unsigned int)unit >= QJS_TEMPORAL_UNIT_COUNT ||
        (unsigned int)base > QJS_INTL_DURATION_DIGITAL ||
        (requested != QJS_INTL_DURATION_UNIT_UNSET &&
         (unsigned int)requested > (largest ? QJS_INTL_DURATION_UNIT_NARROW :
                                    fractional ? QJS_INTL_DURATION_UNIT_NUMERIC : QJS_INTL_DURATION_UNIT_2DIGIT)) ||
        (display != QJS_INTL_DURATION_DISPLAY_UNSET && (unsigned int)display > QJS_INTL_DURATION_ALWAYS) ||
        (previous != QJS_INTL_DURATION_UNIT_UNSET && (unsigned int)previous > QJS_INTL_DURATION_UNIT_FRACTIONAL))
        return QJS_INTL_INVALID_ARGUMENT;
    if (style == QJS_INTL_DURATION_UNIT_UNSET) {
        if (base == QJS_INTL_DURATION_DIGITAL) {
            style = largest ? QJS_INTL_DURATION_UNIT_SHORT : QJS_INTL_DURATION_UNIT_NUMERIC;
            if (largest || fractional)
                default_display = QJS_INTL_DURATION_AUTO;
        } else if (numeric(previous) || previous == QJS_INTL_DURATION_UNIT_FRACTIONAL) {
            style = QJS_INTL_DURATION_UNIT_NUMERIC;
            if (unit != QJS_TEMPORAL_MINUTE && unit != QJS_TEMPORAL_SECOND)
                default_display = QJS_INTL_DURATION_AUTO;
        } else {
            style = base == QJS_INTL_DURATION_LONG ? QJS_INTL_DURATION_UNIT_LONG :
                    base == QJS_INTL_DURATION_SHORT ? QJS_INTL_DURATION_UNIT_SHORT : QJS_INTL_DURATION_UNIT_NARROW;
            default_display = QJS_INTL_DURATION_AUTO;
        }
    }
    if (style == QJS_INTL_DURATION_UNIT_NUMERIC && fractional) {
        style = QJS_INTL_DURATION_UNIT_FRACTIONAL;
        default_display = QJS_INTL_DURATION_AUTO;
    }
    if (display == QJS_INTL_DURATION_DISPLAY_UNSET)
        display = default_display;
    if (!follows(style, display, previous))
        return QJS_INTL_INVALID_ARGUMENT;
    if (unit == QJS_TEMPORAL_HOUR && two_digit_hours)
        style = QJS_INTL_DURATION_UNIT_2DIGIT;
    if ((unit == QJS_TEMPORAL_MINUTE || unit == QJS_TEMPORAL_SECOND) && numeric(previous))
        style = QJS_INTL_DURATION_UNIT_2DIGIT;
    result = (QJSIntlDurationUnitOptions){style, display};
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_duration_options_validate(const QJSIntlDurationOptions *options)
{
    QJSIntlDurationUnitStyle previous = QJS_INTL_DURATION_UNIT_UNSET;
    unsigned int unit;
    if (!options || (unsigned int)options->style > QJS_INTL_DURATION_DIGITAL ||
        options->fractional_digits < -1 || options->fractional_digits > 9 || !options->maximum_output_length)
        return QJS_INTL_INVALID_ARGUMENT;
    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        QJSIntlDurationUnitOptions current = options->units[unit];
        int fractional = unit > QJS_TEMPORAL_SECOND;
        if ((unsigned int)current.style > QJS_INTL_DURATION_UNIT_FRACTIONAL ||
            (unsigned int)current.display > QJS_INTL_DURATION_ALWAYS ||
            (unit < QJS_TEMPORAL_HOUR && current.style > QJS_INTL_DURATION_UNIT_NARROW) ||
            (!fractional && current.style == QJS_INTL_DURATION_UNIT_FRACTIONAL) ||
            (fractional && numeric(current.style)) ||
            !follows(current.style, current.display, previous) ||
            ((unit == QJS_TEMPORAL_MINUTE || unit == QJS_TEMPORAL_SECOND) && numeric(previous) &&
             current.style != QJS_INTL_DURATION_UNIT_2DIGIT))
            return QJS_INTL_INVALID_ARGUMENT;
        if (unit >= QJS_TEMPORAL_HOUR && unit < QJS_TEMPORAL_NANOSECOND)
            previous = current.style;
    }
    return QJS_INTL_OK;
}
