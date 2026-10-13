/*
 * Unicode utilities
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
#ifndef LIBUNICODE_H
#define LIBUNICODE_H

#include <stdint.h>
#include <stddef.h>

/* There is one selection: the actual linked ICU provider. Host Unicode
   generation explicitly defines CONFIG_UNICODE_NATIVE. */
#if defined(CONFIG_UNICODE_ICU) && !defined(CONFIG_ICU)
#error "CONFIG_UNICODE_ICU is derived from CONFIG_ICU, not an independent option"
#endif
#ifdef CONFIG_UNICODE_NATIVE
#undef CONFIG_UNICODE_ICU
#endif
#if defined(CONFIG_ICU) && !defined(CONFIG_UNICODE_NATIVE)
#define CONFIG_UNICODE_ICU
#endif

/* Generated native data version; ICU reports the selected data at runtime. */
#define LIBUNICODE_NATIVE_UNICODE_VERSION_MAJOR 18
#define LIBUNICODE_NATIVE_UNICODE_VERSION_MINOR 0
#define LIBUNICODE_NATIVE_UNICODE_VERSION_PATCH 0
void lre_unicode_version(uint8_t version[4]);
void lre_unicode_provider_version(uint8_t version[4]);
const char *lre_unicode_backend(void);
/* Initializes ICU's own process data; never installs allocator hooks or calls
   u_cleanup. Zero on success, -1 on failure with a stable diagnostic. */
int lre_unicode_initialize(const char **error);
/* Historical compile-time macros identify the bundled native data. They remain
   integer constants for #if/static initializers. Use lre_unicode_version for
   the active provider; linked ICU data may differ from its SDK's data version. */
#define LIBUNICODE_UNICODE_VERSION_MAJOR LIBUNICODE_NATIVE_UNICODE_VERSION_MAJOR
#define LIBUNICODE_UNICODE_VERSION_MINOR LIBUNICODE_NATIVE_UNICODE_VERSION_MINOR
#define LIBUNICODE_UNICODE_VERSION_PATCH LIBUNICODE_NATIVE_UNICODE_VERSION_PATCH

/* define it to include all the unicode tables (40KB larger) */
#define CONFIG_ALL_UNICODE

#define LRE_CC_RES_LEN_MAX 3

/* char ranges */

typedef struct {
    int len; /* in points, always even */
    int size;
    uint32_t *points; /* points sorted by increasing value */
    void *mem_opaque;
    void *(*realloc_func)(void *opaque, void *ptr, size_t size);
} CharRange;

typedef enum {
    CR_OP_UNION,
    CR_OP_INTER,
    CR_OP_XOR,
    CR_OP_SUB,
} CharRangeOpEnum;

void cr_init(CharRange *cr, void *mem_opaque, void *(*realloc_func)(void *opaque, void *ptr, size_t size));
void cr_free(CharRange *cr);
int cr_realloc(CharRange *cr, int size);
int cr_copy(CharRange *cr, const CharRange *cr1);

static inline int cr_add_point(CharRange *cr, uint32_t v)
{
    if (cr->len >= cr->size) {
        if (cr_realloc(cr, cr->len + 1))
            return -1;
    }
    cr->points[cr->len++] = v;
    return 0;
}

static inline int cr_add_interval(CharRange *cr, uint32_t c1, uint32_t c2)
{
    if ((cr->len + 2) > cr->size) {
        if (cr_realloc(cr, cr->len + 2))
            return -1;
    }
    cr->points[cr->len++] = c1;
    cr->points[cr->len++] = c2;
    return 0;
}

int cr_op(CharRange *cr, const uint32_t *a_pt, int a_len,
          const uint32_t *b_pt, int b_len, int op);
int cr_op1(CharRange *cr, const uint32_t *b_pt, int b_len, int op);

int cr_invert(CharRange *cr);

int cr_regexp_canonicalize(CharRange *cr, int is_unicode);

typedef enum {
    UNICODE_NFC,
    UNICODE_NFD,
    UNICODE_NFKC,
    UNICODE_NFKD,
} UnicodeNormalizationEnum;

int unicode_normalize(uint32_t **pdst, const uint32_t *src, int src_len,
                      UnicodeNormalizationEnum n_type,
                      void *opaque, void *(*realloc_func)(void *opaque, void *ptr, size_t size));
/* Every failure leaves *pdst NULL; -1 is allocation/size failure and ICU may
   report -3 for data/API failure. A NULL allocator uses libc allocation. */

/* Unicode18 CCC; collation reuses the sole normalization table owner. */
int unicode_get_combining_class(uint32_t c);

/* Unicode character range functions */

