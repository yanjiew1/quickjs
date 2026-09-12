#include "unicode_gen_norm.h"

#define CC_BLOCK_LEN 32

void build_cc_table(FILE *f)
{
    // Compress combining class table
    // see: https://www.unicode.org/reports/tr44/#Canonical_Combining_Class_Values
    int i, cc, n, type, n1, block_end_pos;
    DynBuf dbuf_s, *dbuf = &dbuf_s;
    DynBuf dbuf1_s, *dbuf1 = &dbuf1_s;
#if defined(DUMP_CC_TABLE) || defined(DUMP_TABLE_SIZE)
    int cw_len_tab[3], cw_start, cc_table_len;
#endif
    uint32_t v;

    dbuf_init(dbuf);
    dbuf_init(dbuf1);
#if defined(DUMP_CC_TABLE) || defined(DUMP_TABLE_SIZE)
    cc_table_len = 0;
    for(i = 0; i < countof(cw_len_tab); i++)
        cw_len_tab[i] = 0;
#endif
    block_end_pos = CC_BLOCK_LEN;
    for(i = 0; i <= CHARCODE_MAX;) {
        cc = unicode_db[i].combining_class;
        assert(cc <= 255);
        /* check increasing values */
        n = 1;
        while ((i + n) <= CHARCODE_MAX &&
               unicode_db[i + n].combining_class == (cc + n))
            n++;
        if (n >= 2) {
            type = 1;
        } else {
            type = 0;
            n = 1;
            while ((i + n) <= CHARCODE_MAX &&
                   unicode_db[i + n].combining_class == cc)
                n++;
        }
        /* no need to encode the last run */
        if (cc == 0 && (i + n - 1) == CHARCODE_MAX)
            break;
#ifdef DUMP_CC_TABLE
        printf("%05x %6d %d %d\n", i, n, type, cc);
#endif
        if (type == 0) {
            if (cc == 0)
                type = 2;
            else if (cc == 230)
                type = 3;
        }
        n1 = n - 1;

        /* add an entry to the index if necessary */
        if (dbuf->size >= block_end_pos) {
            v = i | ((dbuf->size - block_end_pos) << 21);
            dbuf_putc(dbuf1, v);
            dbuf_putc(dbuf1, v >> 8);
            dbuf_putc(dbuf1, v >> 16);
            block_end_pos += CC_BLOCK_LEN;
        }
#if defined(DUMP_CC_TABLE) || defined(DUMP_TABLE_SIZE)
        cw_start = dbuf->size;
#endif
        /* Compressed run length encoding:
           - 2 high order bits are combining class type
           -         0:0, 1:230, 2:extra byte linear progression, 3:extra byte
           - 00..2F: range length (add 1)
           - 30..37: 3-bit range-length + 1 extra byte
           - 38..3F: 3-bit range-length + 2 extra byte
         */
        if (n1 < 48) {
            dbuf_putc(dbuf, n1 | (type << 6));
        } else if (n1 < 48 + (1 << 11)) {
            n1 -= 48;
            dbuf_putc(dbuf, ((n1 >> 8) + 48) | (type << 6));
            dbuf_putc(dbuf, n1);
        } else {
            n1 -= 48 + (1 << 11);
            assert(n1 < (1 << 20));
            dbuf_putc(dbuf, ((n1 >> 16) + 56) | (type << 6));
            dbuf_putc(dbuf, n1 >> 8);
            dbuf_putc(dbuf, n1);
        }
#if defined(DUMP_CC_TABLE) || defined(DUMP_TABLE_SIZE)
        cw_len_tab[dbuf->size - cw_start - 1]++;
        cc_table_len++;
#endif
        if (type == 0 || type == 1)
            dbuf_putc(dbuf, cc);
        i += n;
    }

    /* last index entry */
    v = i;
    dbuf_putc(dbuf1, v);
    dbuf_putc(dbuf1, v >> 8);
    dbuf_putc(dbuf1, v >> 16);

    dump_byte_table(f, "unicode_cc_table", dbuf->buf, dbuf->size);
    dump_index_table(f, "unicode_cc_index", dbuf1->buf, dbuf1->size);

#if defined(DUMP_CC_TABLE) || defined(DUMP_TABLE_SIZE)
    printf("CC table: size=%d (%d entries) [",
           (int)(dbuf->size + dbuf1->size),
           cc_table_len);
    for(i = 0; i < countof(cw_len_tab); i++)
        printf(" %d", cw_len_tab[i]);
    printf(" ]\n");
#endif
    dbuf_free(dbuf);
    dbuf_free(dbuf1);
}

