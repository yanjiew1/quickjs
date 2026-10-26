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
#include "../../src/icu4x/locale.h"
#include "../../src/intl/locale/locale.h"
#include "../../src/intl/locale/preferences.h"
#include "../icu4c/helpers.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Output {
    char text[8192];
    size_t length, calls;
    int error;
} Output;

static int write_bytes(void *opaque, const uint8_t *text, size_t length)
{
    Output *out = opaque;
    out->calls++;
    if (out->error)
        return out->error;
    if (length > sizeof(out->text) - out->length - 1)
        return QJS_ICU4X_RANGE;
    if (length)
        memcpy(out->text + out->length, text, length);
    out->length += length;
    out->text[out->length] = 0;
    return 0;
}

static void identity(const QJSICU4XLocale *service, const char *input, unsigned operation,
                     const char *expected)
{
    Output out = {0};
    QJSICU4XByteSink sink = {&out, write_bytes};
    int status =
        qjs_icu4x_locale_identity(service, (const uint8_t *)input, strlen(input), operation, sink);
    if (status || strcmp(out.text, expected)) {
        fprintf(stderr, "%s: status %d, got %s, expected %s\n", input, status, out.text, expected);
        assert(0);
    }
}

static void preferred_key_contract(void)
{
    static const char pool[] = "\0"
                               "49.0.0\0"
                               "aa\0"
                               "bb\0";
    static const QJSICU4XLocaleKey keys[] = {
        {.name = 8, .preferred = 11, .extension = 'u'},
        {.name = 11, .extension = 'u'},
    };
    const QJSICU4XLocaleAliases data = {pool, sizeof(pool), 1, keys, 2, NULL, 0};
    QJSICU4XLocale *service = NULL;

    assert(!qjs_icu4x_locale_open(&data, &service));
    identity(service, "en-u-aa-foo", 0, "en-u-bb-foo");
    qjs_icu4x_locale_close(service);
}

static void service_contract(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    QJSIntlLocaleService *service;
    QJSIntlBytes value;
    QJSIntlWeekInfo week;
    size_t calls, i, live;
    uint32_t count, cursor;
    unsigned int direction;
    char *result;
    int valid;

    assert(!qjs_intl_locale_open(&allocator, NULL, &service));
    calls = state.calls;
    live = state.live;
    assert(!qjs_intl_locale_validate(service, bytes("en_US"), &valid) && !valid);
    assert(!qjs_intl_locale_validate(service, bytes("en-u-ca"), &valid) && valid);
    assert(qjs_intl_locale_canonicalize(service, bytes("en_US"), &result) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !result);
    assert(!qjs_intl_locale_canonicalize_uvalue(service, bytes("ca"), bytes("islamicc"), &result));
    assert(!strcmp(result, "islamic-civil"));
    qjs_intl_locale_result_free(service, result);
    assert(!qjs_intl_locale_canonicalize_uvalue(service, bytes("kn"), bytes("yes"), &result));
    assert(!*result);
    qjs_intl_locale_result_free(service, result);
    assert(!qjs_intl_locale_canonicalize_uvalue(service, bytes("aa"), bytes("foo-true"), &result));
    assert(!strcmp(result, "foo-true"));
    qjs_intl_locale_result_free(service, result);

    // These are exact metadata reads, so no temporary allocations or hidden
    // host timezone/calendar fallback may occur.
    i = state.calls;
    assert(!qjs_intl_locale_source_type(service, bytes("ca"), bytes("gregorian"), &value));
    assert(value.length == 7 && !memcmp(value.data, "gregory", 7));
    assert(qjs_intl_locale_source_type(service, bytes("ca"), bytes(""), &value) ==
               QJS_INTL_UNSUPPORTED &&
           !value.data && !value.length);
    assert(!qjs_intl_locale_week_data(service, bytes("GB"), &week));
    assert(week.first_day == 1 && week.minimum_days == 4 && week.weekend == 96);
    assert(!qjs_intl_locale_hour_data(service, bytes("US"), &value, &count));
    assert(count && value.length >= 3 && !memcmp(value.data, "h12", 3));
    assert(!qjs_intl_locale_calendar_data(service, bytes("TH"), &value, &count));
    assert(count && value.length >= 8 && !memcmp(value.data, "buddhist", 8));
    cursor = 0;
    assert(!qjs_intl_locale_timezone_count(service, bytes("SJ"), &count) && count == 1);
    assert(!qjs_intl_locale_timezone_next(service, bytes("SJ"), &cursor, &value));
    assert(value.length == strlen("Arctic/Longyearbyen") &&
           !memcmp(value.data, "Arctic/Longyearbyen", value.length));
    assert(qjs_intl_locale_timezone_next(service, bytes("SJ"), &cursor, &value) ==
               QJS_INTL_UNSUPPORTED &&
           !value.data && !value.length);
    assert(!qjs_intl_locale_timezone_count(service, bytes("US"), &count) && count > 1);
    assert(qjs_intl_locale_timezone_count(service, bytes("us"), &count) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !count);
    assert(!qjs_intl_locale_script_direction(service, bytes("Arab"), &direction) &&
           direction == QJS_INTL_DIRECTION_RTL);
    assert(!qjs_intl_locale_script_direction(service, bytes("Latn"), &direction) &&
           direction == QJS_INTL_DIRECTION_LTR);
    assert(!qjs_intl_locale_script_direction(service, bytes("Brai"), &direction) &&
           direction == QJS_INTL_DIRECTION_UNKNOWN);
    assert(state.calls == i);

    assert(!qjs_intl_locale_week_info(service, bytes("en-US-u-rg-gbzzzz"), bytes(""), &week));
    assert(week.first_day == 1 && week.minimum_days == 4);
    assert(!qjs_intl_locale_text_direction(service, bytes("ar"), &direction) &&
           direction == QJS_INTL_DIRECTION_RTL);
    assert(!qjs_intl_locale_profile_match(service, bytes("en-US-u-ca-gregory"), &value));
    assert(value.length && value.data[0] == 'e');

    // All engine allocator failure sites must release partial results. Foreign
    // Rust OOM is separately documented; it cannot be injected through C here.
    state.fail_at = state.calls + 1;
    assert(qjs_intl_locale_canonicalize(service, bytes("sh"), &result) == QJS_INTL_NO_MEMORY &&
           !result && state.live == live);
    state.fail_at = 0;
    i = state.calls;
    assert(!qjs_intl_locale_canonicalize_uvalue(service, bytes("ca"), bytes("islamicc"), &result));
    i = state.calls - i;
    qjs_intl_locale_result_free(service, result);
    for (size_t failure = 1; failure <= i; failure++) {
        state.fail_at = state.calls + failure;
        assert(qjs_intl_locale_canonicalize_uvalue(service, bytes("ca"), bytes("islamicc"),
                                                   &result) == QJS_INTL_NO_MEMORY);
        assert(!result && state.live == live);
    }
    state.fail_at = 0;
    qjs_intl_locale_close(service);
    assert(!state.live);
    for (i = 1; i <= calls; i++) {
        state.fail_at = state.calls + i;
        assert(qjs_intl_locale_open(&allocator, NULL, &service) == QJS_INTL_NO_MEMORY);
        assert(!service && !state.live);
    }
    qjs_intl_locale_close(NULL);
}

