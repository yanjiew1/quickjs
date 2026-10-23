/* Shared identity grammar, fixed-offset arithmetic and rule disambiguation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/timezone/identifier.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

int main(void)
{
    static const struct { const char *input, *canonical; int32_t offset; } valid[] = {
        {"+05", "+05:00", 18000}, {"+0530", "+05:30", 19800},
        {"-23:59", "-23:59", -86340}, {"-00", "+00:00", 0},
        {"-0000", "+00:00", 0}, {"-00:00", "+00:00", 0}
    };
    static const char *const invalid[] = {"", "+", "+5", "+1:00", "+010", "+01:0",
        "+24", "+00:60", "+01:00:00", "+010000", "+01.0", "Z", "z", " UTC", "+01 ",
        ("\xe2\x88\x92" "01:00"), "+0a:00", "not/a-zone"};
    QJSTimeZoneIdentifier zone, saved;
    int32_t offset;
    int64_t result;
    size_t i;
    for (i = 0; i < sizeof(valid) / sizeof(*valid); i++) {
        assert(!qjs_timezone_parse_identifier(&zone, valid[i].input, strlen(valid[i].input)));
        assert(zone.is_offset && !zone.named && !strcmp(zone.identifier, valid[i].canonical));
        assert(!qjs_timezone_identifier_offset(&zone, -1, &offset) && offset == valid[i].offset);
        assert(!qjs_timezone_identifier_compatible(&zone, -1, &result) && result == -1 - valid[i].offset);
    }
    assert(!qjs_timezone_parse_identifier(&zone, "uTc", 3));
    assert(!zone.is_offset && zone.named && !strcmp(zone.identifier, "UTC"));
    saved = zone;
    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        assert(qjs_timezone_parse_identifier(&zone, invalid[i], strlen(invalid[i])));
        assert(!memcmp(&zone, &saved, sizeof(zone)));
    }
    assert(qjs_timezone_parse_identifier(&zone, "UTC\0", 4));
    assert(!memcmp(&zone, &saved, sizeof(zone)));
    assert(qjs_timezone_parse_identifier(&zone, NULL, 3));
    assert(!memcmp(&zone, &saved, sizeof(zone)));
    assert(!qjs_timezone_identifier_compatible(&zone, INT64_MIN, &result) && result == INT64_MIN);
    assert(!qjs_timezone_parse_identifier(&zone, "+01", 3));
    result = 123;
    assert(qjs_timezone_identifier_compatible(&zone, INT64_MIN, &result) == QJS_TZ_RANGE && result == 123);
    assert(!qjs_timezone_parse_identifier(&zone, "-01", 3));
    assert(qjs_timezone_identifier_compatible(&zone, INT64_MAX, &result) == QJS_TZ_RANGE && result == 123);
#ifdef TEST_NATIVE_ZONE
    assert(!qjs_timezone_parse_identifier(&zone, "us/eastern", 10));
    assert(!strcmp(zone.identifier, "US/Eastern"));
    assert(!strcmp(qjs_timezone_primary_identifier(zone.named), "America/New_York"));
    /* 2024-03-10 02:30 local is skipped; compatible chooses 07:30 UTC. */
    assert(!qjs_timezone_identifier_compatible(&zone, INT64_C(1710037800), &result));
    assert(result == INT64_C(1710055800));
    /* 2024-11-03 01:30 occurs twice; compatible chooses 05:30 UTC. */
    assert(!qjs_timezone_identifier_compatible(&zone, INT64_C(1730597400), &result));
    assert(result == INT64_C(1730611800));
#endif
    return 0;
}
