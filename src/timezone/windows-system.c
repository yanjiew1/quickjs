/* Windows host timezone discovery, independent of its mapping provider.
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
#ifdef _WIN32
/* Obtain the Vista API declarations without changing the project-wide target. */
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#endif

#ifdef _WIN32
/* These geographic APIs are available on older Windows versions than the
   dynamic time-zone API, so no optional modern API import is required. */
static const char *wz_user_territory(char territory[3])
{
    GEOID geo;
    WCHAR name[4];
    int length;
    size_t i;
    geo = GetUserGeoID(GEOCLASS_NATION);
    if (geo == GEOID_NOT_AVAILABLE)
        return NULL;
    for (i = 0; i < sizeof(name) / sizeof(name[0]); i++)
        name[i] = 0xffff;
    /* Require a positive count compatible with two letters, and verify the
       NUL independently. This accepts either inclusion or exclusion of the
       terminator in the API's copied-word count. */
    length = GetGeoInfoW(geo, GEO_ISO2, name, (int)(sizeof(name) / sizeof(name[0])), 0);
    if ((length != 2 && length != 3) || name[2] != 0 || name[0] < L'A' || name[0] > L'Z' ||
        name[1] < L'A' || name[1] > L'Z')
        return NULL;
    territory[0] = (char)name[0];
    territory[1] = (char)name[1];
    territory[2] = '\0';
    return territory;
}

int qjs_tz_windows_system_identifier(char *output, size_t capacity)
{
    DYNAMIC_TIME_ZONE_INFORMATION zone;
    char key[128], territory[3];
    size_t i;
    if (!output || !capacity || GetDynamicTimeZoneInformation(&zone) == TIME_ZONE_ID_INVALID)
        return -1;
    /* CLDR Windows keys are ASCII. Reject rather than lossy-convert a key.
       StandardName is localized and is deliberately never used. */
    for (i = 0; i < sizeof(key); i++) {
        unsigned int c = (unsigned int)zone.TimeZoneKeyName[i];
        if (!c) {
            key[i] = '\0';
            break;
        }
        if (c < 32 || c > 126)
            return -1;
        key[i] = (char)c;
    }
    if (i == sizeof(key) || !i)
        return -1;
    /* Use the user-selected country to choose the CLDR territory row, with
       001 fallback when the geographic query or exact row is unavailable.
       DynamicDaylightTimeDisabled disables year-specific rules, not
       necessarily DST. Neither this flag nor the current TIME_ZONE_ID status
       identifies a different IANA zone. The backend uses IANA rules; custom
       Windows rules may therefore differ. */
    return qjs_tz_windows_lookup_identifier(key, wz_user_territory(territory), output, capacity);
}
#endif
