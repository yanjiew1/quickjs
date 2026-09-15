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
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>

#include "cutils.h"
#include "src/unicode-gen/internal.h"

/* Ideas:
   - Generalize run length encoding + index for all tables
   - remove redundant tables for ID_start, ID_continue, Case_Ignorable, Cased

   Case conversion:
   - use a single entry for consecutive U/LF runs
   - allow EXT runs of length > 1

   Decomposition:
   - Greek lower case (+1f10/1f10) ?
   - allow holes in B runs
   - suppress more upper / lower case redundancy
*/

static void *mallocz(size_t size)
{
    void *ptr;
    ptr = malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

static const char *get_field(const char *p, int n)
{
    int i;
    for(i = 0; i < n; i++) {
        while (*p != ';' && *p != '\0')
            p++;
        if (*p == '\0')
            return NULL;
        p++;
    }
    return p;
}

static const char *get_field_buf(char *buf, size_t buf_size, const char *p,
                                 int n)
{
    char *q;
    p = get_field(p, n);
    q = buf;
    while (*p != ';' && *p != '\0') {
        if ((q - buf) < buf_size - 1)
            *q++ = *p;
        p++;
    }
    *q = '\0';
    return buf;
}

static void add_char(int **pbuf, int *psize, int *plen, int c)
{
    int len, size, *buf;
    buf = *pbuf;
    size = *psize;
    len = *plen;
    if (len >= size) {
        size = *psize;
        size = max_int(len + 1, size * 3 / 2);
        buf = realloc(buf, sizeof(buf[0]) * size);
        *pbuf = buf;
        *psize = size;
    }
    buf[len++] = c;
    *plen = len;
}

int *unicode_gen_get_field_str(int *plen, const char *str, int n)
{
    const char *p;
    int *buf, len, size;
    p = get_field(str, n);
    if (!p) {
        *plen = 0;
        return NULL;
    }
    len = 0;
    size = 0;
    buf = NULL;
    for(;;) {
        while (isspace(*p))
            p++;
        if (!isxdigit(*p))
            break;
        add_char(&buf, &size, &len, strtoul(p, (char **)&p, 16));
    }
    *plen = len;
    return buf;
}

char *unicode_gen_get_line(char *buf, int buf_size, FILE *f)
{
    int len;
    if (!fgets(buf, buf_size, f))
        return NULL;
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';
    return buf;
}

static uint32_t re_string_hash(int len, const uint32_t *buf)
{
    int i;
    uint32_t h;
    h = 1;
    for(i = 0; i < len; i++)
        h = h * 263 + buf[i];
    return h * 0x61C88647;
}

void unicode_gen_string_list_init(REStringList *s)
{
    s->n_strings = 0;
    s->hash_size = 0;
    s->hash_bits = 0;
    s->hash_table = NULL;
}

void unicode_gen_string_list_free(REStringList *s)
{
    REString *p, *p_next;
    int i;
    for(i = 0; i < s->hash_size; i++) {
        for(p = s->hash_table[i]; p != NULL; p = p_next) {
            p_next = p->next;
            free(p);
        }
    }
    free(s->hash_table);
}

static void lre_print_char(int c, BOOL is_range)
{
    if (c == '\'' || c == '\\' ||
        (is_range && (c == '-' || c == ']'))) {
        printf("\\%c", c);
    } else if (c >= ' ' && c <= 126) {
        printf("%c", c);
    } else {
        printf("\\u{%04x}", c);
    }
}

static __maybe_unused void re_string_list_dump(const char *str, const REStringList *s)
{
    REString *p;
    int i, j, k;

    printf("%s:\n", str);
    
    j = 0;
    for(i = 0; i < s->hash_size; i++) {
        for(p = s->hash_table[i]; p != NULL; p = p->next) {
            printf("  %d/%d: '", j, s->n_strings);
            for(k = 0; k < p->len; k++) {
                lre_print_char(p->buf[k], FALSE);
            }
            printf("'\n");
            j++;
        }
    }
}

static REString *re_string_find2(REStringList *s, int len, const uint32_t *buf,
                                 uint32_t h0, BOOL add_flag)
{
    uint32_t h = 0; /* avoid warning */
    REString *p;
    if (s->n_strings != 0) {
        h = h0 >> (32 - s->hash_bits);
        for(p = s->hash_table[h]; p != NULL; p = p->next) {
            if (p->hash == h0 && p->len == len &&
                !memcmp(p->buf, buf, len * sizeof(buf[0]))) {
                return p;
            }
        }
    }
    /* not found */
    if (!add_flag)
        return NULL;
    /* increase the size of the hash table if needed */
    if (unlikely((s->n_strings + 1) > s->hash_size)) {
        REString **new_hash_table, *p_next;
        int new_hash_bits, i;
        uint32_t new_hash_size;
        new_hash_bits = max_int(s->hash_bits + 1, 4);
        new_hash_size = 1 << new_hash_bits;
        new_hash_table = malloc(sizeof(new_hash_table[0]) * new_hash_size);
        if (!new_hash_table)
            return NULL;
        memset(new_hash_table, 0, sizeof(new_hash_table[0]) * new_hash_size);
        for(i = 0; i < s->hash_size; i++) {
            for(p = s->hash_table[i]; p != NULL; p = p_next) {
                p_next = p->next;
                h = p->hash >> (32 - new_hash_bits);
                p->next = new_hash_table[h];
                new_hash_table[h] = p;
            }
        }
        free(s->hash_table);
        s->hash_bits = new_hash_bits;
        s->hash_size = new_hash_size;
        s->hash_table = new_hash_table;
        h = h0 >> (32 - s->hash_bits);
    }

    p = malloc(sizeof(REString) + len * sizeof(buf[0]));
    if (!p)
        return NULL;
    p->next = s->hash_table[h];
    s->hash_table[h] = p;
    s->n_strings++;
    p->hash = h0;
    p->len = len;
    p->flags = 0;
    memcpy(p->buf, buf, sizeof(buf[0]) * len);
    return p;
}

REString *unicode_gen_string_find(REStringList *s, int len,
                                  const uint32_t *buf, BOOL add_flag)
{
    uint32_t h0;
    h0 = re_string_hash(len, buf);
    return re_string_find2(s, len, buf, h0, add_flag);
}

void unicode_gen_string_add(REStringList *s, int len, const uint32_t *buf)
{
    unicode_gen_string_find(s, len, buf, TRUE);
}

#define UNICODE_GENERAL_CATEGORY

const char *const unicode_gc_name[] = {
#define DEF(id, str) #id,
#include "unicode_gen_def.h"
#undef DEF
};

const char *const unicode_gc_short_name[] = {
#define DEF(id, str) str,
#include "unicode_gen_def.h"
#undef DEF
};

#undef UNICODE_GENERAL_CATEGORY

#define UNICODE_SCRIPT

const char *const unicode_script_name[] = {
#define DEF(id, str) #id,
#include "unicode_gen_def.h"
#undef DEF
};

const char *const unicode_script_short_name[] = {
#define DEF(id, str) str,
#include "unicode_gen_def.h"
#undef DEF
};

#undef UNICODE_SCRIPT

#define UNICODE_PROP_LIST

const char *const unicode_prop_name[] = {
#define DEF(id, str) #id,
#include "unicode_gen_def.h"
#undef DEF
};

const char *const unicode_prop_short_name[] = {
#define DEF(id, str) str,
#include "unicode_gen_def.h"
#undef DEF
};

#undef UNICODE_PROP_LIST

#define UNICODE_SEQUENCE_PROP_LIST

const char *const unicode_sequence_prop_name[] = {
#define DEF(id) #id,
#include "unicode_gen_def.h"
#undef DEF
};

#undef UNICODE_SEQUENCE_PROP_LIST

typedef struct {
    int count;
    int size;
    int *tab;
} UnicodeSequenceProperties;

static int find_name(const char *const *tab, int tab_len, const char *name)
{
    int i, len, name_len;
    const char *p, *r;

    name_len = strlen(name);
    for(i = 0; i < tab_len; i++) {
        p = tab[i];
        for(;;) {
            r = strchr(p, ',');
            if (!r)
                len = strlen(p);
            else
                len = r - p;
            if (len == name_len && memcmp(p, name, len) == 0)
                return i;
            if (!r)
                break;
            p = r + 1;
        }
    }
    return -1;
}

static BOOL get_prop(const UnicodeGenState *s, uint32_t c, int prop_idx)
{
    return unicode_gen_get_prop(s, c, prop_idx);
}

static void set_prop(UnicodeGenState *s, uint32_t c, int prop_idx, int val)
{
    unicode_gen_set_prop(s, c, prop_idx, val);
}

static void parse_unicode_data(UnicodeGenState *s, const char *filename)
{
    FILE *f;
    char line[1024];
    char buf1[256];
    const char *p;
    int code, lc, uc, last_code;
    CCInfo *ci, *tab = s->db;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    last_code = 0;
    for(;;) {
        if (!unicode_gen_get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#')
            continue;

        p = get_field(line, 0);
        if (!p)
            continue;
        code = strtoul(p, NULL, 16);
        lc = 0;
        uc = 0;

        p = get_field(line, 12);
        if (p && *p != ';') {
            uc = strtoul(p, NULL, 16);
        }

        p = get_field(line, 13);
        if (p && *p != ';') {
            lc = strtoul(p, NULL, 16);
        }
        ci = &tab[code];
        if (uc > 0 || lc > 0) {
            assert(code <= CHARCODE_MAX);
            if (uc > 0) {
                assert(ci->u_len == 0);
                ci->u_len = 1;
                ci->u_data[0] = uc;
            }
            if (lc > 0) {
                assert(ci->l_len == 0);
                ci->l_len = 1;
                ci->l_data[0] = lc;
            }
        }

        {
            int i;
            get_field_buf(buf1, sizeof(buf1), line, 2);
            i = find_name(unicode_gc_name, countof(unicode_gc_name), buf1);
            if (i < 0) {
                fprintf(stderr, "General category '%s' not found\n",
                        buf1);
                exit(1);
            }
            ci->general_category = i;
        }

        p = get_field(line, 3);
        if (p && *p != ';' && *p != '\0') {
            int cc;
            cc = strtoul(p, NULL, 0);
            if (cc != 0) {
                assert(code <= CHARCODE_MAX);
                ci->combining_class = cc;
                //                printf("%05x: %d\n", code, ci->combining_class);
            }
        }

        p = get_field(line, 5);
        if (p && *p != ';' && *p != '\0') {
            int size;
            assert(code <= CHARCODE_MAX);
            ci->is_compat = 0;
            if (*p == '<') {
                while (*p != '\0' && *p != '>')
                    p++;
                if (*p == '>')
                    p++;
                ci->is_compat = 1;
            }
            size = 0;
            for(;;) {
                while (isspace(*p))
                    p++;
                if (!isxdigit(*p))
                    break;
                add_char(&ci->decomp_data, &size, &ci->decomp_len, strtoul(p, (char **)&p, 16));
            }
#if 0
            {
                int i;
                static int count, d_count;

                printf("%05x: %c", code, ci->is_compat ? 'C': ' ');
                for(i = 0; i < ci->decomp_len; i++)
                    printf(" %05x", ci->decomp_data[i]);
                printf("\n");
                count++;
                d_count += ci->decomp_len;
                //                printf("%d %d\n", count, d_count);
            }
#endif
        }

        p = get_field(line, 9);
        if (p && *p == 'Y') {
            set_prop(s, code, PROP_Bidi_Mirrored, 1);
        }

        /* handle ranges */
        get_field_buf(buf1, sizeof(buf1), line, 1);
        if (strstr(buf1, " Last>")) {
            int i;
            //            printf("range: 0x%x-%0x\n", last_code, code);
            assert(ci->decomp_len == 0);
            assert(ci->script_ext_len == 0);
            for(i = last_code + 1; i < code; i++) {
                tab[i] = *ci;
            }
        }
        last_code = code;
    }

    fclose(f);
}

static void parse_special_casing(CCInfo *tab, const char *filename)
{
    FILE *f;
    char line[1024];
    const char *p;
    int code;
    CCInfo *ci;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!unicode_gen_get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#')
            continue;

        p = get_field(line, 0);
        if (!p)
            continue;
        code = strtoul(p, NULL, 16);
        assert(code <= CHARCODE_MAX);
        ci = &tab[code];

        p = get_field(line, 4);
        if (p) {
            /* locale dependent casing */
            while (isspace(*p))
                p++;
            if (*p != '#' && *p != '\0')
                continue;
        }


        p = get_field(line, 1);
        if (p && *p != ';') {
            ci->l_len = 0;
            for(;;) {
                while (isspace(*p))
                    p++;
                if (*p == ';')
                    break;
                assert(ci->l_len < CC_LEN_MAX);
                ci->l_data[ci->l_len++] = strtoul(p, (char **)&p, 16);
            }

            if (ci->l_len == 1 && ci->l_data[0] == code)
                ci->l_len = 0;
        }

        p = get_field(line, 3);
        if (p && *p != ';') {
            ci->u_len = 0;
            for(;;) {
                while (isspace(*p))
                    p++;
                if (*p == ';')
                    break;
                assert(ci->u_len < CC_LEN_MAX);
                ci->u_data[ci->u_len++] = strtoul(p, (char **)&p, 16);
            }

            if (ci->u_len == 1 && ci->u_data[0] == code)
                ci->u_len = 0;
        }
    }

    fclose(f);
}

static void parse_case_folding(CCInfo *tab, const char *filename)
{
    FILE *f;
    char line[1024];
    const char *p;
    int code, status;
    CCInfo *ci;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!unicode_gen_get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#')
            continue;

        p = get_field(line, 0);
        if (!p)
            continue;
        code = strtoul(p, NULL, 16);
        assert(code <= CHARCODE_MAX);
        ci = &tab[code];

        p = get_field(line, 1);
        if (!p)
            continue;
        /* locale dependent casing */
        while (isspace(*p))
            p++;
        status = *p;
        if (status != 'C' && status != 'S' && status != 'F')
            continue;

        p = get_field(line, 2);
        assert(p != NULL);
        if (status == 'S') {
            /* we always select the simple case folding and assume it
             * comes after the full case folding case */
            assert(ci->f_len >= 2);
            ci->f_len = 0;
        } else {
            assert(ci->f_len == 0);
        }
        for(;;) {
            while (isspace(*p))
                p++;
            if (*p == ';')
                break;
            assert(ci->l_len < CC_LEN_MAX);
            ci->f_data[ci->f_len++] = strtoul(p, (char **)&p, 16);
        }
    }

    fclose(f);
}

