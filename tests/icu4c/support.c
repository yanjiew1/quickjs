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
#include "helpers.h"
#include "../../src/intl/icu4c/common.h"
#include "../../src/intl/icu4c/numbering.h"

static const char *inventory(int32_t index)
{
    static const char *const ids[] = {"root", "en_US",      "de",         "en_US_POSIX", "en",
                                      "de",   "zh_Hant_TW", "az_Cyrl_AZ", "az_Latn_AZ"};
    return ids[index];
}

static void test_helpers(const QJSIntlAllocator *allocator)
{
    QJSICU4CLocales locales;
    Allocation *state = allocator->opaque;
    char *id, *tag, *selected;
    size_t calls, i;

    assert(!qjs_icu4c_locales_open(allocator, 9, inventory, &locales));
    {
        static const char *const expected[] = {"az-AZ", "az-Cyrl-AZ", "az-Latn-AZ", "de",
                                               "en",    "en-US",      "zh-Hant-TW", "zh-TW"};

        assert(locales.count == sizeof(expected) / sizeof(*expected));
        for (i = 0; i < locales.count; i++)
            assert(!strcmp(locales.tags[i].data, expected[i]));
    }
    qjs_icu4c_locales_clear(&locales);
    qjs_icu4c_locales_clear(&locales);
    assert(!state->live);
    assert(!qjs_icu4c_locale_id(allocator, bytes("de-DE"), &id));
    assert(!qjs_icu4c_locale_keyword(allocator, id, "collation", bytes("phonebk"), &selected));
    assert(!qjs_icu4c_language_tag(allocator, selected, &tag));
    assert(!strcmp(tag, "de-DE-u-co-phonebk"));
    icu4c_test_free(state, tag);
    icu4c_test_free(state, selected);
    icu4c_test_free(state, id);
    assert(qjs_icu4c_locale_id(allocator, bytes("en-@bogus"), &id) == QJS_INTL_INVALID_ARGUMENT &&
           !id);
    assert(qjs_icu4c_copy_bytes(allocator, (QJSIntlBytes){"e\0n", 3}, &id) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !id);

    /* This deliberately small inventory permits an exhaustive allocation
       sweep without repeatedly constructing the full ICU locale catalogue. */
    calls = state->calls;
    assert(!qjs_icu4c_locales_open(allocator, 9, inventory, &locales));
    calls = state->calls - calls;
    qjs_icu4c_locales_clear(&locales);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_icu4c_locales_open(allocator, 9, inventory, &locales) == QJS_INTL_NO_MEMORY);
        assert(!locales.count && !locales.tags && !state->live);
    }
    state->fail_at = 0;
}

static void test_numbering(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSICU4CLocales locales;
    QJSICU4CNumbering view;
    QJSIntlBytes name;
    size_t calls, i;
    int supported;

    assert(!qjs_icu4c_locales_open(allocator, 9, inventory, &locales));
    calls = state->calls;
    assert(!qjs_icu4c_numbering_open(allocator, &locales, &view));
    calls = state->calls - calls;
    for (i = 0; i < locales.count; i++) {
        assert(!qjs_icu4c_numbering_default(&view, i, &name));
        assert(name.length && !qjs_icu4c_numbering_supported(&view, name, &supported) && supported);
    }
    assert(!qjs_icu4c_numbering_supported(&view, bytes("arab"), &supported) && supported);
    assert(!qjs_icu4c_numbering_supported(&view, bytes("roman"), &supported) && !supported);
    qjs_icu4c_numbering_clear(&view);
    for (i = 1; i <= calls; i++) {
        size_t live = state->live;

        state->fail_at = state->calls + i;
        assert(qjs_icu4c_numbering_open(allocator, &locales, &view) == QJS_INTL_NO_MEMORY);
        assert(!view.names && !view.defaults && !view.count && state->live == live);
    }
    state->fail_at = 0;
    qjs_icu4c_locales_clear(&locales);
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);

    test_helpers(&allocator);
    test_numbering(&allocator);
    assert(!state.live);
    return 0;
}
