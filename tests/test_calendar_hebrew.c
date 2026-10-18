/* Hebrew postponement equalities and bounded proleptic invariants. */
#include "../src/calendar/arithmetic.h"
#include "../src/calendar/iso.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>

static void witness(int32_t year, int64_t expected, int length)
{
    int64_t first, next;
    int month, days, total = 0;

    assert(!qjs_cal_hebrew_year_start(year, &first));
    assert(!qjs_cal_hebrew_year_start(year + 1, &next));
    assert(first == expected && next - first == length);
    for (month = 1; (days = qjs_cal_hebrew_month_length(year, month)); month++) {
        assert(days == 29 || days == 30);
        total += days;
    }
    assert(total == length && month - 1 == 12 + qjs_cal_hebrew_leap(year));
}

int main(void)
{
    const int32_t years[] = {QJS_CAL_MIN_YEAR, -271821, -1, 0, 1, 5784, 5785,
                             275760, QJS_CAL_MAX_YEAR};
    int64_t day, saved = 123;
    int32_t year;
    size_t i;

    /* Equality witnesses cover both rare postponement thresholds. */
    witness(-114911, -44063831, 354);
    witness(-102335, -39470496, 354);
    witness(88369, 30183524, 383);
    witness(88370, 30183907, 354);
    witness(193151, 68454820, 354);
    assert(!qjs_iso_date_to_days(&day, (QJSISODate){2023, 9, 16}));
    witness(5784, day, 383);
    assert(!qjs_iso_date_to_days(&day, (QJSISODate){2024, 10, 3}));
    witness(5785, day, 355);
    for (i = 0; i < sizeof(years) / sizeof(years[0]); i++) {
        int64_t first, next;
        assert(!qjs_cal_hebrew_year_start(years[i], &first));
        assert(!qjs_cal_hebrew_year_start(years[i] + 1, &next));
        witness(years[i], first, (int)(next - first));
    }
    for (year = -271821; year < 275760; year += 137) {
        int64_t first, next;
        int leap = qjs_cal_hebrew_leap(year);

        assert(!qjs_cal_hebrew_year_start(year, &first));
        assert(!qjs_cal_hebrew_year_start(year + 1, &next));
        assert(next - first >= (leap ? 383 : 353) &&
               next - first <= (leap ? 385 : 355));
        assert(qjs_cal_hebrew_months_before_year(year + 1) -
               qjs_cal_hebrew_months_before_year(year) == 12 + leap);
    }
    assert(qjs_cal_hebrew_year_start(INT32_MIN, &saved) == QJS_CAL_RANGE);
    assert(saved == 123);
    assert(qjs_cal_hebrew_year_start(INT32_MAX, &saved) == QJS_CAL_RANGE);
    assert(saved == 123);
    assert(!qjs_cal_hebrew_month_length(INT32_MAX, 1));
    assert(!qjs_cal_hebrew_month_length(5784, 0));
    assert(!qjs_cal_hebrew_month_length(5784, 14));
    assert(!qjs_cal_hebrew_month_length(5785, 13));
    (void)qjs_cal_hebrew_leap(INT32_MIN);
    (void)qjs_cal_hebrew_leap(INT32_MAX);
    (void)qjs_cal_hebrew_months_before_year(INT32_MIN);
    (void)qjs_cal_hebrew_months_before_year(INT32_MAX);
    return 0;
}
