/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../../src/icu4x/names.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Output {
    uint16_t units[1024];
    size_t length, calls;
    int error;
} Output;

static int write_text(void *opaque, const uint16_t *text, size_t length)
{
    Output *out = opaque;
    out->calls++;
    if (out->error)
        return out->error;
    assert(length <= sizeof(out->units) / sizeof(*out->units) - out->length);
    if (length)
        memcpy(out->units + out->length, text, length * sizeof(*text));
    out->length += length;
    return 0;
}

static void check(const char *locale, unsigned style, unsigned dialect, unsigned kind,
                  const char *code, const char *expected)
{
    QJSICU4XNames *handle = NULL;
    Output output = {0};
    QJSICU4XTextSink sink = {&output, write_text};
    size_t i;
    int status;

    assert(!qjs_icu4x_names_open((const uint8_t *)locale, strlen(locale), style, dialect, &handle));
    status = qjs_icu4x_names_format(handle, kind, (const uint8_t *)code, strlen(code), sink);
    if (!expected) {
        assert(status == QJS_ICU4X_UNSUPPORTED && !output.calls);
    } else {
        if (status) {
            fprintf(stderr, "%s %s returned status %d\n", locale, code, status);
            assert(0);
        }
        assert(output.length == strlen(expected));
        for (i = 0; i < output.length; i++)
            assert(output.units[i] == (unsigned char)expected[i]);
        output = (Output){.error = QJS_ICU4X_MEMORY};
        assert(qjs_icu4x_names_format(handle, kind, (const uint8_t *)code, strlen(code), sink) ==
               QJS_ICU4X_MEMORY);
        assert(output.calls == 1 && !output.length);
    }
    qjs_icu4x_names_close(handle);
}

typedef struct LocaleInventory {
    char previous[128];
    size_t count, calls;
    int en, fr, zz, error;
} LocaleInventory;

static int locale_name(void *opaque, const uint8_t *text, size_t length)
{
    LocaleInventory *out = opaque;
    out->calls++;
    if (out->error)
        return out->error;
    assert(length && length < sizeof(out->previous));
    assert(!memchr(text, 0, length));
    if (out->count) {
        char current[128];
        memcpy(current, text, length);
        current[length] = 0;
        assert(strcmp(out->previous, current) < 0);
    }
    memcpy(out->previous, text, length);
    out->previous[length] = 0;
    out->count++;
    out->en |= !strcmp(out->previous, "en");
    out->fr |= !strcmp(out->previous, "fr");
    out->zz |= !strcmp(out->previous, "zz");
    assert(strcmp(out->previous, "und") && strcmp(out->previous, "root"));
    return 0;
}

/* Context transforms must use the selected Unicode data and locale-sensitive
   titlecasing, while leaving the remaining characters untouched. */
static void context(const char *locale, unsigned mask, unsigned kind, const char *text,
                    const uint16_t *expected, size_t length)
{
    QJSICU4XNames *handle = NULL;
    Output output = {0};
    QJSICU4XTextSink sink = {&output, write_text};

    assert(!qjs_icu4x_names_open_context((const uint8_t *)locale, strlen(locale), 0, 1, mask,
                                         &handle));
    assert(!qjs_icu4x_names_text(handle, kind, (const uint8_t *)text, strlen(text), sink));
    assert(output.length == length);
    assert(!length || !memcmp(output.units, expected, length * sizeof(*expected)));
    output = (Output){.error = QJS_ICU4X_MEMORY};
    assert(qjs_icu4x_names_text(handle, kind, (const uint8_t *)text, strlen(text), sink) ==
           QJS_ICU4X_MEMORY);
    assert(output.calls == 1 && !output.length);
    qjs_icu4x_names_close(handle);
}

