/*
 * Unicode Utilities: Properties, Scripts, Categories, and Sequences
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
#include "unicode_prop.h"

#ifdef CONFIG_ALL_UNICODE

/* char ranges for various unicode properties */

static int unicode_find_name(const char *name_table, const char *name)
{
    const char *p, *r;
    int pos;
    size_t name_len, len;

    p = name_table;
    pos = 0;
    name_len = strlen(name);
    while (*p) {
        for(;;) {
            r = strchr(p, ',');
            if (!r)
                len = strlen(p);
            else
                len = r - p;
            if (len == name_len && !memcmp(p, name, name_len))
                return pos;
            p += len + 1;
            if (!r)
                break;
        }
        pos++;
    }
    return -1;
}

/* 'cr' must be initialized and empty. Return 0 if OK, -1 if error, -2
   if not found */
int unicode_script(CharRange *cr,
                   const char *script_name, BOOL is_ext)
{
    int script_idx;
    const uint8_t *p, *p_end;
    uint32_t c, c1, b, n, v, v_len, i, type;
    CharRange cr1_s, *cr1;
    CharRange cr2_s, *cr2 = &cr2_s;
    BOOL is_common;

    script_idx = unicode_find_name(unicode_script_name_table, script_name);
    if (script_idx < 0)
        return -2;

    is_common = (script_idx == UNICODE_SCRIPT_Common ||
                 script_idx == UNICODE_SCRIPT_Inherited);
    if (is_ext) {
        cr1 = &cr1_s;
        cr_init(cr1, cr->mem_opaque, cr->realloc_func);
        cr_init(cr2, cr->mem_opaque, cr->realloc_func);
    } else {
        cr1 = cr;
    }

    p = unicode_script_table;
    p_end = unicode_script_table + countof(unicode_script_table);
    c = 0;
    while (p < p_end) {
        b = *p++;
        type = b >> 7;
        n = b & 0x7f;
        if (n < 96) {
        } else if (n < 112) {
            n = (n - 96) << 8;
            n |= *p++;
            n += 96;
        } else {
            n = (n - 112) << 16;
            n |= *p++ << 8;
            n |= *p++;
            n += 96 + (1 << 12);
        }
        c1 = c + n + 1;
        if (type != 0) {
            v = *p++;
            if (v == script_idx || script_idx == UNICODE_SCRIPT_Unknown) {
                if (cr_add_interval(cr1, c, c1))
                    goto fail;
            }
        }
        c = c1;
    }
    if (script_idx == UNICODE_SCRIPT_Unknown) {
        /* Unknown is all the characters outside scripts */
        if (cr_invert(cr1))
            goto fail;
    }

    if (is_ext) {
        /* add the script extensions */
        p = unicode_script_ext_table;
        p_end = unicode_script_ext_table + countof(unicode_script_ext_table);
        c = 0;
        while (p < p_end) {
            b = *p++;
            if (b < 128) {
                n = b;
            } else if (b < 128 + 64) {
                n = (b - 128) << 8;
                n |= *p++;
                n += 128;
            } else {
                n = (b - 128 - 64) << 16;
                n |= *p++ << 8;
                n |= *p++;
                n += 128 + (1 << 14);
            }
            c1 = c + n + 1;
            v_len = *p++;
            if (is_common) {
                if (v_len != 0) {
                    if (cr_add_interval(cr2, c, c1))
                        goto fail;
                }
            } else {
                for(i = 0; i < v_len; i++) {
                    if (p[i] == script_idx) {
                        if (cr_add_interval(cr2, c, c1))
                            goto fail;
                        break;
                    }
                }
            }
            p += v_len;
            c = c1;
        }
        if (is_common) {
            /* remove all the characters with script extensions */
            if (cr_invert(cr2))
                goto fail;
            if (cr_op(cr, cr1->points, cr1->len, cr2->points, cr2->len,
                      CR_OP_INTER))
                goto fail;
        } else {
            if (cr_op(cr, cr1->points, cr1->len, cr2->points, cr2->len,
                      CR_OP_UNION))
                goto fail;
        }
        cr_free(cr1);
        cr_free(cr2);
    }
    return 0;
 fail:
    if (is_ext) {
        cr_free(cr1);
        cr_free(cr2);
    }
    goto fail;
}

#define M(id) (1U << UNICODE_GC_ ## id)

