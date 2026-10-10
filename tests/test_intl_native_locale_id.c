/* Authored unit proposal. Link the frozen reader and emitted locale-metadata.c.
 * Not run in the source preparation packet. Oracles use pinned ECMA402 and
 * CLDR49 XML; they are not inferred from ICU output.
 */
#include "intl/native-locale-id.h"
#include "locale-metadata.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct AllocationState { size_t calls, fail_at, live; } AllocationState;
static void *test_malloc(void *opaque, size_t n)
{
    AllocationState *s = opaque; void *p;
    assert(n);
    if (++s->calls == s->fail_at) return NULL;
    p = malloc(n); if (p) s->live++; return p;
}
static void *test_realloc(void *opaque, void *old, size_t n)
{
    AllocationState *s = opaque; void *p; int had_old = old != NULL;
    assert(n);
    if (++s->calls == s->fail_at) return NULL;
    p = realloc(old, n); if (p && !had_old) s->live++; return p;
}
static void test_free(void *opaque, void *p)
{
    AllocationState *s = opaque;
    if (p) { assert(s->live); s->live--; free(p); }
}
static QJSIntlBytes bytes(const char *s)
{
    QJSIntlBytes result = { s, strlen(s) }; return result;
}
typedef QJSIntlStatus (*Operation)(const QJSIntlAllocator *, const QJSIntlDataView *, QJSIntlBytes, char **);
static void check(Operation op, QJSIntlAllocator *a, QJSIntlDataView *v,
                  const char *input, const char *expected)
{
    AllocationState *s = a->opaque; char *out = NULL;
    assert(op(a, v, bytes(input), &out) == QJS_INTL_OK);
    assert(out && !strcmp(out, expected)); test_free(s, out); assert(!s->live);
}
int main(void)
{
    AllocationState state = { 0 };
    QJSIntlAllocator a = { &state, test_malloc, test_realloc, test_free };
    QJSIntlDataView v, bad_view = { 0 };
    char *out; int well_formed; size_t i, calls;
    const char *invalid[] = {
        "", "en-", "-en", "en--US", "abcd", "i-klingon", "x-private",
        "en_US", "zh-cmn", "sgn-BE-FR", "en-u", "en-x", "en-a-foo-A-bar",
        "sl-rozaj-ROZAJ", "en-t-sl-rozaj-ROZAJ", "en-t-m0", "en-t-00-abc",
        "en-u-a0-abc", "en-u-ca-abcdefghi", "en-a-a", "en-\x80"
    };
    assert(qjs_intl_data_open(qjs_intl_locale_metadata_blob,
                              qjs_intl_locale_metadata_blob_size, &v) == QJS_INTL_DATA_OK);
    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        assert(qjs_intl_native_locale_validate(&a, bytes(invalid[i]), &well_formed) == QJS_INTL_OK);
        assert(!well_formed && !state.live);
        out = (char *)1;
        assert(qjs_intl_native_locale_canonicalize(&a, &v, bytes(invalid[i]), &out) == QJS_INTL_INVALID_ARGUMENT);
        assert(!out && !state.live);
    }
    /* Embedded NUL is never silently truncated; the input need not terminate. */
    {
        const char input[] = { 'e', 'n', '\0', '-', 'u', 's' };
        const char slice[] = { 'E', 'N', '-', 'u', 's' };
        QJSIntlBytes b = { input, sizeof(input) };
        assert(qjs_intl_native_locale_validate(&a, b, &well_formed) == QJS_INTL_OK && !well_formed);
        b.data = slice; b.length = sizeof(slice);
        assert(qjs_intl_native_locale_canonicalize(&a, &v, b, &out) == QJS_INTL_OK);
        assert(!strcmp(out, "en-US")); test_free(&state, out);
    }
    check(qjs_intl_native_locale_canonicalize, &a, &v, "SH-Cyrl-BA", "sr-Cyrl-BA");
    check(qjs_intl_native_locale_canonicalize, &a, &v, "sh-BA", "sr-Latn-BA");
    check(qjs_intl_native_locale_canonicalize, &a, &v, "mo-Cyrl-MD", "ro-Cyrl-MD");
    check(qjs_intl_native_locale_canonicalize, &a, &v, "iw-Qaai-BU", "he-Zinh-MM");
    check(qjs_intl_native_locale_canonicalize, &a, &v, "hy-SU", "hy-AM");
    check(qjs_intl_native_locale_canonicalize, &a, &v, "en-AN", "en-CW");
    check(qjs_intl_native_locale_canonicalize, &a, &v, "ja-hepburn-heploc", "ja-alalc97");
    check(qjs_intl_native_locale_canonicalize, &a, &v,
          "EN-u-zzz-AAA-zzz-NU-latn-ca-islamicc-nu-arab-KN-TRUE-x-AbC-X", 
          "en-u-aaa-zzz-ca-islamic-civil-kn-nu-latn-x-abc-x");
    check(qjs_intl_native_locale_canonicalize, &a, &v,
          "en-u-zz-unknown-t-SH-Cyrl-BA-m0-names-h0-true-a-ABC",
          "en-a-abc-t-sr-cyrl-ba-h0-true-m0-prprname-u-zz-unknown");
    check(qjs_intl_native_locale_canonicalize, &a, &v,
          "en-u-ca-ethiopic-amete-alem-sd-cn11-rg-fi01", "en-u-ca-ethioaa-rg-axzzzz-sd-cnbj");
    check(qjs_intl_native_locale_canonicalize, &a, &v,
          "en-t-m0-beta-metsehaf-m0-unknown", "en-t-m0-betamets");
    check(qjs_intl_native_locale_maximize, &a, &v, "en-u-kn-x-foo", "en-Latn-US-u-kn-x-foo");
    check(qjs_intl_native_locale_maximize, &a, &v, "zh-Hant", "zh-Hant-TW");
    check(qjs_intl_native_locale_maximize, &a, &v, "und", "en-Latn-US");
    check(qjs_intl_native_locale_maximize, &a, &v, "en-Zzzz-ZZ", "en-Latn-US");
    check(qjs_intl_native_locale_maximize, &a, &v, "qaa-Qaaa-XY", "qaa-Qaaa-XY");
    check(qjs_intl_native_locale_minimize, &a, &v, "zh-Hant-TW-u-kn-x-foo", "zh-TW-u-kn-x-foo");
    check(qjs_intl_native_locale_minimize, &a, &v, "en-Latn-US-fonipa", "en-fonipa");
    check(qjs_intl_native_locale_minimize, &a, &v, "sr-Latn-RS", "sr-Latn");
    assert(qjs_intl_native_locale_canonicalize_uvalue(&a, &v, bytes("CA"), bytes("ISLAMICC"), &out) == QJS_INTL_OK);
    assert(!strcmp(out, "islamic-civil")); test_free(&state, out);
    assert(qjs_intl_native_locale_canonicalize_uvalue(&a, &v, bytes("kn"), bytes("TRUE"), &out) == QJS_INTL_OK);
    assert(!*out); test_free(&state, out);
    assert(qjs_intl_native_locale_canonicalize_uvalue(&a, &v, bytes("zz"), bytes("UNKNOWN"), &out) == QJS_INTL_OK);
    assert(!strcmp(out, "unknown")); test_free(&state, out);
    assert(qjs_intl_native_locale_canonicalize_uvalue(&a, &v, bytes("a0"), bytes("unknown"), &out) == QJS_INTL_INVALID_ARGUMENT && !out);
    out = (char *)1;
    assert(qjs_intl_native_locale_canonicalize(&a, &bad_view, bytes("en"), &out) == QJS_INTL_DATA_ERROR && !out);
    assert(qjs_intl_native_locale_canonicalize(NULL, &v, bytes("en"), &out) == QJS_INTL_INVALID_ARGUMENT && !out);
    /* Fail each distinct allocation position, including late output emission.
     * A baseline determines the complete count, avoiding guessed cutoffs. */
    state.calls = 0;
    check(qjs_intl_native_locale_canonicalize, &a, &v,
          "sh-u-aaa-ca-islamicc-sd-cn11-t-iw-m0-names-x-foo", 
          "sr-Latn-t-he-m0-prprname-u-aaa-ca-islamic-civil-sd-cnbj-x-foo");
    calls = state.calls;
    for (i = 1; i <= calls; i++) {
        state.calls = 0; state.fail_at = i; out = (char *)1;
        assert(qjs_intl_native_locale_canonicalize(&a, &v,
               bytes("sh-u-aaa-ca-islamicc-sd-cn11-t-iw-m0-names-x-foo"), &out) == QJS_INTL_NO_MEMORY);
        assert(!out && !state.live);
    }
    state.fail_at = 0;
    {
        Operation operations[] = { qjs_intl_native_locale_maximize, qjs_intl_native_locale_minimize };
        const char *inputs[] = { "und-u-kn-x-foo", "zh-Hant-TW-u-kn-x-foo" };
        const char *outputs[] = { "en-Latn-US-u-kn-x-foo", "zh-TW-u-kn-x-foo" };
        size_t operation;
        for (operation = 0; operation < 2; operation++) {
            state.calls = 0;
            check(operations[operation], &a, &v, inputs[operation], outputs[operation]);
            calls = state.calls;
            for (i = 1; i <= calls; i++) {
                state.calls = 0; state.fail_at = i; out = (char *)1;
                assert(operations[operation](&a, &v, bytes(inputs[operation]), &out) == QJS_INTL_NO_MEMORY);
                assert(!out && !state.live);
            }
            state.fail_at = 0;
        }
    }
    {
        QJSIntlBytes huge = { "en", SIZE_MAX };
        out = (char *)1;
        assert(qjs_intl_native_locale_canonicalize(&a, &v, huge, &out) == QJS_INTL_OVERFLOW && !out);
    }
    return 0;
}
