/* Ordinary and bounded Intl domains share checked fields.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, project LICENSE. */
#include "../src/calendar/calendar.h"
#include "../src/calendar/lunisolar.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    QJSCalendarDate ordinary, intl, before;
    int id, side, k, status;
    memset(&before, 0xa5, sizeof(before));
    for (id = 0; id < QJS_CAL_COUNT; id++) {
        if (id == QJS_CAL_CHINESE || id == QJS_CAL_DANGI) continue;
        for (side = 0; side < 2; side++) {
            int64_t edge = side ? QJS_CAL_MAX_EPOCH_DAY : QJS_CAL_MIN_EPOCH_DAY;
            for (k = 0; k <= 34; k++) {
                int64_t day = edge + (side ? k : -k);
                ordinary = before; intl = before;
                status = qjs_calendar_from_epoch_day_for_intl((QJSCalendarId)id, day, &intl);
                if (day >= QJS_CAL_INTL_MIN_EPOCH_DAY && day <= QJS_CAL_INTL_MAX_EPOCH_DAY) {
                    assert(status == QJS_CAL_OK);
                    assert(intl.day >= 1 && intl.day <= intl.days_in_month);
                } else {
                    assert(status == QJS_CAL_RANGE && !memcmp(&intl, &before, sizeof(intl)));
                }
                status = qjs_calendar_from_epoch_day((QJSCalendarId)id, day, &ordinary);
                if (!k) {
                    assert(status == QJS_CAL_OK && !memcmp(&ordinary, &intl, sizeof(intl)));
                } else {
                    assert(status == QJS_CAL_RANGE && !memcmp(&ordinary, &before, sizeof(ordinary)));
                }
            }
        }
    }
    /* Extreme lunisolar approximation can fail numerically. Intl range
     * acceptance never promises physical accuracy or a valid extreme year. */
    for (id = QJS_CAL_CHINESE; id <= QJS_CAL_DANGI; id += QJS_CAL_DANGI - QJS_CAL_CHINESE) {
        for (side = 0; side < 2; side++) {
            int64_t edge = side ? QJS_CAL_INTL_MAX_EPOCH_DAY : QJS_CAL_INTL_MIN_EPOCH_DAY;
            intl = before;
            status = qjs_calendar_from_epoch_day_for_intl((QJSCalendarId)id, edge, &intl);
            assert(status == QJS_CAL_OK || status == QJS_CAL_BACKEND);
            if (status) assert(!memcmp(&intl, &before, sizeof(intl)));
            ordinary = before;
            assert(qjs_calendar_from_epoch_day((QJSCalendarId)id, edge, &ordinary) == QJS_CAL_RANGE);
            assert(!memcmp(&ordinary, &before, sizeof(ordinary)));
            intl = before;
            assert(qjs_calendar_from_epoch_day_for_intl((QJSCalendarId)id, edge + (side ? 1 : -1), &intl) == QJS_CAL_RANGE);
            assert(!memcmp(&intl, &before, sizeof(intl)));
        }
    }
    return 0;
}
