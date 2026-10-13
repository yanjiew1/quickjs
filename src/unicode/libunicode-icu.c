/*
 * QuickJS ICU Unicode provider
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 *
 * ICU owns case conversion, case folding, normalization and Unicode properties.
 * ECMAScript grammar exceptions and CharRange storage remain engine glue.
 * No generated native Unicode tables are included in this implementation.
 * ICU resource allocations belong to ICU; all conversion/scratch/range buffers
 * below use the caller allocator. Never change ICU's global memory hooks or
 * call u_cleanup from a JSRuntime lifetime.
 */
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "cutils.h"
#include "libunicode.h"
#include <unicode/uchar.h>
#include <unicode/uclean.h>
#include <unicode/unorm2.h>
#include <unicode/uset.h>
#include <unicode/ustring.h>
#include <unicode/utf16.h>
#include <unicode/uversion.h>

#if U_ICU_VERSION_MAJOR_NUM < 70
#error "The ICU Unicode provider requires ICU 70 or later for Unicode string properties"
#endif

#include "libunicode-common.inc"
#include "libunicode-es-aliases.h"

const char *lre_unicode_backend(void)
{
    return "icu";
}

void lre_unicode_version(uint8_t version[4])
{
    u_getUnicodeVersion(version);
}

void lre_unicode_provider_version(uint8_t version[4])
{
    u_getVersion(version);
}

int lre_unicode_initialize(const char **error)
{
    UErrorCode status = U_ZERO_ERROR;
    u_init(&status);
    if (error)
        *error = U_FAILURE(status) ? u_errorName(status) : NULL;
    return U_FAILURE(status) ? -1 : 0;
}

static int unicode_icu_scalar(uint32_t c)
{
    return c <= 0x10ffff && (c < 0xd800 || c > 0xdfff);
}

int lre_case_conv(uint32_t *res, uint32_t c, int conv_type)
{
    UChar input[2], output[2 * LRE_CC_RES_LEN_MAX];
    int32_t input_len = 0, output_len, pos = 0;
    UErrorCode status = U_ZERO_ERROR;
    int count = 0;
    if (conv_type < 0 || conv_type > 2)
        return -1;
    if (!unicode_icu_scalar(c)) {
        res[0] = c;
        return 1;
    }
    U16_APPEND_UNSAFE(input, input_len, c);
    if (conv_type == 0)
        output_len = u_strToUpper(output, countof(output), input, input_len, "", &status);
    else if (conv_type == 1)
        output_len = u_strToLower(output, countof(output), input, input_len, "", &status);
    else
        output_len = u_strFoldCase(output, countof(output), input, input_len,
                                  U_FOLD_CASE_DEFAULT, &status);
    if (U_FAILURE(status) || output_len < 1 || output_len > (int32_t)countof(output))
        return -1;
    while (pos < output_len) {
        UChar32 value;
        if (count >= LRE_CC_RES_LEN_MAX)
            return -1;
        U16_NEXT(output, pos, output_len, value);
        res[count++] = (uint32_t)value;
    }
    return count;
}

/* This ABI has no recoverable error channel. ICU failure is diagnosed, never
   converted into an identity mapping or a native-table fallback. Runtime
   creation calls lre_unicode_initialize before these infallible predicates. */
static void unicode_icu_fatal(const char *operation)
{
    fprintf(stderr, "QuickJS ICU Unicode provider failure: %s\n", operation);
    abort();
}

int lre_canonicalize_slow(uint32_t c, int is_unicode)
{
    uint32_t upper[LRE_CC_RES_LEN_MAX];
    int len;
    if (!unicode_icu_scalar(c))
        return c;
    if (is_unicode)
        return u_foldCase((UChar32)c, U_FOLD_CASE_DEFAULT);
    len = lre_case_conv(upper, c, 0);
    if (len < 0)
        unicode_icu_fatal("legacy full uppercase canonicalization");
    /* ECMA-262 Canonicalize: expansions and non-ASCII -> ASCII stay unchanged. */
    return len == 1 && (c < 128 || upper[0] >= 128) ? upper[0] : c;
}

static int unicode_icu_has(uint32_t c, UProperty property)
{
    return c <= 0x10ffff && u_hasBinaryProperty((UChar32)c, property);
}

