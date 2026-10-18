/* A synthetic sixth era proves lookup uses data rather than five C entries. */
#include "../src/calendar/data.h"
#include "../src/calendar/eras.h"
#include "../src/calendar/iso.h"
#include <assert.h>
#include <string.h>

/* One Japanese dataset, a padded source-version string and six era records.
   Fill this test fixture before exposing its immutable compiled view. */
static uint8_t bytes[72 + 6 * 40];
const QJSCalendarData qjs_native_calendar_data = {bytes, sizeof(bytes), 1};

static void put_u32(size_t at, uint32_t value)
{
    bytes[at] = value >> 24;
    bytes[at + 1] = value >> 16;
    bytes[at + 2] = value >> 8;
    bytes[at + 3] = value;
}

int main(void)
{
    static const struct {
        const char *name;
        QJSISODate start;
        int offset;
    } records[] = {
        {"one", {1873, 1, 1}, 1867},
        {"two", {1900, 1, 1}, 1899},
        {"three", {1920, 1, 1}, 1919},
        {"four", {1940, 1, 1}, 1939},
        {"five", {1960, 1, 1}, 1959},
        {"future", {2035, 2, 3}, 2034},
    };
    QJSCalendarData checked;
    QJSCalendarDate date = {0};
    int64_t day = 0;
    int32_t year = 0;
    size_t i;

    memcpy(bytes, "QJCA", 4);
    put_u32(4, 2);
    put_u32(8, sizeof(bytes));
    put_u32(12, 1);
    put_u32(16, 32);
    put_u32(20, 64);
    put_u32(24, 72);
    put_u32(28, 1);
    put_u32(32, 50);
    put_u32(40, 6);
    put_u32(48, 72);
    put_u32(52, 6 * 40);
    put_u32(56, 64);
    memcpy(bytes + 64, "test", 5);
    for (i = 0; i < sizeof(records) / sizeof(records[0]); i++) {
        assert(!qjs_iso_date_to_days(&day, records[i].start));
        put_u32(72 + i * 40, (uint32_t)day);
        put_u32(76 + i * 40, records[i].offset);
        strcpy((char *)bytes + 80 + i * 40, records[i].name);
    }
    assert(!qjs_calendar_data_open(&checked, bytes, sizeof(bytes)));
    assert(qjs_calendar_data_japanese_era_count(&checked) == 6);
    date.year = 2035;
    qjs_cal_set_era(QJS_CAL_JAPANESE, day - 1, &date);
    assert(!strcmp(date.era, "five") && date.era_year == 76);
    qjs_cal_set_era(QJS_CAL_JAPANESE, day, &date);
    assert(!strcmp(date.era, "future") && date.era_year == 1);
    assert(!qjs_cal_year_from_era(QJS_CAL_JAPANESE, "future", 3, &year));
    assert(year == 2037);
    return 0;
}
