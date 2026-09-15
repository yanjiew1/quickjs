/*
 * Unicode utilities - normalization
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
#include <string.h>

#include "../../cutils.h"
#include "../../libunicode.h"
#define LIBUNICODE_TABLE_NORMALIZE
#include "../../libunicode-table.h"
#include "internal-table.h"

#ifdef CONFIG_ALL_UNICODE

#define UNICODE_DECOMP_LEN_MAX 18

typedef enum {
    DECOMP_TYPE_C1, /* 16 bit char */
    DECOMP_TYPE_L1, /* 16 bit char table */
    DECOMP_TYPE_L2,
    DECOMP_TYPE_L3,
    DECOMP_TYPE_L4,
    DECOMP_TYPE_L5, /* XXX: not used */
    DECOMP_TYPE_L6, /* XXX: could remove */
    DECOMP_TYPE_L7, /* XXX: could remove */
    DECOMP_TYPE_LL1, /* 18 bit char table */
    DECOMP_TYPE_LL2,
    DECOMP_TYPE_S1, /* 8 bit char table */
    DECOMP_TYPE_S2,
    DECOMP_TYPE_S3,
    DECOMP_TYPE_S4,
    DECOMP_TYPE_S5,
    DECOMP_TYPE_I1, /* increment 16 bit char value */
    DECOMP_TYPE_I2_0,
    DECOMP_TYPE_I2_1,
    DECOMP_TYPE_I3_1,
    DECOMP_TYPE_I3_2,
    DECOMP_TYPE_I4_1,
    DECOMP_TYPE_I4_2,
    DECOMP_TYPE_B1, /* 16 bit base + 8 bit offset */
    DECOMP_TYPE_B2,
    DECOMP_TYPE_B3,
    DECOMP_TYPE_B4,
    DECOMP_TYPE_B5,
    DECOMP_TYPE_B6,
    DECOMP_TYPE_B7,
    DECOMP_TYPE_B8,
    DECOMP_TYPE_B18,
    DECOMP_TYPE_LS2,
    DECOMP_TYPE_PAT3,
    DECOMP_TYPE_S2_UL,
    DECOMP_TYPE_LS2_UL,
} DecompTypeEnum;

static uint32_t unicode_get_short_code(uint32_t c)
{
    static const uint16_t unicode_short_table[2] = { 0x2044, 0x2215 };

    if (c < 0x80)
        return c;
    else if (c < 0x80 + 0x50)
        return c - 0x80 + 0x300;
    else
        return unicode_short_table[c - 0x80 - 0x50];
}

static uint32_t unicode_get_lower_simple(uint32_t c)
{
    if (c < 0x100 || (c >= 0x410 && c <= 0x42f))
        c += 0x20;
    else
        c++;
    return c;
}

static uint16_t unicode_get16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

