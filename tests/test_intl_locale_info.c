/* Locale preference/territory source selection precedes service capability.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../src/intl/locale/preferences.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *allocate(void *opaque, size_t size) { return malloc(size); }
static void *resize(void *opaque, void *ptr, size_t size) { return realloc(ptr, size); }
static void release(void *opaque, void *ptr) { free(ptr); }
static QJSIntlBytes bytes(const char *text) { return (QJSIntlBytes){text, strlen(text)}; }

static void check(QJSIntlLocaleService *service, const char *tag, const char *expected,
                   uint32_t expected_count)
{
    QJSIntlBytes list;
    uint32_t count;
    assert(!qjs_intl_locale_calendar_preferences(service, bytes(tag), &list, &count));
    if (count != expected_count || list.length != strlen(expected) ||
        (list.length && memcmp(list.data, expected, list.length)))
        fprintf(stderr, "%s: expected %s (%u), got %.*s (%u)\n", tag, expected,
                expected_count, (int)list.length, list.data ? list.data : "", count);
    assert(count == expected_count && list.length == strlen(expected));
    assert(!list.length || !memcmp(list.data, expected, list.length));
}

int main(void)
{
    QJSIntlAllocator allocator = {NULL, allocate, resize, release};
    QJSIntlLocaleService *service;
    QJSIntlBytes list;
    uint32_t count, cursor;
    assert(!qjs_intl_locale_open(&allocator, NULL, &service));
    check(service, "en", "gregorian", 1); /* Likely US. */
    check(service, "en-TH", "oldfoo foo unavailable", 3);
    check(service, "fr-TH", "buddhist gregorian", 2);
    check(service, "en-US-u-rg-thzzzz", "oldfoo foo unavailable", 3);
    check(service, "en-u-sd-th10", "oldfoo foo unavailable", 3);
    check(service, "en-US-u-rg-zzzzzz", "gregorian", 1);
    check(service, "en-ZZ", "", 0); /* Not the source's world list. */
    check(service, "en-001", "world", 1);
    /* Source order/aliases survive selection so later canonical filtering
       cannot restart at TH merely because the first list is unsupported. */
    assert(!qjs_intl_locale_source_type(service, bytes("ca"), bytes("oldfoo"), &list));
    assert(list.length == 3 && !memcmp(list.data, "foo", 3));
    assert(qjs_intl_locale_source_type(service, bytes("ca"), bytes("unknown"), &list)
           == QJS_INTL_UNSUPPORTED && !list.data && !list.length);
    list = bytes("previous");
    assert(qjs_intl_locale_source_type(NULL, bytes("ca"), bytes("foo"), &list)
           == QJS_INTL_INVALID_ARGUMENT && !list.data && !list.length);
    assert(qjs_intl_locale_calendar_preferences(NULL, bytes("en"), &list, &count)
           == QJS_INTL_INVALID_ARGUMENT && !list.data && !count);
    assert(qjs_intl_locale_calendar_preferences(service, bytes("en"), NULL, &count)
           == QJS_INTL_INVALID_ARGUMENT && !count);
    /* The explicit CA override wins over the US-looking short identifier;
       deprecated aliases are not additional timezone identities. */
    assert(!qjs_intl_locale_timezone_count(service, bytes("US"), &count) && count == 1);
    cursor = 0;
    assert(!qjs_intl_locale_timezone_next(service, bytes("US"), &cursor, &list));
    assert(list.length == 16 && !memcmp(list.data, "America/New_York", 16));
    assert(qjs_intl_locale_timezone_next(service, bytes("US"), &cursor, &list)
           == QJS_INTL_UNSUPPORTED && !list.data);
    {
        uint32_t exhausted = cursor;
        assert(qjs_intl_locale_timezone_next(service, bytes("US"), &cursor, &list)
               == QJS_INTL_UNSUPPORTED && !list.data && cursor == exhausted);
    }
    cursor = 0;
    assert(!qjs_intl_locale_timezone_next(service, bytes("CA"), &cursor, &list));
    assert(list.length == 15 && !memcmp(list.data, "America/Detroit", 15));
    assert(!qjs_intl_locale_timezone_count(service, bytes("001"), &count) && !count);
    assert(!qjs_intl_locale_timezone_count(service, bytes("ZZ"), &count) && !count);
    cursor = 0;
    assert(qjs_intl_locale_timezone_next(service, bytes("us"), &cursor, &list)
           == QJS_INTL_INVALID_ARGUMENT && !list.data && !cursor);
    cursor = UINT32_MAX;
    assert(qjs_intl_locale_timezone_next(service, bytes("US"), &cursor, &list)
           == QJS_INTL_INVALID_ARGUMENT && !list.data && cursor == UINT32_MAX);
    qjs_intl_locale_close(service);
    return 0;
}
