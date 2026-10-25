/* ICU4X identity admission and Windows territorial mapping regressions.
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
#include "../src/timezone/iana.h"
#include "../src/timezone/windows-zone.h"
#include "../src/icu4x/timezone.h"
#include <assert.h>
#include <string.h>

static const QJSTimeZone *lookup(const char *name)
{
    const QJSTimeZone *zone;
    assert(!qjs_timezone_lookup(&zone, name, strlen(name)));
    return zone;
}

static void windows(const char *key, const char *country, const char *primary)
{
    char name[256];
    assert(!qjs_tz_windows_lookup_identifier(key, country, name, sizeof(name)));
    assert(!strcmp(qjs_timezone_primary_identifier(lookup(name)), primary));
}

int main(void)
{
    const QJSTimeZone *zone = lookup("factory"), *saved;
    QJSICU4XZoneName name, snapshot;
    int32_t offset;
    size_t i;
    char unchanged[16] = "unchanged";

    /* Factory is an actual IANA zone, not ICU's invalid Etc/Unknown sentinel
       or a spelling of UTC. Its real rules happen to have a zero offset. */
    assert(!strcmp(zone->identifier, "Factory"));
    assert(!strcmp(zone->primary, "Factory"));
    assert(zone->rules);
    assert(!qjs_timezone_offset(zone, 0, &offset) && offset == 0);
    saved = zone;
    assert(qjs_timezone_lookup(&zone, "Canada/East-Saskatchewan", 24) == QJS_TZ_RANGE);
    assert(zone == saved);
    assert(qjs_timezone_lookup(&zone, "US/Pacific-New", 14) == QJS_TZ_RANGE);
    assert(zone == saved);
    assert(qjs_timezone_lookup(&zone, "Etc/Unknown", 11) == QJS_TZ_RANGE);
    assert(zone == saved);

    /* Every enumerated ID has the exact admitted spelling and a stable handle.
       Do not accept a foreign alias by silently changing it to another name. */
    for (i = 0; i < qjs_timezone_count(); i++) {
        const QJSTimeZone *entry = qjs_timezone_at(i);
        assert(entry && lookup(qjs_timezone_identifier(entry)) == entry);
    }
    assert(!strcmp(qjs_timezone_identifier(lookup("Asia/CALCUTTA")), "Asia/Calcutta"));
    assert(
        !strcmp(qjs_timezone_primary_identifier(lookup("Europe/Bratislava")), "Europe/Bratislava"));

    windows("Central Europe Standard Time", "SK", "Europe/Bratislava");
    windows("Central Europe Standard Time", "CZ", "Europe/Prague");
    windows("India Standard Time", "IN", "Asia/Kolkata");
    windows("Taipei Standard Time", "TW", "Asia/Taipei");
    windows("pacific standard time", "US", "America/Los_Angeles");
    windows("Pacific Standard Time", "TW", "America/Los_Angeles");
    windows("Pacific Standard Time", NULL, "America/Los_Angeles");
    assert(qjs_tz_windows_lookup_identifier("unknown", "US", unchanged, sizeof(unchanged)));
    assert(!strcmp(unchanged, "unchanged"));
    assert(qjs_tz_windows_lookup_identifier("Pacific Standard Time", "US", unchanged, 1));
    assert(!strcmp(unchanged, "unchanged"));

    memset(&name, 7, sizeof(name));
    snapshot = name;
    assert(qjs_icu4x_timezone_normalize(NULL, 1, &name) == QJS_ICU4X_RANGE);
    assert(!memcmp(&name, &snapshot, sizeof(name)));
    assert(qjs_icu4x_timezone_normalize(NULL, 0, &name) == QJS_ICU4X_UNSUPPORTED);
    assert(!memcmp(&name, &snapshot, sizeof(name)));
    return 0;
}
