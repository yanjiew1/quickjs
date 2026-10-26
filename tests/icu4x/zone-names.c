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
#include "../../src/icu4x/zone-names.h"
#include <assert.h>
#include <string.h>

static int same(QJSICU4XDateText text, const char *s)
{
    return text.length == strlen(s) && !memcmp(text.data, s, text.length);
}

int main(void)
{
    QJSICU4XZoneNames *owner = NULL;
    QJSICU4XDateText text, retained, unchanged = {(const uint8_t *)"keep", 4};
    const char *zone = "America/Los_Angeles";
    assert(!qjs_icu4x_zone_names_open((const uint8_t *)"en", 2, &owner));
    /* Recover a dataset-local metazone ID through a known membership. The
       query reads defaults; a zone override must not fall back to them. */
    assert(!qjs_icu4x_zone_names_text(owner, (const uint8_t *)zone, strlen(zone), 1577836800, 1, 1,
                                      0, &text));
    assert(same(text, "Pacific Time"));
    retained = text;
    assert(!qjs_icu4x_zone_names_text(owner, (const uint8_t *)zone, strlen(zone), 1577836800, 1, 1,
                                      1, &text));
    assert(same(text, "Pacific Standard Time"));
    text = unchanged;
    assert(qjs_icu4x_zone_names_text(owner, (const uint8_t *)zone, strlen(zone), 1577836800, 0, 1,
                                     0, &text) == QJS_ICU4X_UNSUPPORTED);
    assert(text.data == unchanged.data && text.length == unchanged.length);
    assert(same(retained, "Pacific Time"));
    qjs_icu4x_zone_names_close(owner);
    owner = NULL;
    assert(!qjs_icu4x_zone_names_open((const uint8_t *)"en", 2, &owner));
    zone = "Etc/UTC";
    assert(
        !qjs_icu4x_zone_names_text(owner, (const uint8_t *)zone, strlen(zone), 0, 0, 1, 1, &text));
    assert(same(text, "Coordinated Universal Time"));
    qjs_icu4x_zone_names_close(owner);
    qjs_icu4x_zone_names_close(NULL);
    return 0;
}
