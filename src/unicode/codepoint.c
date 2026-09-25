/*
 * Unicode code point classification
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "cutils.h"
#include "libunicode.h"
#include "libunicode-table.h"
#include "unicode-encoding.h"
#include "codepoint.h"
static uint32_t get_le24(const uint8_t *ptr)
{
    return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16);
}

#define UNICODE_INDEX_BLOCK_LEN 32

/* return -1 if not in table, otherwise the offset in the block */
int get_index_pos(uint32_t *pcode, uint32_t c,
                  const uint8_t *index_table, int index_table_len)
{
    uint32_t code, v;
    int idx_min, idx_max, idx;

    idx_min = 0;
    v = get_le24(index_table);
    code = v & ((1 << 21) - 1);
    if (c < code) {
        *pcode = 0;
        return 0;
    }
    idx_max = index_table_len - 1;
    code = get_le24(index_table + idx_max * 3);
    if (c >= code)
        return -1;
    /* invariant: tab[idx_min] <= c < tab2[idx_max] */
    while ((idx_max - idx_min) > 1) {
        idx = (idx_max + idx_min) / 2;
        v = get_le24(index_table + idx * 3);
        code = v & ((1 << 21) - 1);
        if (c < code) {
            idx_max = idx;
        } else {
            idx_min = idx;
        }
    }
    v = get_le24(index_table + idx_min * 3);
    *pcode = v & ((1 << 21) - 1);
    return (idx_min + 1) * UNICODE_INDEX_BLOCK_LEN + (v >> 21);
}

static BOOL lre_is_in_table(uint32_t c, const uint8_t *table,
                            const uint8_t *index_table, int index_table_len)
{
    uint32_t code, b, bit;
    int pos;
    const uint8_t *p;

    pos = get_index_pos(&code, c, index_table, index_table_len);
    if (pos < 0)
        return FALSE; /* outside the table */
    p = table + pos;
    bit = 0;
    /* Compressed run length encoding:
       00..3F: 2 packed lengths: 3-bit + 3-bit
       40..5F: 5-bits plus extra byte for length
       60..7F: 5-bits plus 2 extra bytes for length
       80..FF: 7-bit length
       lengths must be incremented to get character count
       Ranges alternate between false and true return value.
     */
    for(;;) {
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

BOOL lre_is_cased(uint32_t c)
{
    uint32_t v, code, len;
    int idx, idx_min, idx_max;

    idx_min = 0;
    idx_max = countof(case_conv_table1) - 1;
    while (idx_min <= idx_max) {
        idx = (unsigned)(idx_max + idx_min) / 2;
        v = case_conv_table1[idx];
        code = v >> (32 - 17);
        len = (v >> (32 - 17 - 7)) & 0x7f;
        if (c < code) {
            idx_max = idx - 1;
        } else if (c >= code + len) {
            idx_min = idx + 1;
        } else {
            return TRUE;
        }
    }
    return lre_is_in_table(c, unicode_prop_Cased1_table,
                           unicode_prop_Cased1_index,
                           sizeof(unicode_prop_Cased1_index) / 3);
}

BOOL lre_is_case_ignorable(uint32_t c)
{
    return lre_is_in_table(c, unicode_prop_Case_Ignorable_table,
                           unicode_prop_Case_Ignorable_index,
                           sizeof(unicode_prop_Case_Ignorable_index) / 3);
}

#ifdef CONFIG_ALL_UNICODE

BOOL lre_is_id_start(uint32_t c)
{
    return lre_is_in_table(c, unicode_prop_ID_Start_table,
                           unicode_prop_ID_Start_index,
                           sizeof(unicode_prop_ID_Start_index) / 3);
}

BOOL lre_is_id_continue(uint32_t c)
{
    return lre_is_id_start(c) ||
        lre_is_in_table(c, unicode_prop_ID_Continue1_table,
                        unicode_prop_ID_Continue1_index,
                        sizeof(unicode_prop_ID_Continue1_index) / 3);
}

#endif /* CONFIG_ALL_UNICODE */

/*---- lre codepoint categorizing functions ----*/

#define S  UNICODE_C_SPACE
#define D  UNICODE_C_DIGIT
#define X  UNICODE_C_XDIGIT
#define U  UNICODE_C_UPPER
#define L  UNICODE_C_LOWER
#define _  UNICODE_C_UNDER
#define d  UNICODE_C_DOLLAR

uint8_t const lre_ctype_bits[256] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, S, S, S, S, S, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    S, 0, 0, 0, d, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    X|D, X|D, X|D, X|D, X|D, X|D, X|D, X|D,
    X|D, X|D, 0, 0, 0, 0, 0, 0,

    0, X|U, X|U, X|U, X|U, X|U, X|U, U,
    U, U, U, U, U, U, U, U,
    U, U, U, U, U, U, U, U,
    U, U, U, 0, 0, 0, 0, _,

    0, X|L, X|L, X|L, X|L, X|L, X|L, L,
    L, L, L, L, L, L, L, L,
    L, L, L, L, L, L, L, L,
    L, L, L, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    S, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

#undef S
#undef D
#undef X
#undef U
#undef L
#undef _
#undef d

/* code point ranges for Zs,Zl or Zp property */
static const uint16_t char_range_s[] = {
    10,
    0x0009, 0x000D + 1,
    0x0020, 0x0020 + 1,
    0x00A0, 0x00A0 + 1,
    0x1680, 0x1680 + 1,
    0x2000, 0x200A + 1,
    /* 2028;LINE SEPARATOR;Zl;0;WS;;;;;N;;;;; */
    /* 2029;PARAGRAPH SEPARATOR;Zp;0;B;;;;;N;;;;; */
    0x2028, 0x2029 + 1,
    0x202F, 0x202F + 1,
    0x205F, 0x205F + 1,
    0x3000, 0x3000 + 1,
    /* FEFF;ZERO WIDTH NO-BREAK SPACE;Cf;0;BN;;;;;N;BYTE ORDER MARK;;;; */
    0xFEFF, 0xFEFF + 1,
};

BOOL lre_is_space_non_ascii(uint32_t c)
{
    size_t i, n;

    n = countof(char_range_s);
    for(i = 5; i < n; i += 2) {
        uint32_t low = char_range_s[i];
        uint32_t high = char_range_s[i + 1];
        if (c < low)
            return FALSE;
        if (c < high)
            return TRUE;
    }
    return FALSE;
}
