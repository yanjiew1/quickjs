/* Native hms projection, quoting, scalar text and owned literal storage.
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
#include "../src/intl/duration/native-digital.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void *allocate(void *opaque, size_t size)
{
    assert(size);
    return *(int *)opaque ? NULL : malloc(size);
}

static void release(void *opaque, void *ptr)
{
    (void)opaque;
    assert(ptr);
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static void check(const QJSIntlAllocator *allocator, const char *pattern, const char *hm,
                  const char *ms, unsigned int width)
{
    QJSIntlNativeDurationDigital result;
    assert(!qjs_intl_duration_native_digital(allocator, bytes(pattern), &result));
    assert(result.hour_width == width);
    assert(result.hour_minute.length == strlen(hm) &&
           (!result.hour_minute.length || !memcmp(result.hour_minute.data, hm, strlen(hm))));
    assert(result.minute_second.length == strlen(ms) &&
           (!result.minute_second.length || !memcmp(result.minute_second.data, ms, strlen(ms))));
    qjs_intl_duration_native_digital_clear(allocator, &result);
    qjs_intl_duration_native_digital_clear(allocator, &result);
}

int main(void)
{
    int fail = 0;
    QJSIntlAllocator allocator = {&fail, allocate, NULL, release};
    QJSIntlNativeDurationDigital result, zero = {0};
    static const char *const bad[] = {"", "h:mm", "m:ss", "h:mm:ssh", "h:mmm:ss", "hhh:mm:ss",
                                      "h:ss:mm", "h:'mm:ss", "'h:mm:ss'", "h:mm:\xffss"};
    char mutable[] = "hh.mm:ss";
    size_t i;
    assert(!qjs_intl_duration_native_digital_validate(bytes("hh.mm:ss")));
    check(&allocator, "h:mm:ss", ":", ":", 1);
    check(&allocator, "h.mm:ss", ".", ":", 1);
    check(&allocator, "hh.mm.ss", ".", ".", 2);
    check(&allocator, "'ga' h:mm:ss", ":", ":", 1);
    check(&allocator, "'hms' hh:mm:ss 'hms'", ":", ":", 2);
    check(&allocator, "h' dot 'mm' tick'' 'ss", " dot ", " tick' ", 1);
    check(&allocator, "''h''mm''ss''", "'", "'", 1);
    check(&allocator, "h時mm分ss", "時", "分", 1);
    check(&allocator, "hmmss", "", "", 1);
    for (i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
        assert(qjs_intl_duration_native_digital_validate(bytes(bad[i])) == QJS_INTL_DATA_ERROR);
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_duration_native_digital(&allocator, bytes(bad[i]), &result) == QJS_INTL_DATA_ERROR);
        assert(!memcmp(&result, &zero, sizeof(result)));
    }
    assert(qjs_intl_duration_native_digital(&allocator, (QJSIntlBytes){"h:mm:ss\0", 8}, &result) ==
           QJS_INTL_DATA_ERROR);
    assert(qjs_intl_duration_native_digital(&allocator, (QJSIntlBytes){NULL, 1}, &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_duration_native_digital(&allocator, bytes(mutable), &result));
    mutable[2] = mutable[5] = '!';
    assert(result.hour_minute.length == 1 && result.hour_minute.data[0] == '.');
    assert(result.minute_second.length == 1 && result.minute_second.data[0] == ':');
    qjs_intl_duration_native_digital_clear(&allocator, &result);
    fail = 1;
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_duration_native_digital(&allocator, bytes("h:mm:ss"), &result) == QJS_INTL_NO_MEMORY);
    assert(!memcmp(&result, &zero, sizeof(result)));
    fail = 0;
    check(&allocator, "hh:mm:ss", ":", ":", 2);
    return 0;
}
