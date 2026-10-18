/* Shared mathematical division for calendar arithmetic; divisors are positive.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_CALENDAR_ARITHMETIC_INTERNAL_H
#define QUICKJS_CALENDAR_ARITHMETIC_INTERNAL_H
#include <stdint.h>

static inline int64_t qjs_cal_floor_divide(int64_t value, int64_t divisor)
{
    return value / divisor - (value % divisor < 0);
}

static inline int64_t qjs_cal_floor_modulo(int64_t value, int64_t divisor)
{
    int64_t remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}
#endif