static int unicode_general_category1(CharRange *cr, uint32_t gc_mask)
{
    const uint8_t *p, *p_end;
    uint32_t c, c0, b, n, v;

    p = unicode_gc_table;
    p_end = unicode_gc_table + countof(unicode_gc_table);
    c = 0;
    /* Compressed range encoding:
       initial byte:
       bits 0..4: category number (special case 31)
       bits 5..7: range length (add 1)
       special case bits 5..7 == 7: read an extra byte
       - 00..7F: range length (add 7 + 1)
       - 80..BF: 6-bits plus extra byte for range length (add 7 + 128)
       - C0..FF: 6-bits plus 2 extra bytes for range length (add 7 + 128 + 16384)
     */
    while (p < p_end) {
        b = *p++;
        n = b >> 5;
        v = b & 0x1f;
        if (n == 7) {
            n = *p++;
            if (n < 128) {
                n += 7;
            } else if (n < 128 + 64) {
                n = (n - 128) << 8;
                n |= *p++;
                n += 7 + 128;
            } else {
                n = (n - 128 - 64) << 16;
                n |= *p++ << 8;
                n |= *p++;
                n += 7 + 128 + (1 << 14);
            }
        }
        c0 = c;
        c += n + 1;
        if (v == 31) {
            /* run of Lu / Ll */
            b = gc_mask & (M(Lu) | M(Ll));
            if (b != 0) {
                if (b == (M(Lu) | M(Ll))) {
                    goto add_range;
                } else {
                    c0 += ((gc_mask & M(Ll)) != 0);
                    for(; c0 < c; c0 += 2) {
                        if (cr_add_interval(cr, c0, c0 + 1))
                            return -1;
                    }
                }
            }
        } else if ((gc_mask >> v) & 1) {
        add_range:
            if (cr_add_interval(cr, c0, c))
                return -1;
        }
    }
    return 0;
}

static int unicode_prop1(CharRange *cr, int prop_idx)
{
    const uint8_t *p, *p_end;
    uint32_t c, c0, b, bit;

    p = unicode_prop_table[prop_idx];
    p_end = p + unicode_prop_len_table[prop_idx];
    c = 0;
    bit = 0;
    /* Compressed range encoding:
       00..3F: 2 packed lengths: 3-bit + 3-bit
       40..5F: 5-bits plus extra byte for length
       60..7F: 5-bits plus 2 extra bytes for length
       80..FF: 7-bit length
       lengths must be incremented to get character count
       Ranges alternate between false and true return value.
     */
    while (p < p_end) {
        c0 = c;
        b = *p++;
        if (b < 64) {
            c += (b >> 3) + 1;
            if (bit)  {
                if (cr_add_interval(cr, c0, c))
                    return -1;
            }
            bit ^= 1;
            c0 = c;
            c += (b & 7) + 1;
        } else if (b >= 0x80) {
            c += b - 0x80 + 1;
        } else if (b < 0x60) {
            c += (((b - 0x40) << 8) | p[0]) + 1;
            p++;
        } else {
            c += (((b - 0x60) << 16) | (p[0] << 8) | p[1]) + 1;
            p += 2;
        }
        if (bit)  {
            if (cr_add_interval(cr, c0, c))
                return -1;
        }
        bit ^= 1;
    }
    return 0;
}

typedef enum {
    POP_GC,
    POP_PROP,
    POP_CASE,
    POP_UNION,
    POP_INTER,
    POP_XOR,
    POP_INVERT,
    POP_END,
} PropOPEnum;

#define POP_STACK_LEN_MAX 4

