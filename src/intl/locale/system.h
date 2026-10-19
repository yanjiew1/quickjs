/* Host locale discovery, separate from canonicalization and service support.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
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
