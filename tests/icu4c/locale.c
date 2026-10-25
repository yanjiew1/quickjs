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
#include <stdio.h>
#include "../../src/intl/locale/locale.h"
#include "../../src/intl/locale/preferences.h"
#include "../../src/intl/locale/case.h"

static void test_case(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlCaseResult result;
    static const uint16_t input[] = {'I', 0, 0xd800, 0x130};
    static const uint16_t expected[] = {0x131, 0, 0xd800, 'i'};
    QJSIntlUTF16 text = {input, 4};
    size_t calls = state->calls, i;

    assert(!qjs_intl_case_map(allocator, bytes("tr"), text, 1, 1024, &result));
    assert(result.length == 4 && !memcmp(result.text, expected, sizeof(expected)));
    calls = state->calls - calls;
    qjs_intl_case_result_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_case_map(allocator, bytes("tr"), text, 1, 1024, &result) ==
               QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.length && !state->live);
    }
    state->fail_at = 0;
    assert(qjs_intl_case_map(allocator, bytes("tr"), text, 1, 1, &result) == QJS_INTL_OVERFLOW);
    assert(!qjs_intl_case_map(allocator, bytes("en"), (QJSIntlUTF16){0}, 0, 1024, &result));
    assert(!result.text && !result.length);
    assert(!state->live);
}