static int unicode_prop_ops(CharRange *cr, ...)
{
    va_list ap;
    CharRange stack[POP_STACK_LEN_MAX];
    int stack_len, op, ret, i;
    uint32_t a;

    va_start(ap, cr);
    stack_len = 0;
    for(;;) {
        op = va_arg(ap, int);
        switch(op) {
        case POP_GC:
            assert(stack_len < POP_STACK_LEN_MAX);
            a = va_arg(ap, int);
            cr_init(&stack[stack_len++], cr->mem_opaque, cr->realloc_func);
            if (unicode_general_category1(&stack[stack_len - 1], a))
                goto fail;
            break;
        case POP_PROP:
            assert(stack_len < POP_STACK_LEN_MAX);
            a = va_arg(ap, int);
            cr_init(&stack[stack_len++], cr->mem_opaque, cr->realloc_func);
            if (unicode_prop1(&stack[stack_len - 1], a))
                goto fail;
            break;
        case POP_CASE:
            assert(stack_len < POP_STACK_LEN_MAX);
            a = va_arg(ap, int);
            cr_init(&stack[stack_len++], cr->mem_opaque, cr->realloc_func);
            if (unicode_case1(&stack[stack_len - 1], a))
                goto fail;
            break;
        case POP_UNION:
        case POP_INTER:
        case POP_XOR:
            {
                CharRange *cr1, *cr2, *cr3;
                assert(stack_len >= 2);
                assert(stack_len < POP_STACK_LEN_MAX);
                cr1 = &stack[stack_len - 2];
                cr2 = &stack[stack_len - 1];
                cr3 = &stack[stack_len++];
                cr_init(cr3, cr->mem_opaque, cr->realloc_func);
                /* CR_OP_XOR may be used here */
                if (cr_op(cr3, cr1->points, cr1->len,
                          cr2->points, cr2->len, op - POP_UNION + CR_OP_UNION))
                    goto fail;
                cr_free(cr1);
                cr_free(cr2);
                *cr1 = *cr3;
                stack_len -= 2;
            }
            break;
        case POP_INVERT:
            assert(stack_len >= 1);
            if (cr_invert(&stack[stack_len - 1]))
                goto fail;
            break;
        case POP_END:
            goto done;
        default:
            abort();
        }
    }
 done:
    assert(stack_len == 1);
    ret = cr_copy(cr, &stack[0]);
    cr_free(&stack[0]);
    return ret;
 fail:
    for(i = 0; i < stack_len; i++)
        cr_free(&stack[i]);
    return -1;
}

static const uint32_t unicode_gc_mask_table[] = {
    M(Lu) | M(Ll) | M(Lt), /* LC */
    M(Lu) | M(Ll) | M(Lt) | M(Lm) | M(Lo), /* L */
    M(Mn) | M(Mc) | M(Me), /* M */
    M(Nd) | M(Nl) | M(No), /* N */
    M(Sm) | M(Sc) | M(Sk) | M(So), /* S */
    M(Pc) | M(Pd) | M(Ps) | M(Pe) | M(Pi) | M(Pf) | M(Po), /* P */
    M(Zs) | M(Zl) | M(Zp), /* Z */
    M(Cc) | M(Cf) | M(Cs) | M(Co) | M(Cn), /* C */
};

/* 'cr' must be initialized and empty. Return 0 if OK, -1 if error, -2
   if not found */
int unicode_general_category(CharRange *cr, const char *gc_name)
{
    int gc_idx;
    uint32_t gc_mask;

    gc_idx = unicode_find_name(unicode_gc_name_table, gc_name);
    if (gc_idx < 0)
        return -2;
    if (gc_idx <= UNICODE_GC_Co) {
        gc_mask = (uint64_t)1 << gc_idx;
    } else {
        gc_mask = unicode_gc_mask_table[gc_idx - UNICODE_GC_LC];
    }
    return unicode_general_category1(cr, gc_mask);
}


/* 'cr' must be initialized and empty. Return 0 if OK, -1 if error, -2
   if not found */
