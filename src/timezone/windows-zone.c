/* Windows native discovery. CLDR data is compiled into this module; no ICU/XML. */
#include "windows-zone.h"
#if defined(_WIN32) || defined(QJS_TZ_WINDOWS_TEST)
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
/* Do not require a global target version to obtain the Vista API declaration. */
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#endif
#include "windows-zone-data.inc"

static unsigned int wz_u16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static uint32_t wz_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static unsigned char wz_lower(unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

/* Bound every pool read, including corrupt/truncated fixture inputs. */
static const unsigned char *wz_string(const unsigned char *pool, size_t size,
                                       unsigned int offset)
{
    if (offset >= size || !memchr(pool + offset, 0, size - offset))
        return NULL;
    return pool + offset;
}

static int wz_key_compare(const char *key, const unsigned char *stored)
{
    unsigned char a, b;
    do {
        a = wz_lower((unsigned char)*key++);
        b = *stored++;
        if (a != b)
            return a < b ? -1 : 1;
    } while (a);
    return 0;
}

static int wz_identifier_char(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '/' ||
           c == '+' || c == '-' || c == '.';
}

static int wz_lookup_blob(const unsigned char *data, size_t size,
                          const char *key, const char *territory,
                          char *output, size_t capacity)
{
    const unsigned char *pool, *entry, *stored, *row, *targets, *chosen = NULL;
    size_t strings_at, pool_size, n, low, high, middle, first, count;
    unsigned int key_count, row_count, offset, length, chosen_length = 0;
    char country[3] = { '0', '0', '1' };
    if (output && capacity)
        output[0] = '\0';
    if (!output || !capacity || !key || !data || size < 16)
        return -1;
    for (n = 0; n < 128 && key[n]; n++) {
        if ((unsigned char)key[n] < 32 || (unsigned char)key[n] > 126)
            return -1;
    }
    if (!n || n == 128 || memcmp(data, "WTZ1", 4) || wz_u32(data + 4) != size)
        return -1;
    key_count = wz_u16(data + 8);
    row_count = wz_u16(data + 10);
    strings_at = wz_u32(data + 12);
    if (!key_count || !row_count || strings_at != 16 + (size_t)key_count * 8 +
        (size_t)row_count * 8 || strings_at >= size)
        return -1;
    pool = data + strings_at;
    pool_size = size - strings_at;
    if (territory && territory[0] >= 'A' && territory[0] <= 'Z' &&
        territory[1] >= 'A' && territory[1] <= 'Z' && !territory[2]) {
        country[0] = territory[0];
        country[1] = territory[1];
        country[2] = '\0';
    }
    low = 0;
    high = key_count;
    entry = NULL;
    while (low < high) {
        int comparison;
        middle = low + (high - low) / 2;
        row = data + 16 + middle * 8;
        stored = wz_string(pool, pool_size, wz_u16(row));
        if (!stored)
            return -1;
        comparison = wz_key_compare(key, stored);
        if (!comparison) {
            entry = row;
            break;
        }
        if (comparison < 0)
            high = middle;
        else
            low = middle + 1;
    }
    if (!entry || wz_u16(entry + 6))
        return -1;
    first = wz_u16(entry + 2);
    count = wz_u16(entry + 4);
    if (!count || first >= row_count || count > row_count - first)
        return -1;
    for (n = 0; n < count; n++) {
        row = data + 16 + (size_t)key_count * 8 + (first + n) * 8;
        if (row[3] || !((row[0] == '0' && row[1] == '0' && row[2] == '1') ||
            (row[0] >= 'A' && row[0] <= 'Z' && row[1] >= 'A' &&
             row[1] <= 'Z' && !row[2])))
            return -1;
        offset = wz_u16(row + 4);
        length = wz_u16(row + 6);
        if (!length || offset >= pool_size || length >= pool_size - offset ||
            pool[offset + length] != 0)
            return -1;
        targets = pool + offset;
        if (!memcmp(row, "001", 3) || !memcmp(row, country, 3)) {
            /* Prefer the exact territory even if records are reordered. */
            if (!chosen || !memcmp(row, country, 3)) {
                chosen = targets;
                chosen_length = length;
            }
        }
    }
    if (!chosen)
        return -1;
    /* Validate the complete ordered list, then copy only its first target. */
    for (n = 0; n < chosen_length; n++) {
        if (!wz_identifier_char(chosen[n]) && chosen[n] != ' ')
            return -1;
        if (chosen[n] == ' ' && (!n || n + 1 == chosen_length || chosen[n - 1] == ' '))
            return -1;
    }
    for (n = 0; n < chosen_length && chosen[n] != ' '; n++)
        ;
    if (n >= capacity)
        return -1;
    memcpy(output, chosen, n);
    output[n] = '\0';
    return 0;
}

int qjs_tz_windows_lookup_identifier(const char *key, const char *territory,
                                     char *output, size_t capacity)
{
    return wz_lookup_blob(qjs_windows_zone_data, sizeof(qjs_windows_zone_data),
                          key, territory, output, capacity);
}

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
    length = GetGeoInfoW(geo, GEO_ISO2, name,
                         (int)(sizeof(name) / sizeof(name[0])), 0);
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
    if (output && capacity)
        output[0] = '\0';
    if (!output || !capacity ||
        GetDynamicTimeZoneInformation(&zone) == TIME_ZONE_ID_INVALID)
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
    return qjs_tz_windows_lookup_identifier(key, wz_user_territory(territory),
                                           output, capacity);
}
#endif
#endif