int lre_is_cased(uint32_t c)
{
    return unicode_icu_has(c, UCHAR_CASED);
}

int lre_is_case_ignorable(uint32_t c)
{
    return unicode_icu_has(c, UCHAR_CASE_IGNORABLE);
}

int lre_is_id_start(uint32_t c)
{
    return unicode_icu_has(c, UCHAR_ID_START);
}

int lre_is_id_continue(uint32_t c)
{
    return unicode_icu_has(c, UCHAR_ID_CONTINUE);
}

int lre_is_space_non_ascii(uint32_t c)
{
    /* ECMAScript WhiteSpace + LineTerminator, with Zs supplied by ICU.
       Unicode White_Space differs: it includes NEL and excludes BOM. */
    return (c >= 9 && c <= 13) || c == 0x2028 || c == 0x2029 || c == 0xfeff ||
        (c <= 0x10ffff && u_charType((UChar32)c) == U_SPACE_SEPARATOR);
}

int unicode_get_combining_class(uint32_t c)
{
    return c <= 0x10ffff ? u_getCombiningClass((UChar32)c) : 0;
}

static int unicode_icu_exact_alias(const char *names, const char *name)
{
    size_t length = strlen(name);
    const char *end;
    do {
        end = strchr(names, ',');
        if (length == (end ? (size_t)(end - names) : strlen(names)) &&
            !memcmp(name, names, length))
            return 1;
        if (!end)
            break;
        names = end + 1;
    } while (*names);
    return 0;
}

static int unicode_icu_alias_value(const UnicodeESAlias *aliases, size_t count,
                                   const char *name)
{
    size_t i;
    if (!name || !*name)
        return -1;
    for (i = 0; i < count; i++) {
        if (unicode_icu_exact_alias(aliases[i].names, name))
            return aliases[i].value;
    }
    return -1;
}

static int unicode_icu_set_ranges(CharRange *cr, const USet *set)
{
    int32_t i, count = uset_getItemCount(set);
    for (i = 0; i < count; i++) {
        UErrorCode status = U_ZERO_ERROR;
        UChar32 first = 0, last = 0;
        int32_t length = uset_getItem(set, i, &first, &last, NULL, 0, &status);
        if (U_FAILURE(status) || length != 0 || first < 0 || last < first || last > 0x10ffff)
            return -1;
        if (cr_add_interval(cr, (uint32_t)first, (uint32_t)last + 1))
            return -1;
    }
    return 0;
}

static int unicode_icu_property_ranges(CharRange *cr, UProperty property, int32_t value)
{
    UErrorCode status = U_ZERO_ERROR;
    USet *set = uset_openEmpty();
    int ret;
    if (!set)
        return -1;
    uset_applyIntPropertyValue(set, property, value, &status);
    ret = U_FAILURE(status) ? -1 : unicode_icu_set_ranges(cr, set);
    uset_close(set);
    return ret;
}

int unicode_general_category(CharRange *cr, const char *name)
{
    int mask = unicode_icu_alias_value(unicode_es_gc_aliases,
                                      countof(unicode_es_gc_aliases), name);
    if (mask < 0)
        return -2;
    return unicode_icu_property_ranges(cr, UCHAR_GENERAL_CATEGORY_MASK, mask);
}

int unicode_script(CharRange *cr, const char *name, int is_ext)
{
    int32_t value, max_value = u_getIntPropertyMaxValue(UCHAR_SCRIPT);
    if (!name || !*name || (is_ext != 0 && is_ext != 1))
        return -2;
    /* The selected ICU data supplies exact UCD short/long Script aliases. Do
       not expose u_getPropertyValueEnum's loose matching or script resolver's
       locale names. The two historical UCD aliases are explicit. */
    if (!strcmp(name, "Qaai"))
        name = "Zinh";
    else if (!strcmp(name, "Qaac"))
        name = "Copt";
    for (value = 0; value <= max_value; value++) {
        const char *short_name = u_getPropertyValueName(UCHAR_SCRIPT, value, U_SHORT_PROPERTY_NAME);
        const char *long_name = u_getPropertyValueName(UCHAR_SCRIPT, value, U_LONG_PROPERTY_NAME);
        if ((short_name && !strcmp(name, short_name)) || (long_name && !strcmp(name, long_name)))
            return unicode_icu_property_ranges(cr,
                is_ext ? UCHAR_SCRIPT_EXTENSIONS : UCHAR_SCRIPT, value);
    }
    return -2;
}

