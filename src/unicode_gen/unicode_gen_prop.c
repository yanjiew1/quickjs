#include "unicode_gen_prop.h"

void compute_internal_props(void)
{
    int i;
    BOOL has_ul;

    for(i = 0; i <= CHARCODE_MAX; i++) {
        CCInfo *ci = &unicode_db[i];
        has_ul = (ci->u_len != 0 || ci->l_len != 0 || ci->f_len != 0);
        if (has_ul) {
            assert(get_prop(i, PROP_Cased));
        } else {
            set_prop(i, PROP_Cased1, get_prop(i, PROP_Cased));
        }
        set_prop(i, PROP_ID_Continue1,
                 get_prop(i, PROP_ID_Continue) & (get_prop(i, PROP_ID_Start) ^ 1));
        set_prop(i, PROP_XID_Start1,
                 get_prop(i, PROP_ID_Start) ^ get_prop(i, PROP_XID_Start));
        set_prop(i, PROP_XID_Continue1,
                 get_prop(i, PROP_ID_Continue) ^ get_prop(i, PROP_XID_Continue));
        set_prop(i, PROP_Changes_When_Titlecased1,
                 get_prop(i, PROP_Changes_When_Titlecased) ^ (ci->u_len != 0));
        set_prop(i, PROP_Changes_When_Casefolded1,
                 get_prop(i, PROP_Changes_When_Casefolded) ^ (ci->f_len != 0));
        /* XXX: reduce table size (438 bytes) */
        set_prop(i, PROP_Changes_When_NFKC_Casefolded1,
                 get_prop(i, PROP_Changes_When_NFKC_Casefolded) ^ (ci->f_len != 0));
#if 0
        /* TEST */
#define M(x) (1U << GCAT_ ## x)
        {
            int b;
            b = ((M(Mn) | M(Cf) | M(Lm) | M(Sk)) >>
                 unicode_db[i].general_category) & 1;
            set_prop(i, PROP_Cased1,
                     get_prop(i, PROP_Case_Ignorable) ^ b);
        }
#undef M
#endif
    }
}

#define PROP_BLOCK_LEN 32

void build_prop_table(FILE *f, const char *name, int prop_index, BOOL add_index)
{
    int i, j, n, v, offset, code;
    DynBuf dbuf_s, *dbuf = &dbuf_s;
    DynBuf dbuf1_s, *dbuf1 = &dbuf1_s;
    DynBuf dbuf2_s, *dbuf2 = &dbuf2_s;
    const uint32_t *buf;
    int buf_len, block_end_pos, bit;
    char cname[128];

    dbuf_init(dbuf1);

    for(i = 0; i <= CHARCODE_MAX;) {
        v = get_prop(i, prop_index);
        j = i + 1;
        while (j <= CHARCODE_MAX && get_prop(j, prop_index) == v) {
            j++;
        }
        n = j - i;
        if (j == (CHARCODE_MAX + 1) && v == 0)
            break; /* no need to encode last zero run */
        //printf("%05x: %d %d\n", i, n, v);
        dbuf_put_u32(dbuf1, n - 1);
        i += n;
    }

    dbuf_init(dbuf);
    dbuf_init(dbuf2);
    buf = (uint32_t *)dbuf1->buf;
    buf_len = dbuf1->size / sizeof(buf[0]);

    /* the first value is assumed to be 0 */
    assert(get_prop(0, prop_index) == 0);

    block_end_pos = PROP_BLOCK_LEN;
    i = 0;
    code = 0;
    bit = 0;
    while (i < buf_len) {
        if (add_index && dbuf->size >= block_end_pos && bit == 0) {
            offset = (dbuf->size - block_end_pos);
            /* XXX: offset could be larger in case of runs of small
               lengths. Could add code to change the encoding to
               prevent it at the expense of one byte loss */
            assert(offset <= 7);
            v = code | (offset << 21);
            dbuf_putc(dbuf2, v);
            dbuf_putc(dbuf2, v >> 8);
            dbuf_putc(dbuf2, v >> 16);
            block_end_pos += PROP_BLOCK_LEN;
        }

        /* Compressed byte encoding:
           00..3F: 2 packed lengths: 3-bit + 3-bit
           40..5F: 5-bits plus extra byte for length
           60..7F: 5-bits plus 2 extra bytes for length
           80..FF: 7-bit length
           lengths must be incremented to get character count
           Ranges alternate between false and true return value.
         */
        v = buf[i];
        code += v + 1;
        bit ^= 1;
        if (v < 8 && (i + 1) < buf_len && buf[i + 1] < 8) {
            code += buf[i + 1] + 1;
            bit ^= 1;
            dbuf_putc(dbuf, (v << 3) | buf[i + 1]);
            i += 2;
        } else if (v < 128) {
            dbuf_putc(dbuf, 0x80 + v);
            i++;
        } else if (v < (1 << 13)) {
            dbuf_putc(dbuf, 0x40 + (v >> 8));
            dbuf_putc(dbuf, v);
            i++;
        } else {
            assert(v < (1 << 21));
            dbuf_putc(dbuf, 0x60 + (v >> 16));
            dbuf_putc(dbuf, v >> 8);
            dbuf_putc(dbuf, v);
            i++;
        }
    }

    if (add_index) {
        /* last index entry */
        v = code;
        dbuf_putc(dbuf2, v);
        dbuf_putc(dbuf2, v >> 8);
        dbuf_putc(dbuf2, v >> 16);
    }

#ifdef DUMP_TABLE_SIZE
    printf("prop %s: length=%d bytes\n", unicode_prop_name[prop_index],
           (int)(dbuf->size + dbuf2->size));
#endif
    snprintf(cname, sizeof(cname), "unicode_prop_%s_table", unicode_prop_name[prop_index]);
    dump_byte_table(f, cname, dbuf->buf, dbuf->size);
    if (add_index) {
        snprintf(cname, sizeof(cname), "unicode_prop_%s_index", unicode_prop_name[prop_index]);
        dump_index_table(f, cname, dbuf2->buf, dbuf2->size);
    }

    dbuf_free(dbuf);
    dbuf_free(dbuf1);
    dbuf_free(dbuf2);
}

