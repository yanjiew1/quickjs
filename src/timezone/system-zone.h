/* Host default discovery, separate from immutable timezone rule data.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#ifndef QUICKJS_SYSTEM_ZONE_H
#define QUICKJS_SYSTEM_ZONE_H
#include <stddef.h>

/* Normalize a Unix TZ name (optional colon) or fixed POSIX specification.
   General POSIX DST rules and filesystem paths are not interpreted. */
int qjs_timezone_parse_environment(char *result, size_t capacity,
                                    const char *value);
/* Discover a supported primary name, then optionally sample the host's
   current offset, then UTC. Named discovery reads no installed rule files;
   the opt-in host-offset fallback can use libc's installed host data.
   All failures preserve result. */
int qjs_timezone_system_default(char *result, size_t capacity,
                                int host_offset_fallback);
#endif
