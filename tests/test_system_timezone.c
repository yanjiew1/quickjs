/* Environment parsing, fallback policy and bounded-output contracts.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "../src/timezone/system-zone.h"
#include "../src/timezone/timezone.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void parsed(const char *value, const char *expected)
{
    char result[256];

    assert(!qjs_timezone_parse_environment(result, sizeof(result), value));
    assert(!strcmp(result, expected));
}

int main(void)
{
    const char *invalid[] = {NULL, "/etc/localtime",
        ":/usr/share/zoneinfo/UTC", "UTC+24", "UTC-5:60", "UTC-5:30:01",
        "STD5DST,M3.2.0,M11.1.0", "AB5", "<AB>5", "<ABC5", "UTC999"};
    char result[32] = "unchanged";
    size_t i;

    parsed("", "UTC");
    parsed(":", "UTC");
    parsed("utc", "UTC");
    parsed(":UTC", "UTC");
    parsed("UTC0", "+00:00");
    parsed("GMT+3", "-03:00");
    parsed("ABC-5:30", "+05:30");
    parsed(":UTC-5:30:00", "+05:30");
    parsed("<+0530>-5:30", "+05:30");
    parsed("UTC+23:59", "-23:59");
    if (qjs_timezone_count() > 1) {
        parsed("America/New_York", "America/New_York");
    } else {
        assert(qjs_timezone_parse_environment(result, sizeof(result),
                                              "America/New_York") == QJS_TZ_RANGE);
        assert(!strcmp(result, "unchanged"));
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        assert(qjs_timezone_parse_environment(result, sizeof(result), invalid[i]) == QJS_TZ_RANGE);
        assert(!strcmp(result, "unchanged"));
    }
    assert(qjs_timezone_parse_environment(result, 3, "UTC") == QJS_TZ_RANGE);
    assert(!strcmp(result, "unchanged"));
    assert(qjs_timezone_parse_environment(result, 6, "UTC-5:30") == QJS_TZ_RANGE);
    assert(!strcmp(result, "unchanged"));
#ifndef _WIN32
    {
        const char *original = getenv("TZ");
        char *saved = original ? strdup(original) : NULL;

        assert(!original || saved);
        assert(!setenv("TZ", "UTC-5:30", 1));
        assert(!qjs_timezone_system_default(result, sizeof(result), 0));
        assert(!strcmp(result, "+05:30"));
        strcpy(result, "unchanged");
        assert(qjs_timezone_system_default(result, 4, 0) == QJS_TZ_RANGE);
        assert(!strcmp(result, "unchanged"));
        /* Both seasons have +03:00; the general rule is deliberately not
           interpreted by native discovery, but libc can supply a snapshot. */
        assert(!setenv("TZ", "XXX-3YYY-3,M3.2.0,M11.1.0", 1));
        assert(!qjs_timezone_system_default(result, sizeof(result), 0));
        assert(!strcmp(result, "UTC"));
        assert(!qjs_timezone_system_default(result, sizeof(result), 1));
        assert(!strcmp(result, "+03:00"));
        if (saved) {
            assert(!setenv("TZ", saved, 1));
            free(saved);
        } else {
            assert(!unsetenv("TZ"));
        }
        tzset();
    }
#endif
    return 0;
}