static void parse_composition_exclusions(UnicodeGenState *s,
                                         const char *filename)
{
    FILE *f;
    char line[4096], *p;
    uint32_t c0;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!unicode_gen_get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        assert(c0 > 0 && c0 <= CHARCODE_MAX);
        s->db[c0].is_excluded = TRUE;
    }
    fclose(f);
}

static void parse_derived_core_properties(UnicodeGenState *s,
                                          const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q;
    uint32_t c0, c1, c;
    int i;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!unicode_gen_get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
        } else {
            c1 = c0;
        }
        assert(c1 <= CHARCODE_MAX);
        p += strspn(p, " \t");
        if (*p == ';') {
            p++;
            p += strspn(p, " \t");
            q = buf;
            while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t' && *p != ';') {
                if ((q - buf) < sizeof(buf) - 1)
                    *q++ = *p;
                p++;
            }
            *q = '\0';
            i = find_name(unicode_prop_name,
                          countof(unicode_prop_name), buf);
            if (i < 0) {
                if (!strcmp(buf, "Grapheme_Link"))
                    goto next;
                fprintf(stderr, "Property not found: %s\n", buf);
                exit(1);
            }
            for(c = c0; c <= c1; c++) {
                set_prop(s, c, i, 1);
            }
next: ;
        }
    }
    fclose(f);
}

