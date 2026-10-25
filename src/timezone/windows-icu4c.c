/* Windows host identification through ICU4C CLDR mappings.
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
#include "windows-zone.h"
#include <stdint.h>
#include <string.h>
#include <unicode/ucal.h>
#ifdef _WIN32
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#endif

int qjs_tz_windows_lookup_identifier(const char *key, const char *territory,
                                     char *output, size_t capacity)
{
    UChar windows[128], name[256];
    UErrorCode status = U_ZERO_ERROR;
    size_t i;
    int32_t length;
    const char *region = "001";

    for (i = 0; i < 128 && key[i]; i++) {
        if ((unsigned char)key[i] > 127)
            return -1;
        windows[i] = (unsigned char)key[i];
    }
    if (!i || i == 128)
        return -1;
    if (territory && strlen(territory) == 2 &&
        territory[0] >= 'A' && territory[0] <= 'Z' &&
        territory[1] >= 'A' && territory[1] <= 'Z')
        region = territory;
    length = ucal_getTimeZoneIDForWindowsID(windows, (int32_t)i, region,
                                            name, 256, &status);
    if (U_FAILURE(status) || length <= 0 || (size_t)length >= capacity)
        return -1;
    for (i = 0; i < (size_t)length; i++) {
        if (name[i] > 127)
            return -1;
    }
    for (i = 0; i < (size_t)length; i++)
        output[i] = (char)name[i];
    output[length] = 0;
    return 0;
}

#ifdef _WIN32
int qjs_tz_windows_system_identifier(char *output, size_t capacity)
{
    DYNAMIC_TIME_ZONE_INFORMATION zone;
    GEOID geo;
    WCHAR country[3];
    char key[128], region[3] = {0};
    size_t i;

    if (GetDynamicTimeZoneInformation(&zone) == TIME_ZONE_ID_INVALID)
        return -1;
    for (i = 0; i < 128 && zone.TimeZoneKeyName[i]; i++) {
        if (zone.TimeZoneKeyName[i] > 127)
            return -1;
        key[i] = (char)zone.TimeZoneKeyName[i];
    }
    if (i == 128)
        return -1;
    key[i] = 0;
    /* This pair remains available on older Windows versions, unlike
       GetUserDefaultGeoName. ICU applies CLDR's 001 mapping if no match exists. */
    geo = GetUserGeoID(GEOCLASS_NATION);
    if (geo != GEOID_NOT_AVAILABLE &&
        GetGeoInfoW(geo, GEO_ISO2, country, 3, 0) == 3 &&
        country[0] >= 'A' && country[0] <= 'Z' &&
        country[1] >= 'A' && country[1] <= 'Z') {
        region[0] = (char)country[0];
        region[1] = (char)country[1];
    }
    return qjs_tz_windows_lookup_identifier(key, region, output, capacity);
}
#endif
