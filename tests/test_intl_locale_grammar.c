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
#include "../src/intl/common/locale-grammar.h"
#include <assert.h>
#include <string.h>

static void test_locales(void)
{
    static const char *valid[] = {
        "en",
        "EN-us",
        "zh-Hant-TW",
        "und",
        "qaa-Qaaa-XY",
        "en-001",
        "sl-rozaj-biske-1994",
        "de-1996",
        "abcde",
        "abcdefgh",
        "en-a-foo-b-bar",
        "en-0-foo",
        "en-x-foo-x-u",
        "en-x-x",
        "en-u-kn",
        "en-u-ca-gregory",
        "en-u-abc-def-nu-latn",
        "en-u-ca-gregory-ca-buddhist",
        "en-u-abc-abc",
        "en-u-0a-abc",
        "en-t-en-US",
        "en-t-en-u-ca-gregory",
        "en-u-kn-ca",
        "en-a-12",
        "en-x-1",
        "en-t-m0-ungegn",
        "en-t-en-Latn-US-m0-ungegn",
        "en-t-m0-ungegn-m0-prprname",
        "en-t-sl-rozaj-biske-a-foo",
        "en-a-xx-u-ca-gregory-t-en-m0-ungegn-x-a",
    };
    static const char *invalid[] = {
        "",
        "en-",
        "-en",
        "en--US",
        "abcd",
        "a",
        "abcdefghi",
        "root",
        "Latn-US",
        "en-t-root",
        "en-u-ca-x",
        "i-klingon",
        "x-private",
        "en_US",
        "zh-cmn",
        "sgn-BE-FR",
        "en-u",
        "en-x",
        "en-a-foo-A-bar",
        "sl-rozaj-ROZAJ",
        "en-t-sl-rozaj-ROZAJ",
        "en-t-m0",
        "en-t-00-abc",
        "en-u-a0-abc",
        "en-u-ca-abcdefghi",
        "en-a-a",
        "en-\x80",
        "en-t-und-Zzzz-419-1901-1901",
        "en-t-m0-ab",
        "en-u-ca-12",
        "en-a-foo-u-ca-gregory-a-bar",
    };
    const char slice[] = {'e', 'n', '-', 'U', 'S'};
    const char embedded_nul[] = {'e', 'n', '\0', '-', 'U', 'S'};
    size_t i;

    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
        assert(qjs_intl_unicode_locale_well_formed(valid[i], strlen(valid[i])));
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
        assert(!qjs_intl_unicode_locale_well_formed(invalid[i], strlen(invalid[i])));
    assert(qjs_intl_unicode_locale_well_formed(slice, sizeof(slice)));
    assert(!qjs_intl_unicode_locale_well_formed(embedded_nul, sizeof(embedded_nul)));
    assert(!qjs_intl_unicode_locale_well_formed(NULL, 0));
    assert(!qjs_intl_unicode_locale_well_formed(NULL, 1));
}

static void test_types(void)
{
    static const char *valid[] = {"abc", "123", "gregory", "islamic-civil",
                                  "ABCD-12345678"};
    static const char *invalid[] = {"",     "ab",  "a-bcd",  "abcdefghi", "abc-",
                                    "-abc", "a_b", "abc-12", "\xc3\xa9"};
    const char slice[] = {'a', 'b', 'c'};
    const char embedded_nul[] = {'a', 'b', 'c', '\0'};
    size_t i;

    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
        assert(qjs_intl_unicode_type_well_formed(valid[i], strlen(valid[i])));
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
        assert(!qjs_intl_unicode_type_well_formed(invalid[i], strlen(invalid[i])));
    assert(qjs_intl_unicode_type_well_formed(slice, sizeof(slice)));
    assert(!qjs_intl_unicode_type_well_formed(embedded_nul, sizeof(embedded_nul)));
    assert(!qjs_intl_unicode_type_well_formed(NULL, 0));
    assert(!qjs_intl_unicode_type_well_formed(NULL, 1));
}

int main(void)
{
    test_locales();
    test_types();
    return 0;
}
