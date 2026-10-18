/* Windows host identification, independent of timezone rule storage.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_WINDOWS_ZONE_H
#define QUICKJS_WINDOWS_ZONE_H
#include <stddef.h>

/* CLDR territory match, then 001, choosing the first ordered IANA name.
   Failures preserve output; successful names still require backend lookup. */
int qjs_tz_windows_lookup_identifier(const char *key, const char *territory,
                                     char *output, size_t capacity);
#ifdef _WIN32
int qjs_tz_windows_system_identifier(char *output, size_t capacity);
#endif
#endif