int unicode_prop(CharRange *cr, const char *name)
{
    int property;
    if (!name)
        return -2;
    if (!strcmp(name, "ASCII"))
        return cr_add_interval(cr, 0, 128);
    if (!strcmp(name, "Any"))
        return cr_add_interval(cr, 0, 0x110000);
    if (!strcmp(name, "Assigned"))
        return unicode_icu_property_ranges(cr, UCHAR_GENERAL_CATEGORY_MASK,
            (U_GC_L_MASK | U_GC_M_MASK | U_GC_N_MASK | U_GC_S_MASK |
             U_GC_P_MASK | U_GC_Z_MASK | U_GC_C_MASK) & ~U_GC_CN_MASK);
    property = unicode_icu_alias_value(unicode_es_binary_aliases,
                                      countof(unicode_es_binary_aliases), name);
    if (property < 0)
        return -2;
    return unicode_icu_property_ranges(cr, (UProperty)property, 1);
}

int cr_regexp_canonicalize(CharRange *cr, int is_unicode)
{
    CharRange mask, affected, unchanged, result;
    uint32_t start = UINT32_MAX, end = UINT32_MAX, c, mapped;
    int i, ret = -1;
    cr_init(&mask, cr->mem_opaque, cr->realloc_func);
    cr_init(&affected, cr->mem_opaque, cr->realloc_func);
    cr_init(&unchanged, cr->mem_opaque, cr->realloc_func);
    cr_init(&result, cr->mem_opaque, cr->realloc_func);
    /* ICU Case_Sensitive contains every source/target of a case mapping,
       including simple folds that only change canonical representation.
       Changes_When_Casefolded is a normalization-based derived property and
       cannot bound all C/S mapping changes (for example U+1FD3 in Unicode16+).
       ICU case closure adds full-fold strings and is not ECMA Canonicalize. */
    if (unicode_icu_property_ranges(&mask, UCHAR_CASE_SENSITIVE, 1) ||
        cr_op(&affected, mask.points, mask.len, cr->points, cr->len, CR_OP_INTER) ||
        cr_op(&unchanged, cr->points, cr->len, mask.points, mask.len, CR_OP_SUB))
        goto done;
    for (i = 0; i < affected.len; i += 2) {
        for (c = affected.points[i]; c < affected.points[i + 1]; c++) {
            mapped = (uint32_t)lre_canonicalize_slow(c, is_unicode);
            if (start == UINT32_MAX) {
                start = mapped;
                end = mapped + 1;
            } else if (end == mapped) {
                end++;
            } else {
                if (cr_add_interval(&result, start, end))
                    goto done;
                start = mapped;
                end = mapped + 1;
            }
        }
    }
    if (start != UINT32_MAX && cr_add_interval(&result, start, end))
        goto done;
    cr_sort_and_remove_overlap(&result);
    /* Preserve the caller's set if allocating the replacement fails. */
    mask.len = 0;
    if (cr_op(&mask, result.points, result.len, unchanged.points, unchanged.len, CR_OP_UNION))
        goto done;
    cr_free(cr);
    *cr = mask;
    mask.points = NULL;
    mask.size = mask.len = 0;
    ret = 0;
done:
    cr_free(&mask);
    cr_free(&affected);
    cr_free(&unchanged);
    cr_free(&result);
    return ret;
}

static int32_t unicode_icu_case_string(UChar *dst, int32_t capacity,
                                      const UChar *src, int32_t len,
                                      int kind, UErrorCode *status)
{
    if (kind == 0)
        return u_strToUpper(dst, capacity, src, len, "", status);
    if (kind == 1)
        return u_strToLower(dst, capacity, src, len, "", status);
    return u_strFoldCase(dst, capacity, src, len, U_FOLD_CASE_DEFAULT, status);
}

