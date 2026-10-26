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
#include "../../src/icu4x/compact.h"
#include <assert.h>
#include <string.h>

static uint32_t scale_index(QJSICU4XCompactPatterns *handle, unsigned magnitude)
{
    uint32_t count, i;
    QJSICU4XCompactScale scale;
    assert(!qjs_icu4x_compact_patterns_count(handle, &count));
    for (i = 0; i < count; i++) {
        assert(!qjs_icu4x_compact_patterns_scale(handle, i, &scale));
        if (scale.magnitude == magnitude)
            return i;
    }
    assert(0);
    return 0;
}

static void scales(const char *locale)
{
    QJSICU4XCompactPatterns *handle;
    QJSICU4XCompact *formatter;
    QJSICU4XCompactScale scale;
    uint32_t i, count, previous = 0, index = 0, expected = 0;
    int32_t magnitude, exponent;
    assert(!qjs_icu4x_compact_patterns_open((const uint8_t *)locale, strlen(locale), NULL, 0, 0,
                                            &handle));
    assert(!qjs_icu4x_compact_open((const uint8_t *)locale, strlen(locale), 0, &formatter));
    assert(!qjs_icu4x_compact_patterns_count(handle, &count));
    for (i = 0; i < count; i++) {
        assert(!qjs_icu4x_compact_patterns_scale(handle, i, &scale));
        assert((!i || scale.magnitude > previous) && scale.exponent <= scale.magnitude);
        previous = scale.magnitude;
    }
    /* Compare raw defaults with the real public lookup over every threshold,
       including magnitudes beyond the largest stored (u8) threshold. */
    for (magnitude = -1; magnitude <= 300; magnitude++) {
        while (index < count) {
            assert(!qjs_icu4x_compact_patterns_scale(handle, index, &scale));
            if (magnitude < (int32_t)scale.magnitude)
                break;
            expected = scale.exponent;
            index++;
        }
        assert(!qjs_icu4x_compact_exponent(formatter, magnitude, &exponent));
        assert(exponent == (int32_t)expected);
    }
    qjs_icu4x_compact_close(formatter);
    qjs_icu4x_compact_patterns_close(handle);
}

int main(void)
{
    QJSICU4XCompactPatterns *handle, *failed = (QJSICU4XCompactPatterns *)(uintptr_t)1;
    QJSICU4XCompactPattern pattern;
    QJSICU4XCompactScale scale;
    uint32_t index, count;
    scales("en");
    scales("bn");
    scales("ja");
    assert(!qjs_icu4x_compact_patterns_open((const uint8_t *)"fr", 2, (const uint8_t *)"latn", 4, 1,
                                            &handle));
    index = scale_index(handle, 3);
    assert(!qjs_icu4x_compact_patterns_get(handle, index, 5, 1, &pattern));
    assert(!pattern.has_number && !pattern.suffix.length && pattern.prefix.length == 5);
    assert(!memcmp(pattern.prefix.data, "mille", 5));
    assert(!qjs_icu4x_compact_patterns_get(handle, index, 1, -1, &pattern));
    assert(pattern.has_number && !pattern.prefix.length && pattern.suffix.length == 8);
    assert(!memcmp(pattern.suffix.data, " millier", 8));
    assert(!qjs_icu4x_compact_patterns_get(handle, index, 5, -1, &pattern));
    assert(pattern.has_number && pattern.suffix.length == 6 &&
           !memcmp(pattern.suffix.data, " mille", 6));
    assert(!qjs_icu4x_compact_patterns_count(handle, &count));
    memset(&pattern, 0x5a, sizeof(pattern));
    assert(qjs_icu4x_compact_patterns_get(handle, index, 6, -1, &pattern) == QJS_ICU4X_RANGE);
    assert(pattern.has_number == 0x5a5a5a5a);
    assert(qjs_icu4x_compact_patterns_get(handle, index, 0, 2, &pattern) == QJS_ICU4X_RANGE);
    assert(pattern.has_number == 0x5a5a5a5a);
    memset(&scale, 0x5a, sizeof(scale));
    assert(qjs_icu4x_compact_patterns_scale(handle, count, &scale) == QJS_ICU4X_RANGE);
    assert(scale.exponent == 0x5a5a5a5a);
    qjs_icu4x_compact_patterns_close(handle);
    /* A mandatory system missing numeric digit payloads may still share
       unqualified compact patterns; this API never substitutes its digits. */
    assert(!qjs_icu4x_compact_patterns_open((const uint8_t *)"en", 2, (const uint8_t *)"mathbold",
                                            8, 0, &handle));
    index = scale_index(handle, 3);
    assert(!qjs_icu4x_compact_patterns_get(handle, index, 5, -1, &pattern));
    assert(pattern.has_number && pattern.suffix.length == 1 && pattern.suffix.data[0] == 'K');
    qjs_icu4x_compact_patterns_close(handle);
    assert(qjs_icu4x_compact_patterns_open(NULL, 0, NULL, 0, 0, &failed) == QJS_ICU4X_RANGE);
    assert(failed == (QJSICU4XCompactPatterns *)(uintptr_t)1);
    assert(qjs_icu4x_compact_patterns_open((const uint8_t *)"en", 2, NULL, 0, 2, &failed) ==
           QJS_ICU4X_RANGE);
    assert(failed == (QJSICU4XCompactPatterns *)(uintptr_t)1);
    qjs_icu4x_compact_patterns_close(NULL);
    return 0;
}