/* maximum length of decomposition: 18 chars (1), then 8 */
#ifndef USE_TEST
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
#endif

const char *decomp_type_str[] = {
    "C1",
    "L1",
    "L2",
    "L3",
    "L4",
    "L5",
    "L6",
    "L7",
    "LL1",
    "LL2",
    "S1",
    "S2",
    "S3",
    "S4",
    "S5",
    "I1",
    "I2_0",
    "I2_1",
    "I3_1",
    "I3_2",
    "I4_1",
    "I4_2",
    "B1",
    "B2",
    "B3",
    "B4",
    "B5",
    "B6",
    "B7",
    "B8",
    "B18",
    "LS2",
    "PAT3",
    "S2_UL",
    "LS2_UL",
};

const int decomp_incr_tab[4][4] = {
    { DECOMP_TYPE_I1, 0, -1 },
    { DECOMP_TYPE_I2_0, 0, 1, -1 },
    { DECOMP_TYPE_I3_1, 1, 2, -1 },
    { DECOMP_TYPE_I4_1, 1, 2, -1 },
};

/*
  entry size:
  type   bits
  code   18
  len    7
  compat 1
  type   5
  index  16
  total  47
*/



int get_decomp_run_size(const DecompEntry *de)
{
    int s;
    s = 6;
    if (de->type <= DECOMP_TYPE_C1) {
        /* nothing more */
    } else if (de->type <= DECOMP_TYPE_L7) {
        s += de->len * de->c_len * 2;
    } else if (de->type <= DECOMP_TYPE_LL2) {
        /* 18 bits per char */
        s += (de->len * de->c_len * 18 + 7) / 8;
    } else if (de->type <= DECOMP_TYPE_S5) {
        s += de->len * de->c_len;
    } else if (de->type <= DECOMP_TYPE_I4_2) {
        s += de->c_len * 2;
    } else if (de->type <= DECOMP_TYPE_B18) {
        s += 2 + de->len * de->c_len;
    } else if (de->type <= DECOMP_TYPE_LS2) {
        s += de->len * 3;
    } else if (de->type <= DECOMP_TYPE_PAT3) {
        s += 4 + de->len * 2;
    } else if (de->type <= DECOMP_TYPE_S2_UL) {
        s += de->len;
    } else if (de->type <= DECOMP_TYPE_LS2_UL) {
        s += (de->len / 2) * 3;
    } else {
        abort();
    }
    return s;
}

static const uint16_t unicode_short_table[2] = { 0x2044, 0x2215 };

/* return -1 if not found */
int get_short_code(int c)
{
    int i;
    if (c < 0x80) {
        return c;
    } else if (c >= 0x300 && c < 0x350) {
        return c - 0x300 + 0x80;
    } else {
        for(i = 0; i < countof(unicode_short_table); i++) {
            if (c == unicode_short_table[i])
                return i + 0x80 + 0x50;
        }
        return -1;
    }
}

static BOOL is_short(int code)
{
    return get_short_code(code) >= 0;
}

static BOOL is_short_tab(const int *tab, int len)
{
    int i;
    for(i = 0; i < len; i++) {
        if (!is_short(tab[i]))
            return FALSE;
    }
    return TRUE;
}

static BOOL is_16bit(const int *tab, int len)
{
    int i;
    for(i = 0; i < len; i++) {
        if (tab[i] > 0xffff)
            return FALSE;
    }
    return TRUE;
}

static uint32_t to_lower_simple(uint32_t c)
{
    /* Latin1 and Cyrillic */
    if (c < 0x100 || (c >= 0x410 && c <= 0x42f))
        c += 0x20;
    else
        c++;
    return c;
}