int unicode_case_convert_utf16(uint16_t **pdst, const uint16_t *src, int src_len,
                              int kind, void *opaque,
                              void *(*realloc_func)(void *, void *, size_t))
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t length, actual;
    UChar *result;
    *pdst = NULL;
    if (src_len < 0 || (src_len && !src) || kind < 0 || kind > 2 || !realloc_func)
        return -3;
    if (!src_len)
        return 0;
    length = unicode_icu_case_string(NULL, 0, (const UChar *)src, src_len, kind, &status);
    if ((status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) || length < 0)
        return -3;
    if ((size_t)length > SIZE_MAX / sizeof(UChar))
        return -1;
    result = realloc_func(opaque, NULL, length ? (size_t)length * sizeof(UChar) : sizeof(UChar));
    if (!result)
        return -1;
    status = U_ZERO_ERROR;
    actual = unicode_icu_case_string(result, length, (const UChar *)src, src_len, kind, &status);
    if (U_FAILURE(status) || actual != length) {
        realloc_func(opaque, result, 0);
        return -3;
    }
    *pdst = (uint16_t *)result;
    return length;
}

static const UNormalizer2 *unicode_icu_normalizer(UnicodeNormalizationEnum kind,
                                                UErrorCode *status)
{
    switch (kind) {
    case UNICODE_NFC: return unorm2_getNFCInstance(status);
    case UNICODE_NFD: return unorm2_getNFDInstance(status);
    case UNICODE_NFKC: return unorm2_getNFKCInstance(status);
    case UNICODE_NFKD: return unorm2_getNFKDInstance(status);
    default: *status = U_ILLEGAL_ARGUMENT_ERROR; return NULL;
    }
}

int unicode_normalize(uint32_t **pdst, const uint32_t *src, int src_len,
                      UnicodeNormalizationEnum kind,
                      void *opaque, void *(*realloc_func)(void *, void *, size_t))
{
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2 *normalizer;
    UChar *input = NULL, *normalized = NULL;
    uint32_t *result = NULL;
    size_t total = 0;
    int pass, index, end, ret = -1;
    int32_t in_len, needed, actual, pos;
    *pdst = NULL;
    if (src_len < 0 || (src_len && !src))
        return -1;
    if (!realloc_func)
        realloc_func = cr_default_realloc;
    normalizer = unicode_icu_normalizer(kind, &status);
    if (U_FAILURE(status) || !normalizer)
        return -3;
    if (!src_len)
        return 0;
    if (src_len > INT32_MAX / 2 || (size_t)src_len > SIZE_MAX / (2 * sizeof(UChar)))
        return -1;
    input = realloc_func(opaque, NULL, (size_t)src_len * 2 * sizeof(UChar));
    if (!input)
        goto done;
    /* The public API accepts code points, including individual surrogate
       values. Surrogate entries are barriers, never combined into a scalar
       by the temporary UTF-16 encoding. Two passes compute exact output size
       and normalize every scalar run using ICU, without a custom NFC engine. */
    for (pass = 0; pass < 2; pass++) {
        size_t output_pos = 0;
        for (index = 0; index < src_len; index = end) {
            if (!unicode_icu_scalar(src[index])) {
                end = index + 1;
                if (pass)
                    result[output_pos] = src[index];
                output_pos++;
                continue;
            }
            in_len = 0;
            for (end = index; end < src_len && unicode_icu_scalar(src[end]); end++)
                U16_APPEND_UNSAFE(input, in_len, src[end]);
            status = U_ZERO_ERROR;
            needed = unorm2_normalize(normalizer, input, in_len, NULL, 0, &status);
            if ((status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) || needed < 0) {
                ret = -3;
                goto done;
            }
            if ((size_t)needed > SIZE_MAX / sizeof(UChar))
                goto done;
            if (needed) {
                UChar *replacement = realloc_func(opaque, normalized, (size_t)needed * sizeof(UChar));
                if (!replacement)
                    goto done;
                normalized = replacement;
            }
            status = U_ZERO_ERROR;
            actual = unorm2_normalize(normalizer, input, in_len, normalized, needed, &status);
            if (U_FAILURE(status) || actual != needed) {
                ret = -3;
                goto done;
            }
            pos = 0;
            while (pos < actual) {
                UChar32 code;
                U16_NEXT(normalized, pos, actual, code);
                if (output_pos >= INT_MAX)
                    goto done;
                if (pass) {
                    if (output_pos >= total) {
                        ret = -3;
                        goto done;
                    }
                    result[output_pos] = (uint32_t)code;
                }
                output_pos++;
            }
        }
        if (!pass) {
            total = output_pos;
            if (total > INT_MAX || total > SIZE_MAX / sizeof(uint32_t))
                goto done;
            if (total) {
                result = realloc_func(opaque, NULL, total * sizeof(uint32_t));
                if (!result)
                    goto done;
            }
        } else if (output_pos != total) {
            ret = -3;
            goto done;
        }
    }
    *pdst = result;
    result = NULL;
    ret = (int)total;
done:
    realloc_func(opaque, input, 0);
    realloc_func(opaque, normalized, 0);
    realloc_func(opaque, result, 0);
    return ret;
}

