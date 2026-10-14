/* Unicode provider API contracts and ICU oracle vectors.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 * Source prepared for root execution; no results are claimed here. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cutils.h"
#include "libunicode.h"
#ifdef CONFIG_UNICODE_ICU
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/utf16.h>
#include <unicode/uset.h>
#include "libunicode-es-aliases.h"
#endif

typedef struct {
    int attempts;
    int fail_at;
    int live;
} TestAllocator;

static void *test_realloc(void *opaque, void *ptr, size_t size)
{
    TestAllocator *a = opaque;
    void *result;
    int had_ptr = ptr != NULL;
    if (!size) {
        if (ptr)
            a->live--;
        free(ptr);
        return NULL;
    }
    a->attempts++;
    if (a->fail_at && a->attempts == a->fail_at)
        return NULL;
    result = realloc(ptr, size);
    if (result && !had_ptr)
        a->live++;
    return result;
}

static int contains(const CharRange *cr, uint32_t c)
{
    int lo = 0, hi = cr->len / 2;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (c < cr->points[2 * mid])
            hi = mid;
        else if (c >= cr->points[2 * mid + 1])
            lo = mid + 1;
        else
            return 1;
    }
    return 0;
}

static void normalization(const uint32_t *src, int len,
                          UnicodeNormalizationEnum kind,
                          const uint32_t *expected, int expected_len)
{
    TestAllocator a = { 0, 0, 0 };
    uint32_t *result = NULL;
    int attempts, fail;
    assert(unicode_normalize(&result, src, len, kind, &a, test_realloc) == expected_len);
    assert(!expected_len || !memcmp(result, expected, (size_t)expected_len * sizeof(uint32_t)));
    attempts = a.attempts;
    test_realloc(&a, result, 0);
    assert(!a.live);
    for (fail = 1; fail <= attempts; fail++) {
        a.attempts = 0;
        a.fail_at = fail;
        result = (uint32_t *)(uintptr_t)1;
        assert(unicode_normalize(&result, src, len, kind, &a, test_realloc) < 0);
        assert(!result && !a.live);
    }
}

static void test_normalization(void)
{
    static const uint32_t composed[] = { 0x00e9 };
    static const uint32_t decomposed[] = { 'e', 0x0301 };
    static const uint32_t unordered[] = { 'A', 0x0315, 0x0300 };
    static const uint32_t ordered[] = { 'A', 0x0300, 0x0315 };
    static const uint32_t compat[] = { 0xfb03 };
    static const uint32_t expanded[] = { 'f', 'f', 'i' };
    static const uint32_t hangul[] = { 0xac00 };
    static const uint32_t jamo[] = { 0x1100, 0x1161 };
    static const uint32_t nul[] = { 'a', 0, 'e', 0x0301 };
    static const uint32_t normalized_nul[] = { 'a', 0, 0x00e9 };
    static const uint32_t surrogates[] = { 0xd800, 0xdc00, 'e', 0x0301, 0xdfff };
    static const uint32_t normalized_surrogates[] = { 0xd800, 0xdc00, 0x00e9, 0xdfff };
    TestAllocator a = { 0, 0, 0 };
    uint32_t *result = (uint32_t *)(uintptr_t)1;
    normalization(composed, countof(composed), UNICODE_NFD, decomposed, countof(decomposed));
    normalization(decomposed, countof(decomposed), UNICODE_NFC, composed, countof(composed));
    normalization(unordered, countof(unordered), UNICODE_NFD, ordered, countof(ordered));
    normalization(compat, countof(compat), UNICODE_NFKC, expanded, countof(expanded));
    normalization(compat, countof(compat), UNICODE_NFKD, expanded, countof(expanded));
    normalization(hangul, countof(hangul), UNICODE_NFD, jamo, countof(jamo));
    normalization(jamo, countof(jamo), UNICODE_NFC, hangul, countof(hangul));
    normalization(nul, countof(nul), UNICODE_NFC, normalized_nul, countof(normalized_nul));
    normalization(surrogates, countof(surrogates), UNICODE_NFC,
                  normalized_surrogates, countof(normalized_surrogates));
    assert(unicode_normalize(&result, NULL, 0, UNICODE_NFC, &a, test_realloc) == 0);
    test_realloc(&a, result, 0);
    assert(!a.live);
#ifdef CONFIG_UNICODE_ICU
    result = (uint32_t *)(uintptr_t)1;
    assert(unicode_normalize(&result, composed, -1, UNICODE_NFC, &a, test_realloc) < 0);
    assert(!result && !a.live);
    result = (uint32_t *)(uintptr_t)1;
    assert(unicode_normalize(&result, composed, 1, (UnicodeNormalizationEnum)99,
                             &a, test_realloc) == -3);
    assert(!result && !a.live);
#endif
}

static void test_ranges(void)
{
    TestAllocator a = { 0, 0, 0 };
    CharRange cr, ext;
    static const char *invalid[] = {
        "alphabetic", "ALPHABETIC", "Alphabetic ", "IsAlphabetic", "A-lpha",
        "Block", "Age", "Other_Math", "InCB", "ID_Compat_Math_Start"
    };
    static const struct {
        const char *name;
        uint32_t member;
    } native_private[] = {
        { "IDS_Unary_Operator", 0x2ffe },
        { "IDSU", 0x2ffe },
        { "Modifier_Combining_Mark", 0x0654 },
        { "MCM", 0x0654 }
    };
    size_t i;
    assert(lre_unicode_property_allowed("Alphabetic"));
    assert(lre_unicode_property_allowed("Alpha"));
    assert(lre_unicode_property_allowed("ASCII"));
    assert(lre_unicode_property_allowed("Assigned"));
    assert(!lre_unicode_property_allowed("IDS_Unary_Operator"));
    assert(!lre_unicode_property_allowed("IDSU"));
    assert(!lre_unicode_property_allowed("Modifier_Combining_Mark"));
    assert(!lre_unicode_property_allowed("MCM"));
    for (i = 0; i < countof(invalid); i++)
        assert(!lre_unicode_property_allowed(invalid[i]));
    assert(!lre_unicode_property_allowed("ID_Compat_Math_Continue"));
    cr_init(&cr, &a, test_realloc);
    cr_init(&ext, &a, test_realloc);
    assert(unicode_prop(&cr, "Any") == 0);
    assert(cr.len == 2 && cr.points[0] == 0 && cr.points[1] == 0x110000);
    cr.len = 0;
    assert(unicode_prop(&cr, "ASCII") == 0);
    assert(cr.len == 2 && cr.points[1] == 128);
    cr.len = 0;
    assert(unicode_general_category(&cr, "Cs") == 0);
    assert(contains(&cr, 0xd800) && contains(&cr, 0xdfff) && !contains(&cr, 0xe000));
    cr.len = 0;
    assert(unicode_general_category(&cr, "Cn") == 0);
    assert(contains(&cr, 0x10ffff) && !contains(&cr, 0x110000));
    cr.len = 0;
    assert(unicode_general_category(&cr, "LC") == 0);
    assert(contains(&cr, 'A') && contains(&cr, 'a') && contains(&cr, 0x01c5));
    cr.len = 0;
    assert(unicode_general_category(&cr, "uppercase_letter") == -2);
    assert(unicode_general_category(&cr, "Lu ") == -2);
    assert(unicode_script(&cr, "latin", 0) == -2);
    assert(unicode_script(&cr, "Latn ", 0) == -2);
    assert(unicode_script(&cr, "Hiragana", 0) == 0);
    assert(!contains(&cr, 0x30fc));
    assert(unicode_script(&ext, "Hira", 1) == 0);
    assert(contains(&ext, 0x30fc));
    cr.len = 0;
    assert(unicode_script(&cr, "Qaai", 0) == 0);
    assert(contains(&cr, 0x0301));
    cr.len = 0;
    assert(unicode_script(&cr, "Qaac", 0) == 0);
    assert(contains(&cr, 0x03e2));
    cr.len = 0;
    assert(unicode_prop(&cr, "Assigned") == 0);
    assert(contains(&cr, 'A') && contains(&cr, 0xd800) && !contains(&cr, 0x10ffff));
    cr.len = 0;
    assert(unicode_prop(&cr, "space") == 0);
    assert(contains(&cr, 0x85) && !contains(&cr, 0xfeff));
    cr.len = 0;
    for (i = 0; i < countof(invalid); i++)
        assert(unicode_prop(&cr, invalid[i]) == -2);
    assert(unicode_prop(&cr, "ID_Compat_Math_Continue") == -2);
    /* The parser whitelist is shared; native private lookup additionally
       retains backed UCD helper properties from the original native table. */
    for (i = 0; i < countof(native_private); i++) {
        cr.len = 0;
        assert(!lre_unicode_property_allowed(native_private[i].name));
#ifdef CONFIG_UNICODE_ICU
        assert(unicode_prop(&cr, native_private[i].name) == -2);
        assert(cr.len == 0);
#else
        assert(unicode_prop(&cr, native_private[i].name) == 0);
        assert(contains(&cr, native_private[i].member));
        assert(!contains(&cr, 'A'));
#endif
    }
    cr.len = 0;
    assert(cr_add_interval(&cr, 'K', 'K' + 1) == 0);
    assert(cr_add_interval(&cr, 0x212a, 0x212b) == 0);
    assert(cr_regexp_canonicalize(&cr, 1) == 0);
    assert(cr.len == 2 && cr.points[0] == 'k' && cr.points[1] == 'k' + 1);
    cr.len = 0;
    assert(cr_add_interval(&cr, 0x17f, 0x180) == 0);
    assert(cr_regexp_canonicalize(&cr, 0) == 0);
    assert(contains(&cr, 0x17f) && !contains(&cr, 'S'));
    {
        static const uint32_t folds[] = { 0x1fd3, 0x1fe3, 0xfb05, 0x1f88, 0x10400 };
        for (i = 0; i < countof(folds); i++) {
            uint32_t canonical = (uint32_t)lre_canonicalize(folds[i], 1);
            cr.len = 0;
            assert(cr_add_interval(&cr, folds[i], folds[i] + 1) == 0);
            assert(cr_regexp_canonicalize(&cr, 1) == 0);
            assert(cr.len == 2 && cr.points[0] == canonical && cr.points[1] == canonical + 1);
        }
    }
    cr_free(&cr);
    cr_free(&ext);
    assert(!a.live);
}