static void parse_derived_norm_properties(UnicodeGenState *s,
                                          const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q;
    uint32_t c0, c1, c;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!unicode_gen_get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
        } else {
            c1 = c0;
        }
        assert(c1 <= CHARCODE_MAX);
        p += strspn(p, " \t");
        if (*p == ';') {
            p++;
            p += strspn(p, " \t");
            q = buf;
            while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t') {
                if ((q - buf) < sizeof(buf) - 1)
                    *q++ = *p;
                p++;
            }
            *q = '\0';
            if (!strcmp(buf, "Changes_When_NFKC_Casefolded")) {
                for(c = c0; c <= c1; c++) {
                    set_prop(s, c, PROP_Changes_When_NFKC_Casefolded, 1);
                }
            }
        }
    }
    fclose(f);
}

static void parse_prop_list(UnicodeGenState *s, const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q;
    uint32_t c0, c1, c;
    int i;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!unicode_gen_get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
        } else {
            c1 = c0;
        }
        assert(c1 <= CHARCODE_MAX);
        p += strspn(p, " \t");
        if (*p == ';') {
            p++;
            p += strspn(p, " \t");
            q = buf;
            while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t') {
                if ((q - buf) < sizeof(buf) - 1)
                    *q++ = *p;
                p++;
            }
            *q = '\0';
            i = find_name(unicode_prop_name,
                          countof(unicode_prop_name), buf);
            if (i < 0) {
                fprintf(stderr, "Property not found: %s\n", buf);
                exit(1);
            }
            for(c = c0; c <= c1; c++) {
                set_prop(s, c, i, 1);
            }
        }
    }
    fclose(f);
}