int unicode_prop(CharRange *cr, const char *prop_name)
{
    int prop_idx, ret;

    prop_idx = unicode_find_name(unicode_prop_name_table, prop_name);
    if (prop_idx < 0)
        return -2;
    prop_idx += UNICODE_PROP_ASCII_Hex_Digit;

    ret = 0;
    switch(prop_idx) {
    case UNICODE_PROP_ASCII:
        if (cr_add_interval(cr, 0x00, 0x7f + 1))
            return -1;
        break;
    case UNICODE_PROP_Any:
        if (cr_add_interval(cr, 0x00000, 0x10ffff + 1))
            return -1;
        break;
    case UNICODE_PROP_Assigned:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Cn),
                               POP_INVERT,
                               POP_END);
        break;
    case UNICODE_PROP_Math:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Sm),
                               POP_PROP, UNICODE_PROP_Other_Math,
                               POP_UNION,
                               POP_END);
        break;
    case UNICODE_PROP_Lowercase:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Ll),
                               POP_PROP, UNICODE_PROP_Other_Lowercase,
                               POP_UNION,
                               POP_END);
        break;
    case UNICODE_PROP_Uppercase:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Lu),
                               POP_PROP, UNICODE_PROP_Other_Uppercase,
                               POP_UNION,
                               POP_END);
        break;
    case UNICODE_PROP_Cased:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Lu) | M(Ll) | M(Lt),
                               POP_PROP, UNICODE_PROP_Other_Uppercase,
                               POP_UNION,
                               POP_PROP, UNICODE_PROP_Other_Lowercase,
                               POP_UNION,
                               POP_END);
        break;
    case UNICODE_PROP_Alphabetic:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Lu) | M(Ll) | M(Lt) | M(Lm) | M(Lo) | M(Nl),
                               POP_PROP, UNICODE_PROP_Other_Uppercase,
                               POP_UNION,
                               POP_PROP, UNICODE_PROP_Other_Lowercase,
                               POP_UNION,
                               POP_PROP, UNICODE_PROP_Other_Alphabetic,
                               POP_UNION,
                               POP_END);
        break;
    case UNICODE_PROP_Grapheme_Base:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Cc) | M(Cf) | M(Cs) | M(Co) | M(Cn) | M(Zl) | M(Zp) | M(Me) | M(Mn),
                               POP_PROP, UNICODE_PROP_Other_Grapheme_Extend,
                               POP_UNION,
                               POP_INVERT,
                               POP_END);
        break;
    case UNICODE_PROP_Grapheme_Extend:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Me) | M(Mn),
                               POP_PROP, UNICODE_PROP_Other_Grapheme_Extend,
                               POP_UNION,
                               POP_END);
        break;
    case UNICODE_PROP_XID_Start:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Lu) | M(Ll) | M(Lt) | M(Lm) | M(Lo) | M(Nl),
                               POP_PROP, UNICODE_PROP_Other_ID_Start,
                               POP_UNION,
                               POP_PROP, UNICODE_PROP_Pattern_Syntax,
                               POP_PROP, UNICODE_PROP_Pattern_White_Space,
                               POP_UNION,
                               POP_PROP, UNICODE_PROP_XID_Start1,
                               POP_UNION,
                               POP_INVERT,
                               POP_INTER,
                               POP_END);
        break;
    case UNICODE_PROP_XID_Continue:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Lu) | M(Ll) | M(Lt) | M(Lm) | M(Lo) | M(Nl) |
                               M(Mn) | M(Mc) | M(Nd) | M(Pc),
                               POP_PROP, UNICODE_PROP_Other_ID_Start,
                               POP_UNION,
                               POP_PROP, UNICODE_PROP_Other_ID_Continue,
                               POP_UNION,
                               POP_PROP, UNICODE_PROP_Pattern_Syntax,
                               POP_PROP, UNICODE_PROP_Pattern_White_Space,
                               POP_UNION,
                               POP_PROP, UNICODE_PROP_XID_Continue1,
                               POP_UNION,
                               POP_INVERT,
                               POP_INTER,
                               POP_END);
        break;
    case UNICODE_PROP_Changes_When_Uppercased:
        ret = unicode_case1(cr, CASE_U);
        break;
    case UNICODE_PROP_Changes_When_Lowercased:
        ret = unicode_case1(cr, CASE_L);
        break;
    case UNICODE_PROP_Changes_When_Casemapped:
        ret = unicode_case1(cr, CASE_U | CASE_L | CASE_F);
        break;
    case UNICODE_PROP_Changes_When_Titlecased:
        ret = unicode_prop_ops(cr,
                               POP_CASE, CASE_U,
                               POP_PROP, UNICODE_PROP_Changes_When_Titlecased1,
                               POP_XOR,
                               POP_END);
        break;
    case UNICODE_PROP_Changes_When_Casefolded:
        ret = unicode_prop_ops(cr,
                               POP_CASE, CASE_F,
                               POP_PROP, UNICODE_PROP_Changes_When_Casefolded1,
                               POP_XOR,
                               POP_END);
        break;
    case UNICODE_PROP_Changes_When_NFKC_Casefolded:
        ret = unicode_prop_ops(cr,
                               POP_CASE, CASE_F,
                               POP_PROP, UNICODE_PROP_Changes_When_NFKC_Casefolded1,
                               POP_XOR,
                               POP_END);
        break;
