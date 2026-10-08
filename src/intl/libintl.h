/*
 * QuickJS native internationalization support
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 * Copyright (c) 2026 Yan-Jie Wang
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
#ifndef QUICKJS_LIBINTL_H
#define QUICKJS_LIBINTL_H
/* ICU data/lifecycle policy, independent of JavaScript values and allocators. */
#include <stddef.h>
#include <unicode/utypes.h>
#include <unicode/uversion.h>
#if U_ICU_VERSION_MAJOR_NUM < 78 || \
    (U_ICU_VERSION_MAJOR_NUM == 78 && U_ICU_VERSION_MINOR_NUM < 3)
#error CONFIG_INTL requires ICU4C 78.3 or later
#endif

typedef struct IntlBackendVersions {
    UVersionInfo icu;
    UVersionInfo unicode;
    UVersionInfo data;
    UVersionInfo cldr;
    const char *tzdata; /* Borrowed ICU static data. */
} IntlBackendVersions;

/* ICU owns its process-global data. Never call u_cleanup from engine teardown,
   install global allocator hooks, or change ICU's process-global defaults. */
void intl_backend_initialize(UErrorCode *status);
/* Remove the Unicode extension in place, preserving private-use subtags.
   The input is a validated canonical tag owned by the caller. */
int intl_remove_unicode_extension(char *canonical_tag);
void intl_backend_versions(IntlBackendVersions *versions, UErrorCode *status);
const char *intl_backend_default_locale(void);
/* Parse ICU GMT offsets or canonical system +/-HH:MM identifiers. Returns
   1 for a valid fixed offset, 0 for a named identifier, -1 for invalid offsets. */
int intl_backend_parse_offset_time_zone(const char *identifier, size_t length,
                                        char result[7]);
int32_t intl_backend_default_time_zone(UChar *buffer, int32_t capacity,
                                       UErrorCode *status);
#endif
