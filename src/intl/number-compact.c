/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "number-compact.h"
#include <limits.h>

const QJSIntlNumberCompact *qjs_intl_number_compact_for_magnitude(
    const QJSIntlNumberCompact *rows, size_t count, int64_t magnitude)
{
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t middle = lo + (hi - lo) / 2;
        if (rows[middle].magnitude <= magnitude) lo = middle + 1;
        else hi = middle;
    }
    return lo ? &rows[lo - 1] : NULL;
}
int qjs_intl_number_compact_is_one(const QJSIntlDecimal *d)
{
    size_t i;
    if (!d || !d->digits || !d->length || d->length > INT32_MAX ||
        d->digits[0] != '1' || (int64_t)d->exponent != 1 - (int64_t)d->length)
        return 0;
    for (i = 1; i < d->length; i++) if (d->digits[i] != '0') return 0;
    return 1;
}
