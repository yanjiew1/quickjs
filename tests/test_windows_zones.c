/* Portable lookup fixture; include the module to inspect its bounded decoder.
   Build this one file, without separately linking windows-zone.c. */
#define QJS_TZ_WINDOWS_TEST 1
#ifdef _WIN32
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <string.h>
static DYNAMIC_TIME_ZONE_INFORMATION fixture_zone;
static DWORD fixture_status;
static GEOID fixture_geo_id = 39; /* mock Canada; actual ID is immaterial */
static WCHAR fixture_country[4] = { L'C', L'A', 0, 0 };
static int fixture_geo_result = 3;
static size_t fixture_geo_write_words = 4;
static unsigned int fixture_user_geo_calls, fixture_geo_info_calls;
static int fixture_geo_arguments_ok = 1;
static DWORD WINAPI fixture_get_dynamic_zone(DYNAMIC_TIME_ZONE_INFORMATION *zone)
{
    memcpy(zone, &fixture_zone, sizeof(*zone));
    return fixture_status;
}
static GEOID WINAPI fixture_get_user_geo_id(GEOCLASS geo_class)
{
    fixture_user_geo_calls++;
    if (geo_class != GEOCLASS_NATION)
        fixture_geo_arguments_ok = 0;
    return fixture_geo_id;
}
static int WINAPI fixture_get_geo_info(GEOID geo, GEOTYPE type, LPWSTR output,
                                       int capacity, LANGID language)
{
    fixture_geo_info_calls++;
    if (geo != fixture_geo_id || type != GEO_ISO2 || capacity != 4 || language != 0) {
        fixture_geo_arguments_ok = 0;
        return 0;
    }
    memcpy(output, fixture_country, fixture_geo_write_words * sizeof(fixture_country[0]));
    return fixture_geo_result;
}
#define GetDynamicTimeZoneInformation fixture_get_dynamic_zone
#define GetUserGeoID fixture_get_user_geo_id
#define GetGeoInfoW fixture_get_geo_info
#endif
#include "../src/timezone/windows-zone.c"
#ifdef _WIN32
#undef GetDynamicTimeZoneInformation
#undef GetUserGeoID
#undef GetGeoInfoW
#endif
#include <stdio.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "Windows zone fixture failed at line %d: %s\n", __LINE__, #test); \
    return 1; } } while (0)

static int mapping(const char *key, const char *territory, const char *expected)
{
    char result[128];
    return qjs_tz_windows_lookup_identifier(key, territory, result, sizeof(result)) == 0 &&
           strcmp(result, expected) == 0;
}

#ifdef _WIN32
/* Assert the real wrapper queried both mocked geographic APIs, including
   their arguments. A portable lookup alone cannot establish this behavior. */
static int native_mapping(const char *expected, int expect_info)
{
    char result[128];
    unsigned int user_before = fixture_user_geo_calls;
    unsigned int info_before = fixture_geo_info_calls;
    return qjs_tz_windows_system_identifier(result, sizeof(result)) == 0 &&
           !strcmp(result, expected) && fixture_geo_arguments_ok &&
           fixture_user_geo_calls == user_before + 1 &&
           fixture_geo_info_calls == info_before + (unsigned int)expect_info;
}
#endif

