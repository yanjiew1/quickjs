/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/number-native.h"
#include "../src/intl/number-range.h"
#include "../src/intl/number-unit.h"
#include "../src/intl/number-compact.h"
#include <assert.h>
#include <string.h>

static QJSIntlBytes text(const char *s)
{
    QJSIntlBytes result = {s, strlen(s)};
    return result;
}
int main(void)
{
    QJSIntlNumberRangeSlices slices;
    assert(qjs_intl_number_template_validate(text("{number}"), 0));
    assert(qjs_intl_number_template_validate(text("{currency} {number}"), 2));
    assert(!qjs_intl_number_template_validate(text("{currency} {number}"), 1));
    assert(!qjs_intl_number_template_validate(text("{number}{number}"), 0));
    assert(!qjs_intl_number_template_validate(text("{unknown}"), 0));
    assert(!qjs_intl_number_template_validate(text("\xed\xa0\x80{number}"), 0));
    assert(qjs_intl_number_compact_template_validate(text("{number}K"), 3));
    assert(qjs_intl_number_compact_template_validate(text("{number}"), 0));
    assert(!qjs_intl_number_compact_template_validate(text("{number}K"), 0));
    assert(qjs_intl_number_range_slices(text("[{1} - {0}]"), 1, &slices));
    assert(slices.first_endpoint == 1 && slices.prefix.length == 1);
    assert(slices.separator.length == 3 && !memcmp(slices.separator.data, " - ", 3));
    assert(!qjs_intl_number_range_slices(text("{0}{1}"), 1, &slices));
    assert(!qjs_intl_number_range_slices(text("{0} - {0}"), 1, &slices));
    assert(qjs_intl_number_unit_validate(text("meter-per-second")));
    assert(qjs_intl_number_unit_validate(text("kilogram")));
    assert(!qjs_intl_number_unit_validate(text("made-up-unit")));
    assert(!qjs_intl_number_unit_validate(text("meter-per-second-per-hour")));
    {
        const QJSIntlNumberCompact rows[] = {{.magnitude=3}, {.magnitude=6}};
        assert(!qjs_intl_number_compact_for_magnitude(rows, 2, 2));
        assert(qjs_intl_number_compact_for_magnitude(rows, 2, 5) == rows);
        assert(qjs_intl_number_compact_for_magnitude(rows, 2, 9) == rows + 1);
    }
    return 0;
}
