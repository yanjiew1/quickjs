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

int main(void)
{
    QJSICU4XNames *handle = (QJSICU4XNames *)(uintptr_t)1;

    check("en", 0, 1, 0, "ja", "Japanese");
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
    assert(qjs_icu4x_names_open((const uint8_t *)"en", 2, 3, 1, &handle) == QJS_ICU4X_RANGE);
    assert(handle == (QJSICU4XNames *)(uintptr_t)1);
    qjs_icu4x_names_close(NULL);
    return 0;
}
