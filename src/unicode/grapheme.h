/* Default extended grapheme clusters, Unicode 18 / UAX #29 revision 49.
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
#ifndef UNICODE_GRAPHEME_H
#define UNICODE_GRAPHEME_H
#include <stddef.h>
#include <stdint.h>

/* Return the next boundary in original UTF-16 code units. start must be zero
   or a boundary returned by this function; length ends iteration. text may be
   NULL only for an empty input. No allocation or normalization is performed.
   Repeated forward calls take linear time, including long RI/Extend runs.
   Locale-specific tailoring belongs to the segmentation service above this
   default algorithm. */
size_t unicode_grapheme_next(const uint16_t *text, size_t length, size_t start);
#endif
