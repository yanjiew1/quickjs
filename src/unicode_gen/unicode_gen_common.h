#ifndef UNICODE_GEN_COMMON_H
#define UNICODE_GEN_COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include "cutils.h"

#define CHARCODE_MAX 0x10ffff
#define CC_LEN_MAX 3
#define SEQ_MAX_LEN 16

#define UNICODE_GENERAL_CATEGORY
typedef enum {
#define DEF(id, str) GCAT_ ## id,
#include "unicode_gen_def.h"
#undef DEF
    GCAT_COUNT,
} UnicodeGCEnum1;
#undef UNICODE_GENERAL_CATEGORY

#define UNICODE_SCRIPT
typedef enum {
#define DEF(id, str) SCRIPT_ ## id,
#include "unicode_gen_def.h"
#undef DEF
    SCRIPT_COUNT,
} UnicodeScriptEnum1;
#undef UNICODE_SCRIPT

#define UNICODE_PROP_LIST
typedef enum {
#define DEF(id, str) PROP_ ## id,
#include "unicode_gen_def.h"
#undef DEF
    PROP_COUNT,
} UnicodePropEnum1;
#undef UNICODE_PROP_LIST

#define UNICODE_SEQUENCE_PROP_LIST
typedef enum {
#define DEF(id) SEQUENCE_PROP_ ## id,
#include "unicode_gen_def.h"
#undef DEF
    SEQUENCE_PROP_COUNT,
} UnicodeSequencePropEnum1;
#undef UNICODE_SEQUENCE_PROP_LIST

extern const char *unicode_gc_name[GCAT_COUNT];
extern const char *unicode_gc_short_name[GCAT_COUNT];
extern const char *unicode_script_name[SCRIPT_COUNT];
extern const char *unicode_script_short_name[SCRIPT_COUNT];
extern const char *unicode_prop_name[PROP_COUNT];
extern const char *unicode_prop_short_name[PROP_COUNT];
extern const char *unicode_sequence_prop_name[SEQUENCE_PROP_COUNT];

typedef struct REString {
    struct REString *next;
    uint32_t hash;
    uint32_t len;
    uint32_t flags;
    uint32_t buf[];
} REString;

typedef struct {
    uint32_t n_strings;
    uint32_t hash_size;
    int hash_bits;
    REString **hash_table;
} REStringList;

typedef struct {
    /* case conv */
    uint8_t u_len;
    uint8_t l_len;
    uint8_t f_len;
    int u_data[CC_LEN_MAX]; /* to upper case */
    int l_data[CC_LEN_MAX]; /* to lower case */
    int f_data[CC_LEN_MAX]; /* to case folding */

    uint8_t combining_class;
    uint8_t is_compat:1;
    uint8_t is_excluded:1;
    uint8_t general_category;
    uint8_t script;
    uint8_t script_ext_len;
    uint8_t *script_ext;
    uint32_t prop_bitmap_tab[3];
    /* decomposition */
    int decomp_len;
    int *decomp_data;
} CCInfo;

typedef struct {
    int code;
    int len;
    int type;
    int data;
    int ext_len;
    int ext_data[3];
    int data_index; /* 'data' coming from the table */
} TableEntry;

typedef struct {
    int code;
    uint8_t len;
    uint8_t type;
    uint8_t c_len;
    uint16_t c_min;
    uint16_t data_index;
    int cost; /* size in bytes from this entry to the end */
} DecompEntry;

/* Global state */
extern CCInfo *unicode_db;
extern REStringList rgi_emoji_zwj_sequence;
extern DynBuf rgi_emoji_tag_sequence;
extern uint32_t total_tables;
extern uint32_t total_table_bytes;
extern uint32_t total_index;
extern uint32_t total_index_bytes;

/* Utility functions */
void *mallocz(size_t size);
const char *get_field(const char *p, int n);
const char *get_field_buf(char *buf, size_t buf_size, const char *p, int n);
void add_char(int **pbuf, int *psize, int *plen, int c);
int *get_field_str(int *plen, const char *str, int n);
char *get_line(char *buf, int buf_size, FILE *f);
int find_name(const char **tab, int tab_len, const char *name);
BOOL get_prop(uint32_t c, int prop_idx);
void set_prop(uint32_t c, int prop_idx, int val);

static inline BOOL is_emoji_modifier(uint32_t c)
{
    return (c >= 0x1F3FB && c <= 0x1F3FF);
}

void re_string_list_init(REStringList *s);
void re_string_list_free(REStringList *s);
void re_string_add(REStringList *s, int len, const uint32_t *buf);
REString *re_string_find(REStringList *s, int len, const uint32_t *buf, BOOL add_flag);

int tabcmp(const int *tab1, const int *tab2, int n);
void dump_str(const char *str, const int *buf, int len);
void dump_cc_info(CCInfo *ci, int i);
void dump_unicode_data(CCInfo *tab);
void dump_byte_table(FILE *f, const char *cname, const uint8_t *tab, int len);
void dump_index_table(FILE *f, const char *cname, const uint8_t *tab, int len);

#endif /* UNICODE_GEN_COMMON_H */
