/* Shared runtime time-zone policy. Copyright (c) 2026 Yan-Jie Wang. MIT. */
#ifndef QUICKJS_DEFAULT_TIME_ZONE_H
#define QUICKJS_DEFAULT_TIME_ZONE_H
#ifdef CONFIG_TEMPORAL
#include "../../timezone/identifier.h"
/* Return backend errors without setting a pending exception. */
int js_default_time_zone_get(JSContext *, QJSTimeZoneIdentifier *);
#endif
#endif