void build_flags_tables(FILE *f)
{
    build_prop_table(f, "Cased1", PROP_Cased1, TRUE);
    build_prop_table(f, "Case_Ignorable", PROP_Case_Ignorable, TRUE);
    build_prop_table(f, "ID_Start", PROP_ID_Start, TRUE);
    build_prop_table(f, "ID_Continue1", PROP_ID_Continue1, TRUE);
}

void dump_name_table(FILE *f, const char *cname, const char **tab_name, int len,
                     const char **tab_short_name)
{
    int i, w, maxw;

    maxw = 0;
    for(i = 0; i < len; i++) {
        w = strlen(tab_name[i]);
        if (tab_short_name && tab_short_name[i][0] != '\0') {
            w += 1 + strlen(tab_short_name[i]);
        }
        if (maxw < w)
            maxw = w;
    }

    /* generate a sequence of strings terminated by an empty string */
    fprintf(f, "static const char %s[] =\n", cname);
    for(i = 0; i < len; i++) {
        fprintf(f, "    \"");
        w = fprintf(f, "%s", tab_name[i]);
        if (tab_short_name && tab_short_name[i][0] != '\0') {
            w += fprintf(f, ",%s", tab_short_name[i]);
        }
        fprintf(f, "\"%*s\"\\0\"\n", 1 + maxw - w, "");
    }
    fprintf(f, ";\n\n");
}