static int unicode_decomp_entry(uint32_t *res, uint32_t c,
                                int idx, uint32_t code, uint32_t len,
                                uint32_t type)
{
    uint32_t c1;
    int l, i, p;
    const uint8_t *d;

    if (type == DECOMP_TYPE_C1) {
        res[0] = unicode_decomp_table2[idx];
        return 1;
    } else {
        d = unicode_decomp_data + unicode_decomp_table2[idx];
        switch(type) {
        case DECOMP_TYPE_L1:
        case DECOMP_TYPE_L2:
        case DECOMP_TYPE_L3:
        case DECOMP_TYPE_L4:
        case DECOMP_TYPE_L5:
        case DECOMP_TYPE_L6:
        case DECOMP_TYPE_L7:
            l = type - DECOMP_TYPE_L1 + 1;
            d += (c - code) * l * 2;
            for(i = 0; i < l; i++) {
                if ((res[i] = unicode_get16(d + 2 * i)) == 0)
                    return 0;
            }
            return l;
        case DECOMP_TYPE_LL1:
        case DECOMP_TYPE_LL2:
            {
                uint32_t k, p;
                l = type - DECOMP_TYPE_LL1 + 1;
                k = (c - code) * l;
                p = len * l * 2;
                for(i = 0; i < l; i++) {
                    c1 = unicode_get16(d + 2 * k) |
                        (((d[p + (k / 4)] >> ((k % 4) * 2)) & 3) << 16);
                    if (!c1)
                        return 0;
                    res[i] = c1;
                    k++;
                }
            }
            return l;
        case DECOMP_TYPE_S1:
        case DECOMP_TYPE_S2:
        case DECOMP_TYPE_S3:
        case DECOMP_TYPE_S4:
        case DECOMP_TYPE_S5:
            l = type - DECOMP_TYPE_S1 + 1;
            d += (c - code) * l;
            for(i = 0; i < l; i++) {
                if ((res[i] = unicode_get_short_code(d[i])) == 0)
                    return 0;
            }
            return l;
        case DECOMP_TYPE_I1:
            l = 1;
            p = 0;
            goto decomp_type_i;
        case DECOMP_TYPE_I2_0:
        case DECOMP_TYPE_I2_1:
        case DECOMP_TYPE_I3_1:
        case DECOMP_TYPE_I3_2:
        case DECOMP_TYPE_I4_1:
        case DECOMP_TYPE_I4_2:
            l = 2 + ((type - DECOMP_TYPE_I2_0) >> 1);
            p = ((type - DECOMP_TYPE_I2_0) & 1) + (l > 2);
        decomp_type_i:
            for(i = 0; i < l; i++) {
                c1 = unicode_get16(d + 2 * i);
                if (i == p)
                    c1 += c - code;
                res[i] = c1;
            }
            return l;
        case DECOMP_TYPE_B18:
            l = 18;
            goto decomp_type_b;
        case DECOMP_TYPE_B1:
        case DECOMP_TYPE_B2:
        case DECOMP_TYPE_B3:
        case DECOMP_TYPE_B4:
        case DECOMP_TYPE_B5:
        case DECOMP_TYPE_B6:
        case DECOMP_TYPE_B7:
        case DECOMP_TYPE_B8:
            l = type - DECOMP_TYPE_B1 + 1;
        decomp_type_b:
            {
                uint32_t c_min;
                c_min = unicode_get16(d);
                d += 2 + (c - code) * l;
                for(i = 0; i < l; i++) {
                    c1 = d[i];
                    if (c1 == 0xff)
                        c1 = 0x20;
                    else
                        c1 += c_min;
                    res[i] = c1;
                }
            }
            return l;
        case DECOMP_TYPE_LS2:
            d += (c - code) * 3;
            if (!(res[0] = unicode_get16(d)))
                return 0;
            res[1] = unicode_get_short_code(d[2]);
            return 2;
        case DECOMP_TYPE_PAT3:
            res[0] = unicode_get16(d);
            res[2] = unicode_get16(d + 2);
            d += 4 + (c - code) * 2;
            res[1] = unicode_get16(d);
            return 3;
        case DECOMP_TYPE_S2_UL:
        case DECOMP_TYPE_LS2_UL:
            c1 = c - code;
            if (type == DECOMP_TYPE_S2_UL) {
                d += c1 & ~1;
                c = unicode_get_short_code(*d);
                d++;
            } else {
                d += (c1 >> 1) * 3;
                c = unicode_get16(d);
                d += 2;
            }
            if (c1 & 1)
                c = unicode_get_lower_simple(c);
            res[0] = c;
            res[1] = unicode_get_short_code(*d);
            return 2;
        }
    }
    return 0;
}


/* return the length of the decomposition (length <=
   UNICODE_DECOMP_LEN_MAX) or 0 if no decomposition */
static int unicode_decomp_char(uint32_t *res, uint32_t c, BOOL is_compat1)
{
    uint32_t v, type, is_compat, code, len;
    int idx_min, idx_max, idx;

    idx_min = 0;
    idx_max = countof(unicode_decomp_table1) - 1;
    while (idx_min <= idx_max) {
        idx = (idx_max + idx_min) / 2;
        v = unicode_decomp_table1[idx];
        code = v >> (32 - 18);
        len = (v >> (32 - 18 - 7)) & 0x7f;
        //        printf("idx=%d code=%05x len=%d\n", idx, code, len);
        if (c < code) {
            idx_max = idx - 1;
        } else if (c >= code + len) {
            idx_min = idx + 1;
        } else {
            is_compat = v & 1;
            if (is_compat1 < is_compat)
                break;
            type = (v >> (32 - 18 - 7 - 6)) & 0x3f;
            return unicode_decomp_entry(res, c, idx, code, len, type);
        }
    }
    return 0;
}

