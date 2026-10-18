/* Era boundaries and lenient resolution, independent of Temporal objects. */
#include "../src/calendar/eras.h"
#include "../src/calendar/iso.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

static void check(QJSCalendarId calendar, int32_t year, const char *era,
                  int32_t era_year)
{
    QJSCalendarDate date = {0};
    int32_t resolved = 123;

    date.year = year;
    qjs_cal_set_era(calendar, 0, &date);
    assert(date.has_era && !strcmp(date.era, era) && date.era_year == era_year);
    assert(!qjs_cal_year_from_era(calendar, era, era_year, &resolved));
    assert(resolved == year);
}

int main(void)
{
    static const struct {
        QJSISODate iso;
        const char *before, *after;
        int before_year, after_year;
    } boundaries[] = {
        {{1873, 1, 1}, "ce", "meiji", 1872, 6},
        {{1912, 7, 30}, "meiji", "taisho", 45, 1},
        {{1926, 12, 25}, "taisho", "showa", 15, 1},
        {{1989, 1, 8}, "showa", "heisei", 64, 1},
        {{2019, 5, 1}, "heisei", "reiwa", 31, 1},
    };
    QJSCalendarDate date = {0};
    int32_t year = 123;
    size_t i;

    check(QJS_CAL_GREGORY, 0, "bce", 1);
    check(QJS_CAL_GREGORY, -1, "bce", 2);
    check(QJS_CAL_ROC, 0, "broc", 1);
    check(QJS_CAL_ETHIOPIC, 0, "aa", 5500);
    check(QJS_CAL_ETHIOPIC, -5501, "aa", -1);
    check(QJS_CAL_ETHIOAA, -1, "aa", -1);
    check(QJS_CAL_BUDDHIST, 0, "be", 0);
    check(QJS_CAL_COPTIC, -1, "am", -1);
    check(QJS_CAL_HEBREW, 0, "am", 0);
    check(QJS_CAL_INDIAN, 0, "shaka", 0);
    check(QJS_CAL_PERSIAN, -1, "ap", -1);
    check(QJS_CAL_ISLAMIC_CIVIL, 0, "bh", 1);
    check(QJS_CAL_ISLAMIC_TBLA, -1, "bh", 2);
    check(QJS_CAL_ISLAMIC_UMALQURA, 1, "ah", 1);
    for (i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); i++) {
        QJSISODate before;
        int64_t day;

        assert(!qjs_iso_date_to_days(&day, boundaries[i].iso));
        date.year = boundaries[i].iso.year;
        qjs_cal_set_era(QJS_CAL_JAPANESE, day, &date);
        assert(!strcmp(date.era, boundaries[i].after));
        assert(date.era_year == boundaries[i].after_year);
        assert(!qjs_iso_date_from_days(&before, day - 1));
        date.year = before.year;
        qjs_cal_set_era(QJS_CAL_JAPANESE, day - 1, &date);
        assert(!strcmp(date.era, boundaries[i].before));
        assert(date.era_year == boundaries[i].before_year);
    }
    assert(!qjs_cal_year_from_era(QJS_CAL_JAPANESE, "heisei", 32, &year));
    assert(year == 2020);
    assert(!qjs_cal_year_from_era(QJS_CAL_GREGORY, "ad", 0, &year) && !year);
    assert(!qjs_cal_year_from_era(QJS_CAL_GREGORY, "bc", -1, &year) && year == 2);
    year = 123;
    assert(qjs_cal_year_from_era(QJS_CAL_JAPANESE, "Heisei", 1, &year) == QJS_CAL_RANGE);
    assert(year == 123);
    assert(qjs_cal_year_from_era(QJS_CAL_JAPANESE, "reiwa", INT32_MAX, &year) == QJS_CAL_RANGE);
    assert(qjs_cal_year_from_era(QJS_CAL_ETHIOPIC, "aa", INT32_MIN, &year) == QJS_CAL_RANGE);
    assert(qjs_cal_year_from_era(QJS_CAL_GREGORY, "bce", INT32_MIN, &year) == QJS_CAL_RANGE);
    assert(year == 123);
    for (i = QJS_CAL_CHINESE; i <= QJS_CAL_DANGI; i += QJS_CAL_DANGI - QJS_CAL_CHINESE) {
        qjs_cal_set_era((QJSCalendarId)i, 0, &date);
        assert(!date.has_era && !date.era[0] && !date.era_year);
    }
    return 0;
}
