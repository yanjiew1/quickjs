/*
 * Unicode Utilities: Case Conversion, Folding, and Ranges
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
#include "unicode_case.h"

enum {
    RUN_TYPE_U,
    RUN_TYPE_L,
    RUN_TYPE_UF,
    RUN_TYPE_LF,
    RUN_TYPE_UL,
    RUN_TYPE_LSU,
    RUN_TYPE_U2L_399_EXT2,
    RUN_TYPE_UF_D20,
    RUN_TYPE_UF_D1_EXT,
    RUN_TYPE_U_EXT,
    RUN_TYPE_LF_EXT,
    RUN_TYPE_UF_EXT2,
    RUN_TYPE_LF_EXT2,
    RUN_TYPE_UF_EXT3,
};

static int lre_case_conv1(uint32_t c, int conv_type)
{
    uint32_t res[LRE_CC_RES_LEN_MAX];
    lre_case_conv(res, c, conv_type);
    return res[0];
}

/* case conversion using the table entry 'idx' with value 'v' */
static int lre_case_conv_entry(uint32_t *res, uint32_t c, int conv_type, uint32_t idx, uint32_t v)
{
    uint32_t code, data, type, a, is_lower;
    is_lower = (conv_type != 0);
    type = (v >> (32 - 17 - 7 - 4)) & 0xf;
    data = ((v & 0xf) << 8) | case_conv_table2[idx];
    code = v >> (32 - 17);
    switch(type) {
    case RUN_TYPE_U:
    case RUN_TYPE_L:
    case RUN_TYPE_UF:
    case RUN_TYPE_LF:
        if (conv_type == (type & 1) ||
            (type >= RUN_TYPE_UF && conv_type == 2)) {
            c = c - code + (case_conv_table1[data] >> (32 - 17));
        }
        break;
    case RUN_TYPE_UL:
        a = c - code;
        if ((a & 1) != (1 - is_lower))
            break;
        c = (a ^ 1) + code;
        break;
    case RUN_TYPE_LSU:
        a = c - code;
        if (a == 1) {
            c += 2 * is_lower - 1;
        } else if (a == (1 - is_lower) * 2) {
            c += (2 * is_lower - 1) * 2;
        }
        break;
    case RUN_TYPE_U2L_399_EXT2:
        if (!is_lower) {
            res[0] = c - code + case_conv_ext[data >> 6];
            res[1] = 0x399;
            return 2;
        } else {
            c = c - code + case_conv_ext[data & 0x3f];
        }
        break;
    case RUN_TYPE_UF_D20:
        if (conv_type == 1)
            break;
        c = data + (conv_type == 2) * 0x20;
        break;
    case RUN_TYPE_UF_D1_EXT:
        if (conv_type == 1)
            break;
        c = case_conv_ext[data] + (conv_type == 2);
        break;
    case RUN_TYPE_U_EXT:
    case RUN_TYPE_LF_EXT:
        if (is_lower != (type - RUN_TYPE_U_EXT))
            break;
        c = case_conv_ext[data];
        break;
    case RUN_TYPE_LF_EXT2:
        if (!is_lower)
            break;
        res[0] = c - code + case_conv_ext[data >> 6];
        res[1] = case_conv_ext[data & 0x3f];
        return 2;
    case RUN_TYPE_UF_EXT2:
        if (conv_type == 1)
            break;
        res[0] = c - code + case_conv_ext[data >> 6];
        res[1] = case_conv_ext[data & 0x3f];
        if (conv_type == 2) {
            /* convert to lower */
            res[0] = lre_case_conv1(res[0], 1);
            res[1] = lre_case_conv1(res[1], 1);
        }
        return 2;
    default:
    case RUN_TYPE_UF_EXT3:
        if (conv_type == 1)
            break;
        res[0] = case_conv_ext[data >> 8];
        res[1] = case_conv_ext[(data >> 4) & 0xf];
        res[2] = case_conv_ext[data & 0xf];
        if (conv_type == 2) {
            /* convert to lower */
            res[0] = lre_case_conv1(res[0], 1);
            res[1] = lre_case_conv1(res[1], 1);
            res[2] = lre_case_conv1(res[2], 1);
        }
        return 3;
    }
    res[0] = c;
    return 1;
}