int main(void)
{
    QJSICU4XNames *handle = (QJSICU4XNames *)(uintptr_t)1;
    LocaleInventory inventory = {0}, failed = {.error = QJS_ICU4X_MEMORY};

    context("tr", 1, 0, "istanBUL", (const uint16_t[]){0x130, 's', 't', 'a', 'n', 'B', 'U', 'L'},
            8);
    context("nl", 4, 2, "ijssel", (const uint16_t[]){'I', 'J', 's', 's', 'e', 'l'}, 6);
    context("de", 16, 4,
            "\xc3\x9f"
            "eta",
            (const uint16_t[]){'S', 's', 'e', 't', 'a'}, 5);
    context("en", 1, 0, "UPper", (const uint16_t[]){'U', 'P', 'p', 'e', 'r'}, 5);
    context("en", 1, 3, "lower", (const uint16_t[]){'l', 'o', 'w', 'e', 'r'}, 5);
    context("en", 0, 0, "lower", (const uint16_t[]){'l', 'o', 'w', 'e', 'r'}, 5);
    assert(!qjs_icu4x_names_check_version((const uint8_t *)"49.0.0", 6));
    assert(qjs_icu4x_names_check_version((const uint8_t *)"48", 2) == QJS_ICU4X_BACKEND);
    assert(qjs_icu4x_names_open_context((const uint8_t *)"en", 2, 0, 1, 8, &handle) ==
           QJS_ICU4X_RANGE);
    assert(handle == (QJSICU4XNames *)(uintptr_t)1);
    assert(!qjs_icu4x_names_locales(&inventory, locale_name));
    assert(inventory.count > 100 && inventory.en && inventory.fr && !inventory.zz);
    assert(qjs_icu4x_names_locales(&failed, locale_name) == QJS_ICU4X_MEMORY);
    assert(failed.calls == 1 && !failed.count);
    check("en", 0, 1, 0, "ja", "Japanese");
    check("en-US", 0, 1, 0, "fr", "French");
    check("en", 0, 1, 0, "abcdefgh-Latn-US", NULL);
    check("en", 0, 1, 0, "en-GB", "British English");
    check("en", 0, 0, 0, "en-GB", "English (United Kingdom)");
    check("en", 0, 1, 0, "zz", NULL);
    // A known prefix with an absent qualifier must not emit partial output
    // before returning absence, or turn fallback into a spurious output OOM.
    check("en", 0, 0, 0, "en-AA", NULL);
    check("en", 0, 1, 1, "US", "United States");
    check("en", 1, 1, 1, "US", "US");
    check("en", 2, 1, 1, "US", "US");
    check("en", 0, 1, 1, "AA", NULL);
    check("en", 0, 1, 2, "Latn", "Latin");
    check("en", 0, 1, 2, "Xsux", "Sumero-Akkadian Cuneiform");
    {
        Output output = {0};
        QJSICU4XTextSink sink = {&output, write_text};
        assert(!qjs_icu4x_names_open((const uint8_t *)"ja", 2, 0, 1, &handle));
        assert(!qjs_icu4x_names_format(handle, 0, (const uint8_t *)"en", 2, sink));
        assert(output.length == 2 && output.units[0] == 0x82f1 && output.units[1] == 0x8a9e);
        qjs_icu4x_names_close(handle);
        handle = (QJSICU4XNames *)(uintptr_t)1;
    }
    {
        Output output = {0};
        QJSICU4XTextSink sink = {&output, write_text};
        assert(!qjs_icu4x_names_open((const uint8_t *)"en", 2, 0, 1, &handle));
        assert(qjs_icu4x_names_format(handle, 0, (const uint8_t *)"abcdefgh-u", 10, sink) ==
               QJS_ICU4X_RANGE);
        assert(!output.calls);
        qjs_icu4x_names_close(handle);
        handle = (QJSICU4XNames *)(uintptr_t)1;
    }
    assert(qjs_icu4x_names_open((const uint8_t *)"en", 2, 3, 1, &handle) == QJS_ICU4X_RANGE);
    assert(handle == (QJSICU4XNames *)(uintptr_t)1);
    qjs_icu4x_names_close(NULL);
    return 0;
}
