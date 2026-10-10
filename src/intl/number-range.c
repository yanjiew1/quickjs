/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "number-range.h"
#include <string.h>

int qjs_intl_number_range_slices(QJSIntlBytes pattern, unsigned int range,
                                QJSIntlNumberRangeSlices *out)
{
    size_t i, first = SIZE_MAX, second = SIZE_MAX, positions[2] = {SIZE_MAX, SIZE_MAX};
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (!pattern.data || !pattern.length || range > 1) return 0;
    for (i = 0; i < pattern.length; i++) {
        if (pattern.data[i] == '}') return 0;
        if (pattern.data[i] != '{') continue;
        if (pattern.length - i < 3 || pattern.data[i + 2] != '}') return 0;
        if (pattern.data[i + 1] != '0' && !(range && pattern.data[i + 1] == '1')) return 0;
        { unsigned int endpoint = (unsigned int)(pattern.data[i + 1] - '0');
          if (positions[endpoint] != SIZE_MAX) return 0;
          positions[endpoint] = i;
          if (first == SIZE_MAX) { first = i; out->first_endpoint = (uint8_t)endpoint; }
          else second = i; }
        i += 2;
    }
    if (positions[0] == SIZE_MAX || (range && positions[1] == SIZE_MAX)) return 0;
    out->prefix.data = pattern.data; out->prefix.length = first;
    if (range) {
        out->separator.data = pattern.data + first + 3;
        out->separator.length = second - first - 3;
        /* Empty separators could create ambiguous representations. */
        if (!out->separator.length) { memset(out, 0, sizeof(*out)); return 0; }
    }
    i = (range ? second : first) + 3;
    out->suffix.data = pattern.data + i; out->suffix.length = pattern.length - i;
    return 1;
}
int qjs_intl_number_range_equal(const QJSIntlFormatted *a, const QJSIntlFormatted *b)
{
    return a->length == b->length && (!a->length ||
        !memcmp(a->text, b->text, a->length * sizeof(*a->text)));
}