/* conv_type:
   0 = to upper
   1 = to lower
   2 = case folding (= to lower with modifications)
*/
int lre_case_conv(uint32_t *res, uint32_t c, int conv_type)
{
    if (c < 128) {
        if (conv_type) {
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
        } else {
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
        }
    } else {
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
                return lre_case_conv_entry(res, c, conv_type, idx, v);
            }
        }
    }
    res[0] = c;
    return 1;
}

static int lre_case_folding_entry(uint32_t c, uint32_t idx, uint32_t v, BOOL is_unicode)
{
    uint32_t res[LRE_CC_RES_LEN_MAX];
    int len;

    if (is_unicode) {
        len = lre_case_conv_entry(res, c, 2, idx, v);
        if (len == 1) {
            c = res[0];
        } else {
            /* handle the few specific multi-character cases (see
               unicode_gen.c:dump_case_folding_special_cases()) */
            if (c == 0xfb06) {
                c = 0xfb05;
            } else if (c == 0x01fd3) {
                c = 0x390;
            } else if (c == 0x01fe3) {
                c = 0x3b0;
            }
        }
    } else {
        if (likely(c < 128)) {
            if (c >= 'a' && c <= 'z')
                c = c - 'a' + 'A';
        } else {
            /* legacy regexp: to upper case if single char >= 128 */
            len = lre_case_conv_entry(res, c, FALSE, idx, v);
            if (len == 1 && res[0] >= 128)
                c = res[0];
        }
    }
    return c;
}

/* JS regexp specific rules for case folding */
int lre_canonicalize(uint32_t c, BOOL is_unicode)
{
    if (c < 128) {
        /* fast case */
        if (is_unicode) {
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
        } else {
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
        }
    } else {
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
                return lre_case_folding_entry(c, idx, v, is_unicode);
            }
        }
    }
    return c;
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

/* character range */

static __maybe_unused void cr_dump(CharRange *cr)
{
    int i;
    for(i = 0; i < cr->len; i++)
        printf("%d: 0x%04x\n", i, cr->points[i]);
}

