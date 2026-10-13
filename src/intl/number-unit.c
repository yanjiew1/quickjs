/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "number-unit.h"
#include <string.h>
#include "number-native.h"

static int simple(QJSIntlBytes unit)
{
    static const char *const names[] = {
        "acre", "bit", "byte", "celsius", "centimeter", "day", "degree", "fahrenheit",
        "fluid-ounce", "foot", "gallon", "gigabit", "gigabyte", "gram", "hectare", "hour",
        "inch", "kilobit", "kilobyte", "kilogram", "kilometer", "liter", "megabit", "megabyte",
        "meter", "microsecond", "mile", "mile-scandinavian", "milliliter", "millimeter",
        "millisecond", "minute", "month", "nanosecond", "ounce", "percent", "petabyte",
        "pound", "second", "stone", "terabit", "terabyte", "week", "yard", "year"
    };
    size_t i;
    if (!unit.data) return 0;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (unit.length == strlen(names[i]) && !memcmp(unit.data, names[i], unit.length)) return 1;
    return 0;
}
int qjs_intl_number_unit_validate(QJSIntlBytes unit)
{
    size_t i;
    if (simple(unit)) return 1;
    if (!unit.data) return 0;
    for (i = 0; i + 5 <= unit.length; i++) {
        if (!memcmp(unit.data + i, "-per-", 5)) {
            QJSIntlBytes numerator = {unit.data, i};
            QJSIntlBytes denominator = {unit.data + i + 5, unit.length - i - 5};
            return simple(numerator) && simple(denominator);
        }
    }
    return 0;
}
void qjs_intl_number_unit_composed_clear(const QJSIntlAllocator *a, QJSIntlBytes out[6])
{
    size_t i;
    if (!a || !a->free || !out) return;
    for (i = 0; i < 6; i++) {
        if (out[i].data) a->free(a->opaque, (void *)out[i].data);
        out[i].data = NULL; out[i].length = 0;
    }
}
static QJSIntlStatus substitute(const QJSIntlAllocator *a, QJSIntlBytes pattern,
    const char *first, QJSIntlBytes x, const char *second, QJSIntlBytes y,
    size_t maximum, QJSIntlBytes *out)
{
    size_t i, size = 0, position = 0, first_length = strlen(first);
    size_t second_length = second ? strlen(second) : 0;
    unsigned int first_count = 0, second_count = 0;
    char *result;
    for (i = 0; i < pattern.length;) {
        size_t add = 1;
        if (pattern.data[i] == '{') {
            if (first_length <= pattern.length - i && !memcmp(pattern.data + i, first, first_length)) {
                if (++first_count > 1) return QJS_INTL_DATA_ERROR;
                add = x.length; i += first_length;
            } else if (second && second_length <= pattern.length - i &&
                       !memcmp(pattern.data + i, second, second_length)) {
                if (++second_count > 1) return QJS_INTL_DATA_ERROR;
                add = y.length; i += second_length;
            } else return QJS_INTL_DATA_ERROR;
        } else {
            if (pattern.data[i++] == '}') return QJS_INTL_DATA_ERROR;
        }
        if (add > maximum - size) return QJS_INTL_OVERFLOW;
        size += add;
    }
    if (first_count != 1 || second_count != (second ? 1u : 0u)) return QJS_INTL_DATA_ERROR;
    if (size == SIZE_MAX) return QJS_INTL_OVERFLOW;
    result = a->malloc(a->opaque, size + 1); if (!result) return QJS_INTL_NO_MEMORY;
    for (i = 0; i < pattern.length;) {
        if (pattern.data[i] != '{') { result[position++] = pattern.data[i++]; continue; }
        if (first_length <= pattern.length - i && !memcmp(pattern.data + i, first, first_length)) {
            if (x.length) memcpy(result + position, x.data, x.length);
            position += x.length; i += first_length;
        } else {
            if (y.length) memcpy(result + position, y.data, y.length);
            position += y.length; i += second_length;
        }
    }
    result[size] = 0; out->data = result; out->length = size;
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_number_unit_compose(const QJSIntlAllocator *a,
    const QJSIntlBytes numerator[6], QJSIntlBytes per_unit, QJSIntlBytes denominator,
    QJSIntlBytes compound_pattern, size_t maximum, QJSIntlBytes out[6])
{
    size_t i;
    QJSIntlStatus s;
    QJSIntlBytes empty = {NULL, 0};
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, 6 * sizeof(*out));
    if (!a || !a->malloc || !a->free || !numerator || !maximum ||
        (per_unit.length && !per_unit.data) || (!per_unit.length &&
         (!denominator.data || !denominator.length || !compound_pattern.data || !compound_pattern.length)))
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < 6; i++) {
        if (!qjs_intl_number_template_validate(numerator[i], 3)) { s = QJS_INTL_DATA_ERROR; goto fail; }
        s = per_unit.length ? substitute(a, per_unit, "{number}", numerator[i], NULL, empty, maximum, &out[i]) :
            substitute(a, compound_pattern, "{numerator}", numerator[i], "{denominator}", denominator, maximum, &out[i]);
        if (s != QJS_INTL_OK) goto fail;
        if (!qjs_intl_number_template_validate(out[i], 3)) { s = QJS_INTL_DATA_ERROR; goto fail; }
    }
    return QJS_INTL_OK;
fail:
    qjs_intl_number_unit_composed_clear(a, out);
    return s;
}
