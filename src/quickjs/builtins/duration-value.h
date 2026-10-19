/* Shared ECMAScript duration conversion, without allocating Temporal objects.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QUICKJS_BUILTINS_DURATION_VALUE_H
#define QUICKJS_BUILTINS_DURATION_VALUE_H

#include "../internal/base.h"
#include "../../temporal/duration.h"

int js_duration_to_integral_field(JSContext *ctx, JSValueConst value,
                                  double *result);
/* Update existing fields in alphabetical property order. Undefined leaves a
   field unchanged; throw if no recognized property has a defined value.
   May partially update on error. */
int js_duration_partial_fields(JSContext *ctx, JSValueConst object,
                               QJSTemporalDuration *fields);
/* ToTemporalDuration's validated slots, with no temporary JS object or
   intrinsic initialization. Copy branded slots when Temporal is enabled.
   Otherwise accept strings and duration-like objects. Leave out unchanged
   on failure; all returned zero fields are positive zero. */
int js_duration_to_record(JSContext *ctx, JSValueConst value,
                          QJSTemporalDuration *out);

#endif