/* select best encoding with dynamic programming */
void find_decomp_run(DecompEntry *tab_de, int i)
{
    DecompEntry de_s, *de = &de_s;
    CCInfo *ci, *ci1, *ci2;
    int l, j, n, len_max;

    ci = &unicode_db[i];
    l = ci->decomp_len;
    if (l == 0) {
        tab_de[i].cost = tab_de[i + 1].cost;
        return;
    }

    /* the offset for the compose table has only 6 bits, so we must
       limit if it can be used by the compose table */
    if (!ci->is_compat && !ci->is_excluded && l == 2)
        len_max = 64;
    else
        len_max = 127;

    tab_de[i].cost = 0x7fffffff;

    if (!is_16bit(ci->decomp_data, l)) {
        assert(l <= 2);

        n = 1;
        for(;;) {
            de->code = i;
            de->len = n;
            de->type = DECOMP_TYPE_LL1 + l - 1;
            de->c_len = l;
            de->cost = get_decomp_run_size(de) + tab_de[i + n].cost;
            if (de->cost < tab_de[i].cost) {
                tab_de[i] = *de;
            }
            if (!((i + n) <= CHARCODE_MAX && n < len_max))
                break;
            ci1 = &unicode_db[i + n];
            /* Note: we accept a hole */
            if (!(ci1->decomp_len == 0 ||
                  (ci1->decomp_len == l &&
                   ci1->is_compat == ci->is_compat)))
                break;
            n++;
        }
        return;
    }

    if (l <= 7) {
        n = 1;
        for(;;) {
            de->code = i;
            de->len = n;
            if (l == 1 && n == 1) {
                de->type = DECOMP_TYPE_C1;
            } else {
                assert(l <= 8);
                de->type = DECOMP_TYPE_L1 + l - 1;
            }
            de->c_len = l;
            de->cost = get_decomp_run_size(de) + tab_de[i + n].cost;
            if (de->cost < tab_de[i].cost) {
                tab_de[i] = *de;
            }

            if (!((i + n) <= CHARCODE_MAX && n < len_max))
                break;
            ci1 = &unicode_db[i + n];
            /* Note: we accept a hole */
            if (!(ci1->decomp_len == 0 ||
                  (ci1->decomp_len == l &&
                   ci1->is_compat == ci->is_compat &&
                   is_16bit(ci1->decomp_data, l))))
                break;
            n++;
        }
    }

    if (l <= 8 || l == 18) {
        int c_min, c_max, c;
        c_min = c_max = -1;
        n = 1;
        for(;;) {
            ci1 = &unicode_db[i + n - 1];
            for(j = 0; j < l; j++) {
                c = ci1->decomp_data[j];
                if (c == 0x20) {
                    /* we accept space for Arabic */
                } else if (c_min == -1) {
                    c_min = c_max = c;
                } else {
                    c_min = min_int(c_min, c);
                    c_max = max_int(c_max, c);
                }
            }
            if ((c_max - c_min) > 254)
                break;
            de->code = i;
            de->len = n;
            if (l == 18)
                de->type = DECOMP_TYPE_B18;
            else
                de->type = DECOMP_TYPE_B1 + l - 1;
            de->c_len = l;
            de->c_min = c_min;
            de->cost = get_decomp_run_size(de) + tab_de[i + n].cost;
            if (de->cost < tab_de[i].cost) {
                tab_de[i] = *de;
            }
            if (!((i + n) <= CHARCODE_MAX && n < len_max))
                break;
            ci1 = &unicode_db[i + n];
            if (!(ci1->decomp_len == l &&
                  ci1->is_compat == ci->is_compat))
                break;
            n++;
        }
    }

    /* find an ascii run */
    if (l <= 5 && is_short_tab(ci->decomp_data, l)) {
        n = 1;
        for(;;) {
            de->code = i;
            de->len = n;
            de->type = DECOMP_TYPE_S1 + l - 1;
            de->c_len = l;
            de->cost = get_decomp_run_size(de) + tab_de[i + n].cost;
            if (de->cost < tab_de[i].cost) {
                tab_de[i] = *de;
            }

            if (!((i + n) <= CHARCODE_MAX && n < len_max))
                break;
            ci1 = &unicode_db[i + n];
            /* Note: we accept a hole */
            if (!(ci1->decomp_len == 0 ||
                  (ci1->decomp_len == l &&
                   ci1->is_compat == ci->is_compat &&
                   is_short_tab(ci1->decomp_data, l))))
                break;
            n++;
        }
    }

    /* check if a single char is increasing */
    if (l <= 4) {
        int idx1, idx;

        for(idx1 = 1; (idx = decomp_incr_tab[l - 1][idx1]) >= 0; idx1++) {
            n = 1;
            for(;;) {
                de->code = i;
                de->len = n;
                de->type = decomp_incr_tab[l - 1][0] + idx1 - 1;
                de->c_len = l;
                de->cost = get_decomp_run_size(de) + tab_de[i + n].cost;
                if (de->cost < tab_de[i].cost) {
                    tab_de[i] = *de;
                }

                if (!((i + n) <= CHARCODE_MAX && n < len_max))
                    break;
                ci1 = &unicode_db[i + n];
                if (!(ci1->decomp_len == l &&
                      ci1->is_compat == ci->is_compat))
                    goto next1;
                for(j = 0; j < l; j++) {
                    if (j == idx) {
                        if (ci1->decomp_data[j] != ci->decomp_data[j] + n)
                            goto next1;
                    } else {
                        if (ci1->decomp_data[j] != ci->decomp_data[j])
                            goto next1;
                    }
                }
                n++;
            }
        next1: ;
        }
    }

    if (l == 3) {
        n = 1;
        for(;;) {
            de->code = i;
            de->len = n;
            de->type = DECOMP_TYPE_PAT3;
            de->c_len = l;
            de->cost = get_decomp_run_size(de) + tab_de[i + n].cost;
            if (de->cost < tab_de[i].cost) {
                tab_de[i] = *de;
            }
            if (!((i + n) <= CHARCODE_MAX && n < len_max))
                break;
            ci1 = &unicode_db[i + n];
            if (!(ci1->decomp_len == l &&
                  ci1->is_compat == ci->is_compat &&
                  ci1->decomp_data[1] <= 0xffff &&
                  ci1->decomp_data[0] == ci->decomp_data[0] &&
                  ci1->decomp_data[l - 1] == ci->decomp_data[l - 1]))
                break;
            n++;
        }
    }

    if (l == 2 && is_short(ci->decomp_data[1])) {
        n = 1;
        for(;;) {
            de->code = i;
            de->len = n;
            de->type = DECOMP_TYPE_LS2;
            de->c_len = l;
            de->cost = get_decomp_run_size(de) + tab_de[i + n].cost;
            if (de->cost < tab_de[i].cost) {
                tab_de[i] = *de;
            }
            if (!((i + n) <= CHARCODE_MAX && n < len_max))
                break;
            ci1 = &unicode_db[i + n];
            if (!(ci1->decomp_len == 0 ||
                  (ci1->decomp_len == l &&
                   ci1->is_compat == ci->is_compat &&
                   ci1->decomp_data[0] <= 0xffff &&
                   is_short(ci1->decomp_data[1]))))
                break;
            n++;
        }
    }

    if (l == 2) {
        BOOL is_16bit;

        n = 0;
        is_16bit = FALSE;
        for(;;) {
            if (!((i + n + 1) <= CHARCODE_MAX && n + 2 <= len_max))
                break;
            ci1 = &unicode_db[i + n];
            if (!(ci1->decomp_len == l &&
                  ci1->is_compat == ci->is_compat &&
                  is_short(ci1->decomp_data[1])))
                break;
            if (!is_16bit && !is_short(ci1->decomp_data[0]))
                is_16bit = TRUE;
            ci2 = &unicode_db[i + n + 1];
            if (!(ci2->decomp_len == l &&
                  ci2->is_compat == ci->is_compat &&
                  ci2->decomp_data[0] == to_lower_simple(ci1->decomp_data[0])  &&
                  ci2->decomp_data[1] == ci1->decomp_data[1]))
                break;
            n += 2;
            de->code = i;
            de->len = n;
            de->type = DECOMP_TYPE_S2_UL + is_16bit;
            de->c_len = l;
            de->cost = get_decomp_run_size(de) + tab_de[i + n].cost;
            if (de->cost < tab_de[i].cost) {
                tab_de[i] = *de;
            }
        }
    }
}

