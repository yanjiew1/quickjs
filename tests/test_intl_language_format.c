/* Source-driven language composition and bounded owned UTF-16 results.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../src/intl/display/language-format.h"
#include "../src/cutils/cutils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t calls, fail_at = SIZE_MAX, live;

static void *allocate(void *opaque, size_t size)
{
    void *ptr;
    (void)opaque;
    assert(size);
    if (calls++ == fail_at)
        return NULL;
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

static const QJSIntlAllocator allocator = {NULL, allocate, NULL, release};

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static uint32_t locale_id(const QJSIntlData *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_locale(data, i, &text, &parent));
        if (text.length == strlen(name) && !memcmp(text.data, name, text.length))
            return i;
    }
    abort();
}

static void equal(const QJSIntlNameText *result, const char *expected)
{
    const uint8_t *at = (const uint8_t *)expected, *end = at + strlen(expected);
    size_t i = 0;
    while (at < end) {
        int c = unicode_from_utf8(at, end - at < 4 ? (int)(end - at) : 4, &at);
        assert(c > 0);
        if (c > 0xffff) {
            c -= 0x10000;
            assert(i < result->length && result->text[i++] == 0xd800 + (c >> 10));
            c = 0xdc00 + (c & 1023);
        }
        assert(i < result->length && result->text[i++] == c);
    }
    assert(i == result->length && result->found);
}

static void check(const QJSIntlNameData *owner, const char *locale, const char *input,
                  int dialect, const char *expected)
{
    QJSIntlNameText result, zero = {0};
    assert(!qjs_intl_language_name_format(owner, locale_id(&owner->data, locale),
                                         QJS_INTL_NAME_LONG, dialect, bytes(input),
                                         &allocator, 8192, &result));
    if (expected)
        equal(&result, expected);
    else
        assert(!memcmp(&result, &zero, sizeof(result)));
    qjs_intl_name_text_clear(&allocator, &result);
    qjs_intl_name_text_clear(&allocator, &result);
    assert(!live && !memcmp(&result, &zero, sizeof(result)));
}

static void failure(const QJSIntlNameData *owner, uint32_t locale, const char *input,
                    size_t limit, QJSIntlStatus expected)
{
    QJSIntlNameText result, zero = {0};
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_language_name_format(owner, locale, QJS_INTL_NAME_LONG, 0, bytes(input),
                                         &allocator, limit, &result) == expected);
    assert(!memcmp(&result, &zero, sizeof(result)) && !live);
}

int main(void)
{
    QJSIntlData data;
    QJSIntlNameData owner;
    QJSIntlNameText result;
    uint32_t en;
    size_t start, count, i;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_name_data_open(&data, &allocator, &owner) && !live);
    en = locale_id(&data, "en");
#if TEST_INTL_LANGUAGE_FORMAT_PREPARED
    check(&owner, "en", "en-US", 0, "English (United States)");
    check(&owner, "en", "nl-Cyrl-BE", 1, "Flemish (Cyrillic)");
#else
    check(&owner, "en", "en", 1, "English (base)");
    check(&owner, "en", "en-Latn-US-valencia", 1,
          "English (base) (PPLatin [script]SUnited States [region]ZSValencian [variant]Z)");
    check(&owner, "en-GB", "en-Latn-US-valencia", 1,
          "O'PValencian (variant)SPUnited States (region)SLatin (script)ZZ'MEnglish (base)E");
    check(&owner, "en", "nl-Cyrl-BE", 1, "Flemish (Cyrillic （script）)");
    check(&owner, "de", "nl-Cyrl-BE", 1, "Flemish（Cyrillic ［script］）");
    check(&owner, "en", "ca-fonipa-valencia", 1, "Valencian (fonipa)");
    check(&owner, "en", "ca-fonipa-valencia", 0, "Catalan (PfonipaSValencian [variant]Z)");
    check(&owner, "en", "fr", 1, "");
    check(&owner, "en", "fr-US", 1, " (United States [region])");
    check(&owner, "en", "es-MX", 1, "");
    check(&owner, "en", "en-AQ", 1, "English (base) ()");
    check(&owner, "en", "en-Latn-AQ", 1, "English (base) (PLatin [script]SZ)");
    check(&owner, "en", "en-CA", 1, "English (base) (Region 😀)");
    check(&owner, "en", "en-Qaaa-QQ-20000", 1, "English (base) (PPQaaaSQQZS20000Z)");
    check(&owner, "en-1901-1996", "en", 1, "English (base)");
    failure(&owner, locale_id(&data, "en-1901-1996"), "en-GB", 8192, QJS_INTL_DATA_ERROR);
    failure(&owner, locale_id(&data, "en-1901"), "en-GB", 8192, QJS_INTL_UNSUPPORTED);
    /* One qualifier never consults the malformed separator, or bracket facts
       for punctuation that is not present in the selected qualifier. */
    check(&owner, "en-1996", "en-GB", 1, "English (base) (GB)");
    failure(&owner, locale_id(&data, "en-1996"), "en-Latn-GB", 8192, QJS_INTL_DATA_ERROR);
    failure(&owner, locale_id(&data, "en-1996"), "en-Latn", 8192, QJS_INTL_DATA_ERROR);
    {
        char input[1024] = "en";
        size_t at = 2;
        for (i = 0; i < 128; i++) {
            int length = snprintf(input + at, sizeof(input) - at, "-%u", 20000 + (unsigned int)i);
            assert(length > 0 && (size_t)length < sizeof(input) - at);
            at += length;
        }
        assert(!qjs_intl_language_name_format(&owner, en, QJS_INTL_NAME_LONG, 1, bytes(input),
                                             &allocator, 8192, &result));
        assert(result.found && result.length > 128 * 5);
        qjs_intl_name_text_clear(&allocator, &result);
        failure(&owner, en, input, 4, QJS_INTL_OVERFLOW);
    }
