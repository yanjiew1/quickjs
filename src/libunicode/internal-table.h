/*
 * Unicode utilities private compressed-table helpers
 *
 * Copyright (c) 2017-2018 Fabrice Bellard
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
#ifndef LIBUNICODE_INTERNAL_TABLE_H
#define LIBUNICODE_INTERNAL_TABLE_H

#include "../../cutils.h"

#define UNICODE_INDEX_BLOCK_LEN 32

static inline uint32_t unicode_get_le24(const uint8_t *ptr)
{
    return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16);
}

static inline int get_index_pos(uint32_t *pcode, uint32_t c,
                                const uint8_t *index_table,
                                int index_table_len)
{
    uint32_t code, v;
    int idx_min, idx_max, idx;

    idx_min = 0;
    v = unicode_get_le24(index_table);
    code = v & ((1 << 21) - 1);
    if (c < code) {
        *pcode = 0;
        return 0;
    }
    idx_max = index_table_len - 1;
    code = unicode_get_le24(index_table + idx_max * 3);
    if (c >= code)
        return -1;
    while ((idx_max - idx_min) > 1) {
        idx = (idx_max + idx_min) / 2;
        v = unicode_get_le24(index_table + idx * 3);
        code = v & ((1 << 21) - 1);
        if (c < code)
            idx_max = idx;
        else
            idx_min = idx;
    }
    v = unicode_get_le24(index_table + idx_min * 3);
    *pcode = v & ((1 << 21) - 1);
    return (idx_min + 1) * UNICODE_INDEX_BLOCK_LEN + (v >> 21);
}

static inline BOOL lre_is_in_table(uint32_t c, const uint8_t *table,
                                   const uint8_t *index_table,
                                   int index_table_len)
{
    uint32_t code, b, bit;
    int pos;
    const uint8_t *p;

    pos = get_index_pos(&code, c, index_table, index_table_len);
    if (pos < 0)
        return FALSE;
    p = table + pos;
    bit = 0;
    for (;;) {
        b = *p++;
        if (b < 64) {
            code += (b >> 3) + 1;
            if (c < code)
                return bit;
            bit ^= 1;
            code += (b & 7) + 1;
        } else if (b >= 0x80) {
            code += b - 0x80 + 1;
        } else if (b < 0x60) {
            code += (((b - 0x40) << 8) | p[0]) + 1;
            p++;
        } else {
            code += (((b - 0x60) << 16) | (p[0] << 8) | p[1]) + 1;
            p += 2;
        }
        if (c < code)
            return bit;
        bit ^= 1;
    }
}

#endif /* LIBUNICODE_INTERNAL_TABLE_H */