void put16(uint8_t *data_buf, int *pidx, uint16_t c)
{
    int idx;
    idx = *pidx;
    data_buf[idx++] = c;
    data_buf[idx++] = c >> 8;
    *pidx = idx;
}

void add_decomp_data(uint8_t *data_buf, int *pidx, DecompEntry *de)
{
    int i, j, idx, c;
    CCInfo *ci;

    idx = *pidx;
    de->data_index = idx;
    if (de->type <= DECOMP_TYPE_C1) {
        ci = &unicode_db[de->code];
        assert(ci->decomp_len == 1);
        de->data_index = ci->decomp_data[0];
    } else if (de->type <= DECOMP_TYPE_L7) {
        for(i = 0; i < de->len; i++) {
            ci = &unicode_db[de->code + i];
            for(j = 0; j < de->c_len; j++) {
                if (ci->decomp_len == 0)
                    c = 0;
                else
                    c = ci->decomp_data[j];
                put16(data_buf, &idx,  c);
            }
        }
    } else if (de->type <= DECOMP_TYPE_LL2) {
        int n, p, k;
        n = (de->len * de->c_len * 18 + 7) / 8;
        p = de->len * de->c_len * 2;
        memset(data_buf + idx, 0, n);
        k = 0;
        for(i = 0; i < de->len; i++) {
            ci = &unicode_db[de->code + i];
            for(j = 0; j < de->c_len; j++) {
                if (ci->decomp_len == 0)
                    c = 0;
                else
                    c = ci->decomp_data[j];
                data_buf[idx + k * 2] = c;
                data_buf[idx + k * 2 + 1] = c >> 8;
                data_buf[idx + p + (k / 4)] |= (c >> 16) << ((k % 4) * 2);
                k++;
            }
        }
        idx += n;
    } else if (de->type <= DECOMP_TYPE_S5) {
        for(i = 0; i < de->len; i++) {
            ci = &unicode_db[de->code + i];
            for(j = 0; j < de->c_len; j++) {
                if (ci->decomp_len == 0)
                    c = 0;
                else
                    c = ci->decomp_data[j];
                c = get_short_code(c);
                assert(c >= 0);
                data_buf[idx++] = c;
            }
        }
    } else if (de->type <= DECOMP_TYPE_I4_2) {
        ci = &unicode_db[de->code];
        assert(ci->decomp_len == de->c_len);
        for(j = 0; j < de->c_len; j++)
            put16(data_buf, &idx, ci->decomp_data[j]);
    } else if (de->type <= DECOMP_TYPE_B18) {
        c = de->c_min;
        data_buf[idx++] = c;
        data_buf[idx++] = c >> 8;
        for(i = 0; i < de->len; i++) {
            ci = &unicode_db[de->code + i];
            for(j = 0; j < de->c_len; j++) {
                assert(ci->decomp_len == de->c_len);
                c = ci->decomp_data[j];
                if (c == 0x20) {
                    c = 0xff;
                } else {
                    c -= de->c_min;
                    assert((uint32_t)c <= 254);
                }
                data_buf[idx++] = c;
            }
        }
    } else if (de->type <= DECOMP_TYPE_LS2) {
        assert(de->c_len == 2);
        for(i = 0; i < de->len; i++) {
            ci = &unicode_db[de->code + i];
            if (ci->decomp_len == 0)
                c = 0;
            else
                c = ci->decomp_data[0];
            put16(data_buf, &idx,  c);

            if (ci->decomp_len == 0)
                c = 0;
            else
                c = ci->decomp_data[1];
            c = get_short_code(c);
            assert(c >= 0);
            data_buf[idx++] = c;
        }
    } else if (de->type <= DECOMP_TYPE_PAT3) {
        ci = &unicode_db[de->code];
        assert(ci->decomp_len == 3);
        put16(data_buf, &idx,  ci->decomp_data[0]);
        put16(data_buf, &idx,  ci->decomp_data[2]);
        for(i = 0; i < de->len; i++) {
            ci = &unicode_db[de->code + i];
            assert(ci->decomp_len == 3);
            put16(data_buf, &idx,  ci->decomp_data[1]);
        }
    } else if (de->type <= DECOMP_TYPE_S2_UL) {
        for(i = 0; i < de->len; i += 2) {
            ci = &unicode_db[de->code + i];
            c = ci->decomp_data[0];
            c = get_short_code(c);
            assert(c >= 0);
            data_buf[idx++] = c;
            c = ci->decomp_data[1];
            c = get_short_code(c);
            assert(c >= 0);
            data_buf[idx++] = c;
        }
    } else if (de->type <= DECOMP_TYPE_LS2_UL) {
        for(i = 0; i < de->len; i += 2) {
            ci = &unicode_db[de->code + i];
            c = ci->decomp_data[0];
            put16(data_buf, &idx,  c);
            c = ci->decomp_data[1];
            c = get_short_code(c);
            assert(c >= 0);
            data_buf[idx++] = c;
        }
    } else {
        abort();
    }
    *pidx = idx;
}