int main(void)
{
    unsigned char copy[sizeof(qjs_windows_zone_data)];
    char result[128], tiny[2] = { 'x', 'y' };
    size_t i, rows_at = 16 + (size_t)wz_u16(qjs_windows_zone_data + 8) * 8;
    CHECK(mapping("Pacific Standard Time", "CA", "America/Vancouver"));
    CHECK(mapping("Pacific Standard Time", "001", "America/Los_Angeles"));
    CHECK(mapping("Pacific Standard Time", NULL, "America/Los_Angeles"));
    CHECK(mapping("Pacific Standard Time", "XX", "America/Los_Angeles"));
    CHECK(mapping("Pacific Standard Time", "ca", "America/Los_Angeles"));
    CHECK(mapping("pAcIfIc StAnDaRd TiMe", "CA", "America/Vancouver"));
    CHECK(mapping("Central Standard Time", "CA", "America/Winnipeg"));
    CHECK(mapping("Central Standard Time", "MX", "America/Matamoros"));
    CHECK(mapping("Central Standard Time", "US", "America/Chicago"));
    CHECK(mapping("Alaskan Standard Time", "US", "America/Anchorage"));
    CHECK(mapping("India Standard Time", "IN", "Asia/Calcutta"));
    CHECK(mapping("UTC", NULL, "Etc/UTC"));
    CHECK(qjs_tz_windows_lookup_identifier("Unknown Time", NULL, result, sizeof(result)) == -1);
    CHECK(result[0] == 0);
    CHECK(qjs_tz_windows_lookup_identifier("", NULL, result, sizeof(result)) == -1);
    CHECK(qjs_tz_windows_lookup_identifier(NULL, NULL, result, sizeof(result)) == -1);
    CHECK(qjs_tz_windows_lookup_identifier("UTC", NULL, NULL, 128) == -1);
    CHECK(qjs_tz_windows_lookup_identifier("UTC", NULL, tiny, 0) == -1 && tiny[0] == 'x');
    CHECK(qjs_tz_windows_lookup_identifier("UTC", NULL, tiny, sizeof(tiny)) == -1);
    CHECK(tiny[0] == 0 && tiny[1] == 'y');
    /* Every truncation must fail without reading past the supplied extent. */
    for (i = 0; i < sizeof(copy); i++)
        CHECK(wz_lookup_blob(qjs_windows_zone_data, i, "UTC", NULL,
                             result, sizeof(result)) == -1);
    memcpy(copy, qjs_windows_zone_data, sizeof(copy));
    copy[0] = '!';
    CHECK(wz_lookup_blob(copy, sizeof(copy), "UTC", NULL, result, sizeof(result)) == -1);
    memcpy(copy, qjs_windows_zone_data, sizeof(copy));
    memset(copy + 12, 0xff, 4);
    CHECK(wz_lookup_blob(copy, sizeof(copy), "UTC", NULL, result, sizeof(result)) == -1);
    memcpy(copy, qjs_windows_zone_data, sizeof(copy));
    for (i = 16; i < rows_at; i += 8) {
        copy[i] = 0xff;
        copy[i + 1] = 0xff;
    }
    CHECK(wz_lookup_blob(copy, sizeof(copy), "UTC", NULL, result, sizeof(result)) == -1);
    memcpy(copy, qjs_windows_zone_data, sizeof(copy));
    for (i = rows_at; i < wz_u32(copy + 12); i += 8) {
        copy[i + 6] = 0xff;
        copy[i + 7] = 0xff;
    }
    CHECK(wz_lookup_blob(copy, sizeof(copy), "UTC", NULL, result, sizeof(result)) == -1);
#ifdef _WIN32
    /* Native wrapper tests use mocked APIs. Live system discovery is a
       separate gate and must not be reported from this fixture. */
    memcpy(fixture_zone.TimeZoneKeyName, L"Pacific Standard Time", sizeof(L"Pacific Standard Time"));
    fixture_status = TIME_ZONE_ID_STANDARD;
    CHECK(native_mapping("America/Vancouver", 1));
    fixture_country[0] = L'U';
    fixture_country[1] = L'S';
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_country[0] = L'X';
    fixture_country[1] = L'X';
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_country[0] = L'C';
    fixture_country[1] = L'A';
    fixture_geo_id = GEOID_NOT_AVAILABLE;
    CHECK(native_mapping("America/Los_Angeles", 0));
    fixture_geo_id = 39;
    fixture_geo_result = 0; /* API failure even if buffer contains CA */
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_geo_result = 2; /* count excludes NUL; terminated CA remains valid */
    CHECK(native_mapping("America/Vancouver", 1));
    fixture_geo_result = 1; /* too short to describe two letters */
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_geo_result = -1;
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_geo_result = 5; /* reported result exceeds supplied capacity */
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_geo_result = 3;
    fixture_geo_write_words = 0; /* claims success without populating buffer */
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_geo_write_words = 2; /* letters written without their NUL */
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_geo_write_words = 4;
    fixture_country[0] = 0;
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_country[0] = L'c';
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_country[0] = 0x00c7; /* non-ASCII UTF-16 code unit */
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_country[0] = 0xd800; /* unpaired surrogate */
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_country[0] = L'C';
    fixture_country[1] = 0;
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_country[1] = L'A';
    fixture_country[2] = L'A'; /* nonterminated even though return claims 3 */
    fixture_country[3] = L'A';
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_geo_result = 4; /* full buffer with no NUL */
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_country[0] = L'1'; /* numeric/non-country geographic result */
    fixture_country[1] = L'2';
    fixture_country[2] = L'3';
    fixture_country[3] = 0;
    CHECK(native_mapping("America/Los_Angeles", 1));
    fixture_country[0] = L'U';
    fixture_country[1] = L'S';
    fixture_country[2] = 0;
    fixture_geo_result = 3;
    fixture_zone.DynamicDaylightTimeDisabled = TRUE;
    fixture_zone.StandardDate.wMonth = 11;
    fixture_zone.StandardDate.wDay = 1;
    fixture_zone.DaylightDate.wMonth = 3;
    fixture_zone.DaylightDate.wDay = 2;
    fixture_status = TIME_ZONE_ID_DAYLIGHT;
    CHECK(native_mapping("America/Los_Angeles", 1));
    memset(&fixture_zone.StandardDate, 0, sizeof(fixture_zone.StandardDate));
    memset(&fixture_zone.DaylightDate, 0, sizeof(fixture_zone.DaylightDate));
    fixture_status = TIME_ZONE_ID_UNKNOWN;
    CHECK(native_mapping("America/Los_Angeles", 1));
    CHECK(qjs_tz_windows_system_identifier(NULL, 128) == -1);
    CHECK(qjs_tz_windows_system_identifier(result, 0) == -1);
    CHECK(qjs_tz_windows_system_identifier(tiny, sizeof(tiny)) == -1 && tiny[0] == 0);
    fixture_status = TIME_ZONE_ID_INVALID;
    CHECK(qjs_tz_windows_system_identifier(result, sizeof(result)) == -1 && result[0] == 0);
    fixture_status = TIME_ZONE_ID_DAYLIGHT;
    fixture_zone.TimeZoneKeyName[0] = 0;
    CHECK(qjs_tz_windows_system_identifier(result, sizeof(result)) == -1);
    fixture_zone.TimeZoneKeyName[0] = 0xd800; /* unpaired UTF-16 surrogate */
    CHECK(qjs_tz_windows_system_identifier(result, sizeof(result)) == -1);
    for (i = 0; i < 128; i++)
        fixture_zone.TimeZoneKeyName[i] = L'A';
    CHECK(qjs_tz_windows_system_identifier(result, sizeof(result)) == -1);
    memset(&fixture_zone, 0, sizeof(fixture_zone));
    memcpy(fixture_zone.TimeZoneKeyName, L"Unknown Time", sizeof(L"Unknown Time"));
    CHECK(qjs_tz_windows_system_identifier(result, sizeof(result)) == -1);
#endif
#ifdef _WIN32
    CHECK(fixture_user_geo_calls > 0 && fixture_geo_info_calls > 0);
    CHECK(fixture_geo_arguments_ok);
    puts("Windows CLDR lookup and native geographic API mock fixtures passed");
#else
    puts("Windows CLDR lookup fixture passed");
#endif
    return 0;
}
