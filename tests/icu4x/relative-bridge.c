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
#include "../../src/icu4x/relative.h"
#include <assert.h>
#include <string.h>

static void text(QJSICU4XRelativeText span, const char *expected)
{
    assert(span.length == strlen(expected));
    assert(!span.length || !memcmp(span.data, expected, span.length));
}

int main(void)
{
    QJSICU4XRelative *handle, *failed = (QJSICU4XRelative *)(uintptr_t)1;
    QJSICU4XRelativePattern pattern;
    QJSICU4XRelativeText literal;
    unsigned width, unit, past, category;

    for (width = 0; width < 3; width++) {
        assert(!qjs_icu4x_relative_open((const uint8_t *)"en", 2, width, &handle));
        for (unit = 0; unit < 8; unit++) {
            for (past = 0; past < 2; past++) {
                for (category = 0; category < 6; category++) {
                    assert(!qjs_icu4x_relative_pattern(handle, unit, past, category, &pattern));
                    assert(pattern.has_number <= 1);
                }
            }
        }
        assert(!qjs_icu4x_relative_literal(handle, 3, 0, &literal));
        text(literal, "today");
        assert(!qjs_icu4x_relative_literal(handle, 3, -1, &literal));
        text(literal, "yesterday");
        literal.length = 123;
        assert(qjs_icu4x_relative_literal(handle, 3, 100, &literal) == QJS_ICU4X_UNSUPPORTED);
        assert(literal.length == 123);
        qjs_icu4x_relative_close(handle);
    }
    assert(!qjs_icu4x_relative_open((const uint8_t *)"en", 2, 0, &handle));
    assert(!qjs_icu4x_relative_pattern(handle, 3, 0, 1, &pattern));
    text(pattern.prefix, "in ");
    text(pattern.suffix, " day");
    assert(pattern.has_number);
    assert(!qjs_icu4x_relative_pattern(handle, 3, 1, 5, &pattern));
    text(pattern.prefix, "");
    text(pattern.suffix, " days ago");
    memset(&pattern, 0x5a, sizeof(pattern));
    assert(qjs_icu4x_relative_pattern(handle, 8, 0, 0, &pattern) == QJS_ICU4X_RANGE);
    assert(pattern.has_number == 0x5a5a5a5a);
    assert(qjs_icu4x_relative_pattern(handle, 0, 2, 0, &pattern) == QJS_ICU4X_RANGE);
    assert(pattern.has_number == 0x5a5a5a5a);
    assert(qjs_icu4x_relative_pattern(handle, 0, 0, 6, &pattern) == QJS_ICU4X_RANGE);
    assert(pattern.has_number == 0x5a5a5a5a);
    qjs_icu4x_relative_close(handle);

    /* Category TWO is metadata, not an artificial value to be reclassified.
       Arabic's literal dual form deliberately has no inserted numeric body. */
    assert(!qjs_icu4x_relative_open((const uint8_t *)"ar", 2, 0, &handle));
    assert(!qjs_icu4x_relative_pattern(handle, 3, 0, 2, &pattern));
    assert(!pattern.has_number);
    text(pattern.prefix, "خلال يومين");
    qjs_icu4x_relative_close(handle);
    assert(qjs_icu4x_relative_open(NULL, 0, 0, &failed) == QJS_ICU4X_RANGE);
    assert(failed == (QJSICU4XRelative *)(uintptr_t)1);
    assert(qjs_icu4x_relative_open((const uint8_t *)"en", 2, 3, &failed) == QJS_ICU4X_RANGE);
    assert(failed == (QJSICU4XRelative *)(uintptr_t)1);
    qjs_icu4x_relative_close(NULL);
    return 0;
}