#if 0
void dump_large_char(void)
{
    int i, j;
    for(i = 0; i <= CHARCODE_MAX; i++) {
        CCInfo *ci = &unicode_db[i];
        for(j = 0; j < ci->decomp_len; j++) {
            if (ci->decomp_data[j] > 0xffff)
                printf("%05x\n", ci->decomp_data[j]);
        }
    }
}
#endif

void build_compose_table(FILE *f, const DecompEntry *tab_de);

void build_decompose_table(FILE *f)
{
    int i, array_len, code_max, data_len, count;
    DecompEntry *tab_de, de_s, *de = &de_s;
    uint8_t *data_buf;

    code_max = CHARCODE_MAX;

    tab_de = mallocz((code_max + 2) * sizeof(*tab_de));

    for(i = code_max; i >= 0; i--) {
        find_decomp_run(tab_de, i);
    }

    /* build the data buffer */
    data_buf = malloc(100000);
    data_len = 0;
    array_len = 0;
    for(i = 0; i <= code_max; i++) {
        de = &tab_de[i];
        if (de->len != 0) {
            add_decomp_data(data_buf, &data_len, de);
            i += de->len - 1;
            array_len++;
        }
    }

#ifdef DUMP_DECOMP_TABLE
    /* dump */
    {
        int size, size1;

        printf("START LEN   TYPE  L C SIZE\n");
        size = 0;
        for(i = 0; i <= code_max; i++) {
            de = &tab_de[i];
            if (de->len != 0) {
                size1 = get_decomp_run_size(de);
                printf("%05x %3d %6s %2d %1d %4d\n", i, de->len,
                       decomp_type_str[de->type], de->c_len,
                       unicode_db[i].is_compat, size1);
                i += de->len - 1;
                size += size1;
            }
        }

        printf("array_len=%d estimated size=%d bytes actual=%d bytes\n",
               array_len, size, array_len * 6 + data_len);
    }
#endif

    total_tables++;
    total_table_bytes += array_len * sizeof(uint32_t);
    fprintf(f, "static const uint32_t unicode_decomp_table1[%d] = {", array_len);
    count = 0;
    for(i = 0; i <= code_max; i++) {
        de = &tab_de[i];
        if (de->len != 0) {
            uint32_t v;
            if (count++ % 4 == 0)
                fprintf(f, "\n   ");
            v = (de->code << (32 - 18)) |
                (de->len << (32 - 18 - 7)) |
                (de->type << (32 - 18 - 7 - 6)) |
                unicode_db[de->code].is_compat;
            fprintf(f, " 0x%08x,", v);
            i += de->len - 1;
        }
    }
    fprintf(f, "\n};\n\n");

    total_tables++;
    total_table_bytes += array_len * sizeof(uint16_t);
    fprintf(f, "static const uint16_t unicode_decomp_table2[%d] = {", array_len);
    count = 0;
    for(i = 0; i <= code_max; i++) {
        de = &tab_de[i];
        if (de->len != 0) {
            if (count++ % 8 == 0)
                fprintf(f, "\n   ");
            fprintf(f, " 0x%04x,", de->data_index);
            i += de->len - 1;
        }
    }
    fprintf(f, "\n};\n\n");

    total_tables++;
    total_table_bytes += data_len;
    fprintf(f, "static const uint8_t unicode_decomp_data[%d] = {", data_len);
    for(i = 0; i < data_len; i++) {
        if (i % 8 == 0)
            fprintf(f, "\n   ");
        fprintf(f, " 0x%02x,", data_buf[i]);
    }
    fprintf(f, "\n};\n\n");

    build_compose_table(f, tab_de);

    free(data_buf);

    free(tab_de);
}

