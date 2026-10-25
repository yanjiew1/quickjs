/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

/* Host discovery does not mutate the libc locale or claim service support. */
#include "../src/intl/locale/system.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void parsed(const char *source, const char *expected)
{
    char buffer[128];
    assert(qjs_intl_locale_parse_environment(buffer, sizeof(buffer), source) ==
           QJS_INTL_OK);
    assert(!strcmp(buffer, expected));
}

int main(void)
{
    char buffer[128] = "unchanged";
    parsed("en_US.UTF-8", "en-US");
    parsed("sr_RS@latin", "sr-RS");
    parsed("zh_Hant_TW.UTF8@custom", "zh-Hant-TW");
    parsed("en-US", "en-US");
    parsed("C.UTF-8", "en");
    parsed("POSIX", "en");
    assert(qjs_intl_locale_parse_environment(buffer, sizeof(buffer), "") ==
           QJS_INTL_UNSUPPORTED);
    assert(!strcmp(buffer, "unchanged"));
    assert(qjs_intl_locale_parse_environment(buffer, 2, "C") == QJS_INTL_UNSUPPORTED);
    assert(!strcmp(buffer, "unchanged"));
    assert(qjs_intl_locale_parse_environment(buffer, 3, "en_US") ==
           QJS_INTL_UNSUPPORTED);
    assert(!strcmp(buffer, "unchanged"));
    assert(qjs_intl_locale_parse_environment(NULL, 3, "en") ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_locale_parse_environment(buffer, 0, "en") ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_locale_parse_environment(buffer, sizeof(buffer), NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
#ifndef _WIN32
    {
        const char *names[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
        char *saved[3];
        size_t i;
        for (i = 0; i < 3; i++) {
            const char *old = getenv(names[i]);
            saved[i] = old ? strdup(old) : NULL;
            assert(!old || saved[i]);
            assert(!unsetenv(names[i]));
        }
        assert(qjs_intl_locale_system_default(buffer, sizeof(buffer)) ==
               QJS_INTL_UNSUPPORTED);
        assert(!setenv("LANG", "de_DE.UTF-8", 1));
        assert(qjs_intl_locale_system_default(buffer, sizeof(buffer)) == QJS_INTL_OK);
        assert(!strcmp(buffer, "de-DE"));
        assert(!setenv("LC_MESSAGES", "fr_CA.UTF-8", 1));
        assert(qjs_intl_locale_system_default(buffer, sizeof(buffer)) == QJS_INTL_OK);
        assert(!strcmp(buffer, "fr-CA"));
        assert(!setenv("LC_ALL", "C", 1));
        assert(qjs_intl_locale_system_default(buffer, sizeof(buffer)) == QJS_INTL_OK);
        assert(!strcmp(buffer, "en"));
        for (i = 0; i < 3; i++) {
            if (saved[i]) {
                assert(!setenv(names[i], saved[i], 1));
                free(saved[i]);
            } else {
                assert(!unsetenv(names[i]));
            }
        }
    }
#endif
    return 0;
}
