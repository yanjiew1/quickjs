/* Unicode Block lookup, independently of assignment and collation policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
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
