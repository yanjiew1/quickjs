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
#include "../icu4c/helpers.h"
#include "../../src/icu4x/duration.h"
#include "../../src/intl/duration/provider.h"
#include "../../src/intl/duration/digital.h"

int main(void)
{
    QJSICU4XDuration *handle = NULL, *unchanged;
    QJSICU4XDurationDigital data, preserved;
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    char *pattern;
    static const uint8_t bad[] = {0xff};
    static const char *locales[] = {"en", "da", "ar", "ru", "ja", "zh", "fa", "bn"};
    size_t i;

    assert(!qjs_icu4x_duration_open((const uint8_t *)"en", 2, &handle));
    assert(!qjs_icu4x_duration_digital(handle, &data));
    assert(data.hour_width == 1 && data.length == 1 && data.separator[0] == ':');
    preserved = data;
    assert(qjs_icu4x_duration_digital(NULL, &data));
    assert(data.separator == preserved.separator && data.length == preserved.length &&
           data.hour_width == preserved.hour_width);
    unchanged = handle;
    assert(qjs_icu4x_duration_open(bad, sizeof(bad), &handle));
    assert(handle == unchanged);
    qjs_icu4x_duration_close(handle);
    qjs_icu4x_duration_close(NULL);

    for (i = 0; i < sizeof(locales) / sizeof(locales[0]); i++) {
        QJSIntlDurationDigital parsed;
        assert(!qjs_intl_duration_digital_pattern(&allocator, bytes(locales[i]), &pattern));
        assert(!qjs_intl_duration_digital(&allocator, bytes(pattern), &parsed));
        assert(parsed.hour_minute.length && parsed.minute_second.length);
        qjs_intl_duration_digital_clear(&allocator, &parsed);
        allocator.free(allocator.opaque, pattern);
        assert(!state.live);
    }
    state.fail_at = state.calls + 1;
    assert(qjs_intl_duration_digital_pattern(&allocator, bytes("en"), &pattern) ==
           QJS_INTL_NO_MEMORY);
    assert(!pattern && !state.live);
    state.fail_at = 0;
    assert(qjs_intl_duration_digital_pattern(&allocator, (QJSIntlBytes){NULL, 1}, &pattern) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!pattern && !state.live);
    return 0;
}