int unicode_sequence_prop_checked(const char *name,
                                 UnicodeSequencePropCheckedCB *cb, void *opaque,
                                 CharRange *cr)
{
    int property = unicode_icu_alias_value(unicode_es_string_aliases,
                                          countof(unicode_es_string_aliases), name);
    UErrorCode status = U_ZERO_ERROR;
    USet *set;
    UChar *string = NULL;
    uint32_t *sequence = NULL;
    int32_t i, count;
    int ret = -1;
    if (property < 0)
        return -2;
    if (!cb)
        return -1;
    set = uset_openEmpty();
    if (!set)
        return -1;
    uset_applyIntPropertyValue(set, (UProperty)property, 1, &status);
    if (U_FAILURE(status))
        goto done;
    count = uset_getItemCount(set);
    for (i = 0; i < count; i++) {
        UChar32 first = 0, last = 0;
        int32_t length, actual, pos = 0;
        int seq_len = 0;
        status = U_ZERO_ERROR;
        length = uset_getItem(set, i, &first, &last, NULL, 0, &status);
        if (!length && U_SUCCESS(status)) {
            uint32_t code;
            if (first < 0 || last < first || last > 0x10ffff)
                goto done;
            for (code = (uint32_t)first; code <= (uint32_t)last; code++) {
                if (cb(opaque, &code, 1))
                    goto done;
            }
            continue;
        }
        if ((status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) || length <= 0 ||
            (size_t)length > SIZE_MAX / sizeof(UChar) ||
            (size_t)length > SIZE_MAX / sizeof(uint32_t))
            goto done;
        {
            UChar *new_string = cr->realloc_func(cr->mem_opaque, string, (size_t)length * sizeof(UChar));
            uint32_t *new_sequence;
            if (!new_string)
                goto done;
            string = new_string;
            new_sequence = cr->realloc_func(cr->mem_opaque, sequence, (size_t)length * sizeof(uint32_t));
            if (!new_sequence)
                goto done;
            sequence = new_sequence;
        }
        status = U_ZERO_ERROR;
        actual = uset_getItem(set, i, &first, &last, string, length, &status);
        if (U_FAILURE(status) || actual != length)
            goto done;
        while (pos < length) {
            UChar32 code;
            U16_NEXT(string, pos, length, code);
            sequence[seq_len++] = (uint32_t)code;
        }
        if (cb(opaque, sequence, seq_len))
            goto done;
    }
    ret = 0;
done:
    uset_close(set);
    cr->realloc_func(cr->mem_opaque, string, 0);
    cr->realloc_func(cr->mem_opaque, sequence, 0);
    return ret;
}

typedef struct {
    UnicodeSequencePropCB *cb;
    void *opaque;
} UnicodeICUSequenceContext;

static int unicode_icu_sequence_compat(void *opaque, const uint32_t *buf, int len)
{
    UnicodeICUSequenceContext *s = opaque;
    s->cb(s->opaque, buf, len);
    return 0;
}

int unicode_sequence_prop(const char *name, UnicodeSequencePropCB *cb, void *opaque,
                          CharRange *cr)
{
    UnicodeICUSequenceContext s = { cb, opaque };
    if (!cb)
        return -1;
    return unicode_sequence_prop_checked(name, unicode_icu_sequence_compat, &s, cr);
}