typedef struct {
    int single;
    int strings;
    int keycap;
    int flag;
    int zwj;
    int seen;
    int fail_at;
} SequenceContext;

static int sequence_cb(void *opaque, const uint32_t *seq, int len)
{
    SequenceContext *s = opaque;
    assert(len > 0);
    if (++s->seen == s->fail_at)
        return -1;
    if (len == 1)
        s->single++;
    else
        s->strings++;
    if (len == 3 && seq[0] == '#' && seq[1] == 0xfe0f && seq[2] == 0x20e3)
        s->keycap++;
    if (len == 2 && seq[0] == 0x1f1fa && seq[1] == 0x1f1f8)
        s->flag++;
    if (len == 3 && seq[0] == 0x1f469 && seq[1] == 0x200d && seq[2] == 0x1f4bb)
        s->zwj++;
    return 0;
}

static void test_sequences(void)
{
    TestAllocator a = { 0, 0, 0 };
    SequenceContext s = { 0 };
    CharRange cr;
    int fail, attempts;
    cr_init(&cr, &a, test_realloc);
    assert(unicode_sequence_prop_checked("RGI_Emoji", sequence_cb, &s, &cr) == 0);
    assert(s.single && s.strings && s.keycap && s.flag && s.zwj);
    assert(unicode_sequence_prop_checked("rgi_emoji", sequence_cb, &s, &cr) == -2);
    cr_free(&cr);
    assert(!a.live);
    memset(&s, 0, sizeof(s));
    s.fail_at = 1;
    cr_init(&cr, &a, test_realloc);
    assert(unicode_sequence_prop_checked("RGI_Emoji", sequence_cb, &s, &cr) == -1);
    assert(s.seen == 1);
    cr_free(&cr);
    assert(!a.live);
#ifdef CONFIG_UNICODE_ICU
    memset(&s, 0, sizeof(s));
    a.attempts = 0;
    cr_init(&cr, &a, test_realloc);
    assert(unicode_sequence_prop_checked("Emoji_Keycap_Sequence", sequence_cb, &s, &cr) == 0);
    assert(s.keycap == 1 && s.strings > 0);
    attempts = a.attempts;
    cr_free(&cr);
    assert(!a.live);
    for (fail = 1; fail <= attempts; fail++) {
        memset(&s, 0, sizeof(s));
        a.attempts = 0;
        a.fail_at = fail;
        cr_init(&cr, &a, test_realloc);
        assert(unicode_sequence_prop_checked("Emoji_Keycap_Sequence", sequence_cb, &s, &cr) == -1);
        cr_free(&cr);
        assert(!a.live);
    }
#else
    (void)attempts;
    (void)fail;
#endif
}

