/* Released CLDR territory mappings, without requiring a Windows host.
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
#include "../src/timezone/windows-zone.h"
#include <assert.h>
#include <string.h>

static void mapping(const char *key, const char *country, const char *expected)
{
    char result[256];

    assert(!qjs_tz_windows_lookup_identifier(key, country, result, sizeof(result)));
    assert(!strcmp(result, expected));
}

int main(void)
{
    char saved[16] = "unchanged";

    mapping("Pacific Standard Time", "US", "America/Los_Angeles");
    mapping("pacific standard time", "CA", "America/Los_Angeles");
    mapping("British Columbia Standard Time", "CA", "America/Vancouver");
    mapping("Alberta Standard Time", "CA", "America/Edmonton");
    mapping("Manitoba Standard Time", "CA", "America/Winnipeg");
    mapping("Pacific Standard Time", "TW", "America/Los_Angeles");
    mapping("Pacific Standard Time", NULL, "America/Los_Angeles");
    mapping("Pacific Standard Time", "001", "America/Los_Angeles");
    mapping("Taipei Standard Time", "TW", "Asia/Taipei");
    mapping("UTC", "001", "Etc/UTC");
    mapping("Eastern Standard Time", "CA", "America/Toronto");
    assert(qjs_tz_windows_lookup_identifier("unknown", "US", saved, sizeof(saved)));
    assert(!strcmp(saved, "unchanged"));
    assert(qjs_tz_windows_lookup_identifier("Pacific Standard Time", "US", saved, 1));
    assert(!strcmp(saved, "unchanged"));
    assert(qjs_tz_windows_lookup_identifier("Pacific Standard Time\n", "US", saved, sizeof(saved)));
    assert(!strcmp(saved, "unchanged"));
    return 0;
}