void build_general_category_table(FILE *f)
{
    int i, v, j, n, n1;
    DynBuf dbuf_s, *dbuf = &dbuf_s;
#ifdef DUMP_TABLE_SIZE
    int cw_count, cw_len_count[4], cw_start;
#endif

    fprintf(f, "typedef enum {\n");
    for(i = 0; i < GCAT_COUNT; i++)
        fprintf(f, "    UNICODE_GC_%s,\n", unicode_gc_name[i]);
    fprintf(f, "    UNICODE_GC_COUNT,\n");
    fprintf(f, "} UnicodeGCEnum;\n\n");

    dump_name_table(f, "unicode_gc_name_table",
                    unicode_gc_name, GCAT_COUNT,
                    unicode_gc_short_name);


    dbuf_init(dbuf);
#ifdef DUMP_TABLE_SIZE
    cw_count = 0;
    for(i = 0; i < 4; i++)
        cw_len_count[i] = 0;
#endif
    for(i = 0; i <= CHARCODE_MAX;) {
        v = unicode_db[i].general_category;
        j = i + 1;
        while (j <= CHARCODE_MAX && unicode_db[j].general_category == v)
            j++;
        n = j - i;
        /* compress Lu/Ll runs */
        if (v == GCAT_Lu) {
            n1 = 1;
            while ((i + n1) <= CHARCODE_MAX && unicode_db[i + n1].general_category == (v + (n1 & 1))) {
                n1++;
            }
            if (n1 > n) {
                v = 31;
                n = n1;
            }
        }
        //        printf("%05x %05x %d\n", i, n, v);
        n--;
#ifdef DUMP_TABLE_SIZE
        cw_count++;
        cw_start = dbuf->size;
#endif
        if (n < 7) {
            dbuf_putc(dbuf, (n << 5) | v);
        } else if (n < 7 + 128) {
            n1 = n - 7;
            assert(n1 < 128);
            dbuf_putc(dbuf, (0xf << 5) | v);
            dbuf_putc(dbuf, n1);
        } else if (n < 7 + 128 + (1 << 14)) {
            n1 = n - (7 + 128);
            assert(n1 < (1 << 14));
            dbuf_putc(dbuf, (0xf << 5) | v);
            dbuf_putc(dbuf, (n1 >> 8) + 128);
            dbuf_putc(dbuf, n1);
        } else {
            n1 = n - (7 + 128 + (1 << 14));
            assert(n1 < (1 << 22));
            dbuf_putc(dbuf, (0xf << 5) | v);
            dbuf_putc(dbuf, (n1 >> 16) + 128 + 64);
            dbuf_putc(dbuf, n1 >> 8);
            dbuf_putc(dbuf, n1);
        }
#ifdef DUMP_TABLE_SIZE
        cw_len_count[dbuf->size - cw_start - 1]++;
#endif
        i += n + 1;
    }
#ifdef DUMP_TABLE_SIZE
    printf("general category: %d entries [", cw_count);
    for(i = 0; i < 4; i++)
        printf(" %d", cw_len_count[i]);
    printf(" ], length=%d bytes\n", (int)dbuf->size);
#endif

    dump_byte_table(f, "unicode_gc_table", dbuf->buf, dbuf->size);

    dbuf_free(dbuf);
}

void build_script_table(FILE *f)
{
    int i, v, j, n, n1, type;
    DynBuf dbuf_s, *dbuf = &dbuf_s;
#ifdef DUMP_TABLE_SIZE
    int cw_count, cw_len_count[4], cw_start;
#endif

    fprintf(f, "typedef enum {\n");
    for(i = 0; i < SCRIPT_COUNT; i++)
        fprintf(f, "    UNICODE_SCRIPT_%s,\n", unicode_script_name[i]);
    fprintf(f, "    UNICODE_SCRIPT_COUNT,\n");
    fprintf(f, "} UnicodeScriptEnum;\n\n");

    dump_name_table(f, "unicode_script_name_table",
                    unicode_script_name, SCRIPT_COUNT,
                    unicode_script_short_name);

    dbuf_init(dbuf);
#ifdef DUMP_TABLE_SIZE
    cw_count = 0;
    for(i = 0; i < 4; i++)
        cw_len_count[i] = 0;
#endif
    for(i = 0; i <= CHARCODE_MAX;) {
        v = unicode_db[i].script;
        j = i + 1;
        while (j <= CHARCODE_MAX && unicode_db[j].script == v)
            j++;
        n = j - i;
        if (v == 0 && j == (CHARCODE_MAX + 1))
            break;
        //        printf("%05x %05x %d\n", i, n, v);
        n--;
#ifdef DUMP_TABLE_SIZE
        cw_count++;
        cw_start = dbuf->size;
#endif
        if (v == 0)
            type = 0;
        else
            type = 1;
        if (n < 96) {
            dbuf_putc(dbuf, n | (type << 7));
        } else if (n < 96 + (1 << 12)) {
            n1 = n - 96;
            assert(n1 < (1 << 12));
            dbuf_putc(dbuf, ((n1 >> 8) + 96) | (type << 7));
            dbuf_putc(dbuf, n1);
        } else {
            n1 = n - (96 + (1 << 12));
            assert(n1 < (1 << 20));
            dbuf_putc(dbuf, ((n1 >> 16) + 112) | (type << 7));
            dbuf_putc(dbuf, n1 >> 8);
            dbuf_putc(dbuf, n1);
        }
        if (type != 0)
            dbuf_putc(dbuf, v);

#ifdef DUMP_TABLE_SIZE
        cw_len_count[dbuf->size - cw_start - 1]++;
#endif
        i += n + 1;
    }
#ifdef DUMP_TABLE_SIZE
    printf("script: %d entries [", cw_count);
    for(i = 0; i < 4; i++)
        printf(" %d", cw_len_count[i]);
    printf(" ], length=%d bytes\n", (int)dbuf->size);
#endif

    dump_byte_table(f, "unicode_script_table", dbuf->buf, dbuf->size);

    dbuf_free(dbuf);
}