static void test_range_oom(void)
{
    TestAllocator a = { 0, 0, 0 };
    CharRange cr;
    int attempts, fail;
    cr_init(&cr, &a, test_realloc);
    assert(unicode_prop(&cr, "Alphabetic") == 0);
    attempts = a.attempts;
    cr_free(&cr);
    assert(!a.live);
    for (fail = 1; fail <= attempts; fail++) {
        a.attempts = 0;
        a.fail_at = fail;
        cr_init(&cr, &a, test_realloc);
        assert(unicode_prop(&cr, "Alphabetic") == -1);
        cr_free(&cr);
        assert(!a.live);
    }
    a.attempts = a.fail_at = 0;
    cr_init(&cr, &a, test_realloc);
    assert(cr_add_interval(&cr, 'A', 'A' + 1) == 0);
    assert(cr_add_interval(&cr, 'C', 'C' + 1) == 0);
    a.attempts = 0;
    assert(cr_regexp_canonicalize(&cr, 1) == 0);
    attempts = a.attempts;
    cr_free(&cr);
    for (fail = 1; fail <= attempts; fail++) {
        a.attempts = a.fail_at = 0;
        cr_init(&cr, &a, test_realloc);
        assert(cr_add_interval(&cr, 'A', 'A' + 1) == 0);
        assert(cr_add_interval(&cr, 'C', 'C' + 1) == 0);
        a.attempts = 0;
        a.fail_at = fail;
        assert(cr_regexp_canonicalize(&cr, 1) == -1);
#ifdef CONFIG_UNICODE_ICU
        assert(cr.len == 4 && cr.points[0] == 'A' && cr.points[1] == 'A' + 1 &&
               cr.points[2] == 'C' && cr.points[3] == 'C' + 1);
#endif
        cr_free(&cr);
        assert(!a.live);
    }
}