typedef struct {
    uint32_t c[2];
    uint32_t p;
} ComposeEntry;

#define COMPOSE_LEN_MAX 10000

static int ce_cmp(const void *p1, const void *p2)
{
    const ComposeEntry *ce1 = p1;
    const ComposeEntry *ce2 = p2;
    int i;

    for(i = 0; i < 2; i++) {
        if (ce1->c[i] < ce2->c[i])
            return -1;
        else if (ce1->c[i] > ce2->c[i])
            return 1;
    }
    return 0;
}


static int get_decomp_pos(const DecompEntry *tab_de, int c)
{
    int i, v, k;
    const DecompEntry *de;

    k = 0;
    for(i = 0; i <= CHARCODE_MAX; i++) {
        de = &tab_de[i];
        if (de->len != 0) {
            if (c >= de->code && c < de->code + de->len) {
                v = c - de->code;
                assert(v < 64);
                v |= k << 6;
                assert(v < 65536);
                return v;
            }
            i += de->len - 1;
            k++;
        }
    }
    return -1;
}

void build_compose_table(FILE *f, const DecompEntry *tab_de)
{
    int i, v, tab_ce_len;
    ComposeEntry *ce, *tab_ce;

    tab_ce = malloc(sizeof(*tab_ce) * COMPOSE_LEN_MAX);
    tab_ce_len = 0;
    for(i = 0; i <= CHARCODE_MAX; i++) {
        CCInfo *ci = &unicode_db[i];
        if (ci->decomp_len == 2 && !ci->is_compat &&
            !ci->is_excluded) {
            assert(tab_ce_len < COMPOSE_LEN_MAX);
            ce = &tab_ce[tab_ce_len++];
            ce->c[0] = ci->decomp_data[0];
            ce->c[1] = ci->decomp_data[1];
            ce->p = i;
        }
    }
    qsort(tab_ce, tab_ce_len, sizeof(*tab_ce), ce_cmp);

#if 0
    {
        printf("tab_ce_len=%d\n", tab_ce_len);
        for(i = 0; i < tab_ce_len; i++) {
            ce = &tab_ce[i];
            printf("%05x %05x %05x\n", ce->c[0], ce->c[1], ce->p);
        }
    }
#endif

    total_tables++;
    total_table_bytes += tab_ce_len * sizeof(uint16_t);
    fprintf(f, "static const uint16_t unicode_comp_table[%u] = {", tab_ce_len);
    for(i = 0; i < tab_ce_len; i++) {
        if (i % 8 == 0)
            fprintf(f, "\n   ");
        v = get_decomp_pos(tab_de, tab_ce[i].p);
        if (v < 0) {
            printf("ERROR: entry for c=%04x not found\n",
                   tab_ce[i].p);
            exit(1);
        }
        fprintf(f, " 0x%04x,", v);
    }
    fprintf(f, "\n};\n\n");

    free(tab_ce);
}

