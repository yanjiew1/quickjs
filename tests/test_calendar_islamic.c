/* Tabular Hijri epochs, cyclic arithmetic and invalid-input preservation. */
#include "../src/calendar/arithmetic.h"
#include "../src/calendar/iso.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>

static void witness(QJSCalendarId calendar, int32_t year, QJSISODate date)
{
    int64_t actual, expected;

    assert(!qjs_iso_date_to_days(&expected, date));
    assert(!qjs_cal_islamic_year_start(calendar, year, &actual));
    assert(actual == expected);
}

int main(void)
{
    const int32_t years[] = {QJS_CAL_MIN_YEAR, -271821, -31, -1, 0, 1, 2, 30,
                             1446, 275760, QJS_CAL_MAX_YEAR};
    size_t i;
    int64_t saved = 123;

    witness(QJS_CAL_ISLAMIC_CIVIL, 1, (QJSISODate){622, 7, 19});
    witness(QJS_CAL_ISLAMIC_TBLA, 1, (QJSISODate){622, 7, 18});
    witness(QJS_CAL_ISLAMIC_CIVIL, 1445, (QJSISODate){2023, 7, 19});
    witness(QJS_CAL_ISLAMIC_CIVIL, 1446, (QJSISODate){2024, 7, 8});
    witness(QJS_CAL_ISLAMIC_UMALQURA, 1300, (QJSISODate){1882, 11, 12});
    witness(QJS_CAL_ISLAMIC_UMALQURA, 1446, (QJSISODate){2024, 7, 7});
    witness(QJS_CAL_ISLAMIC_UMALQURA, 1601, (QJSISODate){2174, 11, 26});
    for (int year = 1299; year <= 1601; year++) {
        int64_t first, next, civil;
        int month, days = 0;

        assert(!qjs_cal_islamic_year_start(QJS_CAL_ISLAMIC_UMALQURA, year, &first));
        assert(!qjs_cal_islamic_year_start(QJS_CAL_ISLAMIC_UMALQURA, year + 1, &next));
        for (month = 1; month <= 12; month++) {
            int length = qjs_cal_islamic_month_length(QJS_CAL_ISLAMIC_UMALQURA, year, month);

            assert(length == 29 || length == 30);
            days += length;
            if (year < 1300 || year > 1600)
                assert(length == qjs_cal_islamic_month_length(QJS_CAL_ISLAMIC_CIVIL, year, month));
        }
        assert(next - first == days);
        if (year <= 1300 || year >= 1601) {
            assert(!qjs_cal_islamic_year_start(QJS_CAL_ISLAMIC_CIVIL, year, &civil));
            assert(first == civil);
        }
    }
    for (i = 0; i < sizeof(years) / sizeof(years[0]); i++) {
        int64_t first, next, tabular;
        int month, length, total = 0;

        assert(!qjs_cal_islamic_year_start(QJS_CAL_ISLAMIC_CIVIL, years[i], &first));
        assert(!qjs_cal_islamic_year_start(QJS_CAL_ISLAMIC_CIVIL, years[i] + 1, &next));
        assert(!qjs_cal_islamic_year_start(QJS_CAL_ISLAMIC_TBLA, years[i], &tabular));
        assert(first - tabular == 1);
        if (years[i] < 1300 || years[i] > 1600) {
            assert(!qjs_cal_islamic_year_start(QJS_CAL_ISLAMIC_UMALQURA, years[i], &tabular));
            assert(first == tabular);
        }
        for (month = 1; month <= 12; month++) {
            length = qjs_cal_islamic_month_length(QJS_CAL_ISLAMIC_CIVIL, years[i], month);
            assert(length == 29 || length == 30);
            assert(qjs_cal_islamic_month_length(QJS_CAL_ISLAMIC_TBLA, years[i], month) == length);
            total += length;
        }
        assert(next - first == total && (total == 354 || total == 355));
        if (years[i] <= QJS_CAL_MAX_YEAR - 30) {
            assert(!qjs_cal_islamic_year_start(QJS_CAL_ISLAMIC_CIVIL, years[i] + 30, &next));
            assert(next - first == 10631);
        }
    }
    assert(qjs_cal_islamic_year_start(QJS_CAL_ISLAMIC_CIVIL, INT32_MIN, &saved) == QJS_CAL_RANGE);
    assert(saved == 123);
    assert(qjs_cal_islamic_year_start(QJS_CAL_ISLAMIC_TBLA, INT32_MAX, &saved) == QJS_CAL_RANGE);
    assert(saved == 123);
    assert(qjs_cal_islamic_year_start(QJS_CAL_HEBREW, 1, &saved) == QJS_CAL_UNSUPPORTED);
    assert(saved == 123);
    assert(!qjs_cal_islamic_month_length(QJS_CAL_ISLAMIC_CIVIL, INT32_MAX, 1));
    assert(!qjs_cal_islamic_month_length(QJS_CAL_ISLAMIC_CIVIL, 1, 0));
    assert(!qjs_cal_islamic_month_length(QJS_CAL_ISLAMIC_TBLA, 1, 13));
    assert(!qjs_cal_islamic_month_length(QJS_CAL_HEBREW, 1, 1));
    return 0;
}