#ifdef CONFIG_UNICODE_ICU
typedef struct {
    USet *remaining;
} ICUSequenceOracle;

static int sequence_oracle_cb(void *opaque, const uint32_t *seq, int len)
{
    ICUSequenceOracle *s = opaque;
    UChar *utf16;
    int i;
    int32_t length = 0;
    if (len == 1) {
        assert(uset_contains(s->remaining, (UChar32)seq[0]));
        uset_remove(s->remaining, (UChar32)seq[0]);
        return 0;
    }
    assert(len > 1 && (size_t)len <= SIZE_MAX / (2 * sizeof(UChar)));
    utf16 = malloc((size_t)len * 2 * sizeof(UChar));
    assert(utf16);
    for (i = 0; i < len; i++) {
        assert(seq[i] <= 0x10ffff);
        U16_APPEND_UNSAFE(utf16, length, seq[i]);
    }
    assert(uset_containsString(s->remaining, utf16, length));
    uset_removeString(s->remaining, utf16, length);
    free(utf16);
    return 0;
}

static void test_sequence_oracle(void)
{
    size_t i;
    for (i = 0; i < countof(unicode_es_string_aliases); i++) {
        TestAllocator a = { 0, 0, 0 };
        CharRange cr;
        UErrorCode status = U_ZERO_ERROR;
        ICUSequenceOracle s;
        s.remaining = uset_openEmpty();
        assert(s.remaining);
        uset_applyIntPropertyValue(s.remaining, (UProperty)unicode_es_string_aliases[i].value, 1, &status);
        assert(U_SUCCESS(status));
        cr_init(&cr, &a, test_realloc);
        assert(unicode_sequence_prop_checked(unicode_es_string_aliases[i].names,
                   sequence_oracle_cb, &s, &cr) == 0);
        assert(uset_isEmpty(s.remaining));
        uset_close(s.remaining);
        cr_free(&cr);
        assert(!a.live);
    }
}

