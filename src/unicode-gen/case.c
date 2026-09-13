/*
 * Generation of Unicode tables
 *
 * Copyright (c) 2017-2018 Fabrice Bellard
 * Copyright (c) 2017-2018 Charlie Gordon
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
#include <string.h>
#include <assert.h>

#include "internal.h"

static BOOL is_complicated_case(const CCInfo *ci)
{
    return (ci->u_len > 1 || ci->l_len > 1 ||
            (ci->u_len > 0 && ci->l_len > 0) ||
            (ci->f_len != ci->l_len) ||
            (memcmp(ci->f_data, ci->l_data, ci->f_len * sizeof(ci->f_data[0])) != 0));
}

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

static const char *const run_type_str[] = {
    "U",
    "L",
    "UF",
    "LF",
    "UL",
    "LSU",
    "U2L_399_EXT2",
    "UF_D20",
    "UF_D1_EXT",
    "U_EXT",
    "LF_EXT",
    "UF_EXT2",
    "LF_EXT2",
    "UF_EXT3",
};

typedef struct {
    int code;
    int len;
    int type;
    int data;
    int ext_len;
    int ext_data[3];
    int data_index; /* 'data' coming from the table */
} TableEntry;

static int simple_to_lower(CCInfo *tab, int c)
{
    if (tab[c].l_len != 1)
        return c;
    return tab[c].l_data[0];
}

/* code (17), len (7), type (4) */

