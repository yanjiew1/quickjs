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
#include "../../src/icu4x/collation.h"
#include <assert.h>
#include <string.h>

static QJSICU4XCollator *open_handle(const char *locale, unsigned usage, const char *name,
                                     QJSICU4XCollationOptions options)
{
    QJSICU4XCollator *handle = NULL;

    assert(!qjs_icu4x_collator_open((const uint8_t *)locale, strlen(locale), usage,
                                    (const uint8_t *)name, strlen(name), options, &handle));
    assert(handle);
    return handle;
}

static void compare(const QJSICU4XCollator *handle, const uint16_t *a, size_t n, const uint16_t *b,
                    size_t m, int expected)
{
    int32_t result = 42;

    assert(!qjs_icu4x_collator_compare(handle, a, n, b, m, &result));
    assert(result == expected);
}

static int count_type(void *opaque, const uint8_t *name, size_t length)
{
    unsigned *count = opaque;

    assert(length && memcmp(name, "standard", length < 8 ? length : 8));
    assert(length != 6 || memcmp(name, "search", 6));
    (*count)++;
    return 0;
}

static int stop_emit(void *opaque, const uint8_t *name, size_t length)
{
    (void)opaque;
    (void)name;
    (void)length;
    return 123;
}

int main(void)
{
    QJSICU4XCollationOptions options = {0};
    QJSICU4XCollator *handle, *failed = (QJSICU4XCollator *)(uintptr_t)1;
    uint32_t supported;
    int32_t result = 42;
    unsigned count = 0;
    static const uint16_t a[] = {'a'}, upper[] = {'A'}, accent[] = {0xe1};
    static const uint16_t composed[] = {0xe9}, decomposed[] = {'e', 0x301};
    static const uint16_t non_fcd[] = {'a', 0x301, 0x327}, ordered[] = {'a', 0x327, 0x301};
    static const uint16_t ae[] = {'A', 'E'}, umlaut[] = {0xc4};
    static const uint16_t two[] = {'2'}, ten[] = {'1', '0'};
    static const uint16_t nul_a[] = {'a', 0, 'b'}, nul_b[] = {'a', 0, 'c'};
    static const uint16_t punct[] = {'a', '-'}, symbol[] = {'a', '$'};
    static const uint16_t lone[] = {0xd800}, replacement[] = {0xfffd};

    assert(!qjs_icu4x_collation_types(&count, count_type) && count > 2);
    assert(qjs_icu4x_collation_types(NULL, stop_emit) == 123);
    assert(!qjs_icu4x_collation_defaults((const uint8_t *)"da", 2, 0, &options));
    assert(options.case_first == 1);
    assert(!qjs_icu4x_collation_defaults((const uint8_t *)"th", 2, 0, &options));
    assert(options.ignore_punctuation == 1);
    assert(!qjs_icu4x_collation_supported((const uint8_t *)"de", 2, (const uint8_t *)"phonebk", 7,
                                          &supported) &&
           supported);
    assert(!qjs_icu4x_collation_supported((const uint8_t *)"en", 2, (const uint8_t *)"phonebk", 7,
                                          &supported) &&
           !supported);
    assert(!qjs_icu4x_collation_supported((const uint8_t *)"en", 2, (const uint8_t *)"unknown", 7,
                                          &supported) &&
           !supported);
    assert(!qjs_icu4x_collation_defaults((const uint8_t *)"en", 2, 0, &options));
    assert(!options.sensitivity);
    handle = open_handle("en", 0, "", options);
    compare(handle, composed, 1, decomposed, 2, 0);
    compare(handle, non_fcd, 3, ordered, 3, 0);
    compare(handle, nul_a, 3, nul_b, 3, -1);
    /* ICU4X assigns replacement weights to malformed UTF-16, as permitted by
       UCA's ill-formed-input policy; the counted input is never rewritten. */
    compare(handle, lone, 1, replacement, 1, 0);
    compare(handle, NULL, 0, NULL, 0, 0);
    assert(qjs_icu4x_collator_compare(handle, NULL, 1, NULL, 0, &result) == QJS_ICU4X_RANGE);
    assert(result == 42);
    qjs_icu4x_collator_close(handle);

    options.sensitivity = 1;
    handle = open_handle("en", 0, "", options);
    compare(handle, a, 1, upper, 1, 0);
    compare(handle, a, 1, accent, 1, 0);
    qjs_icu4x_collator_close(handle);
    options.sensitivity = 2;
    handle = open_handle("en", 0, "", options);
    compare(handle, a, 1, upper, 1, 0);
    compare(handle, a, 1, accent, 1, -1);
    qjs_icu4x_collator_close(handle);
    options.sensitivity = 3;
    handle = open_handle("en", 0, "", options);
    compare(handle, a, 1, upper, 1, -1);
    compare(handle, a, 1, accent, 1, 0);
    qjs_icu4x_collator_close(handle);
    options.sensitivity = 0;
    options.case_first = 1;
    options.numeric = 1;
    options.ignore_punctuation = 1;
    handle = open_handle("en", 0, "", options);
    compare(handle, a, 1, upper, 1, 1);
    compare(handle, two, 1, ten, 2, -1);
    compare(handle, a, 1, punct, 2, 0);
    compare(handle, a, 1, symbol, 2, -1);
    qjs_icu4x_collator_close(handle);

    options = (QJSICU4XCollationOptions){0};
    handle = open_handle("de", 1, "", options);
    compare(handle, ae, 2, umlaut, 1, -1);
    qjs_icu4x_collator_close(handle);
    options.sensitivity = 1;
    handle = open_handle("de", 0, "phonebk", options);
    compare(handle, ae, 2, umlaut, 1, 0);
    qjs_icu4x_collator_close(handle);
    assert(qjs_icu4x_collator_open((const uint8_t *)"en", 2, 0, (const uint8_t *)"phonebk", 7,
                                   options, &failed) == QJS_ICU4X_UNSUPPORTED);
    assert(failed == (QJSICU4XCollator *)(uintptr_t)1);
    options.numeric = 2;
    assert(qjs_icu4x_collator_open((const uint8_t *)"en", 2, 0, NULL, 0, options, &failed) ==
           QJS_ICU4X_RANGE);
    assert(failed == (QJSICU4XCollator *)(uintptr_t)1);
    qjs_icu4x_collator_close(NULL);
    return 0;
}
