/* Shared runtime time-zone policy. Copyright (c) 2026 Yan-Jie Wang. MIT. */
#ifndef QUICKJS_DEFAULT_TIME_ZONE_H
#define QUICKJS_DEFAULT_TIME_ZONE_H
#if defined(CONFIG_TEMPORAL) || defined(CONFIG_INTL)
#include "../../timezone/identifier.h"
/* Return pure backend errors. These helpers do not set pending exceptions. */
int js_default_time_zone_identifier(JSContext *, const char **);
int js_default_time_zone_get(JSContext *, QJSTimeZoneIdentifier *);
#endif
#endif