#define SEQ_MAX_LEN 16

static BOOL is_emoji_modifier(uint32_t c)
{
    return (c >= 0x1f3fb && c <= 0x1f3ff);
}

static void add_sequence_prop(UnicodeGenState *s, int idx,
                              int seq_len, int *seq)
{
    int i;
    
    assert(idx < SEQUENCE_PROP_COUNT);
    switch(idx) {
    case SEQUENCE_PROP_Basic_Emoji:
        /* convert to 2 properties lists */
        if (seq_len == 1) {
            set_prop(s, seq[0], PROP_Basic_Emoji1, 1);
        } else if (seq_len == 2 && seq[1] == 0xfe0f) {
            set_prop(s, seq[0], PROP_Basic_Emoji2, 1);
        } else {
            abort();
        }
        break;
    case SEQUENCE_PROP_RGI_Emoji_Modifier_Sequence:
        assert(seq_len == 2);
        assert(is_emoji_modifier(seq[1]));
        assert(get_prop(s, seq[0], PROP_Emoji_Modifier_Base));
        set_prop(s, seq[0], PROP_RGI_Emoji_Modifier_Sequence, 1);
        break;
    case SEQUENCE_PROP_RGI_Emoji_Flag_Sequence:
        {
            int code;
            assert(seq_len == 2);
            assert(seq[0] >= 0x1F1E6 && seq[0] <= 0x1F1FF);
            assert(seq[1] >= 0x1F1E6 && seq[1] <= 0x1F1FF);
            code = (seq[0] - 0x1F1E6) * 26 + (seq[1] - 0x1F1E6);
            /* XXX: would be more compact with a simple bitmap -> 676 bits */
            set_prop(s, code, PROP_RGI_Emoji_Flag_Sequence, 1);
        }
        break;
    case SEQUENCE_PROP_RGI_Emoji_ZWJ_Sequence:
        unicode_gen_string_add(&s->rgi_emoji_zwj_sequence, seq_len,
                               (uint32_t *)seq);
        break;
    case SEQUENCE_PROP_RGI_Emoji_Tag_Sequence:
        {
            assert(seq_len >= 3);
            assert(seq[0] == 0x1F3F4);
            assert(seq[seq_len - 1] == 0xE007F);
            for(i = 1; i < seq_len - 1; i++) {
                assert(seq[i] >= 0xe0001 && seq[i] <= 0xe007e);
                dbuf_putc(&s->rgi_emoji_tag_sequence,
                          seq[i] - 0xe0000);
            }
            dbuf_putc(&s->rgi_emoji_tag_sequence, 0);
        }
        break;
    case SEQUENCE_PROP_Emoji_Keycap_Sequence:
        assert(seq_len == 3);
        assert(seq[1] == 0xfe0f);
        assert(seq[2] == 0x20e3);
        set_prop(s, seq[0], PROP_Emoji_Keycap_Sequence, 1);
        break;
    default:
        assert(0);
    }
}

