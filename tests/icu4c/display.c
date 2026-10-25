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
#include "../../src/intl/display/display.h"

static void test_display(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlDisplayService *service;
    QJSIntlDisplayFormatter *formatter;
    QJSIntlDisplayOptions options = {QJS_INTL_DISPLAY_REGION, QJS_INTL_NAME_LONG, 1, 1024};
    QJSIntlNameText result;
    const QJSIntlBytes *available;
    size_t count, en, type, live, calls, i;
    static const char *const codes[] = {"en-US", "US", "Latn", "USD", "gregory", "year"};

    assert(!qjs_intl_display_open(allocator, NULL, &service));
    assert(!qjs_intl_display_available(service, &available, &count));
    en = find(available, count, "en");
    {
        uint32_t cursor = 0;
        QJSIntlBytes code;

        while (!qjs_intl_display_currency_next(service, &cursor, &code)) {
            /* These ICU metadata-only codes have no plain name in any locale.
               Keeping this fixture catches an unfiltered UCURR_ALL inventory. */
            assert(strcmp(code.data, "CNX") && strcmp(code.data, "LSM"));
        }
    }
    for (type = 0; type < QJS_INTL_DISPLAY_TYPE_COUNT; type++) {
        options.type = (QJSIntlDisplayType)type;
        assert(!qjs_intl_display_formatter_open(service, en, &options, &formatter));
        live = state->live;
        calls = state->calls;
        assert(!qjs_intl_display_format(formatter, bytes(codes[type]), &result));
        assert(result.found && result.length);
        calls = state->calls - calls;
        qjs_intl_name_text_clear(allocator, &result);
        for (i = 1; i <= calls; i++) {
            state->fail_at = state->calls + i;
            assert(qjs_intl_display_format(formatter, bytes(codes[type]), &result) ==
                   QJS_INTL_NO_MEMORY);
            assert(!result.found && !result.text && !result.length && state->live == live);
        }
        state->fail_at = 0;
        if (type == QJS_INTL_DISPLAY_CURRENCY) {
            assert(!qjs_intl_display_format(formatter, bytes("ZZZ"), &result));
            assert(!result.found && !result.text);
        }
        qjs_intl_display_formatter_close(formatter);
    }
    /* ICU may report a root-fallback warning for a genuine code-identical
       currency name; fallback:none must expose that present resource. */
    options.type = QJS_INTL_DISPLAY_CURRENCY;
    assert(!qjs_intl_display_formatter_open(service, find(available, count, "az-Cyrl"), &options,
                                            &formatter));
    assert(!qjs_intl_display_format(formatter, bytes("USD"), &result));
    assert(result.found && result.length == 3 && result.text[0] == 'U');
    qjs_intl_name_text_clear(allocator, &result);
    qjs_intl_display_formatter_close(formatter);
    /* Standalone script names differ from their use inside language names.
       The Russian source additionally requests ICU sentence titlecasing. */
    options.type = QJS_INTL_DISPLAY_SCRIPT;
    assert(!qjs_intl_display_formatter_open(service, en, &options, &formatter));
    assert(!qjs_intl_display_format(formatter, bytes("Hans"), &result));
    assert(result.found && result.length == 14);
    for (i = 0; i < result.length; i++)
        assert(result.text[i] == (unsigned char)"Simplified Han"[i]);
    qjs_intl_name_text_clear(allocator, &result);
    qjs_intl_display_formatter_close(formatter);
    assert(!qjs_intl_display_formatter_open(service, find(available, count, "ru"), &options,
                                            &formatter));
    assert(!qjs_intl_display_format(formatter, bytes("Hans"), &result));
    assert(result.found && result.length && result.text[0] == 0x0423);
    qjs_intl_name_text_clear(allocator, &result);
    qjs_intl_display_formatter_close(formatter);
    options.type = QJS_INTL_DISPLAY_REGION;
    assert(!qjs_intl_display_formatter_open(service, en, &options, &formatter));
    qjs_intl_display_close(service);
    assert(!qjs_intl_display_format(formatter, bytes("XX"), &result));
    assert(!result.found && !result.text);
    assert(!qjs_intl_display_format(formatter, bytes("US"), &result));
    qjs_intl_display_formatter_close(formatter);
    assert(result.found && result.length == 13 && result.text[0] == 'U');
    qjs_intl_name_text_clear(allocator, &result);
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);

    test_display(&allocator);
    assert(!state.live);
    return 0;
}