void build_script_ext_table(FILE *f)
{
    int i, j, n, n1, script_ext_len;
    DynBuf dbuf_s, *dbuf = &dbuf_s;
#if defined(DUMP_TABLE_SIZE)
    int cw_count = 0;
#endif

    dbuf_init(dbuf);
    for(i = 0; i <= CHARCODE_MAX;) {
        script_ext_len = unicode_db[i].script_ext_len;
        j = i + 1;
        while (j <= CHARCODE_MAX &&
               unicode_db[j].script_ext_len == script_ext_len &&
               !memcmp(unicode_db[j].script_ext, unicode_db[i].script_ext,
                       script_ext_len)) {
            j++;
        }
        n = j - i;
#if defined(DUMP_TABLE_SIZE)
        cw_count++;
#endif
        n--;
        if (n < 128) {
            dbuf_putc(dbuf, n);
        } else if (n < 128 + (1 << 14)) {
            n1 = n - 128;
            assert(n1 < (1 << 14));
            dbuf_putc(dbuf, (n1 >> 8) + 128);
            dbuf_putc(dbuf, n1);
        } else {
            n1 = n - (128 + (1 << 14));
            assert(n1 < (1 << 22));
            dbuf_putc(dbuf, (n1 >> 16) + 128 + 64);
            dbuf_putc(dbuf, n1 >> 8);
            dbuf_putc(dbuf, n1);
        }
        dbuf_putc(dbuf, script_ext_len);
        for(j = 0; j < script_ext_len; j++)
            dbuf_putc(dbuf, unicode_db[i].script_ext[j]);
        i += n + 1;
    }
#ifdef DUMP_TABLE_SIZE
    printf("script_ext: %d entries", cw_count);
    printf(", length=%d bytes\n", (int)dbuf->size);
#endif

    dump_byte_table(f, "unicode_script_ext_table", dbuf->buf, dbuf->size);

    dbuf_free(dbuf);
}

/* the following properties are synthetized so no table is necessary */
#define PROP_TABLE_COUNT PROP_ASCII

void build_prop_list_table(FILE *f)
{
    int i;

    for(i = 0; i < PROP_TABLE_COUNT; i++) {
        if (i == PROP_ID_Start ||
            i == PROP_Case_Ignorable ||
            i == PROP_ID_Continue1) {
            /* already generated */
        } else {
            build_prop_table(f, unicode_prop_name[i], i, FALSE);
        }
    }

    fprintf(f, "typedef enum {\n");
    for(i = 0; i < PROP_COUNT; i++)
        fprintf(f, "    UNICODE_PROP_%s,\n", unicode_prop_name[i]);
    fprintf(f, "    UNICODE_PROP_COUNT,\n");
    fprintf(f, "} UnicodePropertyEnum;\n\n");

    i = PROP_ASCII_Hex_Digit;
    dump_name_table(f, "unicode_prop_name_table",
                    unicode_prop_name + i, PROP_XID_Start - i + 1,
                    unicode_prop_short_name + i);

    fprintf(f, "static const uint8_t * const unicode_prop_table[] = {\n");
    for(i = 0; i < PROP_TABLE_COUNT; i++) {
        fprintf(f, "    unicode_prop_%s_table,\n", unicode_prop_name[i]);
    }
    fprintf(f, "};\n\n");

    fprintf(f, "static const uint16_t unicode_prop_len_table[] = {\n");
    for(i = 0; i < PROP_TABLE_COUNT; i++) {
        fprintf(f, "    countof(unicode_prop_%s_table),\n", unicode_prop_name[i]);
    }
    fprintf(f, "};\n\n");
}

static BOOL is_emoji_hair_color(uint32_t c)
{
    return (c >= 0x1F9B0 && c <= 0x1F9B3);
}

#define EMOJI_MOD_NONE   0
#define EMOJI_MOD_TYPE1  1
#define EMOJI_MOD_TYPE2  2
#define EMOJI_MOD_TYPE2D 3