static void find_run_type(TableEntry *te, CCInfo *tab, int code)
{
    int is_lower, len;
    CCInfo *ci, *ci1, *ci2;

    ci = &tab[code];
    ci1 = &tab[code + 1];
    ci2 = &tab[code + 2];
    te->code = code;

    if (ci->l_len == 1 && ci->l_data[0] == code + 2 &&
        ci->f_len == 1 && ci->f_data[0] == ci->l_data[0] &&
        ci->u_len == 0 &&

        ci1->l_len == 1 && ci1->l_data[0] == code + 2 &&
        ci1->f_len == 1 && ci1->f_data[0] == ci1->l_data[0] &&
        ci1->u_len == 1 && ci1->u_data[0] == code &&

        ci2->l_len == 0 &&
        ci2->f_len == 0 &&
        ci2->u_len == 1 && ci2->u_data[0] == code) {
        te->len = 3;
        te->data = 0;
        te->type = RUN_TYPE_LSU;
        return;
    }

    if (is_complicated_case(ci)) {
        len = 1;
        while (code + len <= CHARCODE_MAX) {
            ci1 = &tab[code + len];
            if (ci1->u_len != 1 ||
                ci1->u_data[0] != ci->u_data[0] + len ||
                ci1->l_len != 0 ||
                ci1->f_len != 1 || ci1->f_data[0] != ci1->u_data[0])
                break;
            len++;
        }
        if (len > 1) {
            te->len = len;
            te->type = RUN_TYPE_UF;
            te->data = ci->u_data[0];
            return;
        }

        if (ci->l_len == 0 &&
            ci->u_len == 2 && ci->u_data[1] == 0x399 &&
            ci->f_len == 2 && ci->f_data[1] == 0x3B9 &&
            ci->f_data[0] == simple_to_lower(tab, ci->u_data[0])) {
            len = 1;
            while (code + len <= CHARCODE_MAX) {
                ci1 = &tab[code + len];
                if (!(ci1->u_len == 2 &&
                      ci1->u_data[1] == ci->u_data[1] &&
                      ci1->u_data[0] == ci->u_data[0] + len &&
                      ci1->f_len == 2 &&
                      ci1->f_data[1] == ci->f_data[1] &&
                      ci1->f_data[0] == ci->f_data[0] + len &&
                      ci1->l_len == 0))
                    break;
                len++;
            }
            te->len = len;
            te->type = RUN_TYPE_UF_EXT2;
            te->ext_data[0] = ci->u_data[0];
            te->ext_data[1] = ci->u_data[1];
            te->ext_len = 2;
            return;
        }

        if (ci->u_len == 2 && ci->u_data[1] == 0x399 &&
            ci->l_len == 1 &&
            ci->f_len == 1 && ci->f_data[0] == ci->l_data[0]) {
            len = 1;
            while (code + len <= CHARCODE_MAX) {
                ci1 = &tab[code + len];
                if (!(ci1->u_len == 2 &&
                      ci1->u_data[1] == 0x399 &&
                      ci1->u_data[0] == ci->u_data[0] + len &&
                      ci1->l_len == 1 &&
                      ci1->l_data[0] == ci->l_data[0] + len &&
                      ci1->f_len == 1 && ci1->f_data[0] == ci1->l_data[0]))
                    break;
                len++;
            }
            te->len = len;
            te->type = RUN_TYPE_U2L_399_EXT2;
            te->ext_data[0] = ci->u_data[0];
            te->ext_data[1] = ci->l_data[0];
            te->ext_len = 2;
            return;
        }

        if (ci->l_len == 1 && ci->u_len == 0 && ci->f_len == 0) {
            len = 1;
            while (code + len <= CHARCODE_MAX) {
                ci1 = &tab[code + len];
                if (!(ci1->l_len == 1 &&
                      ci1->l_data[0] == ci->l_data[0] + len &&
                      ci1->u_len == 0 && ci1->f_len == 0))
                    break;
                len++;
            }
            te->len = len;
            te->type = RUN_TYPE_L;
            te->data = ci->l_data[0];
            return;
        }

        if (ci->l_len == 0 &&
            ci->u_len == 1 &&
            ci->u_data[0] < 0x1000 &&
            ci->f_len == 1 && ci->f_data[0] == ci->u_data[0] + 0x20) {
            te->len = 1;
            te->type = RUN_TYPE_UF_D20;
            te->data = ci->u_data[0];
        } else if (ci->l_len == 0 &&
                   ci->u_len == 1 &&
                   ci->f_len == 1 && ci->f_data[0] == ci->u_data[0] + 1) {
            te->len = 1;
            te->type = RUN_TYPE_UF_D1_EXT;
            te->ext_data[0] = ci->u_data[0];
            te->ext_len = 1;
        } else if (ci->l_len == 2 && ci->u_len == 0 && ci->f_len == 2 &&
                   ci->l_data[0] == ci->f_data[0] &&
                   ci->l_data[1] == ci->f_data[1]) {
            te->len = 1;
            te->type = RUN_TYPE_LF_EXT2;
            te->ext_data[0] = ci->l_data[0];
            te->ext_data[1] = ci->l_data[1];
            te->ext_len = 2;
        } else if (ci->u_len == 2 && ci->l_len == 0 && ci->f_len == 2 &&
                   ci->f_data[0] == simple_to_lower(tab, ci->u_data[0]) &&
                   ci->f_data[1] == simple_to_lower(tab, ci->u_data[1])) {
            te->len = 1;
            te->type = RUN_TYPE_UF_EXT2;
            te->ext_data[0] = ci->u_data[0];
            te->ext_data[1] = ci->u_data[1];
            te->ext_len = 2;
        } else if (ci->u_len == 3 && ci->l_len == 0 && ci->f_len == 3 &&
                   ci->f_data[0] == simple_to_lower(tab, ci->u_data[0]) &&
                   ci->f_data[1] == simple_to_lower(tab, ci->u_data[1]) &&
                   ci->f_data[2] == simple_to_lower(tab, ci->u_data[2])) {
            te->len = 1;
            te->type = RUN_TYPE_UF_EXT3;
            te->ext_data[0] = ci->u_data[0];
            te->ext_data[1] = ci->u_data[1];
            te->ext_data[2] = ci->u_data[2];
            te->ext_len = 3;
        } else if (ci->u_len == 2 && ci->l_len == 0 && ci->f_len == 1) {
            // U+FB05 LATIN SMALL LIGATURE LONG S T
            assert(code == 0xFB05);
            te->len = 1;
            te->type = RUN_TYPE_UF_EXT2;
            te->ext_data[0] = ci->u_data[0];
            te->ext_data[1] = ci->u_data[1];
            te->ext_len = 2;
        } else if (ci->u_len == 3 && ci->l_len == 0 && ci->f_len == 1) {
            // U+1FD3 GREEK SMALL LETTER IOTA WITH DIALYTIKA AND OXIA or
            // U+1FE3 GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND OXIA
            assert(code == 0x1FD3 || code == 0x1FE3);
            te->len = 1;
            te->type = RUN_TYPE_UF_EXT3;
            te->ext_data[0] = ci->u_data[0];
            te->ext_data[1] = ci->u_data[1];
            te->ext_data[2] = ci->u_data[2];
            te->ext_len = 3;
        } else {
            printf("unsupported encoding case:\n");
            unicode_gen_dump_cc_info(ci, code);
            abort();
        }
    } else {
        /* look for a run of identical conversions */
        len = 0;
        for(;;) {
            if (code >= CHARCODE_MAX || len >= 126)
                break;
            ci = &tab[code + len];
            ci1 = &tab[code + len + 1];
            if (is_complicated_case(ci) || is_complicated_case(ci1)) {
                break;
            }
            if (ci->l_len != 1 || ci->l_data[0] != code + len + 1)
                break;
            if (ci1->u_len != 1 || ci1->u_data[0] != code + len)
                break;
            len += 2;
        }
        if (len > 0) {
            te->len = len;
            te->type = RUN_TYPE_UL;
            te->data = 0;
            return;
        }

        ci = &tab[code];
        is_lower = ci->l_len > 0;
        len = 1;
        while (code + len <= CHARCODE_MAX) {
            ci1 = &tab[code + len];
            if (is_complicated_case(ci1))
                break;
            if (is_lower) {
                if (ci1->l_len != 1 ||
                    ci1->l_data[0] != ci->l_data[0] + len)
                    break;
            } else {
                if (ci1->u_len != 1 ||
                    ci1->u_data[0] != ci->u_data[0] + len)
                    break;
            }
            len++;
        }
        te->len = len;
        if (is_lower) {
            te->type = RUN_TYPE_LF;
            te->data = ci->l_data[0];
        } else {
            te->type = RUN_TYPE_U;
            te->data = ci->u_data[0];
        }
    }
}