static void parse_sequence_prop_list(UnicodeGenState *s,
                                     const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q, *p_start;
    uint32_t c0, c1, c;
    int idx, seq_len;
    int seq[SEQ_MAX_LEN];
    
    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!unicode_gen_get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        p_start = p;

        /* find the sequence property name */
        p = strchr(p, ';');
        if (!p)
            continue;
        p++;
        p += strspn(p, " \t");
        q = buf;
        while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t' && *p != ';') {
            if ((q - buf) < sizeof(buf) - 1)
                *q++ = *p;
            p++;
        }
        *q = '\0';
        idx = find_name(unicode_sequence_prop_name,
                      countof(unicode_sequence_prop_name), buf);
        if (idx < 0) {
            fprintf(stderr, "Property not found: %s\n", buf);
            exit(1);
        }
        
        p = p_start;
        c0 = strtoul(p, (char **)&p, 16);
        assert(c0 <= CHARCODE_MAX);
        
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
            assert(c1 <= CHARCODE_MAX);
            for(c = c0; c <= c1; c++) {
                seq[0] = c;
                add_sequence_prop(s, idx, 1, seq);
            }
        } else {
            seq_len = 0;
            seq[seq_len++] = c0;
            for(;;) {
                while (isspace(*p))
                    p++;
                if (*p == ';' || *p == '\0')
                    break;
                c0 = strtoul(p, (char **)&p, 16);
                assert(c0 <= CHARCODE_MAX);
                assert(seq_len < countof(seq));
                seq[seq_len++] = c0;
            }
            add_sequence_prop(s, idx, seq_len, seq);
        }
    }
    fclose(f);
}