static BOOL mark_zwj_string(REStringList *sl, uint32_t *buf, int len, int mod_type, int *mod_pos,
                            int hc_pos, BOOL mark_flag)
{
    REString *p;
    int i, n_mod, i0, i1, hc_count, j;

#if 0
    if (mark_flag)
        printf("mod_type=%d\n", mod_type);
#endif
    
    switch(mod_type) {
    case EMOJI_MOD_NONE:
        n_mod = 1;
        break;
    case EMOJI_MOD_TYPE1:
        n_mod = 5;
        break;
    case EMOJI_MOD_TYPE2:
        n_mod = 25;
        break;
    case EMOJI_MOD_TYPE2D:
        n_mod = 20;
        break;
    default:
        assert(0);
    }
    if (hc_pos >= 0)
        hc_count = 4;
    else
        hc_count = 1;
    /* check that all the related strings are present */
    for(j = 0; j < hc_count; j++) {
        for(i = 0; i < n_mod; i++) {
            switch(mod_type) {
            case EMOJI_MOD_NONE:
                break;
            case EMOJI_MOD_TYPE1:
                buf[mod_pos[0]] = 0x1f3fb + i;
                break;
            case EMOJI_MOD_TYPE2:
            case EMOJI_MOD_TYPE2D:
                i0 = i / 5;
                i1 = i % 5;
                /* avoid identical values */
                if (mod_type == EMOJI_MOD_TYPE2D && i0 >= i1)
                    i0++;
                buf[mod_pos[0]] = 0x1f3fb + i0;
                buf[mod_pos[1]] = 0x1f3fb + i1;
                break;
            default:
                assert(0);
            }

            if (hc_pos >= 0)
                buf[hc_pos] = 0x1F9B0 + j;
            
            p = re_string_find(sl, len, buf, FALSE);
            if (!p)
                return FALSE;
            if (mark_flag)
                p->flags |= 1;
        }
    }
    return TRUE;
}

static void zwj_encode_string(DynBuf *dbuf, const uint32_t *buf, int len, int mod_type, int *mod_pos,
                              int hc_pos)
{
    int i, j;
    int c, code;
    uint32_t buf1[SEQ_MAX_LEN];
    
    j = 0;
    for(i = 0; i < len;) {
        c = buf[i++];
        if (c >= 0x2000 && c <= 0x2fff) {
            code = c - 0x2000;
        } else if (c >= 0x1f000 && c <= 0x1ffff) {
            code = c - 0x1f000 + 0x1000;
        } else {
            assert(0);
        }
        if (i < len && is_emoji_modifier(buf[i])) {
            /* modifier */
            code |= (mod_type << 13);
            i++;
        }
        if (i < len && buf[i] == 0xfe0f) {
            /* presentation selector present */
            code |= 0x8000;
            i++;
        }
        if (i < len) {
            /* zero width join */
            assert(buf[i] == 0x200d);
            i++;
        }
        buf1[j++] = code;
    }
    dbuf_putc(dbuf, j);
    for(i = 0; i < j; i++) {
        dbuf_putc(dbuf, buf1[i]);
        dbuf_putc(dbuf, buf1[i] >> 8);
    }
}

