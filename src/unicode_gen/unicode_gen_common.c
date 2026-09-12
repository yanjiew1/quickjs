#include "unicode_gen_common.h"

uint32_t total_tables;
uint32_t total_table_bytes;
uint32_t total_index;
uint32_t total_index_bytes;

CCInfo *unicode_db;
REStringList rgi_emoji_zwj_sequence;
DynBuf rgi_emoji_tag_sequence;

#define UNICODE_GENERAL_CATEGORY
const char *unicode_gc_name[] = {
#define DEF(id, str) #id,
#include "unicode_gen_def.h"
#undef DEF
};

const char *unicode_gc_short_name[] = {
#define DEF(id, str) str,
#include "unicode_gen_def.h"
#undef DEF
};
#undef UNICODE_GENERAL_CATEGORY

#define UNICODE_SCRIPT
const char *unicode_script_name[] = {
#define DEF(id, str) #id,
#include "unicode_gen_def.h"
#undef DEF
};

const char *unicode_script_short_name[] = {
#define DEF(id, str) str,
#include "unicode_gen_def.h"
#undef DEF
};
#undef UNICODE_SCRIPT

#define UNICODE_PROP_LIST
const char *unicode_prop_name[] = {
#define DEF(id, str) #id,
#include "unicode_gen_def.h"
#undef DEF
};

const char *unicode_prop_short_name[] = {
#define DEF(id, str) str,
#include "unicode_gen_def.h"
#undef DEF
};
#undef UNICODE_PROP_LIST

#define UNICODE_SEQUENCE_PROP_LIST
const char *unicode_sequence_prop_name[] = {
#define DEF(id) #id,
#include "unicode_gen_def.h"
#undef DEF
};
#undef UNICODE_SEQUENCE_PROP_LIST

void *mallocz(size_t size)
{
    void *ptr;
    ptr = malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

const char *get_field(const char *p, int n)
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

const char *get_field_buf(char *buf, size_t buf_size, const char *p, int n)
{
    char *q;
    p = get_field(p, n);
    q = buf;
    while (*p != ';' && *p != '\0') {
        if ((size_t)(q - buf) < buf_size - 1)
            *q++ = *p;
        p++;
    }
    *q = '\0';
    return buf;
}

void add_char(int **pbuf, int *psize, int *plen, int c)
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

int *get_field_str(int *plen, const char *str, int n)
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
        while (isspace((unsigned char)*p))
            p++;
        if (!isxdigit((unsigned char)*p))
            break;
        add_char(&buf, &size, &len, strtoul(p, (char **)&p, 16));
    }
    *plen = len;
    return buf;
}

char *get_line(char *buf, int buf_size, FILE *f)
{
    int len;
    if (!fgets(buf, buf_size, f))
        return NULL;
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';
    return buf;
}

int find_name(const char **tab, int tab_len, const char *name)
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

BOOL get_prop(uint32_t c, int prop_idx)
{
    return (unicode_db[c].prop_bitmap_tab[prop_idx >> 5] >> (prop_idx & 0x1f)) & 1;
}

void set_prop(uint32_t c, int prop_idx, int val)
{
    uint32_t mask;
    mask = 1U << (prop_idx & 0x1f);
    if (val)
        unicode_db[c].prop_bitmap_tab[prop_idx >> 5] |= mask;
    else
        unicode_db[c].prop_bitmap_tab[prop_idx >> 5]  &= ~mask;
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

void re_string_list_init(REStringList *s)
{
    s->n_strings = 0;
    s->hash_size = 0;
    s->hash_bits = 0;
    s->hash_table = NULL;
}

void re_string_list_free(REStringList *s)
{
    REString *p, *p_next;
    int i;
    for(i = 0; i < (int)s->hash_size; i++) {
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
    for(i = 0; i < (int)s->hash_size; i++) {
        for(p = s->hash_table[i]; p != NULL; p = p->next) {
            printf("  %d/%d: '", j, s->n_strings);
            for(k = 0; k < (int)p->len; k++) {
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
            if (p->hash == h0 && (int)p->len == len &&
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
        for(i = 0; i < (int)s->hash_size; i++) {
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

REString *re_string_find(REStringList *s, int len, const uint32_t *buf,
                         BOOL add_flag)
{
    uint32_t h0;
    h0 = re_string_hash(len, buf);
    return re_string_find2(s, len, buf, h0, add_flag);
}

void re_string_add(REStringList *s, int len, const uint32_t *buf)
{
    re_string_find(s, len, buf, TRUE);
}

int tabcmp(const int *tab1, const int *tab2, int n)
{
    int i;
    for(i = 0; i < n; i++) {
        if (tab1[i] != tab2[i])
            return -1;
    }
    return 0;
}

void dump_str(const char *str, const int *buf, int len)
{
    int i;
    printf("%s=", str);
    for(i = 0; i < len; i++)
        printf(" %05x", buf[i]);
    printf("\n");
}

void dump_cc_info(CCInfo *ci, int i)
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

void dump_unicode_data(CCInfo *tab)
{
    int i;
    CCInfo *ci;
    for(i = 0; i <= CHARCODE_MAX; i++) {
        ci = &tab[i];
        if (ci->u_len != 0 || ci->l_len != 0 || ci->f_len != 0) {
            dump_cc_info(ci, i);
        }
    }
}

void dump_byte_table(FILE *f, const char *cname, const uint8_t *tab, int len)
{
    int i;

    total_tables++;
    total_table_bytes += len;
    fprintf(f, "static const uint8_t %s[%d] = {", cname, len);
    for(i = 0; i < len; i++) {
        if (i % 8 == 0)
            fprintf(f, "\n   ");
        fprintf(f, " 0x%02x,", tab[i]);
    }
    fprintf(f, "\n};\n\n");
}

void dump_index_table(FILE *f, const char *cname, const uint8_t *tab, int len)
{
    int i, code, offset;

    total_index++;
    total_index_bytes += len;
    fprintf(f, "static const uint8_t %s[%d] = {\n", cname, len);
    for(i = 0; i < len; i += 3) {
        code = tab[i] + (tab[i+1] << 8) + ((tab[i+2] & 0x1f) << 16);
        offset = ((i / 3) + 1) * 32 + (tab[i+2] >> 5);
        fprintf(f, "    0x%02x, 0x%02x, 0x%02x,", tab[i], tab[i+1], tab[i+2]);
        fprintf(f, "  // %6.5X at %d%s\n", code, offset,
                i == len - 3 ? " (upper bound)" : "");
    }
    fprintf(f, "};\n\n");
}