static void parse_scripts(UnicodeGenState *s, const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q;
    uint32_t c0, c1, c;
    int i;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!unicode_gen_get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
        } else {
            c1 = c0;
        }
        assert(c1 <= CHARCODE_MAX);
        p += strspn(p, " \t");
        if (*p == ';') {
            p++;
            p += strspn(p, " \t");
            q = buf;
            while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t') {
                if ((q - buf) < sizeof(buf) - 1)
                    *q++ = *p;
                p++;
            }
            *q = '\0';
            i = find_name(unicode_script_name,
                          countof(unicode_script_name), buf);
            if (i < 0) {
                fprintf(stderr, "Unknown script: '%s'\n", buf);
                exit(1);
            }
            for(c = c0; c <= c1; c++)
                s->db[c].script = i;
        }
    }
    fclose(f);
}

static void parse_script_extensions(UnicodeGenState *s,
                                    const char *filename)
{
    FILE *f;
    char line[4096], *p, buf[256], *q;
    uint32_t c0, c1, c;
    int i;
    uint8_t script_ext[255];
    int script_ext_len;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    for(;;) {
        if (!unicode_gen_get_line(line, sizeof(line), f))
            break;
        p = line;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '@' || *p == '\0')
            continue;
        c0 = strtoul(p, (char **)&p, 16);
        if (*p == '.' && p[1] == '.') {
            p += 2;
            c1 = strtoul(p, (char **)&p, 16);
        } else {
            c1 = c0;
        }
        assert(c1 <= CHARCODE_MAX);
        p += strspn(p, " \t");
        script_ext_len = 0;
        if (*p == ';') {
            p++;
            for(;;) {
                p += strspn(p, " \t");
                q = buf;
                while (*p != '\0' && *p != ' ' && *p != '#' && *p != '\t') {
                    if ((q - buf) < sizeof(buf) - 1)
                        *q++ = *p;
                    p++;
                }
                *q = '\0';
                if (buf[0] == '\0')
                    break;
                i = find_name(unicode_script_short_name,
                              countof(unicode_script_short_name), buf);
                if (i < 0) {
                    fprintf(stderr, "Script not found: %s\n", buf);
                    exit(1);
                }
                assert(script_ext_len < sizeof(script_ext));
                script_ext[script_ext_len++] = i;
            }
            for(c = c0; c <= c1; c++) {
                CCInfo *ci = &s->db[c];
                ci->script_ext_len = script_ext_len;
                ci->script_ext = malloc(sizeof(ci->script_ext[0]) * script_ext_len);
                for(i = 0; i < script_ext_len; i++)
                    ci->script_ext[i] = script_ext[i];
            }
        }
    }
    fclose(f);
}