typedef struct {
    TableEntry conv_table[1000];
    int conv_table_len;
    int ext_data[1000];
    int ext_data_len;
} UnicodeGenCaseState;

static __maybe_unused void dump_case_conv_table1(
    const UnicodeGenCaseState *cs)
{
    int i, j;
    const TableEntry *te;

    for(i = 0; i < cs->conv_table_len; i++) {
        te = &cs->conv_table[i];
        printf("%05x %02x %-10s %05x",
               te->code, te->len, run_type_str[te->type], te->data);
        for(j = 0; j < te->ext_len; j++) {
            printf(" %05x", te->ext_data[j]);
        }
        printf("\n");
    }
    printf("table_len=%d ext_len=%d\n",
           cs->conv_table_len, cs->ext_data_len);
}

static int find_data_index(const TableEntry *conv_table, int len, int data)
{
    int i;
    const TableEntry *te;
    for(i = 0; i < len; i++) {
        te = &conv_table[i];
        if (te->code == data)
            return i;
    }
    return -1;
}

static int find_ext_data_index(UnicodeGenCaseState *cs, int data)
{
    int i;
    for(i = 0; i < cs->ext_data_len; i++) {
        if (cs->ext_data[i] == data)
            return i;
    }
    assert(cs->ext_data_len < countof(cs->ext_data));
    cs->ext_data[cs->ext_data_len++] = data;
    return cs->ext_data_len - 1;
}

