/* Shared CLDR vocabulary; independent of names and clock-rule policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_DAYPERIOD_CODE_H
#define QJS_INTL_DATETIME_DAYPERIOD_CODE_H
#include "../common/types.h"

/* Version-1 wire order, matching tools/intl/dayperiod_codes.py. */
typedef enum QJSIntlDayPeriodCode {
    QJS_INTL_DAYPERIOD_AM,
    QJS_INTL_DAYPERIOD_PM,
    QJS_INTL_DAYPERIOD_MIDNIGHT,
    QJS_INTL_DAYPERIOD_NOON,
    QJS_INTL_DAYPERIOD_MORNING1,
    QJS_INTL_DAYPERIOD_MORNING2,
    QJS_INTL_DAYPERIOD_AFTERNOON1,
    QJS_INTL_DAYPERIOD_AFTERNOON2,
    QJS_INTL_DAYPERIOD_EVENING1,
    QJS_INTL_DAYPERIOD_EVENING2,
    QJS_INTL_DAYPERIOD_NIGHT1,
    QJS_INTL_DAYPERIOD_NIGHT2,
    QJS_INTL_DAYPERIOD_CODE_COUNT
} QJSIntlDayPeriodCode;

/* Borrow static ASCII names. Unknown code yields empty bytes; unknown name -1. */
QJSIntlBytes qjs_intl_dayperiod_code_name(uint32_t code);
int qjs_intl_dayperiod_code_find(QJSIntlBytes name);
#endif