#if 0
    case UNICODE_PROP_ID_Start:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Lu) | M(Ll) | M(Lt) | M(Lm) | M(Lo) | M(Nl),
                               POP_PROP, UNICODE_PROP_Other_ID_Start,
                               POP_UNION,
                               POP_PROP, UNICODE_PROP_Pattern_Syntax,
                               POP_PROP, UNICODE_PROP_Pattern_White_Space,
                               POP_UNION,
                               POP_INVERT,
                               POP_INTER,
                               POP_END);
        break;
    case UNICODE_PROP_ID_Continue:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Lu) | M(Ll) | M(Lt) | M(Lm) | M(Lo) | M(Nl) |
                               M(Mn) | M(Mc) | M(Nd) | M(Pc),
                               POP_PROP, UNICODE_PROP_Other_ID_Start,
                               POP_UNION,
                               POP_PROP, UNICODE_PROP_Other_ID_Continue,
                               POP_UNION,
                               POP_PROP, UNICODE_PROP_Pattern_Syntax,
                               POP_PROP, UNICODE_PROP_Pattern_White_Space,
                               POP_UNION,
                               POP_INVERT,
                               POP_INTER,
                               POP_END);
        break;
    case UNICODE_PROP_Case_Ignorable:
        ret = unicode_prop_ops(cr,
                               POP_GC, M(Mn) | M(Cf) | M(Lm) | M(Sk),
                               POP_PROP, UNICODE_PROP_Case_Ignorable1,
                               POP_XOR,
                               POP_END);
        break;
#else
        /* we use the existing tables */
    case UNICODE_PROP_ID_Continue:
        ret = unicode_prop_ops(cr,
                               POP_PROP, UNICODE_PROP_ID_Start,
                               POP_PROP, UNICODE_PROP_ID_Continue1,
                               POP_XOR,
                               POP_END);
        break;
#endif
    default:
        if (prop_idx >= countof(unicode_prop_table))
            return -2;
        ret = unicode_prop1(cr, prop_idx);
        break;
    }
    return ret;
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

#define SEQ_MAX_LEN 16