int main(void)
{
    static const char *const cases[][2] = {
        {"sh", "sr-Latn"},
        {"hy-SU", "hy-AM"},
        {"en-posix", "en-posix"},
        {"en-u-ca", "en-u-ca"},
        {"en-u-kf", "en-u-kf"},
        {"en-u-fw", "en-u-fw"},
        {"en-u-ca-islamicc", "en-u-ca-islamic-civil"},
        {"en-u-kn-yes", "en-u-kn"},
        {"en-u-aa-yes", "en-u-aa-yes"},
        {"en-u-aa-foo-true", "en-u-aa-foo-true"},
        {"en-u-aa-true-foo", "en-u-aa-true-foo"},
        {"en-t-x0-foo-true", "en-t-x0-foo-true"},
        {"en-t-x0-true-foo", "en-t-x0-true-foo"},
        {"en-t-h0-true", "en-t-h0-true"},
        {"en-u-ca-gregory-ca-hebrew", "en-u-ca-gregory"},
        {"en-t-m0-beta-metsehaf-m0-unknown", "en-t-m0-betamets"},
        {"sh-u-aa-foo-true-bar-rg-fi01", "sr-Latn-u-aa-foo-true-bar-rg-axzzzz"},
        {"en-a-foo-true-x-yes", "en-a-foo-true-x-yes"},
        {"en-u-zzzz-aaaa-aa-foo", "en-u-aaaa-zzzz-aa-foo"},
        {"sh-t-sh-m0-beta-metsehaf", "sr-Latn-t-sr-latn-m0-betamets"},
    };
    QJSICU4XLocale *service = NULL;
    QJSICU4XBytes value;
    QJSICU4XLocaleAliases bad;
    Output output = {.error = QJS_ICU4X_MEMORY};
    QJSICU4XByteSink sink = {&output, write_bytes};
    char long_tag[8192] = "en";
    size_t i, length = 2;

    assert(!qjs_icu4x_locale_open(&qjs_icu4x_locale_aliases, &service));
    for (i = 0; i < sizeof(cases) / sizeof(*cases); i++)
        identity(service, cases[i][0], 0, cases[i][1]);
    identity(service, "zh-TW", 1, "zh-Hant-TW");
    identity(service, "en-Latn-US", 2, "en");
    for (i = 0; i < 1024; i++)
        length += (size_t)snprintf(long_tag + length, sizeof(long_tag) - length, "-v%04zu", i);
    identity(service, long_tag, 0, long_tag);
    assert(!qjs_icu4x_locale_source_type(service, (const uint8_t *)"ca", 2,
                                         (const uint8_t *)"gregorian", 9, &value));
    assert(value.length == 7 && !memcmp(value.data, "gregory", 7));
    assert(qjs_icu4x_locale_source_type(service, (const uint8_t *)"aa", 2, (const uint8_t *)"bogus",
                                        5, &value) == QJS_ICU4X_UNSUPPORTED);
    assert(qjs_icu4x_locale_identity(service, (const uint8_t *)"en", 2, 0, sink) ==
           QJS_ICU4X_MEMORY);
    assert(output.calls == 1 && !output.length);
    qjs_icu4x_locale_close(service);
    qjs_icu4x_locale_close(NULL);

    // Bad compiled descriptors fail admission instead of becoming fallback data.
    bad = qjs_icu4x_locale_aliases;
    bad.version = bad.length;
    service = NULL;
    assert(qjs_icu4x_locale_open(&bad, &service) == QJS_ICU4X_BACKEND && !service);
    bad = qjs_icu4x_locale_aliases;
    bad.types = NULL;
    assert(qjs_icu4x_locale_open(&bad, &service) == QJS_ICU4X_BACKEND && !service);
    preferred_key_contract();
    service_contract();
    return 0;
}
