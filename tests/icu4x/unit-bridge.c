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
#include "../../src/icu4x/unit.h"
#include "../../src/intl/unit/identifier.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static QJSICU4XUnit *open_handle(const char *locale, const char *group, unsigned width)
{
    QJSICU4XUnit *handle = NULL;
    assert(!qjs_icu4x_unit_open((const uint8_t *)locale, strlen(locale), (const uint8_t *)group,
                                strlen(group), width, &handle));
    assert(handle);
    return handle;
}

static void suffix(QJSICU4XUnit *handle, unsigned count, const char *expected)
{
    QJSICU4XUnitPattern pattern;
    assert(!qjs_icu4x_unit_pattern(handle, count, &pattern));
    assert(pattern.has_number == 1 && !pattern.prefix.length);
    assert(pattern.suffix.length == strlen(expected));
    assert(!memcmp(pattern.suffix.data, expected, pattern.suffix.length));
}

int main(void)
{
    QJSICU4XUnit *handle, *failed = (QJSICU4XUnit *)(uintptr_t)1;
    QJSICU4XUnitPattern pattern, retained;
    unsigned i, width, available = 0;

    handle = open_handle("en", "length-meter", 0);
    suffix(handle, 1, " meter");
    suffix(handle, 5, " meters");
    assert(!qjs_icu4x_unit_pattern(handle, 5, &retained));
    suffix(handle, 1, " meter");
    assert(retained.suffix.length == 7 && !memcmp(retained.suffix.data, " meters", 7));
    memset(&pattern, 0x5a, sizeof(pattern));
    assert(qjs_icu4x_unit_pattern(handle, 6, &pattern) == QJS_ICU4X_RANGE);
    assert(pattern.has_number == 0x5a5a5a5a);
    assert(qjs_icu4x_unit_pattern(NULL, 0, &pattern) == QJS_ICU4X_RANGE);
    assert(pattern.has_number == 0x5a5a5a5a);
    qjs_icu4x_unit_close(handle);
    handle = open_handle("en", "length-meter", 1);
    suffix(handle, 5, " m");
    qjs_icu4x_unit_close(handle);
    handle = open_handle("en", "length-meter", 2);
    suffix(handle, 5, "m");
    qjs_icu4x_unit_close(handle);
    /* The extended tier supplies the actual German source while Core only
       inherits und. Constructor success must not hide that distinction. */
    handle = open_handle("de", "length-kilometer", 0);
    suffix(handle, 5, " Kilometer");
    qjs_icu4x_unit_close(handle);
    assert(qjs_icu4x_unit_open((const uint8_t *)"en-GB", 5, (const uint8_t *)"length-inch", 11, 0,
                               &failed) == QJS_ICU4X_UNSUPPORTED);
    assert(failed == (QJSICU4XUnit *)(uintptr_t)1);
    assert(qjs_icu4x_unit_open((const uint8_t *)"en", 2, (const uint8_t *)"digital-bit", 11, 0,
                               &failed) == QJS_ICU4X_UNSUPPORTED);
    assert(failed == (QJSICU4XUnit *)(uintptr_t)1);
    assert(qjs_icu4x_unit_open(NULL, 0, (const uint8_t *)"length-meter", 12, 0, &failed) ==
           QJS_ICU4X_RANGE);
    assert(failed == (QJSICU4XUnit *)(uintptr_t)1);
    assert(qjs_icu4x_unit_open((const uint8_t *)"en", 2, (const uint8_t *)"length-meter", 12, 3,
                               &failed) == QJS_ICU4X_RANGE);
    assert(failed == (QJSICU4XUnit *)(uintptr_t)1);
    /* Exercise the selected release's whole sanctioned repertoire. Missing
       public groups remain explicit gaps for the matching CLDR companion. */
    for (i = 0; i < qjs_intl_unit_definition_count(); i++) {
        const char *group = qjs_intl_unit_definition_at(i)->cldr_identifier;
        for (width = 0; width < 3; width++) {
            int status = qjs_icu4x_unit_open((const uint8_t *)"en", 2, (const uint8_t *)group,
                                             strlen(group), width, &handle);
            assert(status == 0 || status == QJS_ICU4X_UNSUPPORTED);
            if (!status) {
                unsigned category;
                for (category = 0; category < 6; category++)
                    assert(!qjs_icu4x_unit_pattern(handle, category, &pattern));
                available++;
                qjs_icu4x_unit_close(handle);
            }
        }
    }
    assert(available >= 30);
    qjs_icu4x_unit_close(NULL);
    return 0;
}
