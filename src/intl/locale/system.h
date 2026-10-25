/* Host locale discovery, separate from canonicalization and service support.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_INTL_LOCALE_SYSTEM_H
#define QJS_INTL_LOCALE_SYSTEM_H
#include "../common/types.h"

/* Parse the POSIX name before its encoding/modifier suffix, replacing '_' by
   '-'. C/POSIX selects English. No syntax/alias/capability claim is made here.
   Output is NUL-terminated only on success; failure leaves it unchanged. */
QJSIntlStatus qjs_intl_locale_parse_environment(char *out, size_t capacity,
                                                const char *value);
/* On Unix use the first nonempty LC_ALL, LC_MESSAGES, LANG. On Windows query
   the user's locale name without modifying process-global libc locale state.
   UNSUPPORTED means no usable host name; caller owns its fallback policy. */
QJSIntlStatus qjs_intl_locale_system_default(char *out, size_t capacity);
#endif
