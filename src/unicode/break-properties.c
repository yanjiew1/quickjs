/* Indexed Unicode boundary properties; tables have one generated C owner.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../cutils/cutils.h"
#include "libunicode.h"
#include <libunicode-table.h>
#include "break-properties.h"

uint16_t unicode_break_properties(uint32_t code)
{
    uint32_t bucket, lo, hi;
    if (code > 0x10ffff)
        return 0;
    bucket = code >> UNICODE_BREAK_BUCKET_SHIFT;
    lo = unicode_break_index[bucket];
    /* The next bucket's preceding transition may still start in this bucket.
       Keep it as an inclusive upper bound, then find the last start <= code. */
    hi = unicode_break_index[bucket + 1];
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo + 1) / 2;
        if (unicode_break_starts[mid] <= code)
            lo = mid;
        else
            hi = mid - 1;
    }
    return unicode_break_values[lo];
}