void unicode_gen_dump_cc_info(const CCInfo *ci, int i)
{
    int j;
    printf("%05x:", i);
    if (ci->u_len != 0) {
        printf(" U:");
        for(j = 0; j < ci->u_len; j++)
            printf(" %05x", ci->u_data[j]);
    }
    if (ci->l_len != 0) {
        printf(" L:");
        for(j = 0; j < ci->l_len; j++)
            printf(" %05x", ci->l_data[j]);
    }
    if (ci->f_len != 0) {
        printf(" F:");
        for(j = 0; j < ci->f_len; j++)
            printf(" %05x", ci->f_data[j]);
    }
    printf("\n");
}

static __maybe_unused void dump_unicode_data(CCInfo *tab)
{
    int i;
    CCInfo *ci;
    for(i = 0; i <= CHARCODE_MAX; i++) {
        ci = &tab[i];
        if (ci->u_len != 0 || ci->l_len != 0 || ci->f_len != 0) {
            unicode_gen_dump_cc_info(ci, i);
        }
    }
}

int main(int argc, char *argv[])
{
    UnicodeGenState generator_state = { 0 };
    const char *unicode_db_path, *outfilename;
    char filename[1024];
    int arg = 1;

    if (arg >= argc || (!strcmp(argv[arg], "-h") || !strcmp(argv[arg], "--help"))) {
        printf("usage: %s PATH [OUTPUT]\n"
               "  PATH    path to the Unicode database directory\n"
               "  OUTPUT  name of the output file.  If omitted, a self test is performed\n"
               "          using the files from the Unicode library\n"
               , argv[0]);
        return 1;
    }
    unicode_db_path = argv[arg++];
    outfilename = NULL;
    if (arg < argc)
        outfilename = argv[arg++];

    generator_state.db = mallocz(sizeof(generator_state.db[0]) *
                                 (CHARCODE_MAX + 1));
    unicode_gen_string_list_init(&generator_state.rgi_emoji_zwj_sequence);
    dbuf_init(&generator_state.rgi_emoji_tag_sequence);

    snprintf(filename, sizeof(filename), "%s/UnicodeData.txt", unicode_db_path);

    parse_unicode_data(&generator_state, filename);

    snprintf(filename, sizeof(filename), "%s/SpecialCasing.txt", unicode_db_path);
    parse_special_casing(generator_state.db, filename);

    snprintf(filename, sizeof(filename), "%s/CaseFolding.txt", unicode_db_path);
    parse_case_folding(generator_state.db, filename);

    snprintf(filename, sizeof(filename), "%s/CompositionExclusions.txt", unicode_db_path);
    parse_composition_exclusions(&generator_state, filename);

    snprintf(filename, sizeof(filename), "%s/DerivedCoreProperties.txt", unicode_db_path);
    parse_derived_core_properties(&generator_state, filename);

    snprintf(filename, sizeof(filename), "%s/DerivedNormalizationProps.txt", unicode_db_path);
    parse_derived_norm_properties(&generator_state, filename);

    snprintf(filename, sizeof(filename), "%s/PropList.txt", unicode_db_path);
    parse_prop_list(&generator_state, filename);

    snprintf(filename, sizeof(filename), "%s/Scripts.txt", unicode_db_path);
    parse_scripts(&generator_state, filename);

    snprintf(filename, sizeof(filename), "%s/ScriptExtensions.txt",
             unicode_db_path);
    parse_script_extensions(&generator_state, filename);

    snprintf(filename, sizeof(filename), "%s/emoji-data.txt",
             unicode_db_path);
    parse_prop_list(&generator_state, filename);

    snprintf(filename, sizeof(filename), "%s/emoji-sequences.txt",
             unicode_db_path);
    parse_sequence_prop_list(&generator_state, filename);

    snprintf(filename, sizeof(filename), "%s/emoji-zwj-sequences.txt",
             unicode_db_path);
    parse_sequence_prop_list(&generator_state, filename);

    //    dump_unicode_data(generator_state.db);

    if (!outfilename) {
#ifdef CONFIG_UNICODE_TEST
        unicode_gen_run_self_tests(&generator_state, unicode_db_path);
#else
        fprintf(stderr, "Tests are not compiled\n");
        exit(1);
#endif
    } else
    {
        FILE *fo = fopen(outfilename, "wb");
        UnicodeGenOutput out = { .file = fo };

        if (!fo) {
            perror(outfilename);
            exit(1);
        }
        fprintf(fo,
                "/* Compressed unicode tables */\n"
                "/* Automatically generated file - do not edit */\n"
                "\n"
                "#include <stdint.h>\n"
                "\n");
        fprintf(fo, "#ifdef LIBUNICODE_TABLE_CASE\n\n");
        unicode_gen_emit_case(&out, &generator_state);
        unicode_gen_prepare_properties(&generator_state);
        unicode_gen_emit_case_flags(&out, &generator_state);
        fprintf(fo, "#endif /* LIBUNICODE_TABLE_CASE */\n\n");
        fprintf(fo, "#ifdef LIBUNICODE_TABLE_PROPERTY\n\n");
        unicode_gen_emit_property_flags(&out, &generator_state);
        fprintf(fo, "#endif /* LIBUNICODE_TABLE_PROPERTY */\n\n");
        fprintf(fo, "#ifdef CONFIG_ALL_UNICODE\n\n");
        fprintf(fo, "#ifdef LIBUNICODE_TABLE_NORMALIZE\n\n");
        unicode_gen_emit_normalization(&out, &generator_state);
        fprintf(fo, "#endif /* LIBUNICODE_TABLE_NORMALIZE */\n\n");
        fprintf(fo, "#ifdef LIBUNICODE_TABLE_PROPERTY\n\n");
        unicode_gen_emit_properties(&out, &generator_state);
        fprintf(fo, "#endif /* LIBUNICODE_TABLE_PROPERTY */\n\n");
        fprintf(fo, "#endif /* CONFIG_ALL_UNICODE */\n");
        fprintf(fo, "/* %u tables / %u bytes, %u index / %u bytes */\n",
                out.total_tables, out.total_table_bytes,
                out.total_index, out.total_index_bytes);
        fclose(fo);
    }
    unicode_gen_string_list_free(&generator_state.rgi_emoji_zwj_sequence);
    return 0;
}
