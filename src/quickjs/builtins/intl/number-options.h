/* Observable digit-option processing shared by numeric Intl bindings.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QUICKJS_INTL_NUMBER_OPTIONS_H
#define QUICKJS_INTL_NUMBER_OPTIONS_H
#include "../../internal/base.h"
#include "../../../intl/common/decimal.h"

typedef struct JSIntlDigitOptions {
    int minimum_integer_digits;
    /* -1 means the slot is absent, not an effective rounding default. */
    int minimum_fraction_digits, maximum_fraction_digits;
    int minimum_significant_digits, maximum_significant_digits;
    int rounding_increment;
    QJSIntlRoundingMode rounding_mode;
    QJSIntlRoundingType rounding_type;
    QJSIntlTrailingZeroDisplay trailing_zero_display;
    int computed_rounding_priority; /* 0 auto, 1 morePrecision, 2 lessPrecision */
} JSIntlDigitOptions;

/* options is an object already obtained by the caller. Defaults are integers
   in 0..100, minimum <= maximum; compact selects the notation-specific defaults.
   Reads/coercions follow SetNumberFormatDigitOptions. Borrow options; publish
   the output only on success and reset it on failure. No backend is involved. */
int js_intl_set_digit_options(JSContext *ctx, JSValueConst options, int minimum_fraction_default,
                              int maximum_fraction_default, BOOL compact, JSIntlDigitOptions *out);
#endif
