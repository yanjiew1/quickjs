/* Shared IANA rule descriptors over one generated immutable binary owner.
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
#ifndef QUICKJS_TIMEZONE_IANA_H
#define QUICKJS_TIMEZONE_IANA_H

#include "timezone-backend.h"
#include "tzif.h"

struct QJSTimeZoneRules {
    QJSTzifState tzif;
};

extern const unsigned char qjs_iana_timezone_data[];
extern const size_t qjs_iana_timezone_data_size;
/* Canonical UTC is excluded; core supplies it. UTC aliases have NULL rules. */
extern const QJSTimeZone qjs_iana_timezone_zones[];
extern const size_t qjs_iana_timezone_zone_count;
extern const uint16_t qjs_iana_timezone_lookup_order[];
extern const char *const qjs_iana_timezone_data_version;

int qjs_iana_timezone_lookup(const QJSTimeZone **result, const char *identifier, size_t length);
const char *qjs_iana_timezone_version(void);
size_t qjs_iana_timezone_count(void);
const QJSTimeZone *qjs_iana_timezone_at(size_t index);

#endif /* QUICKJS_TIMEZONE_IANA_H */