#ifdef USE_TEST
void check_decompose_table(void)
{
    int c;
    CCInfo *ci;
    int res[UNICODE_DECOMP_LEN_MAX], *ref;
    int len, ref_len, is_compat;

    for(is_compat = 0; is_compat <= 1; is_compat++) {
        for(c = 0; c < CHARCODE_MAX; c++) {
            ci = &unicode_db[c];
            ref_len = ci->decomp_len;
            ref = ci->decomp_data;
            if (!is_compat && ci->is_compat) {
                ref_len = 0;
            }
            len = unicode_decomp_char((uint32_t *)res, c, is_compat);
            if (len != ref_len ||
                tabcmp(res, ref, ref_len) != 0) {
                printf("ERROR c=%05x compat=%d\n", c, is_compat);
                dump_str("res", res, len);
                dump_str("ref", ref, ref_len);
                exit(1);
            }
        }
    }
}

void check_compose_table(void)
{
    int i, p;
    /* XXX: we don't test all the cases */

    for(i = 0; i <= CHARCODE_MAX; i++) {
        CCInfo *ci = &unicode_db[i];
        if (ci->decomp_len == 2 && !ci->is_compat &&
            !ci->is_excluded) {
            p = unicode_compose_pair(ci->decomp_data[0], ci->decomp_data[1]);
            if (p != i) {
                printf("ERROR compose: c=%05x %05x -> %05x ref=%05x\n",
                       ci->decomp_data[0], ci->decomp_data[1], p, i);
                exit(1);
            }
        }
    }



}