int unicode_script(CharRange *cr, const char *script_name, int is_ext);
int unicode_general_category(CharRange *cr, const char *gc_name);
int unicode_prop(CharRange *cr, const char *prop_name);
/* Exact ECMAScript binary-property aliases. Native private helpers may expose
   additional UCD properties; RegExp syntax always checks this grammar table. */
int lre_unicode_property_allowed(const char *name);

typedef void UnicodeSequencePropCB(void *opaque, const uint32_t *buf, int len);
int unicode_sequence_prop(const char *prop_name, UnicodeSequencePropCB *cb, void *opaque,
                          CharRange *cr);
/* The checked variant preserves the original API and propagates callback OOM. */
typedef int UnicodeSequencePropCheckedCB(void *opaque, const uint32_t *buf, int len);
int unicode_sequence_prop_checked(const char *prop_name,
                                 UnicodeSequencePropCheckedCB *cb, void *opaque,
                                 CharRange *cr);

#ifdef CONFIG_UNICODE_ICU
/* Full root string casing, including contextual mappings, by ICU. Buffers use
   the caller's allocator. Embedded NUL and unpaired UTF-16 units are preserved.
   Return UTF-16 length, -1 for allocation failure, -3 for ICU/data failure. */
int unicode_case_convert_utf16(uint16_t **pdst, const uint16_t *src, int src_len,
                              int conv_type, void *opaque,
                              void *(*realloc_func)(void *, void *, size_t));
#endif

int lre_case_conv(uint32_t *res, uint32_t c, int conv_type);
int lre_canonicalize_slow(uint32_t c, int is_unicode);

/* JS regexp specific rules for case folding */
static inline int lre_canonicalize(uint32_t c, int is_unicode)
{
#ifdef CONFIG_UNICODE_ICU
    return lre_canonicalize_slow(c, is_unicode);
#else
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
        return c;
    } else {
        return lre_canonicalize_slow(c, is_unicode);
    }
#endif
}

/* Code point type categories */
enum {
    UNICODE_C_SPACE  = (1 << 0),
    UNICODE_C_DIGIT  = (1 << 1),
    UNICODE_C_UPPER  = (1 << 2),
    UNICODE_C_LOWER  = (1 << 3),
    UNICODE_C_UNDER  = (1 << 4),
    UNICODE_C_DOLLAR = (1 << 5),
    UNICODE_C_XDIGIT = (1 << 6),
};
extern uint8_t const lre_ctype_bits[256];

/* zero or non-zero return value */
int lre_is_cased(uint32_t c);
int lre_is_case_ignorable(uint32_t c);
int lre_is_id_start(uint32_t c);
int lre_is_id_continue(uint32_t c);

static inline int lre_is_space_byte(uint8_t c) {
    return lre_ctype_bits[c] & UNICODE_C_SPACE;
}

static inline int lre_is_id_start_byte(uint8_t c) {
    return lre_ctype_bits[c] & (UNICODE_C_UPPER | UNICODE_C_LOWER |
                                UNICODE_C_UNDER | UNICODE_C_DOLLAR);
}

static inline int lre_is_id_continue_byte(uint8_t c) {
    return lre_ctype_bits[c] & (UNICODE_C_UPPER | UNICODE_C_LOWER |
                                UNICODE_C_UNDER | UNICODE_C_DOLLAR |
                                UNICODE_C_DIGIT);
}

int lre_is_space_non_ascii(uint32_t c);

static inline int lre_is_space(uint32_t c) {
#ifdef CONFIG_UNICODE_ICU
    return lre_is_space_non_ascii(c);
#else
    if (c < 256)
        return lre_is_space_byte(c);
    else
        return lre_is_space_non_ascii(c);
#endif
}

static inline int lre_js_is_ident_first(uint32_t c) {
#ifdef CONFIG_UNICODE_ICU
    return c == '$' || c == '_' || lre_is_id_start(c);
#else
    if (c < 128) {
        return lre_is_id_start_byte(c);
    } else {
#ifdef CONFIG_ALL_UNICODE
        return lre_is_id_start(c);
#else
        return !lre_is_space_non_ascii(c);
#endif
    }
#endif
}

static inline int lre_js_is_ident_next(uint32_t c) {
#ifdef CONFIG_UNICODE_ICU
    return c == '$' || c == '_' || c == 0x200c || c == 0x200d ||
        lre_is_id_continue(c);
#else
    if (c < 128) {
        return lre_is_id_continue_byte(c);
    } else {
        /* ZWNJ and ZWJ are accepted in identifiers */
        if (c >= 0x200C && c <= 0x200D)
            return TRUE;
#ifdef CONFIG_ALL_UNICODE
        return lre_is_id_continue(c);
#else
        return !lre_is_space_non_ascii(c);
#endif
    }
#endif
}

#endif /* LIBUNICODE_H */