static int unicode_sequence_prop1(int seq_prop_idx, UnicodeSequencePropCB *cb, void *opaque,
                                  CharRange *cr)
{
    int i, c, j;
    uint32_t seq[SEQ_MAX_LEN];
    
    switch(seq_prop_idx) {
    case UNICODE_SEQUENCE_PROP_Basic_Emoji:
        if (unicode_prop1(cr, UNICODE_PROP_Basic_Emoji1) < 0)
            return -1;
        for(i = 0; i < cr->len; i += 2) {
            for(c = cr->points[i]; c < cr->points[i + 1]; c++) {
                seq[0] = c;
                cb(opaque, seq, 1);
            }
        }

        cr->len = 0;

        if (unicode_prop1(cr, UNICODE_PROP_Basic_Emoji2) < 0)
            return -1;
        for(i = 0; i < cr->len; i += 2) {
            for(c = cr->points[i]; c < cr->points[i + 1]; c++) {
                seq[0] = c;
                seq[1] = 0xfe0f;
                cb(opaque, seq, 2);
            }
        }

        break;
    case UNICODE_SEQUENCE_PROP_RGI_Emoji_Modifier_Sequence:
        if (unicode_prop1(cr, UNICODE_PROP_Emoji_Modifier_Base) < 0)
            return -1;
        for(i = 0; i < cr->len; i += 2) {
            for(c = cr->points[i]; c < cr->points[i + 1]; c++) {
                for(j = 0; j < 5; j++) {
                    seq[0] = c;
                    seq[1] = 0x1f3fb + j;
                    cb(opaque, seq, 2);
                }
            }
        }
        break;
    case UNICODE_SEQUENCE_PROP_RGI_Emoji_Flag_Sequence:
        if (unicode_prop1(cr, UNICODE_PROP_RGI_Emoji_Flag_Sequence) < 0)
            return -1;
        for(i = 0; i < cr->len; i += 2) {
            for(c = cr->points[i]; c < cr->points[i + 1]; c++) {
                int c0, c1;
                c0 = c / 26;
                c1 = c % 26;
                seq[0] = 0x1F1E6 + c0;
                seq[1] = 0x1F1E6 + c1;
                cb(opaque, seq, 2);
            }
        }
        break;
    case UNICODE_SEQUENCE_PROP_RGI_Emoji_ZWJ_Sequence:
        {
            int len, code, pres, k, mod, mod_count, mod_pos[2], hc_pos, n_mod, n_hc, mod1;
            int mod_idx, hc_idx, i0, i1;
            const uint8_t *tab = unicode_rgi_emoji_zwj_sequence;
            
            for(i = 0; i < countof(unicode_rgi_emoji_zwj_sequence);) {
                len = tab[i++];
                k = 0;
                mod = 0;
                mod_count = 0;
                hc_pos = -1;
                for(j = 0; j < len; j++) {
                    code = tab[i++];
                    code |= tab[i++] << 8;
                    pres = code >> 15;
                    mod1 = (code >> 13) & 3;
                    code &= 0x1fff;
                    if (code < 0x1000) {
                        c = code + 0x2000;
                    } else {
                        c = 0x1f000 + (code - 0x1000);
                    }
                    if (c == 0x1f9b0)
                        hc_pos = k;
                    seq[k++] = c;
                    if (mod1 != 0) {
                        assert(mod_count < 2);
                        mod = mod1;
                        mod_pos[mod_count++] = k;
                        seq[k++] = 0; /* will be filled later */
                    }
                    if (pres) {
                        seq[k++] = 0xfe0f;
                    }
                    if (j < len - 1) {
                        seq[k++] = 0x200d;
                    }
                }

                /* genrate all the variants */
                switch(mod) {
                case 1:
                    n_mod = 5;
                    break;
                case 2:
                    n_mod = 25;
                    break;
                case 3:
                    n_mod = 20;
                    break;
                default:
                    n_mod = 1;
                    break;
                }
                if (hc_pos >= 0)
                    n_hc = 4;
                else
                    n_hc = 1;
                for(hc_idx = 0; hc_idx < n_hc; hc_idx++) {
                    for(mod_idx = 0; mod_idx < n_mod; mod_idx++) {
                        if (hc_pos >= 0)
                            seq[hc_pos] = 0x1f9b0 + hc_idx;
                        
                        switch(mod) {
                        case 1:
                            seq[mod_pos[0]] = 0x1f3fb + mod_idx;
                            break;
                        case 2:
                        case 3:
                            i0 = mod_idx / 5;
                            i1 = mod_idx % 5;
                            /* avoid identical values */
                            if (mod == 3 && i0 >= i1)
                                i0++;
                            seq[mod_pos[0]] = 0x1f3fb + i0;
                            seq[mod_pos[1]] = 0x1f3fb + i1;
                            break;
                        default:
                            break;
                        }
#if 0
                        for(j = 0; j < k; j++)
                            printf(" %04x", seq[j]);
                        printf("\n");
#endif                
                        cb(opaque, seq, k);
                    }
                }
            }
        }
        break;
    case UNICODE_SEQUENCE_PROP_RGI_Emoji_Tag_Sequence:
        {
            for(i = 0; i < countof(unicode_rgi_emoji_tag_sequence);) {
                j = 0;
                seq[j++] = 0x1F3F4;
                for(;;) {
                    c = unicode_rgi_emoji_tag_sequence[i++];
                    if (c == 0x00)
                        break;
                    seq[j++] = 0xe0000 + c;
                }
                seq[j++] = 0xe007f;
                cb(opaque, seq, j);
            }
        }
        break;
    case UNICODE_SEQUENCE_PROP_Emoji_Keycap_Sequence:
        if (unicode_prop1(cr, UNICODE_PROP_Emoji_Keycap_Sequence) < 0)
            return -1;
        for(i = 0; i < cr->len; i += 2) {
            for(c = cr->points[i]; c < cr->points[i + 1]; c++) {
                seq[0] = c;
                seq[1] = 0xfe0f;
                seq[2] = 0x20e3;
                cb(opaque, seq, 3);
            }
        }
        break;
    case UNICODE_SEQUENCE_PROP_RGI_Emoji:
        /* all prevous sequences */
        for(i = UNICODE_SEQUENCE_PROP_Basic_Emoji; i <= UNICODE_SEQUENCE_PROP_RGI_Emoji_ZWJ_Sequence; i++) {
            int ret;
            ret = unicode_sequence_prop1(i, cb, opaque, cr);
            if (ret < 0)
                return ret;
            cr->len = 0;
        }
        break;
    default:
        return -2;
    }
    return 0;
}

/* build a unicode sequence property */
/* return -2 if not found, -1 if other error. 'cr' is used as temporary memory. */
int unicode_sequence_prop(const char *prop_name, UnicodeSequencePropCB *cb, void *opaque,
                          CharRange *cr)
{
    int seq_prop_idx;
    seq_prop_idx = unicode_find_name(unicode_sequence_prop_name_table, prop_name);
    if (seq_prop_idx < 0)
        return -2;
    return unicode_sequence_prop1(seq_prop_idx, cb, opaque, cr);
}