static void build_rgi_emoji_zwj_sequence(FILE *f, REStringList *sl)
{
    int mod_pos[2], mod_count, hair_color_pos, j, h;
    REString *p;
    uint32_t buf[SEQ_MAX_LEN];
    DynBuf dbuf;

#if 0
    {
        for(h = 0; h < sl->hash_size; h++) {
            for(p = sl->hash_table[h]; p != NULL; p = p->next) {
                for(j = 0; j < p->len; j++)
                    printf(" %04x", p->buf[j]);
                printf("\n");
            }
        }
        exit(0);
    }
#endif
    //    printf("rgi_emoji_zwj_sequence: n=%d\n", sl->n_strings);

    dbuf_init(&dbuf);
    
    /* avoid duplicating strings with emoji modifiers or hair colors */
    for(h = 0; h < sl->hash_size; h++) {
        for(p = sl->hash_table[h]; p != NULL; p = p->next) {
            if (p->flags) /* already examined */
                continue;
            mod_count = 0;
            hair_color_pos = -1;
            for(j = 0; j < p->len; j++) {
                if (is_emoji_modifier(p->buf[j])) {
                    assert(mod_count < 2);
                    mod_pos[mod_count++] = j;
                } else if (is_emoji_hair_color(p->buf[j])) {
                    hair_color_pos = j;
                }
                buf[j] = p->buf[j];
            }
            
            if (mod_count != 0 || hair_color_pos >= 0) {
                int mod_type;
                if (mod_count == 0)
                    mod_type = EMOJI_MOD_NONE;
                else if (mod_count == 1)
                    mod_type = EMOJI_MOD_TYPE1;
                else
                    mod_type = EMOJI_MOD_TYPE2;
                
                if (mark_zwj_string(sl, buf, p->len, mod_type, mod_pos, hair_color_pos, FALSE)) {
                    mark_zwj_string(sl, buf, p->len, mod_type, mod_pos, hair_color_pos, TRUE);
                } else if (mod_type == EMOJI_MOD_TYPE2) {
                    mod_type = EMOJI_MOD_TYPE2D;
                    if (mark_zwj_string(sl, buf, p->len, mod_type, mod_pos, hair_color_pos, FALSE)) {
                        mark_zwj_string(sl, buf, p->len, mod_type, mod_pos, hair_color_pos, TRUE);
                    } else {
                        dump_str("not_found", (int *)p->buf, p->len);
                        goto keep;
                    }
                }
                if (hair_color_pos >= 0)
                    buf[hair_color_pos] = 0x1f9b0;
                /* encode the string */
                zwj_encode_string(&dbuf, buf, p->len, mod_type, mod_pos, hair_color_pos);
            } else {
            keep:
                zwj_encode_string(&dbuf, buf, p->len, EMOJI_MOD_NONE, NULL, -1);
            }
        }
    }
    
    /* Encode */
    dump_byte_table(f, "unicode_rgi_emoji_zwj_sequence", dbuf.buf, dbuf.size);

    dbuf_free(&dbuf);
}

void build_sequence_prop_list_table(FILE *f)
{
    int i;
    fprintf(f, "typedef enum {\n");
    for(i = 0; i < SEQUENCE_PROP_COUNT; i++)
        fprintf(f, "    UNICODE_SEQUENCE_PROP_%s,\n", unicode_sequence_prop_name[i]);
    fprintf(f, "    UNICODE_SEQUENCE_PROP_COUNT,\n");
    fprintf(f, "} UnicodeSequencePropertyEnum;\n\n");

    dump_name_table(f, "unicode_sequence_prop_name_table",
                    unicode_sequence_prop_name, SEQUENCE_PROP_COUNT, NULL);

    dump_byte_table(f, "unicode_rgi_emoji_tag_sequence", rgi_emoji_tag_sequence.buf, rgi_emoji_tag_sequence.size);

    build_rgi_emoji_zwj_sequence(f, &rgi_emoji_zwj_sequence);
}

#ifdef USE_TEST
#ifdef PROFILE
static int64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}
#endif


void check_flags(void)
{
    int c;
    BOOL flag_ref, flag;
    for(c = 0; c <= CHARCODE_MAX; c++) {
        flag_ref = get_prop(c, PROP_Cased);
        flag = !!lre_is_cased(c);
        if (flag != flag_ref) {
            printf("ERROR: c=%05x cased=%d ref=%d\n",
                   c, flag, flag_ref);
            exit(1);
        }

        flag_ref = get_prop(c, PROP_Case_Ignorable);
        flag = !!lre_is_case_ignorable(c);
        if (flag != flag_ref) {
            printf("ERROR: c=%05x case_ignorable=%d ref=%d\n",
                   c, flag, flag_ref);
            exit(1);
        }

        flag_ref = get_prop(c, PROP_ID_Start);
        flag = !!lre_is_id_start(c);
        if (flag != flag_ref) {
            printf("ERROR: c=%05x id_start=%d ref=%d\n",
                   c, flag, flag_ref);
            exit(1);
        }

        flag_ref = get_prop(c, PROP_ID_Continue);
        flag = !!lre_is_id_continue(c);
        if (flag != flag_ref) {
            printf("ERROR: c=%05x id_cont=%d ref=%d\n",
                   c, flag, flag_ref);
            exit(1);
        }
    }
#ifdef PROFILE
    {
        int64_t ti, count;
        ti = get_time_ns();
        count = 0;
        for(c = 0x20; c <= 0xffff; c++) {
            flag_ref = get_prop(c, PROP_ID_Start);
            flag = !!lre_is_id_start(c);
            assert(flag == flag_ref);
            count++;
        }
        ti = get_time_ns() - ti;
        printf("flags time=%0.1f ns/char\n",
               (double)ti / count);
    }
#endif
}


#endif
