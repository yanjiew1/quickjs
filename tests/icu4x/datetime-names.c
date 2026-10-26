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
#include "../../src/icu4x/datetime-names.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int text(QJSICU4XDateText value, const char *expected)
{
    return value.length == strlen(expected) &&
           (!value.length || !memcmp(value.data, expected, value.length));
}

static QJSICU4XDateNames *open_names(const char *locale, unsigned calendar, unsigned family,
                                     unsigned width, unsigned standalone)
{
    QJSICU4XDateNames *handle = NULL;
    int status = qjs_icu4x_datetime_names_open((const uint8_t *)locale, strlen(locale), calendar,
                                               family, width, standalone, &handle);
    if (status)
        fprintf(stderr, "%s calendar=%u family=%u width=%u standalone=%u: status=%d\n", locale,
                calendar, family, width, standalone, status);
    assert(!status);
    assert(handle);
    return handle;
}

int main(void)
{
    QJSICU4XDateNames *handle, *unchanged;
    QJSICU4XDateNamesMeta meta, saved;
    QJSICU4XDateText value, retained;
    QJSICU4XDateMonthPattern pattern;
    static const uint8_t bad[] = {0xff};
    unsigned calendar, successes = 0;
    size_t era;

    assert(!qjs_icu4x_datetime_era_index(13, (const uint8_t *)"reiwa", 5, &era) && era == 6);
    assert(!qjs_icu4x_datetime_era_index(7, (const uint8_t *)"bce", 3, &era) && !era);
    assert(!qjs_icu4x_datetime_era_index(15, (const uint8_t *)"roc", 3, &era) && era == 1);
    assert(qjs_icu4x_datetime_era_index(13, (const uint8_t *)"unknown", 7, &era) ==
           QJS_ICU4X_UNSUPPORTED && era == 1);
    assert(!qjs_icu4x_datetime_check_version((const uint8_t *)"49.0.0", 6));
    handle = open_names("en", 7, 1, 4, 0);
    assert(!qjs_icu4x_datetime_names_meta(handle, &meta));
    assert(meta.kind == QJS_ICU4X_DATE_MONTH_LINEAR && meta.count == 12);
    assert(!qjs_icu4x_datetime_names_text(handle, 0, &retained) && text(retained, "January"));
    assert(!qjs_icu4x_datetime_names_text(handle, 1, &value) && text(value, "February"));
    assert(text(retained, "January"));
    saved = meta;
    assert(qjs_icu4x_datetime_names_meta(NULL, &meta));
    assert(meta.kind == saved.kind && meta.count == saved.count);
    value = retained;
    assert(qjs_icu4x_datetime_names_text(handle, meta.count, &value));
    assert(value.data == retained.data && value.length == retained.length);
    unchanged = handle;
    assert(qjs_icu4x_datetime_names_open(bad, sizeof(bad), 7, 1, 4, 0, &handle));
    assert(handle == unchanged);
    qjs_icu4x_datetime_names_close(handle);
    /* Wide LDML4 and narrow LDML5 map to ICU attrs5 and4 respectively. */
    handle = open_names("en", 7, 1, 5, 0);
    assert(!qjs_icu4x_datetime_names_text(handle, 0, &value) && text(value, "J"));
    qjs_icu4x_datetime_names_close(handle);
    handle = open_names("en", 7, 0, 4, 0);
    assert(!qjs_icu4x_datetime_names_text(handle, 1, &value) && text(value, "Anno Domini"));
    qjs_icu4x_datetime_names_close(handle);
    handle = open_names("en", 7, 2, 4, 0);
    assert(!qjs_icu4x_datetime_names_text(handle, 0, &value) && text(value, "Sunday"));
    qjs_icu4x_datetime_names_close(handle);
    handle = open_names("en", 7, 3, 3, 0);
    assert(!qjs_icu4x_datetime_names_text(handle, 0, &value) && text(value, "AM"));
    assert(qjs_icu4x_datetime_names_text(handle, 4, &value));
    qjs_icu4x_datetime_names_close(handle);

    handle = open_names("ru", 7, 1, 4, 0);
    assert(!qjs_icu4x_datetime_names_text(handle, 0, &value) && text(value, "января"));
    qjs_icu4x_datetime_names_close(handle);
    handle = open_names("ru", 7, 1, 4, 1);
    assert(!qjs_icu4x_datetime_names_text(handle, 0, &value) && text(value, "январь"));
    qjs_icu4x_datetime_names_close(handle);
    handle = open_names("root", 2, 1, 4, 0);
    assert(!qjs_icu4x_datetime_month_pattern(handle, 1, &pattern));
    assert(pattern.pattern.has_value &&
           (pattern.pattern.prefix.length || pattern.pattern.suffix.length));
    assert(!pattern.offset);
    qjs_icu4x_datetime_names_close(handle);

    handle = open_names("root", 2, 0, 4, 0);
    assert(!qjs_icu4x_datetime_names_meta(handle, &meta));
    assert(meta.kind == QJS_ICU4X_DATE_CYCLIC && meta.count == 60);
    qjs_icu4x_datetime_names_close(handle);
    handle = NULL;
    assert(qjs_icu4x_datetime_names_open((const uint8_t *)"en", 2, 8, 1, 4, 0, &handle) ==
           QJS_ICU4X_UNSUPPORTED);
    assert(!handle); /* Public Hebrew names at this width inherit root. */
    handle = open_names("root", 8, 1, 4, 0);
    assert(!qjs_icu4x_datetime_names_meta(handle, &meta));
    assert(meta.kind == QJS_ICU4X_DATE_MONTH_LEAP);
    assert(!qjs_icu4x_datetime_month_pattern(handle, 1, &pattern));
    /* Hebrew Adar I is a complete name, not a suffix applied to Shevat. */
    assert(!pattern.pattern.has_value && text(pattern.pattern.prefix, "Adar I"));
    qjs_icu4x_datetime_names_close(handle);
    handle = open_names("root", 8, 1, 1, 0);
    assert(!qjs_icu4x_datetime_names_meta(handle, &meta));
    assert(meta.kind == QJS_ICU4X_DATE_MONTH_NUMERIC_BASE);
    assert(!qjs_icu4x_datetime_month_pattern(handle, 1, &pattern));
    assert(pattern.offset == 1 && pattern.pattern.has_value && text(pattern.pattern.suffix, "a"));
    assert(!qjs_icu4x_datetime_month_pattern(handle, 2, &pattern));
    assert(!pattern.offset && pattern.pattern.has_value && text(pattern.pattern.suffix, "b"));
    qjs_icu4x_datetime_names_close(handle);

    /* Exercise every calendar dispatch; an absent exact-source payload is an
       explicit companion gap, never permission to return a parent's names. */
    for (calendar = 0; calendar < 16; calendar++) {
        int status =
            qjs_icu4x_datetime_names_open((const uint8_t *)"en", 2, calendar, 1, 4, 0, &handle);
        assert(status == QJS_ICU4X_OK || status == QJS_ICU4X_UNSUPPORTED);
        if (!status) {
            assert(!qjs_icu4x_datetime_names_meta(handle, &meta));
            successes++;
            qjs_icu4x_datetime_names_close(handle);
        }
    }
    assert(successes >= 2);
    qjs_icu4x_datetime_names_close(NULL);
    return 0;
}
