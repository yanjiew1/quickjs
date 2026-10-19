/* Exact duration component values for the Number service.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DURATION_DECIMAL_H
#define QJS_INTL_DURATION_DECIMAL_H
#include "../common/types.h"
#include "../../temporal/types.h"

/* Convert one field of a previously validated duration to exact decimal text.
   If fractional is nonzero, unit must be second, millisecond or microsecond;
   include all smaller fields through nanoseconds at successive powers of 1000.
   Perform no binary64 addition/division or allocation. The Number service owns
   rounding and sign-display policy, including a displayed negative zero.
   Capacity 64 covers every valid duration component with its sign, decimal
   point and terminating NUL. Output borrows buffer; fresh disjoint storage is
   required. Failure resets *out and leaves buffer unchanged. Zero is positive.
   Check duration validity once at the format entry, not once per component. */
QJSIntlStatus qjs_intl_duration_component_decimal(const QJSTemporalDuration *, QJSTemporalUnit unit,
                                                 int fractional, char *buffer, size_t capacity,
                                                 QJSIntlMathematicalValue *out);
#endif