static void build_conv_table(UnicodeGenCaseState *cs, CCInfo *tab)
{
    int code, i, j;
    CCInfo *ci;
    TableEntry *te;

    te = cs->conv_table;
    for(code = 0; code <= CHARCODE_MAX; code++) {
        ci = &tab[code];
        if (ci->u_len == 0 && ci->l_len == 0 && ci->f_len == 0)
            continue;
        assert(te - cs->conv_table < countof(cs->conv_table));
        find_run_type(te, tab, code);
#if 0
        if (te->type == RUN_TYPE_TODO) {
            printf("TODO: ");
            dump_cc_info(ci, code);
        }
#endif
        assert(te->len <= 127);
        code += te->len - 1;
        te++;
    }
    cs->conv_table_len = te - cs->conv_table;

    /* find the data index */
    for(i = 0; i < cs->conv_table_len; i++) {
        int data_index;
        te = &cs->conv_table[i];

        switch(te->type) {
        case RUN_TYPE_U:
        case RUN_TYPE_L:
        case RUN_TYPE_UF:
        case RUN_TYPE_LF:
            data_index = find_data_index(cs->conv_table,
                                         cs->conv_table_len, te->data);
            if (data_index < 0) {
                switch(te->type) {
                case RUN_TYPE_U:
                    te->type = RUN_TYPE_U_EXT;
                    te->ext_len = 1;
                    te->ext_data[0] = te->data;
                    break;
                case RUN_TYPE_LF:
                    te->type = RUN_TYPE_LF_EXT;
                    te->ext_len = 1;
                    te->ext_data[0] = te->data;
                    break;
                default:
                    printf("%05x: index not found\n", te->code);
                    exit(1);
                }
            } else {
                te->data_index = data_index;
            }
            break;
        case RUN_TYPE_UF_D20:
            te->data_index = te->data;
            break;
        }
    }

    /* find the data index for ext_data */
    for(i = 0; i < cs->conv_table_len; i++) {
        te = &cs->conv_table[i];
        if (te->type == RUN_TYPE_UF_EXT3) {
            int p, v;
            v = 0;
            for(j = 0; j < 3; j++) {
                p = find_ext_data_index(cs, te->ext_data[j]);
                assert(p < 16);
                v = (v << 4) | p;
            }
            te->data_index = v;
        }
    }

    for(i = 0; i < cs->conv_table_len; i++) {
        te = &cs->conv_table[i];
        if (te->type == RUN_TYPE_LF_EXT2 ||
            te->type == RUN_TYPE_UF_EXT2 ||
            te->type == RUN_TYPE_U2L_399_EXT2) {
            int p, v;
            v = 0;
            for(j = 0; j < 2; j++) {
                p = find_ext_data_index(cs, te->ext_data[j]);
                assert(p < 64);
                v = (v << 6) | p;
            }
            te->data_index = v;
        }
    }

    for(i = 0; i < cs->conv_table_len; i++) {
        te = &cs->conv_table[i];
        if (te->type == RUN_TYPE_UF_D1_EXT ||
            te->type == RUN_TYPE_U_EXT ||
            te->type == RUN_TYPE_LF_EXT) {
            te->data_index = find_ext_data_index(cs, te->ext_data[0]);
        }
    }
#ifdef DUMP_CASE_CONV_TABLE
    dump_case_conv_table1(cs);
#endif
}

