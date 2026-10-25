/* Fixed locale-record composition, borrowing and allocation failure.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT.
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
#include "../src/intl/locale/identifier.h"
#include "../src/intl/common/locale-grammar.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        state->live++;
    return ptr;
}

static void *reallocate(void *opaque, void *old, size_t size)
{
    Allocation *state = opaque;
    int had_old = old != NULL;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = realloc(old, size);
    if (ptr && !had_old)
        state->live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    if (ptr) {
        assert(state->live);
        state->live--;
        free(ptr);
    }
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static void equal(QJSIntlBytes value, const char *text)
{
    assert(value.data && value.length == strlen(text));
    assert(!value.length || !memcmp(value.data, text, value.length));
}

static void test_components(void)
{
    QJSIntlLocaleComponents parts;
    const char *tag = "en-Latn-US-1901-a-foo-u-attr-ca-gregory-kn-x-u-ca";
    const char slice[] = {'s', 'l', '-', '1', '9', '9', '4',
                          '-', 'r', 'o', 'z', 'a', 'j'};

    assert(qjs_intl_locale_components(bytes(tag), &parts) == QJS_INTL_OK);
    equal(parts.base_name, "en-Latn-US-1901");
    equal(parts.language, "en");
    equal(parts.script, "Latn");
    equal(parts.region, "US");
    equal(parts.variants, "1901");
    equal(parts.unicode_extension, "-u-attr-ca-gregory-kn");
    equal(qjs_intl_locale_keyword(&parts, "ca"), "gregory");
    equal(qjs_intl_locale_keyword(&parts, "kn"), "");
    assert(!qjs_intl_locale_keyword(&parts, "nu").data);
    assert(parts.language.data == tag && parts.script.data == tag + 3);
    assert(qjs_intl_locale_components((QJSIntlBytes){slice, sizeof(slice)},
                                      &parts) == QJS_INTL_OK);
    equal(parts.base_name, "sl-1994-rozaj");
    equal(parts.variants, "1994-rozaj");
    assert(!parts.script.data && !parts.region.data &&
           !parts.unicode_extension.data);
    assert(qjs_intl_locale_components(bytes("en-u-ca-kn-false-nu"), &parts) ==
           QJS_INTL_OK);
    equal(qjs_intl_locale_keyword(&parts, "ca"), "");
    equal(qjs_intl_locale_keyword(&parts, "kn"), "false");
    equal(qjs_intl_locale_keyword(&parts, "nu"), "");
    assert(qjs_intl_locale_components(bytes("en-x-u-ca-gregory"), &parts) ==
           QJS_INTL_OK);
    assert(!parts.unicode_extension.data &&
           !qjs_intl_locale_keyword(&parts, "ca").data);
    assert(qjs_intl_locale_components((QJSIntlBytes){NULL, 0}, &parts) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!parts.language.data);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, reallocate, release};
    QJSIntlLocaleBaseOptions options = {0};
    QJSIntlLocaleKeyword keywords[] = {
        {{'c', 'a'}, {"buddhist", 8}},
        {{'k', 'n'}, {"false", 5}},
        {{'n', 'u'}, {"latn", 4}},
    };
    QJSIntlBytes tag =
        bytes("en-Latn-US-a-foo-u-attr-ca-gregory-kn-zz-unknown-x-u-ca");
    char *out;
    size_t operation, calls, i;

    test_components();
    assert(
        qjs_intl_unicode_subtag_well_formed(QJS_INTL_LANGUAGE_SUBTAG, "FR", 2));
    assert(!qjs_intl_unicode_subtag_well_formed(QJS_INTL_LANGUAGE_SUBTAG,
                                                "root", 4));
    assert(
        qjs_intl_unicode_subtag_well_formed(QJS_INTL_SCRIPT_SUBTAG, "Latn", 4));
    assert(!qjs_intl_unicode_subtag_well_formed(QJS_INTL_SCRIPT_SUBTAG, "La\0n",
                                                4));
    assert(
        qjs_intl_unicode_subtag_well_formed(QJS_INTL_REGION_SUBTAG, "419", 3));
    assert(!qjs_intl_unicode_subtag_well_formed(QJS_INTL_REGION_SUBTAG, "419-",
                                                4));
    assert(qjs_intl_unicode_subtag_well_formed(QJS_INTL_VARIANT_SUBTAG, "1994",
                                               4));
    assert(!qjs_intl_unicode_subtag_well_formed(QJS_INTL_VARIANT_SUBTAG, "abcd",
                                                4));

    options.language = bytes("fr");
    options.variants = bytes("biske-rozaj");
    assert(qjs_intl_locale_replace_base(&allocator, tag, &options, &out) ==
           QJS_INTL_OK);
    assert(!strcmp(
        out,
        "fr-Latn-US-biske-rozaj-a-foo-u-attr-ca-gregory-kn-zz-unknown-x-u-ca"));
    release(&state, out);
    {
        QJSIntlLocaleBaseOptions added = {0};
        added.script = bytes("Latn");
        added.region = bytes("419");
        assert(qjs_intl_locale_replace_base(&allocator, bytes("es"), &added,
                                             &out) == QJS_INTL_OK);
        assert(!strcmp(out, "es-Latn-419"));
        release(&state, out);
    }
    assert(qjs_intl_locale_replace_keywords(
               &allocator, bytes("en-t-en-us-m0-ungegn-u-kn-x-t-foo"),
               keywords, 1, &out) == QJS_INTL_OK);
    assert(!strcmp(out, "en-t-en-us-m0-ungegn-u-kn-ca-buddhist-x-t-foo"));
    release(&state, out);
    assert(qjs_intl_locale_replace_keywords(&allocator, tag, keywords, 3,
                                            &out) == QJS_INTL_OK);
    assert(!strcmp(out, "en-Latn-US-a-foo-u-attr-zz-unknown-ca-buddhist-kn-"
                        "false-nu-latn-x-u-ca"));
    release(&state, out);
    assert(qjs_intl_locale_replace_keywords(&allocator, bytes("en-a-foo-x-u"),
                                            keywords, 1, &out) == QJS_INTL_OK);
    assert(!strcmp(out, "en-u-ca-buddhist-a-foo-x-u"));
    release(&state, out);
    assert(qjs_intl_locale_replace_keywords(&allocator, tag, NULL, 0, &out) ==
           QJS_INTL_OK);
    assert(!strcmp(out, tag.data));
    release(&state, out);
    keywords[0].value = bytes("");
    assert(qjs_intl_locale_replace_keywords(&allocator,
                                            bytes("en-u-ca-gregory"), keywords,
                                            1, &out) == QJS_INTL_OK);
    assert(!strcmp(out, "en-u-ca"));
    release(&state, out);
    keywords[0].value = bytes("buddhist");

    for (operation = 0; operation < 2; operation++) {
        state.calls = 0;
        assert((operation
                    ? qjs_intl_locale_replace_keywords(&allocator, tag,
                                                       keywords, 3, &out)
                    : qjs_intl_locale_replace_base(&allocator, tag, &options,
                                                   &out)) == QJS_INTL_OK);
        calls = state.calls;
        release(&state, out);
        assert(calls && !state.live);
        for (i = 1; i <= calls; i++) {
            state.calls = 0;
            state.fail_at = i;
            out = (char *)1;
            assert((operation ? qjs_intl_locale_replace_keywords(
                                    &allocator, tag, keywords, 3, &out)
                              : qjs_intl_locale_replace_base(&allocator, tag,
                                                             &options, &out)) ==
                   QJS_INTL_NO_MEMORY);
            assert(!out && !state.live);
        }
        state.fail_at = 0;
    }
    keywords[1] = keywords[0];
    out = (char *)1;
    assert(qjs_intl_locale_replace_keywords(&allocator, tag, keywords, 2,
                                            &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(!out && !state.live);
    state.calls = 0;
    keywords[0].key[1] = '0';
    assert(qjs_intl_locale_replace_keywords(&allocator, tag, keywords, 1,
                                             &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(!out && !state.calls && !state.live);
    keywords[0].key[1] = 'a';
    keywords[0].value = bytes("x");
    assert(qjs_intl_locale_replace_keywords(&allocator, tag, keywords, 1,
                                             &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(!out && !state.calls && !state.live);
    return 0;
}
