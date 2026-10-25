/* Shared era operations preserve outputs on provider initialization failure.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
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
#include "../src/calendar/eras.h"
#include <assert.h>
#include <string.h>

/* Model a provider failure before its immutable era inventory is published. */
int qjs_cal_japanese_era_count(size_t *out)
{
    return QJS_CAL_MEMORY;
}

int qjs_cal_japanese_era_at(size_t index, QJSCalendarEra *out)
{
    assert(0);
    return QJS_CAL_MEMORY;
}

int main(void)
{
    QJSCalendarDate date, original;
    const char *name = "unchanged";
    int32_t year = 123;

    memset(&original, 0x5a, sizeof(original));
    original.year = 2024;
    memcpy(&date, &original, sizeof(date));
    assert(qjs_cal_set_era(QJS_CAL_JAPANESE, 0, &date) == QJS_CAL_MEMORY);
    assert(!memcmp(&date, &original, sizeof(date)));
    assert(qjs_cal_era_at(QJS_CAL_JAPANESE, 0, &name) == QJS_CAL_MEMORY);
    assert(!strcmp(name, "unchanged"));
    assert(qjs_cal_year_from_era(QJS_CAL_JAPANESE, "reiwa", 6, &year) == QJS_CAL_MEMORY);
    assert(year == 123);
    assert(!qjs_cal_set_era(QJS_CAL_GREGORY, 0, &date));
    assert(!strcmp(date.era, "ce") && date.era_year == 2024);
    return 0;
}
