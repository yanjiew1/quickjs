/* Bounded UTF-16 decoding shared by Unicode algorithms.
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
#ifndef UNICODE_UTF16_H
#define UNICODE_UTF16_H
#include <stddef.h>
#include <stdint.h>

/* Precondition: *position < length. Preserve a lone surrogate as its code
   point value; only a valid adjacent pair consumes two original code units. */
static inline uint32_t unicode_utf16_next(const uint16_t *input, size_t length,
                                         size_t *position)
{
    uint32_t c = input[(*position)++];
    if (c >= 0xd800 && c <= 0xdbff && *position < length &&
        input[*position] >= 0xdc00 && input[*position] <= 0xdfff) {
        c = 0x10000 + ((c - 0xd800) << 10) + input[(*position)++] - 0xdc00;
    }
    return c;
}
#endif
