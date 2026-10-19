/* Exact JS coercion shared by numeric Intl bindings.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QUICKJS_INTL_NUMBER_VALUE_H
#define QUICKJS_INTL_NUMBER_VALUE_H
#include "../../internal/base.h"
#include "../../../intl/common/number-decimal.h"

/* ToIntlMathematicalValue, including one number-hint ToPrimitive. Finite
   strings/BigInts retain exact decimal text; Numbers use shortest decimal.
   String overflow/underflow follows RoundMVResult before preserving text.
   Borrow input. Fresh, disjoint output required; failure resets it. Clear
   successful results with qjs_intl_number_decimal_clear and the runtime's
   js_intl_allocator. No formatter or locale/backend state is involved. */
int js_intl_to_mathematical_value(JSContext *, JSValueConst, QJSIntlNumberDecimal *);
#endif
