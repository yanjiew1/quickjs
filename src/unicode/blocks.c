/* Unicode Block lookup, independently of assignment and collation policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../cutils/cutils.h"
#include "libunicode.h"
#include <libunicode-table.h>

int unicode_get_block(uint32_t c)
{
    size_t lo = 0, hi = countof(unicode_block_ranges);
    if (c > 0x10ffff)
        return UNICODE_BLOCK_No_Block;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (c < unicode_block_ranges[mid][0])
            hi = mid;
        else if (c > unicode_block_ranges[mid][1])
            lo = mid + 1;
        else
            return mid + 1; /* Generated enum IDs follow source range order. */
    }
    return UNICODE_BLOCK_No_Block;
}
