/* Windows live-API smoke fixture, separate from the portable mocked fixture.
   Compile with src/timezone/windows-zone.c on native Windows or MinGW. */
#include "../src/timezone/windows-zone.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
int main(void)
{
    DYNAMIC_TIME_ZONE_INFORMATION zone;
    GEOID geo;
    WCHAR country[4];
    char key[128], territory[3], expected[128], actual[128];
    const char *selection = NULL;
    int geo_length = 0;
    size_t i;
    if (GetDynamicTimeZoneInformation(&zone) == TIME_ZONE_ID_INVALID) {
        fputs("Live Windows time-zone API failed\n", stderr);
        return 1;
    }
    for (i = 0; i < sizeof(key); i++) {
        unsigned int c = (unsigned int)zone.TimeZoneKeyName[i];
        if (c && (c < 32 || c > 126)) {
            fputs("Live Windows time-zone key is not ASCII\n", stderr);
            return 1;
        }
        key[i] = (char)c;
        if (!c)
            break;
    }
    if (!i || i == sizeof(key)) {
        fputs("Live Windows time-zone key is missing or unterminated\n", stderr);
        return 1;
    }
    /* Query the actual geographic APIs separately to establish the live
       expected row. Geographic failure falls back to 001 rather than failing
       discovery for a recognized time-zone key. */
    for (i = 0; i < sizeof(country) / sizeof(country[0]); i++)
        country[i] = 0xffff;
    geo = GetUserGeoID(GEOCLASS_NATION);
    if (geo != GEOID_NOT_AVAILABLE)
        geo_length = GetGeoInfoW(geo, GEO_ISO2, country,
                                 (int)(sizeof(country) / sizeof(country[0])), 0);
    if ((geo_length == 2 || geo_length == 3) && country[2] == 0 && country[0] >= L'A' &&
        country[0] <= L'Z' && country[1] >= L'A' && country[1] <= L'Z') {
        territory[0] = (char)country[0];
        territory[1] = (char)country[1];
        territory[2] = '\0';
        selection = territory;
    }
    if (qjs_tz_windows_lookup_identifier(key, selection, expected, sizeof(expected)) ||
        qjs_tz_windows_system_identifier(actual, sizeof(actual)) || strcmp(actual, expected)) {
        fprintf(stderr, "Live Windows discovery failed for key: %s\n", key);
        return 1;
    }
    printf("Live Windows key: %s; geographic territory: %s; geographic words: %d; "
           "CLDR identifier: %s; disabled dynamic rules: %u\n",
           key, selection ? selection : "001", geo_length, actual,
           (unsigned int)zone.DynamicDaylightTimeDisabled);
    return 0;
}
#else
int main(void)
{
    fputs("Live Windows discovery fixture requires Windows\n", stderr);
    return 77;
}
#endif