static void test_locale(const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlLocaleService *service;
    QJSIntlBytes result;
    QJSIntlWeekInfo week;
    char *text;
    unsigned int direction;
    uint32_t count, cursor = 0, seen = 0;
    size_t calls, i;
    int valid;

    calls = state->calls;
    assert(!qjs_intl_locale_open(allocator, NULL, &service));
    calls = state->calls - calls;
    assert(!qjs_intl_locale_validate(service, bytes("en-US"), &valid) && valid);
    assert(!qjs_intl_locale_validate(service, bytes("en_US"), &valid) && !valid);
    assert(!qjs_intl_locale_canonicalize(service, bytes("sh"), &text));
    assert(!strcmp(text, "sr-Latn"));
    qjs_intl_locale_result_free(service, text);
    assert(!qjs_intl_locale_canonicalize(service, bytes("hy-SU"), &text));
    assert(!strcmp(text, "hy-AM"));
    qjs_intl_locale_result_free(service, text);
    assert(!qjs_intl_locale_canonicalize(service, bytes("en-posix"), &text));
    assert(!strcmp(text, "en-posix"));
    qjs_intl_locale_result_free(service, text);
    assert(
        !qjs_intl_locale_canonicalize(service, bytes("en-t-m0-beta-metsehaf-m0-unknown"), &text));
    assert(!strcmp(text, "en-t-m0-betamets"));
    qjs_intl_locale_result_free(service, text);
    {
        char long_tag[8192] = "en";
        size_t length = 2, variant;

        for (variant = 0; variant < 1024; variant++)
            length +=
                (size_t)snprintf(long_tag + length, sizeof(long_tag) - length, "-v%04zu", variant);
        assert(!qjs_intl_locale_canonicalize(service, (QJSIntlBytes){long_tag, length}, &text));
        assert(!strcmp(text, long_tag));
        qjs_intl_locale_result_free(service, text);
        memcpy(long_tag, "en-u", 4);
        length = 4;
        for (variant = 0; variant < 26; variant++)
            length += (size_t)snprintf(long_tag + length, sizeof(long_tag) - length, "-0%c-abc",
                                       (char)('a' + variant));
        assert(!qjs_intl_locale_canonicalize(service, (QJSIntlBytes){long_tag, length}, &text));
        assert(!strcmp(text, long_tag));
        qjs_intl_locale_result_free(service, text);
    }
    {
        static const char *const inputs[] = {"en-u-aa-yes", "en-u-ka-yes", "en-u-fw-yes",
                                             "en-u-kn-yes", "en-u-kk-yes", "en-x-yes"};
        static const char *const expected[] = {"en-u-aa-yes", "en-u-ka-yes", "en-u-fw-yes",
                                               "en-u-kn",     "en-u-kk",     "en-x-yes"};

        for (i = 0; i < sizeof(inputs) / sizeof(*inputs); i++) {
            assert(!qjs_intl_locale_canonicalize(service, bytes(inputs[i]), &text));
            assert(!strcmp(text, expected[i]));
            qjs_intl_locale_result_free(service, text);
        }
    }
    assert(!qjs_intl_locale_maximize(service, bytes("zh-TW"), &text));
    assert(!strcmp(text, "zh-Hant-TW"));
    qjs_intl_locale_result_free(service, text);
    assert(!qjs_intl_locale_minimize(service, bytes("en-Latn-US"), &text));
    assert(!strcmp(text, "en"));
    qjs_intl_locale_result_free(service, text);
    assert(!qjs_intl_locale_canonicalize_uvalue(service, bytes("ca"), bytes("islamicc"), &text));
    assert(!strcmp(text, "islamic-civil"));
    qjs_intl_locale_result_free(service, text);
    assert(!qjs_intl_locale_canonicalize_uvalue(service, bytes("kn"), bytes("true"), &text));
    assert(!*text);
    qjs_intl_locale_result_free(service, text);
    {
        size_t before = state->calls;

        assert(!qjs_intl_locale_source_type(service, bytes("ca"), bytes("gregorian"), &result));
        assert(result.length == 7 && !memcmp(result.data, "gregory", 7));
        assert(!qjs_intl_locale_week_data(service, bytes("GB"), &week));
        assert(week.first_day == 1 && week.minimum_days == 4 && week.weekend == 96);
        assert(!qjs_intl_locale_week_data(service, bytes("US"), &week));
        assert(week.first_day == 7 && week.minimum_days == 1 && week.weekend == 96);
        assert(!qjs_intl_locale_hour_data(service, bytes("US"), &result, &count));
        assert(count && result.length >= 3 && !memcmp(result.data, "h12", 3));
        assert(!qjs_intl_locale_calendar_data(service, bytes("TH"), &result, &count));
        assert(count && result.length >= 8 && !memcmp(result.data, "buddhist", 8));
        assert(!qjs_intl_locale_timezone_count(service, bytes("US"), &count) && count);
        while (!qjs_intl_locale_timezone_next(service, bytes("US"), &cursor, &result))
            seen++;
        assert(seen == count && state->calls == before);
    }
    assert(!qjs_intl_locale_script_direction(service, bytes("Arab"), &direction) &&
           direction == QJS_INTL_DIRECTION_RTL);
    assert(!qjs_intl_locale_script_direction(service, bytes("Latn"), &direction) &&
           direction == QJS_INTL_DIRECTION_LTR);
    {
        static const char *const unknown[] = {"Brai", "Zinh", "Zyyy", "Zzzz", "Zxxx",
                                              "Aran", "Hrkt", "Latf", "Latg"};

        for (i = 0; i < sizeof(unknown) / sizeof(*unknown); i++)
            assert(!qjs_intl_locale_script_direction(service, bytes(unknown[i]), &direction) &&
                   direction == QJS_INTL_DIRECTION_UNKNOWN);
        assert(qjs_intl_locale_script_direction(service, bytes("root"), &direction) ==
               QJS_INTL_UNSUPPORTED);
        assert(qjs_intl_locale_timezone_count(service, bytes("us"), &count) ==
                   QJS_INTL_INVALID_ARGUMENT &&
               !count);
    }
    qjs_intl_locale_close(service);
    assert(!state->live);

    /* Sample cold owners across locale, preference and registry admission.
       Exact per-reader native-pack sweeps are separate from this ICU adapter. */
    for (i = 0; i < 10; i++) {
        state->fail_at = state->calls + (i < 8 ? i + 1 : i == 8 ? calls / 2 : calls);
        assert(qjs_intl_locale_open(allocator, NULL, &service) == QJS_INTL_NO_MEMORY && !service);
        assert(!state->live);
    }
    state->fail_at = 0;
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);

    test_locale(&allocator);
    test_case(&allocator);
    assert(!state.live);
    return 0;
}
