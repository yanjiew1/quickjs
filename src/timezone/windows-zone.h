/* Windows native time-zone discovery and the compiled CLDR mapping. */
#ifndef QJS_WINDOWS_ZONE_H
#define QJS_WINDOWS_ZONE_H
#include <stddef.h>
#if defined(_WIN32) || defined(QJS_TZ_WINDOWS_TEST)
/* ASCII case-insensitive Windows key; uppercase ISO country or NULL for 001.
   Unknown territories fall back to 001. Returns 0, or -1 with empty output. */
int qjs_tz_windows_lookup_identifier(const char *key, const char *territory,
                                     char *output, size_t capacity);
#endif
#ifdef _WIN32
int qjs_tz_windows_system_identifier(char *output, size_t capacity);
#endif
#endif