/* return 0 if no pair found */
static int unicode_compose_pair(uint32_t c0, uint32_t c1)
{
    uint32_t code, len, type, v, idx1, d_idx, d_offset, ch;
    int idx_min, idx_max, idx, d;
    uint32_t pair[2];

    idx_min = 0;
    idx_max = countof(unicode_comp_table) - 1;
    while (idx_min <= idx_max) {
        idx = (idx_max + idx_min) / 2;
        idx1 = unicode_comp_table[idx];

        /* idx1 represent an entry of the decomposition table */
        d_idx = idx1 >> 6;
        d_offset = idx1 & 0x3f;
        v = unicode_decomp_table1[d_idx];
        code = v >> (32 - 18);
        len = (v >> (32 - 18 - 7)) & 0x7f;
        type = (v >> (32 - 18 - 7 - 6)) & 0x3f;
        ch = code + d_offset;
        unicode_decomp_entry(pair, ch, d_idx, code, len, type);
        d = c0 - pair[0];
        if (d == 0)
            d = c1 - pair[1];
        if (d < 0) {
            idx_max = idx - 1;
        } else if (d > 0) {
            idx_min = idx + 1;
        } else {
            return ch;
        }
    }
    return 0;
}

/* return the combining class of character c (between 0 and 255) */
static int unicode_get_cc(uint32_t c)
{
    uint32_t code, n, type, cc, c1, b;
    int pos;
    const uint8_t *p;

    pos = get_index_pos(&code, c,
                        unicode_cc_index, sizeof(unicode_cc_index) / 3);
    if (pos < 0)
        return 0;
    p = unicode_cc_table + pos;
    /* Compressed run length encoding:
       - 2 high order bits are combining class type
       -         0:0, 1:230, 2:extra byte linear progression, 3:extra byte
       - 00..2F: range length (add 1)
       - 30..37: 3-bit range-length + 1 extra byte
       - 38..3F: 3-bit range-length + 2 extra byte
     */
    for(;;) {
        b = *p++;
        type = b >> 6;
        n = b & 0x3f;
        if (n < 48) {
        } else if (n < 56) {
            n = (n - 48) << 8;
            n |= *p++;
            n += 48;
        } else {
            n = (n - 56) << 8;
            n |= *p++ << 8;
            n |= *p++;
            n += 48 + (1 << 11);
        }
        if (type <= 1)
            p++;
        c1 = code + n + 1;
        if (c < c1) {
            switch(type) {
            case 0:
                cc = p[-1];
                break;
            case 1:
                cc = p[-1] + c - code;
                break;
            case 2:
                cc = 0;
                break;
            default:
            case 3:
                cc = 230;
                break;
            }
            return cc;
        }
        code = c1;
    }
}

static void sort_cc(int *buf, int len)
{
    int i, j, k, cc, cc1, start, ch1;

    for(i = 0; i < len; i++) {
        cc = unicode_get_cc(buf[i]);
        if (cc != 0) {
            start = i;
            j = i + 1;
            while (j < len) {
                ch1 = buf[j];
                cc1 = unicode_get_cc(ch1);
                if (cc1 == 0)
                    break;
                k = j - 1;
                while (k >= start) {
                    if (unicode_get_cc(buf[k]) <= cc1)
                        break;
                    buf[k + 1] = buf[k];
                    k--;
                }
                buf[k + 1] = ch1;
                j++;
            }
#if 0
            printf("cc:");
            for(k = start; k < j; k++) {
                printf(" %3d", unicode_get_cc(buf[k]));
            }
            printf("\n");
#endif
            i = j;
        }
    }
}