#endif



#ifdef USE_TEST

void check_str(const char *msg, int num, const int *in_buf, int in_len,
               const int *buf1, int len1,
               const int *buf2, int len2)
{
    if (len1 != len2 || tabcmp(buf1, buf2, len1) != 0) {
        printf("%d: ERROR %s:\n", num, msg);
        dump_str(" in", in_buf, in_len);
        dump_str("res", buf1, len1);
        dump_str("ref", buf2, len2);
        exit(1);
    }
}

void check_cc_table(void)
{
    int cc, cc_ref, c;

    for(c = 0; c <= CHARCODE_MAX; c++) {
        cc_ref = unicode_db[c].combining_class;
        cc = unicode_get_cc(c);
        if (cc != cc_ref) {
            printf("ERROR: c=%04x cc=%d cc_ref=%d\n",
                   c, cc, cc_ref);
            exit(1);
        }
    }
#ifdef PROFILE
    {
        int64_t ti, count;

        ti = get_time_ns();
        count = 0;
        /* only do it on meaningful chars */
        for(c = 0x20; c <= 0xffff; c++) {
            cc_ref = unicode_db[c].combining_class;
            cc = unicode_get_cc(c);
            count++;
        }
        ti = get_time_ns() - ti;
        printf("cc time=%0.1f ns/char\n",
               (double)ti / count);
    }
#endif
}

void normalization_test(const char *filename)
{
    FILE *f;
    char line[4096], *p;
    int *in_str, *nfc_str, *nfd_str, *nfkc_str, *nfkd_str;
    int in_len, nfc_len, nfd_len, nfkc_len, nfkd_len;
    int *buf, buf_len, pos;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }
    pos = 0;
    for(;;) {
        if (!get_line(line, sizeof(line), f))
            break;
        pos++;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@')
            continue;
        in_str = get_field_str(&in_len, p, 0);
        nfc_str = get_field_str(&nfc_len, p, 1);
        nfd_str = get_field_str(&nfd_len, p, 2);
        nfkc_str = get_field_str(&nfkc_len, p, 3);
        nfkd_str = get_field_str(&nfkd_len, p, 4);

        //        dump_str("in", in_str, in_len);

        buf_len = unicode_normalize((uint32_t **)&buf, (uint32_t *)in_str, in_len, UNICODE_NFD, NULL, NULL);
        check_str("nfd", pos, in_str, in_len, buf, buf_len, nfd_str, nfd_len);
        free(buf);

        buf_len = unicode_normalize((uint32_t **)&buf, (uint32_t *)in_str, in_len, UNICODE_NFKD, NULL, NULL);
        check_str("nfkd", pos, in_str, in_len, buf, buf_len, nfkd_str, nfkd_len);
        free(buf);

        buf_len = unicode_normalize((uint32_t **)&buf, (uint32_t *)in_str, in_len, UNICODE_NFC, NULL, NULL);
        check_str("nfc", pos, in_str, in_len, buf, buf_len, nfc_str, nfc_len);
        free(buf);

        buf_len = unicode_normalize((uint32_t **)&buf, (uint32_t *)in_str, in_len, UNICODE_NFKC, NULL, NULL);
        check_str("nfkc", pos, in_str, in_len, buf, buf_len, nfkc_str, nfkc_len);
        free(buf);

        free(in_str);
        free(nfc_str);
        free(nfd_str);
        free(nfkc_str);
        free(nfkd_str);
    }
    fclose(f);
}
#endif
