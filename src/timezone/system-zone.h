/* Host default discovery, separate from immutable timezone rule data.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
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