#endif
    check(&owner, "en", "zz-GB", 1, NULL);
    start = calls;
    assert(!qjs_intl_language_name_format(&owner, en, QJS_INTL_NAME_LONG, 0, bytes("en-Latn-US"),
                                         &allocator, 8192, &result));
    count = calls - start;
    assert(count == 2 && result.found);
    {
        size_t length = result.length;
        qjs_intl_name_text_clear(&allocator, &result);
        failure(&owner, en, "en-Latn-US", length - 1, QJS_INTL_OVERFLOW);
        assert(!qjs_intl_language_name_format(&owner, en, QJS_INTL_NAME_LONG, 0,
                                             bytes("en-Latn-US"), &allocator, length, &result));
        assert(result.length == length);
        qjs_intl_name_text_clear(&allocator, &result);
    }
    for (i = 0; i < count; i++) {
        fail_at = calls + i;
        failure(&owner, en, "en-Latn-US", 8192, QJS_INTL_NO_MEMORY);
        fail_at = SIZE_MAX;
    }
    failure(&owner, en, "en", 0, QJS_INTL_INVALID_ARGUMENT);
    failure(&owner, UINT32_MAX, "en", 8192, QJS_INTL_INVALID_ARGUMENT);
    failure(&owner, en, "en-u-ca-gregory", 8192, QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_language_name_format(&owner, en, QJS_INTL_NAME_LONG, 1, bytes("en"),
                                         NULL, 8192, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!result.text && !result.length && !result.found);
    assert(qjs_intl_language_name_format(&owner, en, QJS_INTL_NAME_LONG, 1, bytes("en"),
                                         &allocator, 8192, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_language_name_format(&owner, en, -1, 1, bytes("en"),
                                         &allocator, 8192, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!result.text && !result.length && !result.found);
    {
        uint8_t *copy = malloc(qjs_intl_native_length);
        assert(copy);
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        assert(!qjs_intl_data_open(copy, qjs_intl_native_length, &allocator, &data));
        assert(!qjs_intl_name_data_open(&data, &allocator, &owner));
        assert(!qjs_intl_language_name_format(&owner, en, QJS_INTL_NAME_LONG, 0, bytes("en"),
                                             &allocator, 8192, &result));
        free(copy);
#if TEST_INTL_LANGUAGE_FORMAT_PREPARED
        equal(&result, "English");
#else
        equal(&result, "English (base)");
#endif
        qjs_intl_name_text_clear(&allocator, &result);
    }
    assert(!live);
    qjs_intl_name_text_clear(&allocator, NULL);
    return 0;
}
