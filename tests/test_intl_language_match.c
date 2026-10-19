/* Source-driven longest dialect matching and inherited candidate ranking.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/display/language-match.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t allocations, live;

static void *allocate(void *opaque, size_t size)
{
    void *ptr;
    (void)opaque;
    assert(size);
    allocations++;
    ptr = malloc(size);
    if (ptr)
        live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    (void)opaque;
    assert(ptr && live);
    live--;
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    QJSIntlBytes result = {text, strlen(text)};
    return result;
}

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) &&
           (!text.length || !memcmp(text.data, expected, text.length));
}

static uint32_t locale_id(const QJSIntlData *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_locale(data, i, &text, &parent));
        if (equal(text, name))
            return i;
    }
    abort();
}

static void check(const QJSIntlNameData *owner, const char *locale, QJSIntlNameStyle style,
                  int dialect, const char *input, const char *key, size_t count,
                  const char *text, const char *origin)
{
    QJSIntlLanguageNameMatch result, zero = {0};
    assert(!qjs_intl_language_name_match(owner, locale_id(&owner->data, locale), style,
                                         dialect, bytes(input), &result));
    if (!key) {
        assert(!memcmp(&result, &zero, sizeof(result)));
        return;
    }
    assert(result.label.found && result.subtags == count && equal(result.code, key));
    assert(equal(result.label.entry.text, text) && equal(result.label.origin_locale, origin));
}

int main(void)
{
    QJSIntlAllocator allocator = {NULL, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlNameData owner;
    QJSIntlLanguageNameMatch result, zero = {0};
    size_t calls;
    uint32_t en;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_name_data_open(&data, &allocator, &owner) && !live);
    calls = allocations;
    en = locale_id(&data, "en");
#if TEST_INTL_LANGUAGE_MATCH_PREPARED
    check(&owner, "en", QJS_INTL_NAME_LONG, 1, "en", "en", 1, "English", "en");
    {
        QJSIntlLanguageNameMatch match;
        assert(!qjs_intl_language_name_match(&owner, en, QJS_INTL_NAME_LONG, 1,
                                             bytes("ca-fonipa-valencia"), &match));
        assert(match.label.found && equal(match.code, "ca") && match.subtags == 1);
        assert(!qjs_intl_language_name_match(&owner, en, QJS_INTL_NAME_LONG, 1,
                                             bytes("nl-Cyrl-BE"), &match));
        assert(match.label.found && equal(match.code, "nl-be") && match.subtags == 2);
    }
#else
    unsigned int style;
    for (style = QJS_INTL_NAME_LONG; style <= QJS_INTL_NAME_NARROW; style++) {
        check(&owner, "en", style, 1, "nl-Cyrl-BE", "nl-be", 2, "Flemish", "root");
        check(&owner, "en", style, 0, "nl-Cyrl-BE", "nl", 1, "Child Dutch", "en");
        check(&owner, "en", style, 1, "ca-fonipa-valencia", "ca-valencia", 2, "Valencian", "root");
        check(&owner, "en", style, 0, "ca-fonipa-valencia", "ca", 1, "Catalan", "root");
        check(&owner, "en", style, 1, "az-Cyrl-AZ", "az-cyrl", 2, "Cyrillic Azerbaijani", "root");
        check(&owner, "en", style, 1, "de-1901-1996", "de-1901", 2, "Traditional German", "root");
        check(&owner, "en", style, 1, "sr-Latn-RS-1994", "sr-latn-rs", 3, "Serbia Latin Serbian", "root");
        check(&owner, "en", style, 1, "fr-CA", "fr", 1, "", "en");
        check(&owner, "en", style, 1, "es-MX", "es-mx", 2, "", "root");
        check(&owner, "en", style, 1, "it-CH", "it", 1, "Italian", "root");
        check(&owner, "en", style, 1, "ro-MD", "ro-md", 2, "ro-MD", "root");
        check(&owner, "en", style, 1, "en-US", "en", 1, "English", "root");
        check(&owner, "en", style, 1, "zz", NULL, 0, NULL, NULL);
        if (style == QJS_INTL_NAME_LONG)
            check(&owner, "en", style, 1, "pt-BR", "pt", 1, "Child Portuguese", "en");
        else
            check(&owner, "en", style, 1, "pt-BR", "pt-br", 2, "Brazilian short", "root");
    }
    {
        char input[1024] = "de";
        size_t at = 2;
        unsigned int i;
        for (i = 0; i < 128; i++) {
            int length = snprintf(input + at, sizeof(input) - at, "-%u", 20000 + i);
            assert(length > 0 && (size_t)length < sizeof(input) - at);
            at += length;
        }
        check(&owner, "root", QJS_INTL_NAME_LONG, 1, input, "de", 1, "German", "root");
    }
#endif
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_language_name_match(&owner, en, -1, 1, bytes("en"), &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_language_name_match(&owner, en, QJS_INTL_NAME_LONG, 2, bytes("en"), &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_language_name_match(&owner, en, QJS_INTL_NAME_LONG, 1,
                                         bytes("en-u-ca-gregory"), &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_language_name_match(&owner, en, QJS_INTL_NAME_LONG, 1,
                                         bytes("en-x-test"), &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_language_name_match(&owner, en, QJS_INTL_NAME_LONG, 1,
                                         (QJSIntlBytes){"en", SIZE_MAX}, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_language_name_match(&owner, UINT32_MAX, QJS_INTL_NAME_LONG, 1,
                                         bytes("zz"), &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_language_name_match(NULL, en, QJS_INTL_NAME_LONG, 1,
                                         bytes("en"), &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_language_name_match(&owner, en, QJS_INTL_NAME_LONG, 1,
                                         bytes("en"), NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(allocations == calls && !live);
    return 0;
}