static void to_nfd_rec(DynBuf *dbuf,
                       const int *src, int src_len, int is_compat)
{
    uint32_t c, v;
    int i, l;
    uint32_t res[UNICODE_DECOMP_LEN_MAX];

    for(i = 0; i < src_len; i++) {
        c = src[i];
        if (c >= 0xac00 && c < 0xd7a4) {
            /* Hangul decomposition */
            c -= 0xac00;
            dbuf_put_u32(dbuf, 0x1100 + c / 588);
            dbuf_put_u32(dbuf, 0x1161 + (c % 588) / 28);
            v = c % 28;
            if (v != 0)
                dbuf_put_u32(dbuf, 0x11a7 + v);
        } else {
            l = unicode_decomp_char(res, c, is_compat);
            if (l) {
                to_nfd_rec(dbuf, (int *)res, l, is_compat);
            } else {
                dbuf_put_u32(dbuf, c);
            }
        }
    }
}

/* return 0 if not found */
static int compose_pair(uint32_t c0, uint32_t c1)
{
    /* Hangul composition */
    if (c0 >= 0x1100 && c0 < 0x1100 + 19 &&
        c1 >= 0x1161 && c1 < 0x1161 + 21) {
        return 0xac00 + (c0 - 0x1100) * 588 + (c1 - 0x1161) * 28;
    } else if (c0 >= 0xac00 && c0 < 0xac00 + 11172 &&
               (c0 - 0xac00) % 28 == 0 &&
               c1 >= 0x11a7 && c1 < 0x11a7 + 28) {
        return c0 + c1 - 0x11a7;
    } else {
        return unicode_compose_pair(c0, c1);
    }
}

int unicode_normalize(uint32_t **pdst, const uint32_t *src, int src_len,
                      UnicodeNormalizationEnum n_type,
                      void *opaque, DynBufReallocFunc *realloc_func)
{
    int *buf, buf_len, i, p, starter_pos, cc, last_cc, out_len;
    BOOL is_compat;
    DynBuf dbuf_s, *dbuf = &dbuf_s;

    is_compat = n_type >> 1;

    dbuf_init2(dbuf, opaque, realloc_func);
    if (dbuf_claim(dbuf, sizeof(int) * src_len))
        goto fail;

    /* common case: latin1 is unaffected by NFC */
    if (n_type == UNICODE_NFC) {
        for(i = 0; i < src_len; i++) {
            if (src[i] >= 0x100)
                goto not_latin1;
        }
        buf = (int *)dbuf->buf;
        if (src_len != 0)
            memcpy(buf, src, src_len * sizeof(int));
        *pdst = (uint32_t *)buf;
        return src_len;
    not_latin1: ;
    }

    to_nfd_rec(dbuf, (const int *)src, src_len, is_compat);
    if (dbuf_error(dbuf)) {
    fail:
        *pdst = NULL;
        return -1;
    }
    buf = (int *)dbuf->buf;
    buf_len = dbuf->size / sizeof(int);

    sort_cc(buf, buf_len);

    if (buf_len <= 1 || (n_type & 1) != 0) {
        /* NFD / NFKD */
        *pdst = (uint32_t *)buf;
        return buf_len;
    }

    i = 1;
    out_len = 1;
    while (i < buf_len) {
        /* find the starter character and test if it is blocked from
           the character at 'i' */
        last_cc = unicode_get_cc(buf[i]);
        starter_pos = out_len - 1;
        while (starter_pos >= 0) {
            cc = unicode_get_cc(buf[starter_pos]);
            if (cc == 0)
                break;
            if (cc >= last_cc)
                goto next;
            last_cc = 256;
            starter_pos--;
        }
        if (starter_pos >= 0 &&
            (p = compose_pair(buf[starter_pos], buf[i])) != 0) {
            buf[starter_pos] = p;
            i++;
        } else {
        next:
            buf[out_len++] = buf[i++];
        }
    }
    *pdst = (uint32_t *)buf;
    return out_len;
}

#ifdef CONFIG_UNICODE_TEST
#include "internal-test.h"

int qjs_unicode_test_decomp_char(uint32_t *res, uint32_t c, BOOL is_compat)
{
    return unicode_decomp_char(res, c, is_compat);
}

int qjs_unicode_test_compose_pair(uint32_t c0, uint32_t c1)
{
    return unicode_compose_pair(c0, c1);
}

int qjs_unicode_test_get_cc(uint32_t c)
{
    return unicode_get_cc(c);
}
#endif

#endif /* CONFIG_ALL_UNICODE */
