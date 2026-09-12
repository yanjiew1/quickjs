#include "unicode_gen_case.h"

BOOL is_complicated_case(const CCInfo *ci)
{
    return (ci->u_len > 1 || ci->l_len > 1 ||
            (ci->u_len > 0 && ci->l_len > 0) ||
            (ci->f_len != ci->l_len) ||
            (memcmp(ci->f_data, ci->l_data, ci->f_len * sizeof(ci->f_data[0])) != 0));
}

#ifndef USE_TEST
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
#endif

const char *run_type_str[] = {
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


static int simple_to_lower(CCInfo *tab, int c)
{
    if (tab[c].l_len != 1)
        return c;
    return tab[c].l_data[0];
}

/* code (17), len (7), type (4) */

void find_run_type(TableEntry *te, CCInfo *tab, int code)
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
            dump_cc_info(ci, code);
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

TableEntry conv_table[1000];
int conv_table_len;
int ext_data[1000];
int ext_data_len;

void dump_case_conv_table1(void)
{
    int i, j;
    const TableEntry *te;

    for(i = 0; i < conv_table_len; i++) {
        te = &conv_table[i];
        printf("%05x %02x %-10s %05x",
               te->code, te->len, run_type_str[te->type], te->data);
        for(j = 0; j < te->ext_len; j++) {
            printf(" %05x", te->ext_data[j]);
        }
        printf("\n");
    }
    printf("table_len=%d ext_len=%d\n", conv_table_len, ext_data_len);
}

int find_data_index(const TableEntry *conv_table, int len, int data)
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

int find_ext_data_index(int data)
{
    int i;
    for(i = 0; i < ext_data_len; i++) {
        if (ext_data[i] == data)
            return i;
    }
    assert(ext_data_len < countof(ext_data));
    ext_data[ext_data_len++] = data;
    return ext_data_len - 1;
}

void build_conv_table(CCInfo *tab)
{
    int code, i, j;
    CCInfo *ci;
    TableEntry *te;

    te = conv_table;
    for(code = 0; code <= CHARCODE_MAX; code++) {
        ci = &tab[code];
        if (ci->u_len == 0 && ci->l_len == 0 && ci->f_len == 0)
            continue;
        assert(te - conv_table < countof(conv_table));
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
    conv_table_len = te - conv_table;

    /* find the data index */
    for(i = 0; i < conv_table_len; i++) {
        int data_index;
        te = &conv_table[i];

        switch(te->type) {
        case RUN_TYPE_U:
        case RUN_TYPE_L:
        case RUN_TYPE_UF:
        case RUN_TYPE_LF:
            data_index = find_data_index(conv_table, conv_table_len, te->data);
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
    for(i = 0; i < conv_table_len; i++) {
        te = &conv_table[i];
        if (te->type == RUN_TYPE_UF_EXT3) {
            int p, v;
            v = 0;
            for(j = 0; j < 3; j++) {
                p = find_ext_data_index(te->ext_data[j]);
                assert(p < 16);
                v = (v << 4) | p;
            }
            te->data_index = v;
        }
    }

    for(i = 0; i < conv_table_len; i++) {
        te = &conv_table[i];
        if (te->type == RUN_TYPE_LF_EXT2 ||
            te->type == RUN_TYPE_UF_EXT2 ||
            te->type == RUN_TYPE_U2L_399_EXT2) {
            int p, v;
            v = 0;
            for(j = 0; j < 2; j++) {
                p = find_ext_data_index(te->ext_data[j]);
                assert(p < 64);
                v = (v << 6) | p;
            }
            te->data_index = v;
        }
    }

    for(i = 0; i < conv_table_len; i++) {
        te = &conv_table[i];
        if (te->type == RUN_TYPE_UF_D1_EXT ||
            te->type == RUN_TYPE_U_EXT ||
            te->type == RUN_TYPE_LF_EXT) {
            te->data_index = find_ext_data_index(te->ext_data[0]);
        }
    }
#ifdef DUMP_CASE_CONV_TABLE
    dump_case_conv_table1();
#endif
}

void dump_case_conv_table(FILE *f)
{
    int i;
    uint32_t v;
    const TableEntry *te;

    total_tables++;
    total_table_bytes += conv_table_len * sizeof(uint32_t);
    fprintf(f, "static const uint32_t case_conv_table1[%d] = {", conv_table_len);
    for(i = 0; i < conv_table_len; i++) {
        if (i % 4 == 0)
            fprintf(f, "\n   ");
        te = &conv_table[i];
        v = te->code << (32 - 17);
        v |= te->len << (32 - 17 - 7);
        v |= te->type << (32 - 17 - 7 - 4);
        v |= te->data_index >> 8;
        fprintf(f, " 0x%08x,", v);
    }
    fprintf(f, "\n};\n\n");

    total_tables++;
    total_table_bytes += conv_table_len;
    fprintf(f, "static const uint8_t case_conv_table2[%d] = {", conv_table_len);
    for(i = 0; i < conv_table_len; i++) {
        if (i % 8 == 0)
            fprintf(f, "\n   ");
        te = &conv_table[i];
        fprintf(f, " 0x%02x,", te->data_index & 0xff);
    }
    fprintf(f, "\n};\n\n");

    total_tables++;
    total_table_bytes += ext_data_len * sizeof(uint16_t);
    fprintf(f, "static const uint16_t case_conv_ext[%d] = {", ext_data_len);
    for(i = 0; i < ext_data_len; i++) {
        if (i % 8 == 0)
            fprintf(f, "\n   ");
        fprintf(f, " 0x%04x,", ext_data[i]);
    }
    fprintf(f, "\n};\n\n");
}


static CCInfo *global_tab;

static int sp_cc_cmp(const void *p1, const void *p2)
{
    CCInfo *c1 = &global_tab[*(const int *)p1];
    CCInfo *c2 = &global_tab[*(const int *)p2];
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
void dump_case_folding_special_cases(CCInfo *tab)
{
    int i, len, j;
    int *perm;

    perm = malloc(sizeof(perm[0]) * (CHARCODE_MAX + 1));
    for(i = 0; i <= CHARCODE_MAX; i++)
        perm[i] = i;
    global_tab = tab;
    qsort(perm, CHARCODE_MAX + 1, sizeof(perm[0]), sp_cc_cmp);
    for(i = 0; i <= CHARCODE_MAX;) {
        if (tab[perm[i]].f_len <= 1) {
            i++;
        } else {
            len = 1;
            while ((i + len) <= CHARCODE_MAX && !sp_cc_cmp(&perm[i], &perm[i + len]))
                len++;

            if (len > 1) {
                for(j = i; j < i + len; j++)
                    dump_cc_info(&tab[perm[j]], perm[j]);
            }
            i += len;
        }
    }
    free(perm);
    global_tab = NULL;
}

#ifdef USE_TEST
int check_conv(uint32_t *res, uint32_t c, int conv_type)
{
    return lre_case_conv(res, c, conv_type);
}

void check_case_conv(void)
{
    CCInfo *tab = unicode_db;
    uint32_t res[3];
    int l, error;
    CCInfo ci_s, *ci1, *ci = &ci_s;
    int code;

    for(code = 0; code <= CHARCODE_MAX; code++) {
        ci1 = &tab[code];
        *ci = *ci1;
        if (ci->l_len == 0) {
            ci->l_len = 1;
            ci->l_data[0] = code;
        }
        if (ci->u_len == 0) {
            ci->u_len = 1;
            ci->u_data[0] = code;
        }
        if (ci->f_len == 0) {
            ci->f_len = 1;
            ci->f_data[0] = code;
        }

        error = 0;
        l = check_conv(res, code, 0);
        if (l != ci->u_len || tabcmp((int *)res, ci->u_data, l)) {
            printf("ERROR: L\n");
            error++;
        }
        l = check_conv(res, code, 1);
        if (l != ci->l_len || tabcmp((int *)res, ci->l_data, l)) {
            printf("ERROR: U\n");
            error++;
        }
        l = check_conv(res, code, 2);
        if (l != ci->f_len || tabcmp((int *)res, ci->f_data, l)) {
            printf("ERROR: F\n");
            error++;
        }
        if (error) {
            dump_cc_info(ci, code);
            exit(1);
        }
    }
}
#endif /* USE_TEST */