static void *cr_default_realloc(void *opaque, void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void cr_init(CharRange *cr, void *mem_opaque, DynBufReallocFunc *realloc_func)
{
    cr->len = cr->size = 0;
    cr->points = NULL;
    cr->mem_opaque = mem_opaque;
    cr->realloc_func = realloc_func ? realloc_func : cr_default_realloc;
}

void cr_free(CharRange *cr)
{
    cr->realloc_func(cr->mem_opaque, cr->points, 0);
}

int cr_realloc(CharRange *cr, int size)
{
    int new_size;
    uint32_t *new_buf;

    if (size > cr->size) {
        new_size = max_int(size, cr->size * 3 / 2);
        new_buf = cr->realloc_func(cr->mem_opaque, cr->points,
                                   new_size * sizeof(cr->points[0]));
        if (!new_buf)
            return -1;
        cr->points = new_buf;
        cr->size = new_size;
    }
    return 0;
}

int cr_copy(CharRange *cr, const CharRange *cr1)
{
    if (cr_realloc(cr, cr1->len))
        return -1;
    memcpy(cr->points, cr1->points, sizeof(cr->points[0]) * cr1->len);
    cr->len = cr1->len;
    return 0;
}

/* merge consecutive intervals and remove empty intervals */
static void cr_compress(CharRange *cr)
{
    int i, j, k, len;
    uint32_t *pt;

    pt = cr->points;
    len = cr->len;
    i = 0;
    j = 0;
    k = 0;
    while ((i + 1) < len) {
        if (pt[i] == pt[i + 1]) {
            /* empty interval */
            i += 2;
        } else {
            j = i;
            while ((j + 3) < len && pt[j + 1] == pt[j + 2])
                j += 2;
            /* just copy */
            pt[k] = pt[i];
            pt[k + 1] = pt[j + 1];
            k += 2;
            i = j + 2;
        }
    }
    cr->len = k;
}

/* union or intersection */
int cr_op(CharRange *cr, const uint32_t *a_pt, int a_len,
          const uint32_t *b_pt, int b_len, int op)
{
    int a_idx, b_idx, is_in;
    uint32_t v;

    a_idx = 0;
    b_idx = 0;
    for(;;) {
        /* get one more point from a or b in increasing order */
        if (a_idx < a_len && b_idx < b_len) {
            if (a_pt[a_idx] < b_pt[b_idx]) {
                goto a_add;
            } else if (a_pt[a_idx] == b_pt[b_idx]) {
                v = a_pt[a_idx];
                a_idx++;
                b_idx++;
            } else {
                goto b_add;
            }
        } else if (a_idx < a_len) {
        a_add:
            v = a_pt[a_idx++];
        } else if (b_idx < b_len) {
        b_add:
            v = b_pt[b_idx++];
        } else {
            break;
        }
        /* add the point if the in/out status changes */
        switch(op) {
        case CR_OP_UNION:
            is_in = (a_idx & 1) | (b_idx & 1);
            break;
        case CR_OP_INTER:
            is_in = (a_idx & 1) & (b_idx & 1);
            break;
        case CR_OP_XOR:
            is_in = (a_idx & 1) ^ (b_idx & 1);
            break;
        case CR_OP_SUB:
            is_in = (a_idx & 1) & ((b_idx & 1) ^ 1);
            break;
        default:
            abort();
        }
        if (is_in != (cr->len & 1)) {
            if (cr_add_point(cr, v))
                return -1;
        }
    }
    cr_compress(cr);
    return 0;
}

int cr_op1(CharRange *cr, const uint32_t *b_pt, int b_len, int op)
{
    CharRange a = *cr;
    int ret;
    cr->len = 0;
    cr->size = 0;
    cr->points = NULL;
    ret = cr_op(cr, a.points, a.len, b_pt, b_len, op);
    cr_free(&a);
    return ret;
}

int cr_invert(CharRange *cr)
{
    int len;
    len = cr->len;
    if (cr_realloc(cr, len + 2))
        return -1;
    memmove(cr->points + 1, cr->points, len * sizeof(cr->points[0]));
    cr->points[0] = 0;
    cr->points[len + 1] = UINT32_MAX;
    cr->len = len + 2;
    cr_compress(cr);
    return 0;
}

int unicode_case1(CharRange *cr, int case_mask)
{
#define MR(x) (1 << RUN_TYPE_ ## x)
    const uint32_t tab_run_mask[3] = {
        MR(U) | MR(UF) | MR(UL) | MR(LSU) | MR(U2L_399_EXT2) | MR(UF_D20) |
        MR(UF_D1_EXT) | MR(U_EXT) | MR(UF_EXT2) | MR(UF_EXT3),

        MR(L) | MR(LF) | MR(UL) | MR(LSU) | MR(U2L_399_EXT2) | MR(LF_EXT) | MR(LF_EXT2),

        MR(UF) | MR(LF) | MR(UL) | MR(LSU) | MR(U2L_399_EXT2) | MR(LF_EXT) | MR(LF_EXT2) | MR(UF_D20) | MR(UF_D1_EXT) | MR(LF_EXT) | MR(UF_EXT2) | MR(UF_EXT3),
    };
#undef MR
    uint32_t mask, v, code, type, len, i, idx;

    if (case_mask == 0)
        return 0;
    mask = 0;
    for(i = 0; i < 3; i++) {
        if ((case_mask >> i) & 1)
            mask |= tab_run_mask[i];
    }
    for(idx = 0; idx < countof(case_conv_table1); idx++) {
        v = case_conv_table1[idx];
        type = (v >> (32 - 17 - 7 - 4)) & 0xf;
        code = v >> (32 - 17);
        len = (v >> (32 - 17 - 7)) & 0x7f;
        if ((mask >> type) & 1) {
            //            printf("%d: type=%d %04x %04x\n", idx, type, code, code + len - 1);
            switch(type) {
            case RUN_TYPE_UL:
                if ((case_mask & CASE_U) && (case_mask & (CASE_L | CASE_F)))
                    goto def_case;
                code += ((case_mask & CASE_U) != 0);
                for(i = 0; i < len; i += 2) {
                    if (cr_add_interval(cr, code + i, code + i + 1))
                        return -1;
                }
                break;
            case RUN_TYPE_LSU:
                if ((case_mask & CASE_U) && (case_mask & (CASE_L | CASE_F)))
                    goto def_case;
                if (!(case_mask & CASE_U)) {
                    if (cr_add_interval(cr, code, code + 1))
                        return -1;
                }
                if (cr_add_interval(cr, code + 1, code + 2))
                    return -1;
                if (case_mask & CASE_U) {
                    if (cr_add_interval(cr, code + 2, code + 3))
                        return -1;
                }
                break;
            default:
            def_case:
                if (cr_add_interval(cr, code, code + len))
                    return -1;
                break;
            }
        }
    }
    return 0;
}

static int point_cmp(const void *p1, const void *p2, void *arg)
{
    uint32_t v1 = *(uint32_t *)p1;
    uint32_t v2 = *(uint32_t *)p2;
    return (v1 > v2) - (v1 < v2);
}

static void cr_sort_and_remove_overlap(CharRange *cr)
{
    uint32_t start, end, start1, end1, i, j;

    /* the resulting ranges are not necessarily sorted and may overlap */
    rqsort(cr->points, cr->len / 2, sizeof(cr->points[0]) * 2, point_cmp, NULL);
    j = 0;
    for(i = 0; i < cr->len; ) {
        start = cr->points[i];
        end = cr->points[i + 1];
        i += 2;
        while (i < cr->len) {
            start1 = cr->points[i];
            end1 = cr->points[i + 1];
            if (start1 > end) {
                /* |------|
                 *           |-------| */
                break;
            } else if (end1 <= end) {
                /* |------|
                 *    |--| */
                i += 2;
            } else {
                /* |------|
                 *     |-------| */
                end = end1;
                i += 2;
            }
        }
        cr->points[j] = start;
        cr->points[j + 1] = end;
        j += 2;
    }
    cr->len = j;
}

/* canonicalize a character set using the JS regex case folding rules
   (see lre_canonicalize()) */
int cr_regexp_canonicalize(CharRange *cr, BOOL is_unicode)
{
    CharRange cr_inter, cr_mask, cr_result, cr_sub;
    uint32_t v, code, len, i, idx, start, end, c, d_start, d_end, d;

    cr_init(&cr_mask, cr->mem_opaque, cr->realloc_func);
    cr_init(&cr_inter, cr->mem_opaque, cr->realloc_func);
    cr_init(&cr_result, cr->mem_opaque, cr->realloc_func);
    cr_init(&cr_sub, cr->mem_opaque, cr->realloc_func);

    if (unicode_case1(&cr_mask, is_unicode ? CASE_F : CASE_U))
        goto fail;
    if (cr_op(&cr_inter, cr_mask.points, cr_mask.len, cr->points, cr->len, CR_OP_INTER))
        goto fail;

    if (cr_invert(&cr_mask))
        goto fail;
    if (cr_op(&cr_sub, cr_mask.points, cr_mask.len, cr->points, cr->len, CR_OP_INTER))
        goto fail;

    /* cr_inter = cr & cr_mask */
    /* cr_sub = cr & ~cr_mask */

    /* use the case conversion table to compute the result */
    d_start = -1;
    d_end = -1;
    idx = 0;
    v = case_conv_table1[idx];
    code = v >> (32 - 17);
    len = (v >> (32 - 17 - 7)) & 0x7f;
    for(i = 0; i < cr_inter.len; i += 2) {
        start = cr_inter.points[i];
        end = cr_inter.points[i + 1];

        for(c = start; c < end; c++) {
            for(;;) {
                if (c >= code && c < code + len)
                    break;
                idx++;
                assert(idx < countof(case_conv_table1));
                v = case_conv_table1[idx];
                code = v >> (32 - 17);
                len = (v >> (32 - 17 - 7)) & 0x7f;
            }
            d = lre_case_folding_entry(c, idx, v, is_unicode);
            /* try to merge with the current interval */
            if (d_start == -1) {
                d_start = d;
                d_end = d + 1;
            } else if (d_end == d) {
                d_end++;
            } else {
                cr_add_interval(&cr_result, d_start, d_end);
                d_start = d;
                d_end = d + 1;
            }
        }
    }
    if (d_start != -1) {
        if (cr_add_interval(&cr_result, d_start, d_end))
            goto fail;
    }

    /* the resulting ranges are not necessarily sorted and may overlap */
    cr_sort_and_remove_overlap(&cr_result);

    /* or with the character not affected by the case folding */
    cr->len = 0;
    if (cr_op(cr, cr_result.points, cr_result.len, cr_sub.points, cr_sub.len, CR_OP_UNION))
        goto fail;

    cr_free(&cr_inter);
    cr_free(&cr_mask);
    cr_free(&cr_result);
    cr_free(&cr_sub);
    return 0;
 fail:
    cr_free(&cr_inter);
    cr_free(&cr_mask);
    cr_free(&cr_result);
    cr_free(&cr_sub);
    return -1;
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
