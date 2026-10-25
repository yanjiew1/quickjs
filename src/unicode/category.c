/* Allocation-free General_Category queries over the shared Unicode tables.
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
#include "unicode-encoding.h"

#define M(id) (1U << UNICODE_GC_ ## id)

static inline int unicode_gc_contains(uint32_t code, uint32_t mask, uint32_t *run_start)
{
    const uint8_t *p = unicode_gc_table;
    const uint8_t *end = p + countof(unicode_gc_table);
    uint32_t start;
    size_t lo = 0, hi = countof(unicode_gc_index) / 2;
    if (code > 0x10ffff)
        return 0;
    /* Checkpoints bound the scan to 64 encoded runs without duplicating the
       category data. Each pair retains the original run start and byte offset. */
    while (lo + 1 < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (unicode_gc_index[2 * mid] <= code)
            lo = mid;
        else
            hi = mid;
    }
    start = unicode_gc_index[2 * lo];
    p += unicode_gc_index[2 * lo + 1];
    while (p < end) {
        uint32_t b, length, category;
        UNICODE_GC_READ(p, b, length, category);
        length++;
        if (code - start < length) {
            if (run_start)
                *run_start = start;
            /* Category 31 is an alternating Lu/Ll run, beginning with Lu. */
            if (category == 31)
                category = (code - start) & 1 ? UNICODE_GC_Ll : UNICODE_GC_Lu;
            return (mask >> category) & 1;
        }
        start += length;
    }
    return 0;
}

int lre_is_letter(uint32_t c)
{
    if (c < 128)
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    return unicode_gc_contains(c, M(Lu) | M(Ll) | M(Lt) | M(Lm) | M(Lo), NULL);
}

int lre_is_mark(uint32_t c)
{
    if (c < 128)
        return 0;
    return unicode_gc_contains(c, M(Mn) | M(Mc) | M(Me), NULL);
}

int lre_is_assigned(uint32_t c)
{
    /* UCD Assigned includes private-use characters and surrogate code points.
       Scalar validity is a separate question for the consuming algorithm. */
    return c <= 0x10ffff && !unicode_gc_contains(c, M(Cn), NULL);
}

int lre_is_decimal_digit(uint32_t c)
{
    if (c < 128)
        return c >= '0' && c <= '9';
    return unicode_gc_contains(c, M(Nd), NULL);
}

int lre_is_symbol_or_separator(uint32_t c)
{
    return unicode_gc_contains(c, M(Sm) | M(Sc) | M(Sk) | M(So) | M(Zs) | M(Zl) | M(Zp), NULL);
}

int lre_decimal_value(uint32_t c)
{
    uint32_t start;
    if (c < 128)
        return c >= '0' && c <= '9' ? (int)(c - '0') : -1;
    /* Adjacent decimal alphabets can form one GC run (for example the five
       mathematical sets). The generator verifies repeated ascending 0..9. */
    if (!unicode_gc_contains(c, M(Nd), &start))
        return -1;
    return (c - start) % 10;
}