static void test_property_range_oracle(void)
{
    int32_t script, max_script = u_getIntPropertyMaxValue(UCHAR_SCRIPT);
    size_t i;
    for (i = 0; i < countof(unicode_es_gc_aliases); i++) {
        TestAllocator a = { 0, 0, 0 };
        CharRange cr;
        UErrorCode status = U_ZERO_ERROR;
        USet *expected = uset_openEmpty(), *actual = uset_openEmpty();
        const char *comma = strchr(unicode_es_gc_aliases[i].names, ',');
        char name[80];
        size_t len = comma ? (size_t)(comma - unicode_es_gc_aliases[i].names) : strlen(unicode_es_gc_aliases[i].names);
        int j;
        assert(expected && actual && len < sizeof(name));
        memcpy(name, unicode_es_gc_aliases[i].names, len);
        name[len] = 0;
        uset_applyIntPropertyValue(expected, UCHAR_GENERAL_CATEGORY_MASK, unicode_es_gc_aliases[i].value, &status);
        assert(U_SUCCESS(status));
        cr_init(&cr, &a, test_realloc);
        assert(unicode_general_category(&cr, name) == 0);
        for (j = 0; j < cr.len; j += 2)
            uset_addRange(actual, (UChar32)cr.points[j], (UChar32)cr.points[j + 1] - 1);
        assert(uset_equals(actual, expected));
        uset_close(actual);
        uset_close(expected);
        cr_free(&cr);
        assert(!a.live);
    }
    for (script = 0; script <= max_script; script++) {
        const char *name = u_getPropertyValueName(UCHAR_SCRIPT, script, U_SHORT_PROPERTY_NAME);
        int is_ext;
        if (!name)
            continue;
        for (is_ext = 0; is_ext <= 1; is_ext++) {
            TestAllocator a = { 0, 0, 0 };
            CharRange cr;
            UErrorCode status = U_ZERO_ERROR;
            USet *expected = uset_openEmpty(), *actual = uset_openEmpty();
            int j;
            assert(expected && actual);
            uset_applyIntPropertyValue(expected, is_ext ? UCHAR_SCRIPT_EXTENSIONS : UCHAR_SCRIPT, script, &status);
            assert(U_SUCCESS(status));
            cr_init(&cr, &a, test_realloc);
            assert(unicode_script(&cr, name, is_ext) == 0);
            for (j = 0; j < cr.len; j += 2)
                uset_addRange(actual, (UChar32)cr.points[j], (UChar32)cr.points[j + 1] - 1);
            assert(uset_equals(actual, expected));
            uset_close(actual);
            uset_close(expected);
            cr_free(&cr);
            assert(!a.live);
        }
    }
}

static void test_context_case(void)
{
    static const uint16_t src[] = { 0x0391, 0x03a3, 0x0301, 0, 0xd800, 0x0130, 0xdc00 };
    static const uint16_t expected[] = { 0x03b1, 0x03c2, 0x0301, 0, 0xd800, 'i', 0x0307, 0xdc00 };
    TestAllocator a = { 0, 0, 0 };
    uint16_t *dst = NULL;
    int attempts, fail;
    assert(unicode_case_convert_utf16(&dst, src, countof(src), 1, &a, test_realloc) == countof(expected));
    assert(!memcmp(dst, expected, sizeof(expected)));
    attempts = a.attempts;
    test_realloc(&a, dst, 0);
    assert(!a.live);
    for (fail = 1; fail <= attempts; fail++) {
        a.attempts = 0;
        a.fail_at = fail;
        dst = (uint16_t *)(uintptr_t)1;
        assert(unicode_case_convert_utf16(&dst, src, countof(src), 1, &a, test_realloc) == -1);
        assert(!dst && !a.live);
    }
}

