/* Build-host export of explicitly approximate, unpublished boundary years.
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
