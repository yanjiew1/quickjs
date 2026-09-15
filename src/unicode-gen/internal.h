/*
 * QuickJS Unicode table generator private interfaces
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
#ifndef UNICODE_GEN_INTERNAL_H
#define UNICODE_GEN_INTERNAL_H

#include <stdint.h>
#include <stdio.h>

#include "../../cutils.h"

/* Optional generator diagnostics. Uncomment for local generator builds. */
//#define PROFILE
//#define DUMP_CASE_CONV_TABLE
//#define DUMP_TABLE_SIZE
//#define DUMP_CC_TABLE
//#define DUMP_DECOMP_TABLE
//#define DUMP_CASE_FOLDING_SPECIAL_CASES

#define CHARCODE_MAX 0x10ffff
#define CC_LEN_MAX 3
#define UNICODE_DECOMP_LEN_MAX 18
#define SEQ_MAX_LEN 16

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

#define UNICODE_GENERAL_CATEGORY
typedef enum {
#define DEF(id, str) GCAT_ ## id,
#include "../../unicode_gen_def.h"
#undef DEF
    GCAT_COUNT,
} UnicodeGCEnum1;
#undef UNICODE_GENERAL_CATEGORY

#define UNICODE_SCRIPT
typedef enum {
#define DEF(id, str) SCRIPT_ ## id,
#include "../../unicode_gen_def.h"
#undef DEF
    SCRIPT_COUNT,
} UnicodeScriptEnum1;
#undef UNICODE_SCRIPT

#define UNICODE_PROP_LIST
typedef enum {
#define DEF(id, str) PROP_ ## id,
#include "../../unicode_gen_def.h"
#undef DEF
    PROP_COUNT,
} UnicodePropEnum1;
#undef UNICODE_PROP_LIST

#define UNICODE_SEQUENCE_PROP_LIST
typedef enum {
#define DEF(id) SEQUENCE_PROP_ ## id,
#include "../../unicode_gen_def.h"
#undef DEF
    SEQUENCE_PROP_COUNT,
} UnicodeSequencePropEnum1;
#undef UNICODE_SEQUENCE_PROP_LIST

typedef struct {
    uint8_t u_len;
    uint8_t l_len;
    uint8_t f_len;
    int u_data[CC_LEN_MAX];
    int l_data[CC_LEN_MAX];
    int f_data[CC_LEN_MAX];

    uint8_t combining_class;
    uint8_t is_compat:1;
    uint8_t is_excluded:1;
    uint8_t general_category;
    uint8_t script;
    uint8_t script_ext_len;
    uint8_t *script_ext;
    uint32_t prop_bitmap_tab[3];
    int decomp_len;
    int *decomp_data;
} CCInfo;

typedef struct {
    CCInfo *db;
    REStringList rgi_emoji_zwj_sequence;
    DynBuf rgi_emoji_tag_sequence;
} UnicodeGenState;

typedef struct {
    FILE *file;
    uint32_t total_tables;
    uint32_t total_table_bytes;
    uint32_t total_index;
    uint32_t total_index_bytes;
} UnicodeGenOutput;

extern const char *const unicode_gc_name[GCAT_COUNT];
extern const char *const unicode_gc_short_name[GCAT_COUNT];
extern const char *const unicode_script_name[SCRIPT_COUNT];
extern const char *const unicode_script_short_name[SCRIPT_COUNT];
extern const char *const unicode_prop_name[PROP_COUNT];
extern const char *const unicode_prop_short_name[PROP_COUNT];
extern const char *const unicode_sequence_prop_name[SEQUENCE_PROP_COUNT];

static inline BOOL unicode_gen_get_prop(const UnicodeGenState *s,
                                        uint32_t c, int prop_idx)
{
    return (s->db[c].prop_bitmap_tab[prop_idx >> 5] >>
            (prop_idx & 0x1f)) & 1;
}

static inline void unicode_gen_set_prop(UnicodeGenState *s, uint32_t c,
                                        int prop_idx, int val)
{
    uint32_t mask = 1U << (prop_idx & 0x1f);
    if (val)
        s->db[c].prop_bitmap_tab[prop_idx >> 5] |= mask;
    else
        s->db[c].prop_bitmap_tab[prop_idx >> 5] &= ~mask;
}

void unicode_gen_emit_case(UnicodeGenOutput *out, UnicodeGenState *s);
#ifdef CONFIG_UNICODE_TEST
void unicode_gen_check_case_tables(UnicodeGenState *s);
#endif
void unicode_gen_prepare_properties(UnicodeGenState *s);
void unicode_gen_emit_case_flags(UnicodeGenOutput *out, UnicodeGenState *s);
void unicode_gen_emit_property_flags(UnicodeGenOutput *out,
                                     UnicodeGenState *s);
void unicode_gen_emit_properties(UnicodeGenOutput *out, UnicodeGenState *s);
void unicode_gen_emit_normalization(UnicodeGenOutput *out, UnicodeGenState *s);

void unicode_gen_dump_byte_table(UnicodeGenOutput *out, const char *name,
                                 const uint8_t *tab, int len);
void unicode_gen_dump_index_table(UnicodeGenOutput *out, const char *name,
                                  const uint8_t *tab, int len);
void unicode_gen_dump_cc_info(const CCInfo *ci, int code);
void unicode_gen_string_list_init(REStringList *s);
void unicode_gen_string_list_free(REStringList *s);
void unicode_gen_string_add(REStringList *s, int len, const uint32_t *buf);
REString *unicode_gen_string_find(REStringList *s, int len,
                                  const uint32_t *buf, BOOL add);
int *unicode_gen_get_field_str(int *len, const char *str, int field);
char *unicode_gen_get_line(char *buf, int buf_size, FILE *f);

static inline BOOL unicode_gen_is_emoji_modifier(uint32_t c)
{
    return c >= 0x1f3fb && c <= 0x1f3ff;
}

#ifdef CONFIG_UNICODE_TEST
void unicode_gen_run_self_tests(UnicodeGenState *s, const char *db_path);
#endif

#endif /* UNICODE_GEN_INTERNAL_H */
