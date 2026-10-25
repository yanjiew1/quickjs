/* Indexed Unicode boundary properties; tables have one generated C owner.
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