static void dump_case_conv_table(UnicodeGenOutput *out,
                                 const UnicodeGenCaseState *cs)
{
    int i;
    uint32_t v;
    const TableEntry *te;
    FILE *f = out->file;

    out->total_tables++;
    out->total_table_bytes += cs->conv_table_len * sizeof(uint32_t);
    fprintf(f, "static const uint32_t case_conv_table1[%d] = {",
            cs->conv_table_len);
    for(i = 0; i < cs->conv_table_len; i++) {
        if (i % 4 == 0)
            fprintf(f, "\n   ");
        te = &cs->conv_table[i];
        v = te->code << (32 - 17);
        v |= te->len << (32 - 17 - 7);
        v |= te->type << (32 - 17 - 7 - 4);
        v |= te->data_index >> 8;
        fprintf(f, " 0x%08x,", v);
    }
    fprintf(f, "\n};\n\n");

    out->total_tables++;
    out->total_table_bytes += cs->conv_table_len;
    fprintf(f, "static const uint8_t case_conv_table2[%d] = {",
            cs->conv_table_len);
    for(i = 0; i < cs->conv_table_len; i++) {
        if (i % 8 == 0)
            fprintf(f, "\n   ");
        te = &cs->conv_table[i];
        fprintf(f, " 0x%02x,", te->data_index & 0xff);
    }
    fprintf(f, "\n};\n\n");

    out->total_tables++;
    out->total_table_bytes += cs->ext_data_len * sizeof(uint16_t);
    fprintf(f, "static const uint16_t case_conv_ext[%d] = {",
            cs->ext_data_len);
    for(i = 0; i < cs->ext_data_len; i++) {
        if (i % 8 == 0)
            fprintf(f, "\n   ");
        fprintf(f, " 0x%04x,", cs->ext_data[i]);
    }
    fprintf(f, "\n};\n\n");
}


typedef struct {
    const CCInfo *ci;
    int code;
} CaseFoldEntry;

static int sp_cc_cmp(const void *p1, const void *p2)
{
    const CaseFoldEntry *e1 = p1;
    const CaseFoldEntry *e2 = p2;
    const CCInfo *c1 = e1->ci;
    const CCInfo *c2 = e2->ci;
    if (c1->f_len < c2->f_len) {
        return -1;
    } else if (c2->f_len < c1->f_len) {
        return 1;
    } else {
        return memcmp(c1->f_data, c2->f_data, sizeof(c1->f_data[0]) * c1->f_len);
    }
}

/* dump the case special cases (multi character results which are
   identical and need specific handling in lre_canonicalize() */
static __maybe_unused void dump_case_folding_special_cases(CCInfo *tab)
{
    int i, len, j;
    CaseFoldEntry *entries;

    entries = malloc(sizeof(entries[0]) * (CHARCODE_MAX + 1));
    for(i = 0; i <= CHARCODE_MAX; i++) {
        entries[i].ci = &tab[i];
        entries[i].code = i;
    }
    qsort(entries, CHARCODE_MAX + 1, sizeof(entries[0]), sp_cc_cmp);
    for(i = 0; i <= CHARCODE_MAX;) {
        if (entries[i].ci->f_len <= 1) {
            i++;
        } else {
            len = 1;
            while ((i + len) <= CHARCODE_MAX &&
                   !sp_cc_cmp(&entries[i], &entries[i + len]))
                len++;

            if (len > 1) {
                for(j = i; j < i + len; j++)
                    unicode_gen_dump_cc_info(entries[j].ci,
                                             entries[j].code);
            }
            i += len;
        }
    }
    free(entries);
}

void unicode_gen_emit_case(UnicodeGenOutput *out, UnicodeGenState *s)
{
    UnicodeGenCaseState case_state = { 0 };

    build_conv_table(&case_state, s->db);
#ifdef DUMP_CASE_FOLDING_SPECIAL_CASES
    dump_case_folding_special_cases(s->db);
#endif
    dump_case_conv_table(out, &case_state);
}

#ifdef CONFIG_UNICODE_TEST
void unicode_gen_check_case_tables(UnicodeGenState *s)
{
    UnicodeGenCaseState case_state = { 0 };

    build_conv_table(&case_state, s->db);
#ifdef DUMP_CASE_FOLDING_SPECIAL_CASES
    dump_case_folding_special_cases(s->db);
#endif
}
#endif
