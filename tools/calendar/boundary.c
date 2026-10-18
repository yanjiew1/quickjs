/* Build-host export of explicitly approximate, unpublished boundary years.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "../../src/calendar/lunisolar.h"
#include <stdio.h>

int main(void)
{
    const QJSCalendarId calendars[] = {QJS_CAL_CHINESE, QJS_CAL_DANGI};
    const char *const names[] = {"chinese", "dangi"};
    const int years[] = {1899, 2050};
    QJSCalendarYear values[2];
    int i, month;

    /* Finish both calculations before writing any result. The consumer may
       accept only months entirely outside the mandatory Gregorian windows. */
    for (i = 0; i < 2; i++) {
        if (qjs_cal_lunisolar_approximate_year(calendars[i], years[i], &values[i]))
            return 1;
    }
    printf("{\"schema\":1,\"role\":\"approximation\",\"years\":[");
    for (i = 0; i < 2; i++) {
        const QJSCalendarYear *value = &values[i];

        printf("%s{\"calendar\":\"%s\",\"year\":%d,\"first_day\":%ld,"
               "\"end_day\":%ld,\"leap_month\":%u,\"months\":[",
               i ? "," : "", names[i], years[i], (long)value->first_day,
               (long)value->end_day, (unsigned)value->leap_month);
        for (month = 0; month < value->months; month++)
            printf("%s%u", month ? "," : "",
                   29 + ((value->month_lengths >> (2 * month)) & 3));
        printf("]}");
    }
    printf("]}\n");
    return ferror(stdout) ? 1 : 0;
}