static void exhaustive_scalar_oracle(void)
{
    uint32_t c, actual[LRE_CC_RES_LEN_MAX], expected[LRE_CC_RES_LEN_MAX];
    for (c = 0; c <= 0x10ffff; c++) {
        UChar input[2], output[6];
        int32_t in_len = 0;
        int kind;
        if (c >= 0xd800 && c <= 0xdfff)
            continue;
        U16_APPEND_UNSAFE(input, in_len, c);
        assert(lre_canonicalize(c, 1) == u_foldCase((UChar32)c, U_FOLD_CASE_DEFAULT));
        if (u_foldCase((UChar32)c, U_FOLD_CASE_DEFAULT) != (UChar32)c)
            assert(u_hasBinaryProperty((UChar32)c, UCHAR_CASE_SENSITIVE));
        assert(!!lre_is_cased(c) == !!u_hasBinaryProperty((UChar32)c, UCHAR_CASED));
        assert(!!lre_is_case_ignorable(c) == !!u_hasBinaryProperty((UChar32)c, UCHAR_CASE_IGNORABLE));
        assert(!!lre_is_id_start(c) == !!u_hasBinaryProperty((UChar32)c, UCHAR_ID_START));
        assert(!!lre_is_id_continue(c) == !!u_hasBinaryProperty((UChar32)c, UCHAR_ID_CONTINUE));
        assert(unicode_get_combining_class(c) == u_getCombiningClass((UChar32)c));
        for (kind = 0; kind < 3; kind++) {
            UErrorCode status = U_ZERO_ERROR;
            int32_t out_len, pos = 0;
            int count = 0, actual_len;
            if (!kind)
                out_len = u_strToUpper(output, countof(output), input, in_len, "", &status);
            else if (kind == 1)
                out_len = u_strToLower(output, countof(output), input, in_len, "", &status);
            else
                out_len = u_strFoldCase(output, countof(output), input, in_len, U_FOLD_CASE_DEFAULT, &status);
            assert(U_SUCCESS(status));
            while (pos < out_len) {
                UChar32 value;
                assert(count < LRE_CC_RES_LEN_MAX);
                U16_NEXT(output, pos, out_len, value);
                expected[count++] = (uint32_t)value;
            }
            actual_len = lre_case_conv(actual, c, kind);
            assert(actual_len == count && !memcmp(actual, expected, (size_t)count * sizeof(uint32_t)));
            if (!kind) {
                uint32_t legacy = count == 1 && (c < 128 || expected[0] >= 128) ? expected[0] : c;
                assert((uint32_t)lre_canonicalize(c, 0) == legacy);
                if (legacy != c)
                    assert(u_hasBinaryProperty((UChar32)c, UCHAR_CASE_SENSITIVE));
            }
        }
    }
}

static void exhaustive_property_oracle(void)
{
    size_t i;
    for (i = 0; i < countof(unicode_es_binary_aliases); i++) {
        TestAllocator a = { 0, 0, 0 };
        CharRange cr;
        char name[80];
        const char *comma = strchr(unicode_es_binary_aliases[i].names, ',');
        size_t len = comma ? (size_t)(comma - unicode_es_binary_aliases[i].names) : strlen(unicode_es_binary_aliases[i].names);
        uint32_t c;
        assert(len < sizeof(name));
        memcpy(name, unicode_es_binary_aliases[i].names, len);
        name[len] = 0;
        cr_init(&cr, &a, test_realloc);
        assert(unicode_prop(&cr, name) == 0);
        for (c = 0; c <= 0x10ffff; c++)
            assert(contains(&cr, c) == !!u_hasBinaryProperty((UChar32)c, (UProperty)unicode_es_binary_aliases[i].value));
        cr_free(&cr);
        assert(!a.live);
    }
}
#endif

int main(int argc, char **argv)
{
    const char *error = NULL;
    uint8_t version[4], provider[4];
    assert(lre_unicode_initialize(&error) == 0 && !error);
    lre_unicode_version(version);
    lre_unicode_provider_version(provider);
    printf("Unicode backend=%s data=%u.%u.%u.%u provider=%u.%u.%u.%u native=%d.%d.%d\n",
           lre_unicode_backend(), version[0], version[1], version[2], version[3],
           provider[0], provider[1], provider[2], provider[3],
           LIBUNICODE_NATIVE_UNICODE_VERSION_MAJOR, LIBUNICODE_NATIVE_UNICODE_VERSION_MINOR,
           LIBUNICODE_NATIVE_UNICODE_VERSION_PATCH);
    assert(version[0] != 0);
    test_normalization();
    test_ranges();
    test_sequences();
    test_range_oom();
#ifdef CONFIG_UNICODE_ICU
    test_context_case();
    test_sequence_oracle();
    test_property_range_oracle();
    if (argc > 1 && !strcmp(argv[1], "--exhaustive"))
        exhaustive_scalar_oracle();
    if (argc > 1 && !strcmp(argv[1], "--properties"))
        exhaustive_property_oracle();
#else
    (void)argc;
    (void)argv;
#endif
    return 0;
}
